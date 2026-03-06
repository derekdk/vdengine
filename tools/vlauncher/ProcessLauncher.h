#pragma once

#include <filesystem>
#include <string>

namespace vde::tools {

class ProcessLauncher {
  public:
    static bool launchDetached(const std::filesystem::path& executablePath, std::string& error);
    static bool openFileInVSCode(const std::filesystem::path& filePath, std::string& error);
};

}  // namespace vde::tools
