#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace vde::tools {

struct LaunchedProcess {
    std::uintptr_t processHandle = 0;
    std::filesystem::path outputPath;
    std::string commandLine;
#ifdef __APPLE__
    bool completed = false;
    uint32_t exitCode = 0;
#endif
};

class ProcessLauncher {
  public:
    static bool launchDetached(const std::filesystem::path& executablePath, std::string& error);
    static bool launchWithOutputCapture(const std::filesystem::path& executablePath,
                                        LaunchedProcess& launchedProcess, std::string& error,
                                        const std::vector<std::string>& extraArgs = {});
    static bool pollCompletion(LaunchedProcess& launchedProcess, bool& completed,
                               uint32_t& exitCode, std::string& error);
    static void release(LaunchedProcess& launchedProcess);
    static bool readOutputFile(const std::filesystem::path& outputPath, std::string& output,
                               std::string& error);
    static bool openFileInVSCode(const std::filesystem::path& filePath, std::string& error);
};

}  // namespace vde::tools
