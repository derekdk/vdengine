/**
 * @file main.cpp
 * @brief VDE Resource Editor - 2D pixel art editor with command console
 *
 * Usage:
 *   vde_resource_editor                  - Interactive mode with GUI
 *   vde_resource_editor <script.txt>     - Execute commands and exit
 */

#include <vde/Window.h>

#include <iostream>
#include <string>

#include "ResourceEditorScene.h"

using namespace vde::tools;

// =============================================================================
// Tool Game class
// =============================================================================

class ResourceEditorGame : public BaseToolGame<BaseToolInputHandler, ResourceEditorScene> {
  public:
    ResourceEditorGame(ToolMode mode, const std::string& scriptFile = "")
        : BaseToolGame(mode), m_scriptFile(scriptFile) {}

    void onStart() override {
        BaseToolGame::onStart();

        if (m_toolMode == ToolMode::SCRIPT && !m_scriptFile.empty()) {
            auto* scene = getToolScene();
            if (scene) {
                if (!scene->getCommandSystem().executeScript(m_scriptFile)) {
                    std::cerr << "Failed to process script file: " << m_scriptFile << "\n";
                    m_exitCode = 1;
                }
                // Exit after script execution
                quit();
            }
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

    // Parse command line arguments
    // Skip --input-script (handled by runTool/configureInputScriptFromArgs)
    if (argc > 1 && std::string(argv[1]) != "--input-script") {
        scriptFile = argv[1];
        mode = ToolMode::SCRIPT;

        std::cout << "====================================================\n";
        std::cout << "VDE Resource Editor - Script Mode\n";
        std::cout << "====================================================\n";
        std::cout << "Processing script: " << scriptFile << "\n";
        std::cout << "====================================================\n\n";
    }

    ResourceEditorGame tool(mode, scriptFile);

    if (mode == ToolMode::INTERACTIVE) {
        // Adjust resolution based on DPI
        float dpiScale = vde::Window::getPrimaryMonitorDPIScale();
        uint32_t width = static_cast<uint32_t>(1400 * dpiScale);
        uint32_t height = static_cast<uint32_t>(900 * dpiScale);

        return runTool(tool, "VDE Resource Editor", width, height, argc, argv);
    } else {
        // Configure input script from CLI args if provided (beyond the script file arg)
        vde::configureInputScriptFromArgs(tool, argc, argv);

        // For script mode, run headless (minimal window)
        vde::GameSettings settings;
        settings.gameName = "VDE Resource Editor (Script Mode)";
        settings.display.windowWidth = 800;
        settings.display.windowHeight = 600;
        settings.debug.enableValidation = false;

        if (!tool.initialize(settings)) {
            std::cerr << "Failed to initialize resource editor\n";
            return 1;
        }

        tool.run();

        std::cout << "\n====================================================\n";
        std::cout << "Script execution complete\n";
        std::cout << "====================================================\n";

        return tool.getExitCode();
    }
}
