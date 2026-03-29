#include "GitUtils.h"

#include <array>
#include <cstdio>
#include <sstream>

#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#endif

namespace vde::tools {

GitUtils::GitUtils(std::filesystem::path repoRoot) : m_repoRoot(std::move(repoRoot)) {
    CommandResult result = runGitCommand("rev-parse --is-inside-work-tree");
    std::string trimmed = trim(result.output);
    m_gitAvailable = (result.exitCode == 0 && trimmed == "true");
}

void GitUtils::refreshDirtyCache() {
    m_dirtyDirs.clear();
    m_dirtyCacheValid = false;

    if (!m_gitAvailable) {
        return;
    }

    // Single git status call for the entire repo.
    CommandResult result = runGitCommand("status --porcelain");
    if (result.exitCode != 0) {
        return;
    }

    m_dirtyCacheValid = true;

    // Parse each line: first 2 chars are status, then a space, then the path.
    std::istringstream stream(result.output);
    std::string line;
    while (std::getline(stream, line)) {
        if (line.size() < 4) {
            continue;
        }

        // Extract the file path (skip the 3-char status prefix "XY ").
        std::string filePath = line.substr(3);

        // Handle rename lines "XY old -> new".
        auto arrowPos = filePath.find(" -> ");
        if (arrowPos != std::string::npos) {
            filePath = filePath.substr(arrowPos + 4);
        }

        // Normalize to the directory containing the dirty file.
        std::filesystem::path dirtyFile = m_repoRoot / filePath;
        std::filesystem::path dirtyDir = dirtyFile.parent_path();
        m_dirtyDirs.insert(dirtyDir.lexically_normal().string());
    }
}

bool GitUtils::hasUncommittedChanges(const std::filesystem::path& pathInRepo) const {
    if (!m_gitAvailable) {
        return false;
    }

    // Use the batch cache if available.
    if (m_dirtyCacheValid) {
        std::error_code error;
        std::filesystem::path normalized = std::filesystem::absolute(pathInRepo, error);
        if (error) {
            normalized = pathInRepo;
        }
        normalized = normalized.lexically_normal();
        std::string normalizedStr = normalized.string();

        // Check if any dirty directory is within or equal to the queried path,
        // using path-separator-aware containment to avoid false positives
        // (e.g. "examples/foo" must not match "examples/foobar").
        for (const auto& dirtyDir : m_dirtyDirs) {
            if (dirtyDir == normalizedStr) {
                return true;
            }
            // dirtyDir is under normalizedStr.
            if (dirtyDir.size() > normalizedStr.size() && dirtyDir.find(normalizedStr) == 0 &&
                (dirtyDir[normalizedStr.size()] == '/' || dirtyDir[normalizedStr.size()] == '\\')) {
                return true;
            }
            // normalizedStr is under dirtyDir.
            if (normalizedStr.size() > dirtyDir.size() && normalizedStr.find(dirtyDir) == 0 &&
                (normalizedStr[dirtyDir.size()] == '/' || normalizedStr[dirtyDir.size()] == '\\')) {
                return true;
            }
        }
        return false;
    }

    // Fallback: per-directory git status call.
    std::error_code error;
    std::filesystem::path rel = std::filesystem::relative(pathInRepo, m_repoRoot, error);
    if (error) {
        return false;
    }

    std::ostringstream args;
    args << "status --porcelain -- \"" << rel.generic_string() << "\"";

    CommandResult result = runGitCommand(args.str());
    if (result.exitCode != 0) {
        return false;
    }

    return !trim(result.output).empty();
}

void GitUtils::refreshCommitTimeCache(const std::vector<std::filesystem::path>& sourceDirs) {
    m_commitTimeCache.clear();

    if (!m_gitAvailable) {
        return;
    }

    for (const auto& sourceDir : sourceDirs) {
        std::error_code error;
        std::filesystem::path rel = std::filesystem::relative(sourceDir, m_repoRoot, error);
        if (error) {
            continue;
        }

        std::ostringstream args;
        args << "log -1 --format=%ct -- \"" << rel.generic_string() << "\"";

        CommandResult result = runGitCommand(args.str());
        std::string key = sourceDir.string();

        if (result.exitCode != 0) {
            m_commitTimeCache[key] = std::nullopt;
            continue;
        }

        std::string trimmed = trim(result.output);
        if (trimmed.empty()) {
            m_commitTimeCache[key] = std::nullopt;
            continue;
        }

        try {
            std::time_t commitEpoch = static_cast<std::time_t>(std::stoll(trimmed));
            m_commitTimeCache[key] = std::chrono::system_clock::from_time_t(commitEpoch);
        } catch (...) {
            m_commitTimeCache[key] = std::nullopt;
        }
    }
}

std::optional<std::chrono::system_clock::time_point>
GitUtils::getLastCommitTime(const std::filesystem::path& pathInRepo) const {
    if (!m_gitAvailable) {
        return std::nullopt;
    }

    // Use the batch cache if the path is there.
    std::string key = pathInRepo.string();
    auto it = m_commitTimeCache.find(key);
    if (it != m_commitTimeCache.end()) {
        return it->second;
    }

    // Fallback: per-directory git log call.
    std::error_code error;
    std::filesystem::path rel = std::filesystem::relative(pathInRepo, m_repoRoot, error);
    if (error) {
        return std::nullopt;
    }

    std::ostringstream args;
    args << "log -1 --format=%ct -- \"" << rel.generic_string() << "\"";

    CommandResult result = runGitCommand(args.str());
    if (result.exitCode != 0) {
        return std::nullopt;
    }

    std::string trimmed = trim(result.output);
    if (trimmed.empty()) {
        return std::nullopt;
    }

    try {
        std::time_t commitEpoch = static_cast<std::time_t>(std::stoll(trimmed));
        return std::chrono::system_clock::from_time_t(commitEpoch);
    } catch (...) {
        return std::nullopt;
    }
}

GitUtils::CommandResult GitUtils::runGitCommand(const std::string& args) const {
    CommandResult result;

    std::ostringstream command;
    command << "git -C \"" << m_repoRoot.string() << "\" " << args;
#ifdef _WIN32
    // Treat Git probe failures as unavailable without leaking stderr noise to callers.
    command << " 2>nul";
#else
    command << " 2>/dev/null";
#endif

    FILE* pipe = popen(command.str().c_str(), "r");
    if (!pipe) {
        result.exitCode = -1;
        return result;
    }

    std::array<char, 512> buffer{};
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        result.output += buffer.data();
    }

    result.exitCode = pclose(pipe);
    return result;
}

std::string GitUtils::trim(const std::string& value) {
    size_t start = value.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return {};
    }

    size_t end = value.find_last_not_of(" \t\r\n");
    return value.substr(start, end - start + 1);
}

}  // namespace vde::tools
