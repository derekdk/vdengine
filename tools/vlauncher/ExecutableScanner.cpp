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

}  // namespace

ExecutableScanner::ExecutableScanner(std::filesystem::path startPath, std::chrono::seconds interval)
    : m_startPath(std::move(startPath)), m_interval(interval) {}

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

ScanSnapshot ExecutableScanner::getSnapshot() const {
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
        {
            std::lock_guard<std::mutex> lock(m_snapshotMutex);
            m_snapshot = std::move(fresh);
        }

        std::unique_lock<std::mutex> lock(m_controlMutex);
        m_forceRefresh = false;

        bool wake = m_controlCv.wait_for(lock, m_interval,
                                         [this]() { return !m_running || m_forceRefresh; });

        if (!m_running) {
            break;
        }

        if (!wake) {
            m_forceRefresh = true;
        }
    }
}

ScanSnapshot ExecutableScanner::buildSnapshot() const {
    ScanSnapshot snapshot;
    snapshot.scanTime = std::chrono::system_clock::now();

    snapshot.repositoryRoot = findRepositoryRoot(m_startPath);
    if (snapshot.repositoryRoot.empty()) {
        return snapshot;
    }

    auto targetSourceMap = buildTargetSourceMap(snapshot.repositoryRoot);
    auto executablePaths = findExecutablePaths(snapshot.repositoryRoot);

    GitUtils git(snapshot.repositoryRoot);
    snapshot.gitAvailable = git.isAvailable();

    std::unordered_map<std::string, bool> dirtyCache;
    std::unordered_map<std::string, std::optional<std::chrono::system_clock::time_point>>
        commitCache;

    for (const auto& exePath : executablePaths) {
        ExecutableEntry entry;
        entry.executablePath = exePath;
        entry.targetName = exePath.stem().string();

        std::error_code error;
        auto exeWrite = std::filesystem::last_write_time(exePath, error);
        if (!error) {
            entry.executableWriteTime = fileTimeToSystemClock(exeWrite);
        }

        auto sourceIt = targetSourceMap.find(entry.targetName);
        if (sourceIt != targetSourceMap.end()) {
            entry.sourceDirectory = sourceIt->second;
            entry.sourceFound = std::filesystem::exists(entry.sourceDirectory);
        } else {
            std::string baseName = entry.targetName;
            if (baseName.rfind("vde_", 0) == 0) {
                baseName = baseName.substr(4);
            }

            std::filesystem::path exampleGuess = snapshot.repositoryRoot / "examples" / baseName;
            std::filesystem::path toolGuess = snapshot.repositoryRoot / "tools" / baseName;

            if (std::filesystem::exists(exampleGuess)) {
                entry.sourceDirectory = exampleGuess;
                entry.sourceFound = true;
            } else if (std::filesystem::exists(toolGuess)) {
                entry.sourceDirectory = toolGuess;
                entry.sourceFound = true;
            }
        }

        entry.kind = inferKind(entry.sourceDirectory, snapshot.repositoryRoot);

        if (entry.sourceFound) {
            auto newest = newestSourceTimestamp(entry.sourceDirectory);
            if (newest) {
                entry.newestSourceWriteTime = *newest;
                entry.hasNewestSourceWriteTime = true;
                entry.sourceNewerThanExecutable =
                    entry.newestSourceWriteTime > entry.executableWriteTime;
            }

            std::string sourceKey = entry.sourceDirectory.string();
            if (snapshot.gitAvailable) {
                auto dirtyIt = dirtyCache.find(sourceKey);
                if (dirtyIt == dirtyCache.end()) {
                    bool dirty = git.hasUncommittedChanges(entry.sourceDirectory);
                    dirtyCache[sourceKey] = dirty;
                    entry.sourceDirty = dirty;
                } else {
                    entry.sourceDirty = dirtyIt->second;
                }

                auto commitIt = commitCache.find(sourceKey);
                if (commitIt == commitCache.end()) {
                    auto commit = git.getLastCommitTime(entry.sourceDirectory);
                    commitCache[sourceKey] = commit;
                    if (commit) {
                        entry.lastSourceCommitTime = *commit;
                        entry.hasLastSourceCommitTime = true;
                    }
                } else if (commitIt->second) {
                    entry.lastSourceCommitTime = *commitIt->second;
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
    std::regex addExecutableRegex(R"(add_executable\s*\(\s*([A-Za-z0-9_\-]+)\s+\"?([^\s\)\"]+)\"?)",
                                  std::regex::icase);
    std::regex addVdeExampleRegex(R"(add_vde_example\s*\(\s*([A-Za-z0-9_\-]+)\s+\"([^\"]+)\")",
                                  std::regex::icase);

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

}  // namespace vde::tools
