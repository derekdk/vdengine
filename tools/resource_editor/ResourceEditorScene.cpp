/**
 * @file ResourceEditorScene.cpp
 * @brief Implementation of the Resource Editor scene.
 *
 * Wires together all subsystems (CommandSystem, CanvasRegistry, ToolPalette,
 * EditorPanels) and registers all commands.
 */

#include "ResourceEditorScene.h"

#include <vde/Texture.h>
#include <vde/VulkanContext.h>
#include <vde/api/Game.h>

#include <algorithm>
#include <iostream>
#include <sstream>

#include "FileOperations.h"
#include <imgui_impl_vulkan.h>

namespace vde {
namespace tools {

// =============================================================================
// Construction / Destruction
// =============================================================================

ResourceEditorScene::ResourceEditorScene(ToolMode mode) : BaseToolScene(mode) {}

ResourceEditorScene::~ResourceEditorScene() = default;

// =============================================================================
// Scene lifecycle
// =============================================================================

void ResourceEditorScene::onEnter() {
    m_commandSystem.setRegistry(&m_canvasRegistry);

    if (getGame()) {
        m_dpiScale = getGame()->getDPIScale();
    }

    registerGlobalCommands();
    registerCanvasCommands();

    addConsoleMessage("VDE Resource Editor — Phase 1");
    addConsoleMessage("Type 'help' for a list of commands.");
    addConsoleMessage("Type 'new 32 32 mycanvas' to create a canvas.");
}

void ResourceEditorScene::onBeforeImGuiShutdown() {
    // Clean up all ImGui texture descriptors
    auto ids = m_canvasRegistry.getIds();
    for (uint32_t id : ids) {
        Canvas* canvas = m_canvasRegistry.getById(id);
        if (canvas) {
            cleanupCanvasTexture(*canvas);
        }
    }
}

void ResourceEditorScene::update(float deltaTime) {
    BaseToolScene::update(deltaTime);

    // Re-upload canvas textures when generation changes
    auto ids = m_canvasRegistry.getIds();
    for (uint32_t id : ids) {
        Canvas* canvas = m_canvasRegistry.getById(id);
        if (!canvas || !canvas->document)
            continue;

        if (canvas->document->getGeneration() > canvas->lastUploadedGeneration) {
            uploadCanvasTexture(*canvas);
        }
    }
}

void ResourceEditorScene::drawDebugUI() {
    if (!isDebugUIVisible())
        return;

    Canvas* activeCanvas = nullptr;
    if (m_commandSystem.getActiveCanvasId() > 0) {
        activeCanvas = m_canvasRegistry.getById(m_commandSystem.getActiveCanvasId());
    }

    m_editorPanels.drawMenuBar(m_commandSystem, m_canvasRegistry);
    m_editorPanels.drawToolPalette(m_toolPalette, m_commandSystem, m_dpiScale);
    m_editorPanels.drawColorPicker(m_toolPalette, m_commandSystem, m_dpiScale);
    m_editorPanels.drawCanvasTabs(m_canvasRegistry, m_commandSystem, m_dpiScale);
    m_editorPanels.drawAllCanvasViewports(m_canvasRegistry, m_toolPalette, m_commandSystem,
                                          m_dpiScale);
    m_editorPanels.drawPropertiesPanel(activeCanvas, m_dpiScale);
    m_editorPanels.drawCommandConsole(m_commandSystem, m_dpiScale);
}

void ResourceEditorScene::executeCommand(const std::string& cmdLine) {
    m_commandSystem.execute(cmdLine);

    // Mirror to base class console for ToolBase compat
    if (!m_commandSystem.getLog().empty()) {
        const auto& entry = m_commandSystem.getLog().back();
        if (!entry.result.empty()) {
            addConsoleMessage(entry.result);
        }
    }
}

// =============================================================================
// Command registration
// =============================================================================

void ResourceEditorScene::registerGlobalCommands() {
    m_commandSystem.registerGlobalCommand("help",
                                          "Show available commands or help for a specific command",
                                          [this](const std::string& args) { cmdHelp(args); });

    m_commandSystem.registerGlobalCommand("new", "Create a new canvas: new <w> <h> [name]",
                                          [this](const std::string& args) { cmdNew(args); });

    m_commandSystem.registerGlobalCommand("open", "Open an image file: open [filepath] [name]",
                                          [this](const std::string& args) { cmdOpen(args); });

    m_commandSystem.registerGlobalCommand("list", "List all canvases",
                                          [this](const std::string& args) { cmdList(args); });

    m_commandSystem.registerGlobalCommand("set_active", "Set active canvas: set_active <name|id>",
                                          [this](const std::string& args) { cmdSetActive(args); });

    m_commandSystem.registerGlobalCommand("setcolor", "Set drawing color: setcolor <#RRGGBBAA>",
                                          [this](const std::string& args) { cmdSetColor(args); });

    m_commandSystem.registerGlobalCommand(
        "settool", "Set active tool: settool <brush|eraser|fill|line|rect|circle|colorpicker>",
        [this](const std::string& args) { cmdSetTool(args); });

    m_commandSystem.registerGlobalCommand("setsize", "Set brush size: setsize <n>",
                                          [this](const std::string& args) { cmdSetSize(args); });

    m_commandSystem.registerGlobalCommand("log", "Log commands: log save <file> | log clear",
                                          [this](const std::string& args) { cmdLogSave(args); });

    m_commandSystem.registerGlobalCommand("run", "Execute a script file: run <filepath>",
                                          [this](const std::string& args) { cmdRun(args); });

    m_commandSystem.registerGlobalCommand("exit", "Exit the editor",
                                          [this](const std::string& args) { cmdExit(args); });

    // Phase 1 stubs for DSL commands
    m_commandSystem.registerGlobalCommand(
        "dsl_load", "Load a DSL file (not yet implemented)", [this](const std::string&) {
            m_commandSystem.setLastResult("Not yet implemented (Phase 3)", false);
        });

    m_commandSystem.registerGlobalCommand(
        "dsl", "Execute DSL command (not yet implemented)", [this](const std::string&) {
            m_commandSystem.setLastResult("Not yet implemented (Phase 3)", false);
        });

    m_commandSystem.registerGlobalCommand(
        "dsl_export", "Export DSL (not yet implemented)", [this](const std::string&) {
            m_commandSystem.setLastResult("Not yet implemented (Phase 3)", false);
        });
}

void ResourceEditorScene::registerCanvasCommands() {
    m_commandSystem.registerCanvasCommand(
        "paint", "Paint at position: paint <x> <y> <color> [size]",
        [this](uint32_t id, const std::string& args) { cmdPaint(id, args); });

    m_commandSystem.registerCanvasCommand(
        "fill", "Fill entire canvas: fill <color>",
        [this](uint32_t id, const std::string& args) { cmdFill(id, args); });

    m_commandSystem.registerCanvasCommand(
        "floodfill", "Flood fill at position: floodfill <x> <y> <color>",
        [this](uint32_t id, const std::string& args) { cmdFloodFill(id, args); });

    m_commandSystem.registerCanvasCommand(
        "line", "Draw line: line <x1> <y1> <x2> <y2> <color> [size]",
        [this](uint32_t id, const std::string& args) { cmdLine(id, args); });

    m_commandSystem.registerCanvasCommand(
        "rect", "Draw rectangle: rect <x> <y> <w> <h> <color> [filled]",
        [this](uint32_t id, const std::string& args) { cmdRect(id, args); });

    m_commandSystem.registerCanvasCommand(
        "circle", "Draw circle: circle <cx> <cy> <r> <color> [filled]",
        [this](uint32_t id, const std::string& args) { cmdCircle(id, args); });

    m_commandSystem.registerCanvasCommand(
        "pick", "Pick color at position: pick <x> <y>",
        [this](uint32_t id, const std::string& args) { cmdPick(id, args); });

    m_commandSystem.registerCanvasCommand(
        "undo", "Undo last change",
        [this](uint32_t id, const std::string& args) { cmdUndo(id, args); });

    m_commandSystem.registerCanvasCommand(
        "redo", "Redo last undone change",
        [this](uint32_t id, const std::string& args) { cmdRedo(id, args); });

    m_commandSystem.registerCanvasCommand(
        "save", "Save canvas: save [filepath]",
        [this](uint32_t id, const std::string& args) { cmdSave(id, args); });

    m_commandSystem.registerCanvasCommand(
        "saveas", "Save canvas to new file: saveas [filepath]",
        [this](uint32_t id, const std::string& args) { cmdSaveAs(id, args); });

    m_commandSystem.registerCanvasCommand(
        "export", "Export canvas: export [filepath]",
        [this](uint32_t id, const std::string& args) { cmdExport(id, args); });

    m_commandSystem.registerCanvasCommand(
        "close", "Close canvas",
        [this](uint32_t id, const std::string& args) { cmdClose(id, args); });

    m_commandSystem.registerCanvasCommand(
        "zoom", "Set zoom level: zoom <level>",
        [this](uint32_t id, const std::string& args) { cmdZoom(id, args); });

    m_commandSystem.registerCanvasCommand(
        "pan", "Pan canvas: pan <dx> <dy>",
        [this](uint32_t id, const std::string& args) { cmdPan(id, args); });

    m_commandSystem.registerCanvasCommand(
        "fliph", "Flip canvas horizontally",
        [this](uint32_t id, const std::string& args) { cmdFlipH(id, args); });

    m_commandSystem.registerCanvasCommand(
        "flipv", "Flip canvas vertically",
        [this](uint32_t id, const std::string& args) { cmdFlipV(id, args); });

    m_commandSystem.registerCanvasCommand(
        "resize", "Resize canvas: resize <w> <h>",
        [this](uint32_t id, const std::string& args) { cmdResize(id, args); });

    m_commandSystem.registerCanvasCommand(
        "crop", "Crop canvas: crop <x> <y> <w> <h>",
        [this](uint32_t id, const std::string& args) { cmdCrop(id, args); });

    // Phase 1 layer stubs
    for (const auto& name :
         {"layer_add", "layer_remove", "layer_select", "layer_opacity", "layer_visibility"}) {
        m_commandSystem.registerCanvasCommand(
            name, "Layer command (not yet implemented)", [this](uint32_t, const std::string&) {
                m_commandSystem.setLastResult("Not yet implemented (Phase 2)", false);
            });
    }

    // Phase 1 stubs for advanced commands
    m_commandSystem.registerCanvasCommand("gradient_fill", "Gradient fill (not yet implemented)",
                                          [this](uint32_t, const std::string&) {
                                              m_commandSystem.setLastResult(
                                                  "Not yet implemented (Phase 2)", false);
                                          });

    m_commandSystem.registerCanvasCommand(
        "copy_area", "Copy area (not yet implemented)", [this](uint32_t, const std::string&) {
            m_commandSystem.setLastResult("Not yet implemented (Phase 2)", false);
        });
}

// =============================================================================
// Global command handlers
// =============================================================================

void ResourceEditorScene::cmdHelp(const std::string& args) {
    if (args.empty()) {
        auto names = m_commandSystem.getCommandNames();
        std::string helpText = "Available commands:\n";
        for (const auto& name : names) {
            std::string help = m_commandSystem.getHelpText(name);
            helpText += "  " + name + " — " + help + "\n";
        }
        m_commandSystem.setLastResult(helpText);
    } else {
        std::string help = m_commandSystem.getHelpText(args);
        if (help.empty()) {
            m_commandSystem.setLastResult("Unknown command: " + args, false);
        } else {
            m_commandSystem.setLastResult(args + " — " + help);
        }
    }
}

void ResourceEditorScene::cmdNew(const std::string& args) {
    std::istringstream iss(args);
    int w = 0, h = 0;
    std::string name;
    iss >> w >> h >> name;

    if (w <= 0 || h <= 0) {
        m_commandSystem.setLastResult("Usage: new <width> <height> [name]", false);
        return;
    }

    if (name.empty()) {
        name = m_canvasRegistry.generateUniqueName();
    }

    auto doc = std::make_unique<ImageDocument>();
    doc->createNew(static_cast<uint32_t>(w), static_cast<uint32_t>(h));

    Canvas* canvas = m_canvasRegistry.create(name, std::move(doc));
    if (!canvas) {
        m_commandSystem.setLastResult("Error: Canvas name '" + name + "' already exists", false);
        return;
    }

    m_commandSystem.setActiveCanvasId(canvas->id);
    m_commandSystem.setLastResult("Created canvas '" + name + "' (" + std::to_string(w) + "x" +
                                  std::to_string(h) + ")");
}

void ResourceEditorScene::cmdOpen(const std::string& args) {
    std::istringstream iss(args);
    std::string filepath, name;
    iss >> filepath >> name;

    if (filepath.empty()) {
        // Open file dialog
        filepath = openImageFileDialog("Open Image");
        if (filepath.empty()) {
            m_commandSystem.setLastResult("Cancelled");
            return;
        }
    }

    auto doc = std::make_unique<ImageDocument>();
    if (!doc->loadFromFile(filepath)) {
        m_commandSystem.setLastResult("Error: Failed to load image: " + filepath, false);
        return;
    }

    if (name.empty()) {
        // Extract filename without extension
        std::filesystem::path p(filepath);
        name = p.stem().string();
        if (m_canvasRegistry.hasName(name)) {
            name = m_canvasRegistry.generateUniqueName(name);
        }
    }

    doc->setFilePath(filepath);
    Canvas* canvas = m_canvasRegistry.create(name, std::move(doc));
    if (!canvas) {
        m_commandSystem.setLastResult("Error: Canvas name '" + name + "' already exists", false);
        return;
    }

    m_commandSystem.setActiveCanvasId(canvas->id);
    m_commandSystem.setLastResult("Opened '" + filepath + "' as '" + name + "'");
}

void ResourceEditorScene::cmdList(const std::string& /*args*/) {
    auto ids = m_canvasRegistry.getIds();
    if (ids.empty()) {
        m_commandSystem.setLastResult("No canvases");
        return;
    }

    std::string result = "Canvases:\n";
    for (uint32_t id : ids) {
        Canvas* canvas = m_canvasRegistry.getById(id);
        if (!canvas)
            continue;

        bool isActive = (id == m_commandSystem.getActiveCanvasId());
        result += "  " + std::string(isActive ? "* " : "  ");
        result += "[" + std::to_string(id) + "] " + canvas->name;
        if (canvas->document) {
            result += " (" + std::to_string(canvas->document->getWidth()) + "x" +
                      std::to_string(canvas->document->getHeight()) + ")";
            if (canvas->document->isDirty()) {
                result += " [modified]";
            }
        }
        result += "\n";
    }
    m_commandSystem.setLastResult(result);
}

void ResourceEditorScene::cmdSetActive(const std::string& args) {
    if (args.empty()) {
        m_commandSystem.setLastResult("Usage: set_active <name|id>", false);
        return;
    }

    Canvas* canvas = m_canvasRegistry.resolve(args);
    if (!canvas) {
        m_commandSystem.setLastResult("Error: Canvas '" + args + "' not found", false);
        return;
    }

    m_commandSystem.setActiveCanvasId(canvas->id);
    m_commandSystem.setLastResult("Active canvas: " + canvas->name);
}

void ResourceEditorScene::cmdSetColor(const std::string& args) {
    RGBAColor color;
    if (!ToolPalette::hexToColor(args, color)) {
        m_commandSystem.setLastResult("Usage: setcolor <#RRGGBB or #RRGGBBAA>", false);
        return;
    }
    m_toolPalette.setColor(color);
    m_commandSystem.setLastResult("Color: " + ToolPalette::colorToHex(color));
}

void ResourceEditorScene::cmdSetTool(const std::string& args) {
    EditorTool tool;
    if (!ToolPalette::stringToTool(args, tool)) {
        m_commandSystem.setLastResult(
            "Usage: settool <brush|eraser|fill|line|rect|circle|colorpicker>", false);
        return;
    }
    m_toolPalette.setTool(tool);
    m_commandSystem.setLastResult("Tool: " + ToolPalette::toolToString(tool));
}

void ResourceEditorScene::cmdSetSize(const std::string& args) {
    int size = 0;
    try {
        size = std::stoi(args);
    } catch (...) {
        m_commandSystem.setLastResult("Usage: setsize <n>", false);
        return;
    }
    size = std::max(0, std::min(64, size));
    m_toolPalette.setBrushSize(size);
    m_commandSystem.setLastResult("Brush size: " + std::to_string(size));
}

void ResourceEditorScene::cmdLogSave(const std::string& args) {
    std::istringstream iss(args);
    std::string subCmd;
    iss >> subCmd;

    if (subCmd == "save") {
        std::string filepath;
        iss >> filepath;
        if (filepath.empty()) {
            filepath = saveScriptFileDialog("Save Command Log");
            if (filepath.empty()) {
                m_commandSystem.setLastResult("Cancelled");
                return;
            }
        }
        if (m_commandSystem.saveFullLog(filepath)) {
            m_commandSystem.setLastResult("Log saved to " + filepath);
        } else {
            m_commandSystem.setLastResult("Error: Failed to save log", false);
        }
    } else if (subCmd == "clear") {
        m_commandSystem.clearLog();
        m_commandSystem.setLastResult("Log cleared");
    } else {
        m_commandSystem.setLastResult("Usage: log save <file> | log clear", false);
    }
}

void ResourceEditorScene::cmdRun(const std::string& args) {
    if (args.empty()) {
        std::string filepath = openScriptFileDialog("Open Script");
        if (filepath.empty()) {
            m_commandSystem.setLastResult("Cancelled");
            return;
        }
        if (m_commandSystem.executeScript(filepath)) {
            m_commandSystem.setLastResult("Script executed: " + filepath);
        } else {
            m_commandSystem.setLastResult("Error: Script execution failed", false);
        }
    } else {
        if (m_commandSystem.executeScript(args)) {
            m_commandSystem.setLastResult("Script executed: " + args);
        } else {
            m_commandSystem.setLastResult("Error: Script execution failed", false);
        }
    }
}

void ResourceEditorScene::cmdExit(const std::string& /*args*/) {
    if (getGame()) {
        getGame()->quit();
    }
}

// =============================================================================
// Canvas command handlers
// =============================================================================

void ResourceEditorScene::cmdPaint(uint32_t canvasId, const std::string& args) {
    Canvas* canvas = m_canvasRegistry.getById(canvasId);
    if (!canvas || !canvas->document) {
        m_commandSystem.setLastResult("Error: Invalid canvas", false);
        return;
    }

    std::istringstream iss(args);
    int x, y;
    std::string hexColor;
    int size = 0;
    iss >> x >> y >> hexColor >> size;

    RGBAColor color;
    if (!ToolPalette::hexToColor(hexColor, color)) {
        m_commandSystem.setLastResult("Usage: paint <x> <y> <#color> [size]", false);
        return;
    }

    canvas->document->snapshotForUndo();
    canvas->document->drawBrush(x, y, size, color);
    m_commandSystem.setLastResult("OK");
}

void ResourceEditorScene::cmdFill(uint32_t canvasId, const std::string& args) {
    Canvas* canvas = m_canvasRegistry.getById(canvasId);
    if (!canvas || !canvas->document) {
        m_commandSystem.setLastResult("Error: Invalid canvas", false);
        return;
    }

    RGBAColor color;
    if (!ToolPalette::hexToColor(args, color)) {
        m_commandSystem.setLastResult("Usage: fill <#color>", false);
        return;
    }

    canvas->document->snapshotForUndo();
    canvas->document->fill(color);
    m_commandSystem.setLastResult("OK");
}

void ResourceEditorScene::cmdFloodFill(uint32_t canvasId, const std::string& args) {
    Canvas* canvas = m_canvasRegistry.getById(canvasId);
    if (!canvas || !canvas->document) {
        m_commandSystem.setLastResult("Error: Invalid canvas", false);
        return;
    }

    std::istringstream iss(args);
    int x, y;
    std::string hexColor;
    iss >> x >> y >> hexColor;

    RGBAColor color;
    if (!ToolPalette::hexToColor(hexColor, color)) {
        m_commandSystem.setLastResult("Usage: floodfill <x> <y> <#color>", false);
        return;
    }

    canvas->document->snapshotForUndo();
    canvas->document->floodFill(x, y, color);
    m_commandSystem.setLastResult("OK");
}

void ResourceEditorScene::cmdLine(uint32_t canvasId, const std::string& args) {
    Canvas* canvas = m_canvasRegistry.getById(canvasId);
    if (!canvas || !canvas->document) {
        m_commandSystem.setLastResult("Error: Invalid canvas", false);
        return;
    }

    std::istringstream iss(args);
    int x1, y1, x2, y2;
    std::string hexColor;
    int thickness = 1;
    iss >> x1 >> y1 >> x2 >> y2 >> hexColor >> thickness;

    RGBAColor color;
    if (!ToolPalette::hexToColor(hexColor, color)) {
        m_commandSystem.setLastResult("Usage: line <x1> <y1> <x2> <y2> <#color> [thickness]",
                                      false);
        return;
    }

    canvas->document->snapshotForUndo();
    canvas->document->drawLine(x1, y1, x2, y2, color, thickness);
    m_commandSystem.setLastResult("OK");
}

void ResourceEditorScene::cmdRect(uint32_t canvasId, const std::string& args) {
    Canvas* canvas = m_canvasRegistry.getById(canvasId);
    if (!canvas || !canvas->document) {
        m_commandSystem.setLastResult("Error: Invalid canvas", false);
        return;
    }

    std::istringstream iss(args);
    int x, y, w, h;
    std::string hexColor;
    std::string fillStr;
    iss >> x >> y >> w >> h >> hexColor >> fillStr;

    RGBAColor color;
    if (!ToolPalette::hexToColor(hexColor, color)) {
        m_commandSystem.setLastResult("Usage: rect <x> <y> <w> <h> <#color> [filled]", false);
        return;
    }

    bool filled = (fillStr == "filled");
    canvas->document->snapshotForUndo();
    canvas->document->drawRect(x, y, w, h, color, filled);
    m_commandSystem.setLastResult("OK");
}

void ResourceEditorScene::cmdCircle(uint32_t canvasId, const std::string& args) {
    Canvas* canvas = m_canvasRegistry.getById(canvasId);
    if (!canvas || !canvas->document) {
        m_commandSystem.setLastResult("Error: Invalid canvas", false);
        return;
    }

    std::istringstream iss(args);
    int cx, cy, r;
    std::string hexColor;
    std::string fillStr;
    iss >> cx >> cy >> r >> hexColor >> fillStr;

    RGBAColor color;
    if (!ToolPalette::hexToColor(hexColor, color)) {
        m_commandSystem.setLastResult("Usage: circle <cx> <cy> <r> <#color> [filled]", false);
        return;
    }

    bool filled = (fillStr == "filled");
    canvas->document->snapshotForUndo();
    canvas->document->drawCircle(cx, cy, r, color, filled);
    m_commandSystem.setLastResult("OK");
}

void ResourceEditorScene::cmdPick(uint32_t canvasId, const std::string& args) {
    Canvas* canvas = m_canvasRegistry.getById(canvasId);
    if (!canvas || !canvas->document) {
        m_commandSystem.setLastResult("Error: Invalid canvas", false);
        return;
    }

    std::istringstream iss(args);
    int x, y;
    iss >> x >> y;

    RGBAColor color = canvas->document->getPixel(x, y);
    m_toolPalette.setColor(color);
    m_commandSystem.setLastResult("Picked: " + ToolPalette::colorToHex(color));
}

void ResourceEditorScene::cmdUndo(uint32_t canvasId, const std::string& /*args*/) {
    Canvas* canvas = m_canvasRegistry.getById(canvasId);
    if (!canvas || !canvas->document) {
        m_commandSystem.setLastResult("Error: Invalid canvas", false);
        return;
    }

    if (canvas->document->undo()) {
        m_commandSystem.setLastResult("Undo");
    } else {
        m_commandSystem.setLastResult("Nothing to undo");
    }
}

void ResourceEditorScene::cmdRedo(uint32_t canvasId, const std::string& /*args*/) {
    Canvas* canvas = m_canvasRegistry.getById(canvasId);
    if (!canvas || !canvas->document) {
        m_commandSystem.setLastResult("Error: Invalid canvas", false);
        return;
    }

    if (canvas->document->redo()) {
        m_commandSystem.setLastResult("Redo");
    } else {
        m_commandSystem.setLastResult("Nothing to redo");
    }
}

void ResourceEditorScene::cmdSave(uint32_t canvasId, const std::string& args) {
    Canvas* canvas = m_canvasRegistry.getById(canvasId);
    if (!canvas || !canvas->document) {
        m_commandSystem.setLastResult("Error: Invalid canvas", false);
        return;
    }

    std::string filepath = args;
    // Trim
    size_t s = filepath.find_first_not_of(" \t");
    if (s != std::string::npos)
        filepath = filepath.substr(s);
    size_t e = filepath.find_last_not_of(" \t");
    if (e != std::string::npos)
        filepath = filepath.substr(0, e + 1);

    if (filepath.empty()) {
        filepath = canvas->document->getFilePath();
    }

    if (filepath.empty()) {
        filepath = saveImageFileDialog("Save Image");
        if (filepath.empty()) {
            m_commandSystem.setLastResult("Cancelled");
            return;
        }
    }

    if (canvas->document->saveToFile(filepath)) {
        canvas->document->setFilePath(filepath);
        canvas->document->clearDirty();
        m_commandSystem.setLastResult("Saved: " + filepath);
    } else {
        m_commandSystem.setLastResult("Error: Failed to save", false);
    }
}

void ResourceEditorScene::cmdSaveAs(uint32_t canvasId, const std::string& args) {
    Canvas* canvas = m_canvasRegistry.getById(canvasId);
    if (!canvas || !canvas->document) {
        m_commandSystem.setLastResult("Error: Invalid canvas", false);
        return;
    }

    std::string filepath = args;
    size_t s = filepath.find_first_not_of(" \t");
    if (s != std::string::npos)
        filepath = filepath.substr(s);
    size_t e = filepath.find_last_not_of(" \t");
    if (e != std::string::npos)
        filepath = filepath.substr(0, e + 1);

    if (filepath.empty()) {
        filepath = saveImageFileDialog("Save Image As");
        if (filepath.empty()) {
            m_commandSystem.setLastResult("Cancelled");
            return;
        }
    }

    if (canvas->document->saveToFile(filepath)) {
        canvas->document->setFilePath(filepath);
        canvas->document->clearDirty();
        m_commandSystem.setLastResult("Saved: " + filepath);
    } else {
        m_commandSystem.setLastResult("Error: Failed to save", false);
    }
}

void ResourceEditorScene::cmdExport(uint32_t canvasId, const std::string& args) {
    Canvas* canvas = m_canvasRegistry.getById(canvasId);
    if (!canvas || !canvas->document) {
        m_commandSystem.setLastResult("Error: Invalid canvas", false);
        return;
    }

    std::string filepath = args;
    size_t s = filepath.find_first_not_of(" \t");
    if (s != std::string::npos)
        filepath = filepath.substr(s);
    size_t e = filepath.find_last_not_of(" \t");
    if (e != std::string::npos)
        filepath = filepath.substr(0, e + 1);

    if (filepath.empty()) {
        filepath = saveImageFileDialog("Export Image");
        if (filepath.empty()) {
            m_commandSystem.setLastResult("Cancelled");
            return;
        }
    }

    if (canvas->document->exportToFile(filepath)) {
        m_commandSystem.setLastResult("Exported: " + filepath);
    } else {
        m_commandSystem.setLastResult("Error: Export failed", false);
    }
}

void ResourceEditorScene::cmdClose(uint32_t canvasId, const std::string& /*args*/) {
    Canvas* canvas = m_canvasRegistry.getById(canvasId);
    if (!canvas) {
        m_commandSystem.setLastResult("Error: Invalid canvas", false);
        return;
    }

    std::string name = canvas->name;
    cleanupCanvasTexture(*canvas);

    // If closing the active canvas, try to switch to another
    if (m_commandSystem.getActiveCanvasId() == canvasId) {
        m_canvasRegistry.remove(canvasId);
        auto ids = m_canvasRegistry.getIds();
        if (!ids.empty()) {
            m_commandSystem.setActiveCanvasId(ids.front());
        } else {
            m_commandSystem.setActiveCanvasId(0);
        }
    } else {
        m_canvasRegistry.remove(canvasId);
    }

    m_commandSystem.setLastResult("Closed canvas: " + name);
}

void ResourceEditorScene::cmdZoom(uint32_t canvasId, const std::string& args) {
    Canvas* canvas = m_canvasRegistry.getById(canvasId);
    if (!canvas) {
        m_commandSystem.setLastResult("Error: Invalid canvas", false);
        return;
    }

    std::string trimmed = args;
    size_t s = trimmed.find_first_not_of(" \t");
    if (s != std::string::npos)
        trimmed = trimmed.substr(s);

    if (trimmed == "in") {
        canvas->zoomLevel = std::min(64.0f, canvas->zoomLevel * 2.0f);
    } else if (trimmed == "out") {
        canvas->zoomLevel = std::max(1.0f, canvas->zoomLevel / 2.0f);
    } else {
        try {
            float level = std::stof(trimmed);
            canvas->zoomLevel = std::max(1.0f, std::min(64.0f, level));
        } catch (...) {
            m_commandSystem.setLastResult("Usage: zoom <level|in|out>", false);
            return;
        }
    }

    m_commandSystem.setLastResult("Zoom: " + std::to_string(static_cast<int>(canvas->zoomLevel)) +
                                  "x");
}

void ResourceEditorScene::cmdPan(uint32_t canvasId, const std::string& args) {
    Canvas* canvas = m_canvasRegistry.getById(canvasId);
    if (!canvas) {
        m_commandSystem.setLastResult("Error: Invalid canvas", false);
        return;
    }

    std::istringstream iss(args);
    float dx, dy;
    iss >> dx >> dy;

    canvas->panX += dx;
    canvas->panY += dy;
    m_commandSystem.setLastResult("Pan: (" + std::to_string(canvas->panX) + ", " +
                                  std::to_string(canvas->panY) + ")");
}

void ResourceEditorScene::cmdFlipH(uint32_t canvasId, const std::string& /*args*/) {
    Canvas* canvas = m_canvasRegistry.getById(canvasId);
    if (!canvas || !canvas->document) {
        m_commandSystem.setLastResult("Error: Invalid canvas", false);
        return;
    }

    canvas->document->snapshotForUndo();
    canvas->document->flipHorizontal();
    m_commandSystem.setLastResult("Flipped horizontally");
}

void ResourceEditorScene::cmdFlipV(uint32_t canvasId, const std::string& /*args*/) {
    Canvas* canvas = m_canvasRegistry.getById(canvasId);
    if (!canvas || !canvas->document) {
        m_commandSystem.setLastResult("Error: Invalid canvas", false);
        return;
    }

    canvas->document->snapshotForUndo();
    canvas->document->flipVertical();
    m_commandSystem.setLastResult("Flipped vertically");
}

void ResourceEditorScene::cmdResize(uint32_t canvasId, const std::string& args) {
    Canvas* canvas = m_canvasRegistry.getById(canvasId);
    if (!canvas || !canvas->document) {
        m_commandSystem.setLastResult("Error: Invalid canvas", false);
        return;
    }

    std::istringstream iss(args);
    int w, h;
    iss >> w >> h;

    if (w <= 0 || h <= 0) {
        m_commandSystem.setLastResult("Usage: resize <w> <h>", false);
        return;
    }

    canvas->document->snapshotForUndo();
    canvas->document->resize(static_cast<uint32_t>(w), static_cast<uint32_t>(h));

    // Force GPU texture re-creation on next update
    cleanupCanvasTexture(*canvas);
    canvas->lastUploadedGeneration = 0;

    m_commandSystem.setLastResult("Resized to " + std::to_string(w) + "x" + std::to_string(h));
}

void ResourceEditorScene::cmdCrop(uint32_t canvasId, const std::string& args) {
    Canvas* canvas = m_canvasRegistry.getById(canvasId);
    if (!canvas || !canvas->document) {
        m_commandSystem.setLastResult("Error: Invalid canvas", false);
        return;
    }

    std::istringstream iss(args);
    int x, y, w, h;
    iss >> x >> y >> w >> h;

    if (w <= 0 || h <= 0) {
        m_commandSystem.setLastResult("Usage: crop <x> <y> <w> <h>", false);
        return;
    }

    canvas->document->snapshotForUndo();
    canvas->document->crop(x, y, w, h);

    // Force GPU texture re-creation on next update
    cleanupCanvasTexture(*canvas);
    canvas->lastUploadedGeneration = 0;

    m_commandSystem.setLastResult("Cropped to " + std::to_string(canvas->document->getWidth()) +
                                  "x" + std::to_string(canvas->document->getHeight()));
}

// =============================================================================
// GPU texture helpers
// =============================================================================

void ResourceEditorScene::uploadCanvasTexture(Canvas& canvas) {
    if (!canvas.document || !canvas.document->isValid())
        return;

    auto* game = getGame();
    if (!game || !game->getVulkanContext())
        return;

    auto* ctx = game->getVulkanContext();

    uint32_t docW = canvas.document->getWidth();
    uint32_t docH = canvas.document->getHeight();

    // Check if we need to recreate the texture (dimensions changed)
    bool needRecreate = !canvas.gpuTexture || !canvas.gpuTexture->isOnGPU() ||
                        canvas.gpuTexture->getWidth() != docW ||
                        canvas.gpuTexture->getHeight() != docH;

    if (needRecreate) {
        // Clean up old texture
        cleanupCanvasTexture(canvas);

        // Create new texture
        canvas.gpuTexture = std::make_shared<vde::Texture>();
        canvas.gpuTexture->loadFromData(canvas.document->getPixelData(), docW, docH);
        canvas.gpuTexture->uploadToGPU(ctx);

        // Create ImGui descriptor
        if (canvas.gpuTexture->isOnGPU()) {
            canvas.imguiTextureId = ImGui_ImplVulkan_AddTexture(
                canvas.gpuTexture->getSampler(), canvas.gpuTexture->getImageView(),
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }
    } else {
        // Re-upload pixel data to existing texture
        // For small pixel-art canvases, recreating is simplest and fast enough.
        // TODO: Investigate staging buffer reuse for larger canvases in Phase 2.
        cleanupCanvasTexture(canvas);

        canvas.gpuTexture = std::make_shared<vde::Texture>();
        canvas.gpuTexture->loadFromData(canvas.document->getPixelData(), docW, docH);
        canvas.gpuTexture->uploadToGPU(ctx);

        if (canvas.gpuTexture->isOnGPU()) {
            canvas.imguiTextureId = ImGui_ImplVulkan_AddTexture(
                canvas.gpuTexture->getSampler(), canvas.gpuTexture->getImageView(),
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }
    }

    canvas.lastUploadedGeneration = canvas.document->getGeneration();
}

void ResourceEditorScene::cleanupCanvasTexture(Canvas& canvas) {
    if (canvas.imguiTextureId != VK_NULL_HANDLE) {
        ImGui_ImplVulkan_RemoveTexture(canvas.imguiTextureId);
        canvas.imguiTextureId = VK_NULL_HANDLE;
    }

    if (canvas.gpuTexture) {
        canvas.gpuTexture.reset();
    }
}

}  // namespace tools
}  // namespace vde
