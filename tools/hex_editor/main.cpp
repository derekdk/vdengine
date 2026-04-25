#include <vde/Window.h>

#include <iostream>
#include <string>

#include "HexEditorScene.h"

using namespace vde::tools;

// =============================================================================
// Tool Game class
// =============================================================================

class HexEditorTool : public BaseToolGame<BaseToolInputHandler, HexEditorScene> {
  public:
    HexEditorTool(ToolMode mode, const std::string& scriptFile = "")
        : BaseToolGame(mode), m_scriptFile(scriptFile) {}

    void onStart() override {
        BaseToolGame::onStart();

        if (m_toolMode == ToolMode::SCRIPT && !m_scriptFile.empty()) {
            auto* scene = getToolScene();
            if (scene && !scene->processScriptFile(m_scriptFile)) {
                std::cerr << "Failed to process script file: " << m_scriptFile << "\n";
                m_exitCode = 1;
            }
            quit();
        }
    }

  private:
    std::string m_scriptFile;
};

// =============================================================================
// Main
// =============================================================================

int main(int argc, char** argv) {
    ToolMode mode = ToolMode::INTERACTIVE;
    std::string scriptFile;

    if (argc > 1) {
        mode = ToolMode::SCRIPT;

        std::cout << "====================================================\n";
        std::cout << "VDE Hex Editor - Script Mode\n";
        std::cout << "====================================================\n";

        if (std::string(argv[1]) == "--input-script") {
            std::cout << "Processing input script from command line arguments\n";
        } else {
            scriptFile = argv[1];
            std::cout << "Processing script: " << scriptFile << "\n";
        }
        std::cout << "====================================================\n\n";
    }

    HexEditorTool tool(mode, scriptFile);

    if (mode == ToolMode::INTERACTIVE) {
        float dpiScale = vde::Window::getPrimaryMonitorDPIScale();
        uint32_t width = static_cast<uint32_t>(1400 * dpiScale);
        uint32_t height = static_cast<uint32_t>(800 * dpiScale);
        return runTool(tool, "VDE Hex Editor", width, height, argc, argv);
    } else {
        vde::configureInputScriptFromArgs(tool, argc, argv);

        vde::GameSettings settings;
        settings.gameName = "VDE Hex Editor (Script Mode)";
        settings.display.windowWidth = 800;
        settings.display.windowHeight = 600;
        settings.debug.enableValidation = false;

        if (!tool.initialize(settings)) {
            std::cerr << "Failed to initialize tool\n";
            return 1;
        }

        tool.run();
        return tool.getExitCode();
    }
}
