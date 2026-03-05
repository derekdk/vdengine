/**
 * @file main.cpp
 * @brief VDE Resource Editor - 2D pixel art editor with command console
 *
 * Usage:
 *   vde_resource_editor                          - Interactive mode with GUI
 *   vde_resource_editor <script.txt>             - Execute commands and exit
 *   vde_resource_editor <script.txt> --log-console       - Also print log to stdout
 *   vde_resource_editor <script.txt> --log-file <path>   - Also write log to file
 */

#include <vde/Window.h>

#include <fstream>
#include <iostream>
#include <string>

#include "ResourceEditorScene.h"

using namespace vde::tools;

// =============================================================================
// Tool Game class
// =============================================================================

class ResourceEditorGame : public BaseToolGame<BaseToolInputHandler, ResourceEditorScene> {
  public:
    ResourceEditorGame(ToolMode mode, const std::string& scriptFile = "", bool logToConsole = false,
                       const std::string& logFile = "")
        : BaseToolGame(mode), m_scriptFile(scriptFile), m_logToConsole(logToConsole),
          m_logFile(logFile) {}

    void onStart() override {
        BaseToolGame::onStart();

        if (m_toolMode == ToolMode::SCRIPT && !m_scriptFile.empty()) {
            auto* scene = getToolScene();
            if (scene) {
                // In script mode the game loop never runs (quit() is called before it
                // starts), so processPendingSceneChange() is never called and the scene's
                // onEnter() is never invoked by the framework.  Call it explicitly so that
                // the EditorContext (canvases, palette, commands, game) is wired up before
                // any commands execute.
                scene->onEnter();

                if (!scene->getCommandSystem().executeScript(m_scriptFile)) {
                    std::cerr << "Failed to process script file: " << m_scriptFile << "\n";
                    m_exitCode = 1;
                }

                // Debug log output (--log-console / --log-file)
                if (m_logToConsole) {
                    dumpLogToStream(scene->getCommandSystem(), std::cout);
                }
                if (!m_logFile.empty()) {
                    if (!scene->getCommandSystem().saveFullLog(m_logFile)) {
                        std::cerr << "Warning: failed to write log to: " << m_logFile << "\n";
                    } else {
                        std::cout << "Log written to: " << m_logFile << "\n";
                    }
                }

                // Exit after script execution
                quit();
            }
        }
    }

  private:
    std::string m_scriptFile;
    bool m_logToConsole = false;
    std::string m_logFile;

    /** Print the command log to any output stream (console or file preview). */
    static void dumpLogToStream(const CommandSystem& cmdSys, std::ostream& out) {
        out << "\n====================================================\n";
        out << "Command Execution Log\n";
        out << "====================================================\n";
        for (const auto& entry : cmdSys.getLog()) {
            out << "[" << entry.timestamp << "] ";
            if (!entry.canvasName.empty()) {
                out << "@" << entry.canvasName << " ";
            }
            out << entry.commandLine;
            if (!entry.result.empty()) {
                out << " -> " << entry.result;
            }
            if (!entry.success) {
                out << " [FAILED]";
            }
            out << "\n";
        }
        out << "====================================================\n";
    }
};

// =============================================================================
// Main
// =============================================================================

int main(int argc, char** argv) {
    ToolMode mode = ToolMode::INTERACTIVE;
    std::string scriptFile;
    bool logToConsole = false;
    std::string logFile;

    // Parse command line arguments
    // argv[1] (if present and not a flag) is the script file
    // Remaining args may include: --input-script <path>  --log-console  --log-file <path>
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--log-console") {
            logToConsole = true;
        } else if (arg == "--log-file" && i + 1 < argc) {
            logFile = argv[++i];
        } else if (arg == "--input-script") {
            // Skip the value; it is consumed by configureInputScriptFromArgs below
            ++i;
        } else if (arg.rfind("--", 0) != 0 && scriptFile.empty()) {
            // First non-flag argument is the script file
            scriptFile = arg;
            mode = ToolMode::SCRIPT;
        }
    }

    if (mode == ToolMode::SCRIPT) {
        std::cout << "====================================================\n";
        std::cout << "VDE Resource Editor - Script Mode\n";
        std::cout << "====================================================\n";
        std::cout << "Processing script: " << scriptFile << "\n";
        if (logToConsole) {
            std::cout << "Log output     : console (stdout)\n";
        }
        if (!logFile.empty()) {
            std::cout << "Log output     : " << logFile << "\n";
        }
        std::cout << "====================================================\n\n";
    }

    ResourceEditorGame tool(mode, scriptFile, logToConsole, logFile);

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
