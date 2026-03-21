/**
 * @file main.cpp
 * @brief Entry point for the VDE Sprite Editor tool.
 *
 * Supports two modes:
 * - INTERACTIVE (default): Full GUI with ImGui panels, REPL console
 * - SCRIPT: Batch mode, executes commands from a script file and exits
 *
 * Usage:
 *   vde_sprite_editor                   # Interactive mode
 *   vde_sprite_editor myscript.txt      # Script mode
 */

#include <iostream>

#include "SpriteEditorScene.h"

int main(int argc, char** argv) {
    using namespace vde::tools;

    setWorkingDirectoryToExecutablePath();

    ToolMode mode = ToolMode::INTERACTIVE;
    std::string scriptFile;
    std::string execFile;

    // Log all received args for diagnostics
    std::cerr << "[sprite_editor] argc=" << argc << "\n";
    for (int i = 0; i < argc; ++i) {
        std::cerr << "[sprite_editor]   argv[" << i << "]='" << argv[i] << "'\n";
    }

    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--input-script") {
            ++i;  // Skip value (handled by runTool/configureInputScriptFromArgs)
        } else if (arg == "--exec" && i + 1 < argc) {
            execFile = argv[++i];
        } else if (scriptFile.empty()) {
            scriptFile = arg;
            mode = ToolMode::SCRIPT;
        }
    }

    SpriteEditorGame tool(mode, scriptFile, execFile);

    if (mode == ToolMode::INTERACTIVE) {
        float dpiScale = vde::Window::getPrimaryMonitorDPIScale();
        uint32_t width = static_cast<uint32_t>(1400 * dpiScale);
        uint32_t height = static_cast<uint32_t>(900 * dpiScale);

        return runTool(tool, "VDE Sprite Editor", width, height, argc, argv);
    } else {
        vde::GameSettings settings;
        settings.display.windowWidth = 800;
        settings.display.windowHeight = 600;
        settings.gameName = "Sprite Editor (Script Mode)";
        settings.debug.enableValidation = false;

        if (!tool.initialize(settings)) {
            std::cerr << "Failed to initialize Sprite Editor\n";
            return 1;
        }

        tool.run();
        return tool.getExitCode();
    }
}
