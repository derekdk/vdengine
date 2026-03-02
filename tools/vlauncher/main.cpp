#include <vde/Window.h>

#include <cstdint>

#include "VLauncherScene.h"

using namespace vde::tools;

class VLauncherTool : public BaseToolGame<BaseToolInputHandler, VLauncherScene> {
  public:
    explicit VLauncherTool(ToolMode mode) : BaseToolGame(mode) {}
};

int main(int argc, char** argv) {
    VLauncherTool tool(ToolMode::INTERACTIVE);

    float dpiScale = vde::Window::getPrimaryMonitorDPIScale();
    uint32_t width = static_cast<uint32_t>(1280 * dpiScale);
    uint32_t height = static_cast<uint32_t>(800 * dpiScale);

    return runTool(tool, "VDE VLauncher", width, height, argc, argv);
}
