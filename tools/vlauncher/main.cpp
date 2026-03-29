#include <vde/Window.h>
#include <vde/api/GameSettings.h>

#include <cstdint>
#include <iostream>
#include <string>

#include "../ToolBase.h"
#include "VLauncherScene.h"

using namespace vde::tools;

class VLauncherTool : public BaseToolGame<BaseToolInputHandler, VLauncherScene> {
  public:
    explicit VLauncherTool(ToolMode mode) : BaseToolGame(mode) {}
};

int main(int argc, char** argv) {
    VLauncherTool tool(ToolMode::INTERACTIVE);

    // Configure input script from CLI args BEFORE changing working directory.
    if (argc > 0 && argv != nullptr) {
        vde::configureInputScriptFromArgs(tool, argc, argv);
    }

    setWorkingDirectoryToExecutablePath();

    float dpiScale = vde::Window::getPrimaryMonitorDPIScale();
    uint32_t width = static_cast<uint32_t>(1280 * dpiScale);
    uint32_t height = static_cast<uint32_t>(800 * dpiScale);

    vde::GameSettings settings;
    settings.gameName = "VDE VLauncher";
    settings.display.windowWidth = width;
    settings.display.windowHeight = height;
    settings.debug.enableValidation = true;
    settings.graphics.maxFPS = 15;  // Launcher UI doesn't need high FPS

    if (!tool.initialize(settings)) {
        std::cerr << "Failed to initialize VLauncher" << std::endl;
        return 1;
    }

    tool.run();
    return tool.getExitCode();
}
