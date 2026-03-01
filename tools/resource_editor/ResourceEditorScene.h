/**
 * @file ResourceEditorScene.h
 * @brief Resource Editor scene — interactive pixel art editor with multi-canvas support.
 *
 * Wires together CommandSystem, CanvasRegistry, ToolPalette, and EditorPanels
 * into a fully functional image editing tool.
 */

#pragma once

#include <map>
#include <memory>
#include <string>

#include "../ToolBase.h"
#include "CanvasRegistry.h"
#include "CommandSystem.h"
#include "EditorPanels.h"
#include "ToolPalette.h"

namespace vde {
namespace tools {

/**
 * @brief Scene for the Resource Editor tool.
 *
 * Manages all subsystems (command dispatch, canvas registry, tool palette,
 * editor panels) and provides the ImGui-based editing interface.
 */
class ResourceEditorScene : public BaseToolScene {
  public:
    explicit ResourceEditorScene(ToolMode mode = ToolMode::INTERACTIVE);
    ~ResourceEditorScene() override;

    void onEnter() override;
    void onBeforeImGuiShutdown() override;
    void executeCommand(const std::string& cmdLine) override;
    void drawDebugUI() override;
    void update(float deltaTime) override;

    std::string getToolName() const override { return "Resource Editor"; }

    std::string getToolDescription() const override {
        return "Interactive pixel art editor with multi-canvas support";
    }

    /**
     * @brief Get the command system for script execution.
     */
    CommandSystem& getCommandSystem() { return m_commandSystem; }

    /**
     * @brief Get the canvas registry.
     */
    CanvasRegistry& getCanvasRegistry() { return m_canvasRegistry; }

  private:
    CommandSystem m_commandSystem;
    CanvasRegistry m_canvasRegistry;
    ToolPalette m_toolPalette;
    EditorPanels m_editorPanels;
    float m_dpiScale = 1.0f;

    // --- Command registration ---
    void registerGlobalCommands();
    void registerCanvasCommands();

    // --- Global command handlers ---
    void cmdHelp(const std::string& args);
    void cmdNew(const std::string& args);
    void cmdOpen(const std::string& args);
    void cmdList(const std::string& args);
    void cmdSetActive(const std::string& args);
    void cmdSetColor(const std::string& args);
    void cmdSetTool(const std::string& args);
    void cmdSetSize(const std::string& args);
    void cmdLogSave(const std::string& args);
    void cmdLogClear(const std::string& args);
    void cmdRun(const std::string& args);
    void cmdExit(const std::string& args);

    // --- Canvas command handlers ---
    void cmdPaint(uint32_t canvasId, const std::string& args);
    void cmdFill(uint32_t canvasId, const std::string& args);
    void cmdFloodFill(uint32_t canvasId, const std::string& args);
    void cmdLine(uint32_t canvasId, const std::string& args);
    void cmdRect(uint32_t canvasId, const std::string& args);
    void cmdCircle(uint32_t canvasId, const std::string& args);
    void cmdPick(uint32_t canvasId, const std::string& args);
    void cmdUndo(uint32_t canvasId, const std::string& args);
    void cmdRedo(uint32_t canvasId, const std::string& args);
    void cmdSave(uint32_t canvasId, const std::string& args);
    void cmdSaveAs(uint32_t canvasId, const std::string& args);
    void cmdExport(uint32_t canvasId, const std::string& args);
    void cmdClose(uint32_t canvasId, const std::string& args);
    void cmdZoom(uint32_t canvasId, const std::string& args);
    void cmdPan(uint32_t canvasId, const std::string& args);
    void cmdFlipH(uint32_t canvasId, const std::string& args);
    void cmdFlipV(uint32_t canvasId, const std::string& args);
    void cmdResize(uint32_t canvasId, const std::string& args);
    void cmdCrop(uint32_t canvasId, const std::string& args);

    // --- GPU helpers ---
    void uploadCanvasTexture(Canvas& canvas);
    void cleanupCanvasTexture(Canvas& canvas);
};

}  // namespace tools
}  // namespace vde
