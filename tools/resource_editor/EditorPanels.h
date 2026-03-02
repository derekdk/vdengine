#pragma once

/**
 * @file EditorPanels.h
 * @brief ImGui panel drawing routines for the Resource Editor.
 *
 * EditorPanels groups all ImGui window/panel rendering methods so the
 * main application loop stays clean.  Each method is self-contained and
 * creates its own ImGui window.
 */

#include <string>
#include <vector>

#include "CanvasRegistry.h"
#include "ToolPalette.h"
#include <imgui.h>

namespace vde::tools {

class CommandSystem;

/**
 * @brief Draws all ImGui panels that make up the Resource Editor UI.
 */
class EditorPanels {
  public:
    /**
     * @brief Draw the interactive command console (log + text input).
     * @param cmd Command system to execute typed commands against.
     * @param dpiScale DPI scaling factor for layout.
     */
    void drawCommandConsole(CommandSystem& cmd, float dpiScale);

    /**
     * @brief Draw the tool selection palette.
     * @param palette Current tool palette state.
     * @param cmd Command system for tool-change commands.
     * @param dpiScale DPI scaling factor for layout.
     */
    void drawToolPalette(ToolPalette& palette, CommandSystem& cmd, float dpiScale);

    /**
     * @brief Draw the color picker panel.
     * @param palette Current tool palette state.
     * @param cmd Command system for color-change commands.
     * @param dpiScale DPI scaling factor for layout.
     */
    void drawColorPicker(ToolPalette& palette, CommandSystem& cmd, float dpiScale);

    /**
     * @brief Draw the tab bar for switching between open canvases.
     * @param canvases Registry of all open canvases.
     * @param cmd Command system for canvas selection commands.
     * @param dpiScale DPI scaling factor for layout.
     */
    void drawCanvasTabs(CanvasRegistry& canvases, CommandSystem& cmd, float dpiScale);

    /**
     * @brief Draw a single canvas viewport with zoom, pan, and mouse interaction.
     * @param canvas The canvas to render.
     * @param palette Tool palette for mouse event translation.
     * @param cmd Command system for executing drawing commands.
     * @param isActive Whether this canvas is the currently active one.
     * @param dpiScale DPI scaling factor for layout.
     */
    void drawCanvasViewport(Canvas& canvas, ToolPalette& palette, CommandSystem& cmd, bool isActive,
                            float dpiScale);

    /**
     * @brief Draw viewports for all registered canvases.
     * @param canvases Registry of all open canvases.
     * @param palette Tool palette for mouse event translation.
     * @param cmd Command system for executing drawing commands.
     * @param dpiScale DPI scaling factor for layout.
     */
    void drawAllCanvasViewports(CanvasRegistry& canvases, ToolPalette& palette, CommandSystem& cmd,
                                float dpiScale);

    /**
     * @brief Draw the properties panel for the active canvas.
     * @param activeCanvas Pointer to the active canvas, or nullptr.
     * @param dpiScale DPI scaling factor for layout.
     */
    void drawPropertiesPanel(Canvas* activeCanvas, float dpiScale);

    /**
     * @brief Draw the main menu bar (File, Edit, View, Help).
     * @param cmd Command system for menu-triggered commands.
     * @param canvases Registry for "New" name generation.
     */
    void drawMenuBar(CommandSystem& cmd, CanvasRegistry& canvases);

  private:
    // --- Console input ---
    char m_consoleInputBuffer[512] = {};   ///< Text input buffer for the console.
    bool m_scrollConsoleToBottom = false;  ///< Flag to auto-scroll console on next frame.
    bool m_consoleInputFocused = false;    ///< Whether the console input was focused last frame.
    bool m_pendingClear = false;  ///< Clear the input buffer via callback (after multi-line paste).

    // --- Autocomplete state ---
    std::vector<std::string> m_completions;  ///< Current autocomplete suggestions.
    int m_selectedCompletion = -1;           ///< Highlighted completion index (-1 = none).
    bool m_showCompletions = false;          ///< Whether to display the completion popup.
    std::string m_paramHint;                 ///< Ghost text for current parameter hint.
    int m_completionReplaceStart = 0;        ///< Buffer offset where Tab replaces text.

    /** @brief Recompute completions and parameter hint from the current input. */
    void updateCompletions(const std::string& input, const CommandSystem& cmd);

    /** @brief Return a ghost-text hint showing remaining parameters. */
    std::string getParameterHint(const std::string& input) const;

    // --- New-canvas dialog ---
    bool m_showNewCanvasPopup = false;  ///< Trigger flag for the "New Canvas" modal.
    char m_newCanvasName[128] = {};     ///< Name buffer for the new-canvas dialog.
    int m_newCanvasWidth = 32;          ///< Width default for the new-canvas dialog.
    int m_newCanvasHeight = 32;         ///< Height default for the new-canvas dialog.
};

}  // namespace vde::tools
