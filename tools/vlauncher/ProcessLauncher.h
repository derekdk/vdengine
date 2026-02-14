#pragma once

#include <filesystem>
#include <string>

namespace vde::tools {

class ProcessLauncher {
  public:
    static bool launchDetached(const std::filesystem::path& executablePath, std::string& error);
};

}  // namespace vde::tools
