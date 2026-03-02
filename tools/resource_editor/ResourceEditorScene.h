#pragma once

/**
 * @file ResourceEditorScene.h
 * @brief Main scene for the VDE Resource Editor tool.
 *
 * Owns the canvas registry, command system, tool palette, editor panels,
 * and editor context.  Bridges the BaseToolScene framework with the
 * resource editor subsystems.
 */

#include "../ToolBase.h"
#include "CanvasRegistry.h"
#include "CommandSystem.h"
#include "EditorPanels.h"
#include "ToolPalette.h"
#include "commands/EditorContext.h"

namespace vde::tools {

/**
 * @brief Scene that drives the 2D pixel-art resource editor.
 */
class ResourceEditorScene : public BaseToolScene {
  public:
    explicit ResourceEditorScene(ToolMode mode = ToolMode::INTERACTIVE);
    ~ResourceEditorScene() override = default;

    // BaseToolScene interface
    void onEnter() override;
    void update(float deltaTime) override;
    void executeCommand(const std::string& cmdLine) override;
    std::string getToolName() const override;
    std::string getToolDescription() const override;
    void drawDebugUI() override;
    void onBeforeImGuiShutdown() override;

    /** @brief Access the command system (e.g. for script execution). */
    CommandSystem& getCommandSystem() { return m_commandSystem; }

    /** @brief Access the canvas registry. */
    CanvasRegistry& getCanvasRegistry() { return m_canvasRegistry; }

  private:
    void syncGpuTextures();
    void cleanupImGuiTextures();

    CanvasRegistry m_canvasRegistry;
    CommandSystem m_commandSystem;
    ToolPalette m_toolPalette;
    EditorPanels m_editorPanels;
    EditorContext m_editorContext;
};

}  // namespace vde::tools
