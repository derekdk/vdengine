#include "ExecutableScanner.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <condition_variable>
#include <fstream>
#include <optional>
#include <regex>
#include <set>
#include <string_view>
#include <unordered_set>

#include "GitUtils.h"
#include <toml++/toml.hpp>

namespace vde::tools {

namespace {

constexpr std::array<std::string_view, 6> kSourceExtensions = {".cpp", ".cxx", ".cc",
                                                               ".h",   ".hpp", ".inl"};

bool hasKnownSourceExtension(const std::filesystem::path& path) {
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
    return std::find(kSourceExtensions.begin(), kSourceExtensions.end(), std::string_view(ext)) !=
           kSourceExtensions.end();
}

std::string trim(const std::string& value) {
    size_t start = value.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return {};
    }

    size_t end = value.find_last_not_of(" \t\r\n");
    return value.substr(start, end - start + 1);
}

std::filesystem::path normalizePath(const std::filesystem::path& path) {
    std::error_code error;
    std::filesystem::path absolutePath = std::filesystem::absolute(path, error);
    if (error) {
        return path;
    }
    return absolutePath.lexically_normal();
}

std::optional<std::string> sanitizeSmokeScriptName(const std::string& rawScript) {
    if (rawScript.empty()) {
        return std::nullopt;
    }

    if (rawScript.find('/') != std::string::npos || rawScript.find('\\') != std::string::npos) {
        return std::nullopt;
    }

    const std::filesystem::path scriptPath(rawScript);
    if (scriptPath.empty() || scriptPath.is_absolute() || scriptPath.has_parent_path()) {
        return std::nullopt;
    }

    for (const auto& part : scriptPath) {
        if (part.string() == "..") {
            return std::nullopt;
        }
    }

    const std::string fileName = scriptPath.filename().string();
    if (fileName.empty() || fileName == "." || fileName == "..") {
        return std::nullopt;
    }

    return fileName;
}

}  // namespace

ExecutableScanner::ExecutableScanner(std::filesystem::path startPath,
                                     std::chrono::seconds idleInterval,
                                     std::chrono::seconds fastInterval)
    : m_startPath(std::move(startPath)), m_idleInterval(idleInterval),
      m_fastInterval(fastInterval) {}

ExecutableScanner::~ExecutableScanner() {
    stop();
}

void ExecutableScanner::start() {
    std::lock_guard<std::mutex> lock(m_controlMutex);
    if (m_running) {
        return;
    }

    m_running = true;
    m_forceRefresh = true;
    m_worker = std::thread(&ExecutableScanner::workerLoop, this);
}

void ExecutableScanner::stop() {
    {
        std::lock_guard<std::mutex> lock(m_controlMutex);
        if (!m_running) {
            return;
        }
        m_running = false;
        m_forceRefresh = true;
    }

    m_controlCv.notify_all();
    if (m_worker.joinable()) {
        m_worker.join();
    }
}

void ExecutableScanner::requestRefresh() {
    {
        std::lock_guard<std::mutex> lock(m_controlMutex);
        m_forceRefresh = true;
    }
    m_controlCv.notify_all();
}

std::shared_ptr<const ScanSnapshot> ExecutableScanner::getSnapshot() const {
    std::lock_guard<std::mutex> lock(m_snapshotMutex);
    return m_snapshot;
}

void ExecutableScanner::workerLoop() {
    while (true) {
        {
            std::lock_guard<std::mutex> lock(m_controlMutex);
            if (!m_running) {
                break;
            }
        }

        ScanSnapshot fresh = buildSnapshot();

        // Detect whether the scan produced different results from the previous snapshot.
        bool changed = true;
        {
            std::lock_guard<std::mutex> lock(m_snapshotMutex);
            if (m_snapshot) {
                changed = (fresh.entries.size() != m_snapshot->entries.size());
                if (!changed) {
                    for (size_t i = 0; i < fresh.entries.size(); ++i) {
                        const auto& a = fresh.entries[i];
                        const auto& b = m_snapshot->entries[i];
                        if (a.outOfDate != b.outOfDate || a.sourceDirty != b.sourceDirty ||
                            a.targetName != b.targetName ||
                            a.executableWriteTime != b.executableWriteTime) {
                            changed = true;
                            break;
                        }
                    }
                }
            }
            m_snapshot = std::make_shared<const ScanSnapshot>(std::move(fresh));
        }

        // Adaptive interval: use fast interval while changes are detected,
        // decelerate to idle interval after several unchanged scans.
        if (changed) {
            m_unchangedScanCount = 0;
        } else {
            ++m_unchangedScanCount;
        }

        std::chrono::seconds sleepInterval =
            (m_unchangedScanCount >= kFastToIdleThreshold) ? m_idleInterval : m_fastInterval;

        std::unique_lock<std::mutex> lock(m_controlMutex);
        m_forceRefresh = false;

        m_controlCv.wait_for(lock, sleepInterval,
                             [this]() { return !m_running || m_forceRefresh; });

        if (!m_running) {
            break;
        }

        if (m_forceRefresh) {
            m_unchangedScanCount = 0;
        }
    }
}

ScanSnapshot ExecutableScanner::buildSnapshot() {
    ScanSnapshot snapshot;
    snapshot.scanTime = std::chrono::system_clock::now();

    snapshot.repositoryRoot = findRepositoryRoot(m_startPath);
    if (snapshot.repositoryRoot.empty()) {
        return snapshot;
    }

    // Incremental CMake map: only rebuild if any CMakeLists.txt changed.
    if (m_cachedTargetMap.targetMap.empty() || isCmakeMapStale(snapshot.repositoryRoot)) {
        m_cachedTargetMap.targetMap = buildTargetSourceMap(snapshot.repositoryRoot);
        m_cachedTargetMap.cmakeTimestamps = collectCmakeTimestamps(snapshot.repositoryRoot);
    }

    auto executablePaths = findExecutablePaths(snapshot.repositoryRoot);

    // Persistent GitUtils: reuse the same instance across scan cycles.
    // Worker-thread-only — never accessed from UI or other threads.
    if (!m_git || m_gitRoot != snapshot.repositoryRoot) {
        m_git = std::make_unique<GitUtils>(snapshot.repositoryRoot);
        m_gitRoot = snapshot.repositoryRoot;
    }

    snapshot.gitAvailable = m_git->isAvailable();

    // Collect unique source directories first for batch git operations.
    std::vector<std::filesystem::path> sourceDirs;

    // First pass: resolve source directories and collect for batching.
    struct PreEntry {
        std::filesystem::path exePath;
        std::string targetName;
        std::filesystem::path sourceDir;
        bool sourceFound = false;
        std::string kind;
    };

    std::vector<PreEntry> preEntries;
    preEntries.reserve(executablePaths.size());

    for (const auto& exePath : executablePaths) {
        PreEntry pre;
        pre.exePath = exePath;
        pre.targetName = exePath.stem().string();

        auto sourceIt = m_cachedTargetMap.targetMap.find(pre.targetName);
        if (sourceIt != m_cachedTargetMap.targetMap.end()) {
            pre.sourceDir = sourceIt->second;
            pre.sourceFound = std::filesystem::exists(pre.sourceDir);
        } else {
            std::string baseName = pre.targetName;
            if (baseName.rfind("vde_", 0) == 0) {
                baseName = baseName.substr(4);
            }

            std::filesystem::path exampleGuess = snapshot.repositoryRoot / "examples" / baseName;
            std::filesystem::path toolGuess = snapshot.repositoryRoot / "tools" / baseName;

            if (std::filesystem::exists(exampleGuess)) {
                pre.sourceDir = exampleGuess;
                pre.sourceFound = true;
            } else if (std::filesystem::exists(toolGuess)) {
                pre.sourceDir = toolGuess;
                pre.sourceFound = true;
            }
        }

        pre.kind = inferKind(pre.sourceDir, snapshot.repositoryRoot);

        if (pre.sourceFound) {
            sourceDirs.push_back(pre.sourceDir);
        }

        preEntries.push_back(std::move(pre));
    }

    // Batch git operations: one `git status --porcelain` for the whole repo,
    // then batch commit-time queries for all unique source directories.
    if (snapshot.gitAvailable) {
        // Deduplicate source directories before batching to avoid redundant git log calls.
        // Use lexically_normal() to normalize separators before comparing.
        std::unordered_set<std::string> seen;
        std::vector<std::filesystem::path> uniqueSourceDirs;
        for (const auto& dir : sourceDirs) {
            std::filesystem::path normalized = dir.lexically_normal();
            if (seen.insert(normalized.string()).second) {
                uniqueSourceDirs.push_back(normalized);
            }
        }
        m_git->refreshDirtyCache();
        m_git->refreshCommitTimeCache(uniqueSourceDirs);
    }

    // Second pass: build final entries using cached git data.
    for (auto& pre : preEntries) {
        ExecutableEntry entry;
        entry.executablePath = pre.exePath;
        entry.targetName = pre.targetName;
        entry.sourceDirectory = pre.sourceDir;
        entry.sourceFound = pre.sourceFound;
        entry.kind = pre.kind;

        std::error_code error;
        auto exeWrite = std::filesystem::last_write_time(pre.exePath, error);
        if (!error) {
            entry.executableWriteTime = fileTimeToSystemClock(exeWrite);
        }

        if (entry.sourceFound) {
            auto smokeInfo = loadSmokeMetadata(entry.sourceDirectory, entry.targetName);
            entry.smokeScripts = std::move(smokeInfo.scripts);
            entry.smokePriority = smokeInfo.priority;

            auto newest = newestSourceTimestamp(entry.sourceDirectory);
            if (newest) {
                entry.newestSourceWriteTime = *newest;
                entry.hasNewestSourceWriteTime = true;
                entry.sourceNewerThanExecutable =
                    entry.newestSourceWriteTime > entry.executableWriteTime;
            }

            if (snapshot.gitAvailable) {
                entry.sourceDirty = m_git->hasUncommittedChanges(entry.sourceDirectory);

                auto commit = m_git->getLastCommitTime(entry.sourceDirectory);
                if (commit) {
                    entry.lastSourceCommitTime = *commit;
                    entry.hasLastSourceCommitTime = true;
                }
            }
        }

        entry.gitAvailable = snapshot.gitAvailable;
        entry.outOfDate = entry.sourceNewerThanExecutable || entry.sourceDirty;
        if (entry.sourceDirty && entry.sourceNewerThanExecutable) {
            entry.outOfDateReason = "Source modified and newer than executable";
        } else if (entry.sourceDirty) {
            entry.outOfDateReason = "Uncommitted source changes";
        } else if (entry.sourceNewerThanExecutable) {
            entry.outOfDateReason = "Executable older than source files";
        } else if (!entry.sourceFound) {
            entry.outOfDateReason = "Source directory not mapped";
        } else {
            entry.outOfDateReason = "Up to date";
        }

        snapshot.entries.push_back(std::move(entry));
    }

    return snapshot;
}

std::filesystem::path ExecutableScanner::findRepositoryRoot(const std::filesystem::path& fromPath) {
    std::filesystem::path cursor = normalizePath(fromPath);

    if (std::filesystem::is_regular_file(cursor)) {
        cursor = cursor.parent_path();
    }

    while (!cursor.empty()) {
        if (std::filesystem::exists(cursor / "CMakeLists.txt") &&
            std::filesystem::exists(cursor / "examples") &&
            std::filesystem::exists(cursor / "tools") && std::filesystem::exists(cursor / "src")) {
            return cursor;
        }

        std::filesystem::path parent = cursor.parent_path();
        if (parent == cursor) {
            break;
        }
        cursor = parent;
    }

    return {};
}

std::vector<std::filesystem::path>
ExecutableScanner::findExecutablePaths(const std::filesystem::path& repoRoot) {
    std::vector<std::filesystem::path> scanRoots = {
        repoRoot / "build" / "examples", repoRoot / "build" / "tools",
        repoRoot / "build_ninja" / "examples", repoRoot / "build_ninja" / "tools"};

    std::vector<std::filesystem::path> executablePaths;
    std::unordered_set<std::string> seen;

    for (const auto& root : scanRoots) {
        if (!std::filesystem::exists(root)) {
            continue;
        }

        std::error_code error;
        std::filesystem::recursive_directory_iterator it(root, error);
        std::filesystem::recursive_directory_iterator end;

        for (; it != end; it.increment(error)) {
            if (error) {
                continue;
            }

            if (!it->is_regular_file()) {
                continue;
            }

            const auto& path = it->path();
            if (path.extension() != ".exe") {
                continue;
            }

            std::string stem = path.stem().string();
            if (stem.rfind("vde_", 0) != 0) {
                continue;
            }

            std::string key = normalizePath(path).string();
            if (seen.insert(key).second) {
                executablePaths.push_back(path);
            }
        }
    }

    return executablePaths;
}

std::unordered_map<std::string, std::filesystem::path>
ExecutableScanner::buildTargetSourceMap(const std::filesystem::path& repoRoot) {
    std::unordered_map<std::string, std::filesystem::path> targetMap;

    std::vector<std::filesystem::path> roots = {repoRoot / "examples", repoRoot / "tools"};

    // Static regex objects: compiled once, reused across all scan cycles.
    static const std::regex addExecutableRegex(
        R"(add_executable\s*\(\s*([A-Za-z0-9_\-]+)\s+\"?([^\s\)\"]+)\"?)", std::regex::icase);
    static const std::regex addVdeExampleRegex(
        R"(add_vde_example\s*\(\s*([A-Za-z0-9_\-]+)\s+\"([^\"]+)\")", std::regex::icase);

    for (const auto& root : roots) {
        if (!std::filesystem::exists(root)) {
            continue;
        }

        std::error_code error;
        for (std::filesystem::recursive_directory_iterator it(root, error), end; it != end;
             it.increment(error)) {
            if (error || !it->is_regular_file()) {
                continue;
            }

            if (it->path().filename() != "CMakeLists.txt") {
                continue;
            }

            std::ifstream file(it->path());
            if (!file.is_open()) {
                continue;
            }

            std::string content((std::istreambuf_iterator<char>(file)),
                                std::istreambuf_iterator<char>());

            for (std::sregex_iterator matchIt(content.begin(), content.end(), addVdeExampleRegex),
                 matchEnd;
                 matchIt != matchEnd; ++matchIt) {
                std::string target = trim((*matchIt)[1].str());
                std::string sourceRel = trim((*matchIt)[2].str());

                std::filesystem::path sourcePath =
                    normalizePath(it->path().parent_path() / sourceRel);
                targetMap[target] = sourcePath.parent_path();
            }

            for (std::sregex_iterator matchIt(content.begin(), content.end(), addExecutableRegex),
                 matchEnd;
                 matchIt != matchEnd; ++matchIt) {
                std::string target = trim((*matchIt)[1].str());
                std::string sourceToken = trim((*matchIt)[2].str());

                if (sourceToken.empty() || sourceToken.find('$') != std::string::npos) {
                    continue;
                }

                std::filesystem::path sourcePath =
                    normalizePath(it->path().parent_path() / sourceToken);
                targetMap[target] = sourcePath.parent_path();
            }
        }
    }

    return targetMap;
}

std::chrono::system_clock::time_point
ExecutableScanner::fileTimeToSystemClock(const std::filesystem::file_time_type& fileTime) {
    return std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        fileTime - std::filesystem::file_time_type::clock::now() +
        std::chrono::system_clock::now());
}

std::optional<std::chrono::system_clock::time_point>
ExecutableScanner::newestSourceTimestamp(const std::filesystem::path& sourceDir) {
    if (!std::filesystem::exists(sourceDir)) {
        return std::nullopt;
    }

    std::optional<std::chrono::system_clock::time_point> newest;

    std::error_code error;
    for (std::filesystem::recursive_directory_iterator it(sourceDir, error), end; it != end;
         it.increment(error)) {
        if (error || !it->is_regular_file()) {
            continue;
        }

        const auto& path = it->path();
        if (!hasKnownSourceExtension(path) && path.filename() != "CMakeLists.txt") {
            continue;
        }

        auto writeTime = std::filesystem::last_write_time(path, error);
        if (error) {
            continue;
        }

        auto converted = fileTimeToSystemClock(writeTime);
        if (!newest || converted > *newest) {
            newest = converted;
        }
    }

    return newest;
}

ExecutableScanner::SmokeMetadata
ExecutableScanner::loadSmokeMetadata(const std::filesystem::path& sourceDir,
                                     const std::string& targetName) {
    SmokeMetadata result;

    auto tomlPath = sourceDir / "vde.toml";
    std::error_code error;
    if (!std::filesystem::exists(tomlPath, error)) {
        return result;
    }

    try {
        auto config = toml::parse_file(tomlPath.string());
        auto appendScripts = [&result](const toml::array& array) {
            for (const auto& elem : array) {
                if (auto* str = elem.as_string()) {
                    if (auto scriptName = sanitizeSmokeScriptName(str->get())) {
                        result.scripts.push_back(std::move(*scriptName));
                    }
                }
            }
        };

        auto readPriority = [&result](const toml::table& table) {
            if (auto val = table["priority"].as_integer()) {
                int p = static_cast<int>(val->get());
                if (p == 1 || p == 2) {
                    result.priority = p;
                }
            }
        };

        // Per-target section first: [smoke.<targetName>]
        if (auto* perTarget = config["smoke"][targetName].as_table()) {
            if (auto* arr = (*perTarget)["scripts"].as_array()) {
                appendScripts(*arr);
            }
            readPriority(*perTarget);
            if (!result.scripts.empty()) {
                return result;
            }
        }

        // Shared section: [smoke]
        if (auto* smoke = config["smoke"].as_table()) {
            if (auto* arr = (*smoke)["scripts"].as_array()) {
                appendScripts(*arr);
            }
            if (result.priority == 0) {
                readPriority(*smoke);
            }
        }
    } catch (const toml::parse_error&) {
        // Malformed TOML — silently skip.
    }

    return result;
}

std::string ExecutableScanner::inferKind(const std::filesystem::path& sourceDir,
                                         const std::filesystem::path& repoRoot) {
    std::error_code error;
    auto rel = std::filesystem::relative(sourceDir, repoRoot, error);
    if (error || rel.empty()) {
        return "Unknown";
    }

    auto it = rel.begin();
    if (it == rel.end()) {
        return "Unknown";
    }

    std::string first = it->string();
    if (first == "examples") {
        return "Example";
    }
    if (first == "tools") {
        return "Tool";
    }

    return "Unknown";
}

bool ExecutableScanner::isCmakeMapStale(const std::filesystem::path& repoRoot) const {
    if (m_cachedTargetMap.cmakeTimestamps.empty()) {
        return true;
    }

    auto currentTimestamps = collectCmakeTimestamps(repoRoot);

    // Stale if any file was added, removed, or modified.
    if (currentTimestamps.size() != m_cachedTargetMap.cmakeTimestamps.size()) {
        return true;
    }

    for (const auto& [path, timestamp] : currentTimestamps) {
        auto it = m_cachedTargetMap.cmakeTimestamps.find(path);
        if (it == m_cachedTargetMap.cmakeTimestamps.end() || it->second != timestamp) {
            return true;
        }
    }

    return false;
}

std::unordered_map<std::string, std::filesystem::file_time_type>
ExecutableScanner::collectCmakeTimestamps(const std::filesystem::path& repoRoot) {
    std::unordered_map<std::string, std::filesystem::file_time_type> timestamps;

    std::vector<std::filesystem::path> roots = {repoRoot / "examples", repoRoot / "tools"};

    for (const auto& root : roots) {
        if (!std::filesystem::exists(root)) {
            continue;
        }

        std::error_code error;
        for (std::filesystem::recursive_directory_iterator it(root, error), end; it != end;
             it.increment(error)) {
            if (error || !it->is_regular_file()) {
                continue;
            }

            if (it->path().filename() != "CMakeLists.txt") {
                continue;
            }

            auto writeTime = std::filesystem::last_write_time(it->path(), error);
            if (!error) {
                timestamps[it->path().string()] = writeTime;
            }
        }
    }

    return timestamps;
}

}  // namespace vde::tools
