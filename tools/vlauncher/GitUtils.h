#pragma once

#include <chrono>
#include <filesystem>
#include <optional>
#include <string>

namespace vde::tools {

class GitUtils {
  public:
    explicit GitUtils(std::filesystem::path repoRoot);

    bool isAvailable() const { return m_gitAvailable; }

    bool hasUncommittedChanges(const std::filesystem::path& pathInRepo) const;

    std::optional<std::chrono::system_clock::time_point>
    getLastCommitTime(const std::filesystem::path& pathInRepo) const;

  private:
    struct CommandResult {
        int exitCode = -1;
        std::string output;
    };

    std::filesystem::path m_repoRoot;
    bool m_gitAvailable = false;

    CommandResult runGitCommand(const std::string& args) const;
    static std::string trim(const std::string& value);
};

}  // namespace vde::tools
