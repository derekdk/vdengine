#pragma once

#include <array>
#include <filesystem>
#include <optional>
#include <string>

namespace vde::tools {

struct StoredRunLog {
    std::string timestamp;
    int exitCode = 0;
    std::string commandLine;
    std::string output;
};

class RunLogStorage {
  public:
    static std::string buildTargetId(const std::filesystem::path& repositoryRoot,
                                     const std::filesystem::path& executablePath);

    static bool saveLatestRun(const std::string& targetId, const StoredRunLog& run,
                              std::string& error);

    static std::array<std::optional<StoredRunLog>, 2> loadRecentRuns(const std::string& targetId);

  private:
    static std::string makeHistoryKey(const std::string& targetId);
};

}  // namespace vde::tools
