/**
 * @file main.cpp
 * @brief Geometry REPL Tool - Interactive and scriptable geometry creation
 *
 * The Geometry REPL tool allows users to create custom 3D geometry through
 * a command-line interface. It supports both interactive mode (with GUI) and
 * script mode (headless batch processing).
 *
 * Usage:
 *   geometry_repl                  - Launch interactive mode with GUI
 *   geometry_repl <script.txt>     - Execute script in batch mode
 *
 * Commands:
 *   create <name> <type>           - Create new geometry (<type>: polygon, line)
 *   addpoint <name> <x> <y> <z>    - Add a point to geometry
 *   setcolor <name> <r> <g> <b>    - Set fill color (0-1 range)
 *   setvisible <name>              - Make geometry visible in scene
 *   hide <name>                    - Hide geometry from scene
 *   export <name> <filename>       - Export geometry to OBJ file
 *   list                           - List all geometry objects
 *   clear <name>                   - Delete a geometry object
 *   help                           - Show command reference
 *
 * Interactive mode controls:
 *   Left Mouse Drag - Rotate camera (when not over UI)
 *   Mouse Wheel     - Zoom camera in/out
 *   F1              - Toggle UI visibility
 *   F11             - Toggle fullscreen
 *   ESC             - Exit
 *
 * Script file format:
 *   - One command per line
 *   - Lines beginning with # are comments
 *   - Empty lines are ignored
 */

#include <vde/Window.h>

#include <iostream>

#include "GeometryReplScene.h"

using namespace vde::tools;

// =============================================================================
// Tool Game class
// =============================================================================

class GeometryReplTool : public BaseToolGame<BaseToolInputHandler, GeometryReplScene> {
  public:
    GeometryReplTool(ToolMode mode, const std::string& scriptFile = "")
        : BaseToolGame(mode), m_scriptFile(scriptFile) {}

    void onStart() override {
        BaseToolGame::onStart();

        // If in script mode, load and execute the script
        if (m_toolMode == ToolMode::SCRIPT && !m_scriptFile.empty()) {
            auto* scene = getToolScene();
            if (scene) {
                if (!scene->processScriptFile(m_scriptFile)) {
                    std::cerr << "Failed to process script file: " << m_scriptFile << std::endl;
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
    if (argc > 1) {
        scriptFile = argv[1];
        mode = ToolMode::SCRIPT;

        std::cout << "====================================================\n";
        std::cout << "VDE Geometry REPL Tool - Script Mode\n";
        std::cout << "====================================================\n";
        std::cout << "Processing script: " << scriptFile << "\n";
        std::cout << "====================================================\n\n";
    }

    GeometryReplTool tool(mode, scriptFile);

    if (mode == ToolMode::INTERACTIVE) {
        // Adjust resolution based on DPI
        float dpiScale = vde::Window::getPrimaryMonitorDPIScale();
        uint32_t width = static_cast<uint32_t>(1400 * dpiScale);
        uint32_t height = static_cast<uint32_t>(800 * dpiScale);

        return runTool(tool, "VDE Geometry REPL Tool", width, height);
    } else {
        // For script mode, run headless (minimal window)
        vde::GameSettings settings;
        settings.gameName = "VDE Geometry REPL (Script Mode)";
        settings.display.windowWidth = 800;
        settings.display.windowHeight = 600;
        settings.debug.enableValidation = false;  // Disable validation for batch mode

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
