#pragma once

#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace vde::tools {

class GitUtils {
  public:
    explicit GitUtils(std::filesystem::path repoRoot);

    bool isAvailable() const { return m_gitAvailable; }

    bool hasUncommittedChanges(const std::filesystem::path& pathInRepo) const;

    std::optional<std::chrono::system_clock::time_point>
    getLastCommitTime(const std::filesystem::path& pathInRepo) const;

    /// Refresh the cached set of dirty paths from a single `git status` call.
    /// After this call, hasUncommittedChanges() uses the cache without spawning processes.
    void refreshDirtyCache();

    /// Refresh commit-time cache for a set of source directories in a single batch.
    void refreshCommitTimeCache(const std::vector<std::filesystem::path>& sourceDirs);

  private:
    struct CommandResult {
        int exitCode = -1;
        std::string output;
    };

    std::filesystem::path m_repoRoot;
    bool m_gitAvailable = false;

    // Cached dirty directories from the last refreshDirtyCache() call.
    std::unordered_set<std::string> m_dirtyDirs;
    bool m_dirtyCacheValid = false;

    // Cached commit times from the last refreshCommitTimeCache() call.
    std::unordered_map<std::string, std::optional<std::chrono::system_clock::time_point>>
        m_commitTimeCache;

    CommandResult runGitCommand(const std::string& args) const;
    static std::string trim(const std::string& value);
};

}  // namespace vde::tools
