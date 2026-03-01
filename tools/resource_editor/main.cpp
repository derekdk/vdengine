/**
 * @file main.cpp
 * @brief Resource Editor Tool — Interactive and scriptable pixel art editor
 *
 * The Resource Editor allows users to create and edit pixel art images
 * through both an interactive GUI and a command-line scripting interface.
 * It supports multi-canvas editing, undo/redo, and standard image formats.
 *
 * Usage:
 *   vde_resource_editor                 - Launch interactive mode with GUI
 *   vde_resource_editor <script.txt>    - Execute script in batch mode
 *
 * Interactive mode controls:
 *   F1              - Toggle UI visibility
 *   F11             - Toggle fullscreen
 *   ESC             - Exit
 *
 * See README.md for full command reference.
 */

#include <vde/Window.h>

#include <iostream>

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

        // If in script mode, load and execute the script
        if (m_toolMode == ToolMode::SCRIPT && !m_scriptFile.empty()) {
            auto* scene = getToolScene();
            if (scene) {
                if (!scene->getCommandSystem().executeScript(m_scriptFile)) {
                    std::cerr << "Script execution had errors: " << m_scriptFile << std::endl;
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
        // Configure input script from CLI args if provided
        vde::configureInputScriptFromArgs(tool, argc, argv);

        // For script mode, run headless (minimal window)
        vde::GameSettings settings;
        settings.gameName = "VDE Resource Editor (Script Mode)";
        settings.display.windowWidth = 800;
        settings.display.windowHeight = 600;
        settings.debug.enableValidation = false;

        if (!tool.initialize(settings)) {
            std::cerr << "Failed to initialize tool\n";
            return 1;
        }

        tool.run();

        std::cout << "\n====================================================\n";
        std::cout << "Script execution complete\n";
        std::cout << "====================================================\n";

        return tool.getExitCode();
    }
}
