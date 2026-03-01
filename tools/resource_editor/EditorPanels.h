/**
 * @file EditorPanels.h
 * @brief ImGui UI panels for the Resource Editor.
 *
 * All ImGui drawing code for the editor UI is consolidated here: command
 * console, tool palette, color picker, canvas tabs, canvas viewport,
 * properties panel, and menu bar.
 */

#pragma once

#include <string>

#include "CanvasRegistry.h"
#include "CommandSystem.h"
#include "ToolPalette.h"
#include <imgui.h>

namespace vde {
namespace tools {

/**
 * @brief All ImGui UI panels for the Resource Editor.
 *
 * Each draw method is called from ResourceEditorScene::drawDebugUI().
 * The panels communicate with subsystems by executing commands through
 * the CommandSystem rather than modifying state directly.
 */
class EditorPanels {
  public:
    EditorPanels() = default;
    ~EditorPanels() = default;

    /**
     * @brief Draw the command console (log + input field).
     */
    void drawCommandConsole(CommandSystem& cmd, float dpiScale);

    /**
     * @brief Draw the tool palette panel.
     */
    void drawToolPalette(ToolPalette& palette, CommandSystem& cmd, float dpiScale);

    /**
     * @brief Draw the color picker panel.
     */
    void drawColorPicker(ToolPalette& palette, CommandSystem& cmd, float dpiScale);

    /**
     * @brief Draw canvas tab bar.
     */
    void drawCanvasTabs(CanvasRegistry& canvases, CommandSystem& cmd, float dpiScale);

    /**
     * @brief Draw a single canvas viewport with mouse interaction.
     * @param canvas The canvas to draw
     * @param palette Tool palette for mouse event handling
     * @param cmd Command system for executing commands
     * @param isActive Whether this canvas is the active one
     * @param dpiScale DPI scale factor
     */
    void drawCanvasViewport(Canvas& canvas, ToolPalette& palette, CommandSystem& cmd, bool isActive,
                            float dpiScale);

    /**
     * @brief Draw all canvas viewports.
     */
    void drawAllCanvasViewports(CanvasRegistry& canvases, ToolPalette& palette, CommandSystem& cmd,
                                float dpiScale);

    /**
     * @brief Draw the properties/info panel for the active canvas.
     */
    void drawPropertiesPanel(Canvas* activeCanvas, float dpiScale);

    /**
     * @brief Draw the main menu bar.
     */
    void drawMenuBar(CommandSystem& cmd, CanvasRegistry& canvases);

  private:
    char m_consoleInputBuffer[512] = {};
    bool m_scrollConsoleToBottom = false;

    // New canvas popup state
    bool m_showNewCanvasPopup = false;
    int m_newCanvasWidth = 32;
    int m_newCanvasHeight = 32;
    char m_newCanvasName[64] = {};
};

}  // namespace tools
}  // namespace vde
