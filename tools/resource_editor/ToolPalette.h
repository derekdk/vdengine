#pragma once

/**
 * @file ToolPalette.h
 * @brief Tool state and mouse-to-command translation for the Resource Editor.
 *
 * ToolPalette converts canvas mouse events into command strings that
 * the CommandSystem can execute.  It owns the current tool selection,
 * drawing color, brush size, and in-progress shape state.
 */

#include "commands/CommandTypes.h"

#include <cstdint>
#include <string>

namespace vde::tools {

/**
 * @brief Enumerates the available drawing / interaction tools.
 */
enum class EditorTool { Brush, Eraser, ColorPicker, Fill, Line, Rect, Circle };

/**
 * @brief Mutable state of the currently selected tool.
 */
struct ToolState {
    EditorTool activeTool = EditorTool::Brush;  ///< Currently active tool.
    RGBAColor color = {0, 0, 0, 255};           ///< Current drawing color (default: black).
    int brushSize = 1;                           ///< Brush diameter in pixels.
    bool fillShape = true;                       ///< Whether shapes are filled or outline-only.
    bool drawingShape = false;                   ///< True while a shape drag is in progress.
    int shapeStartX = 0;                         ///< X coordinate where the shape drag started.
    int shapeStartY = 0;                         ///< Y coordinate where the shape drag started.
};

/**
 * @brief Translates mouse events on a canvas into command strings.
 *
 * The returned strings are meant to be fed directly into CommandSystem::execute().
 */
class ToolPalette {
public:
    /**
     * @brief Handle a mouse-down event on a canvas.
     * @param canvasId ID of the canvas that received the event.
     * @param pixelX Pixel X coordinate under the cursor.
     * @param pixelY Pixel Y coordinate under the cursor.
     * @return Command string, or empty if no immediate command.
     */
    std::string onCanvasMouseDown(uint32_t canvasId, int pixelX, int pixelY);

    /**
     * @brief Handle a mouse-drag event on a canvas.
     * @param canvasId ID of the canvas that received the event.
     * @param pixelX Current pixel X coordinate.
     * @param pixelY Current pixel Y coordinate.
     * @return Command string, or empty if no action needed.
     */
    std::string onCanvasMouseDrag(uint32_t canvasId, int pixelX, int pixelY);

    /**
     * @brief Handle a mouse-up event on a canvas.
     * @param canvasId ID of the canvas that received the event.
     * @param pixelX Release pixel X coordinate.
     * @param pixelY Release pixel Y coordinate.
     * @return Command string (e.g. shape draw), or empty.
     */
    std::string onCanvasMouseUp(uint32_t canvasId, int pixelX, int pixelY);

    /**
     * @brief Switch the active tool (cancels any in-progress shape).
     * @param tool The new tool to activate.
     */
    void setTool(EditorTool tool);

    /** @brief Read-only access to the current tool state. */
    const ToolState& getState() const { return m_state; }

    /** @brief Mutable access to the current tool state. */
    ToolState& getStateMutable() { return m_state; }

    /**
     * @brief Convert an RGBAColor to its "#RRGGBBAA" hex string.
     * @param color Input color.
     * @return Hex string with leading '#'.
     */
    static std::string colorToHex(RGBAColor color);

    /**
     * @brief Parse a hex color string ("#RRGGBB" or "#RRGGBBAA").
     * @param hex Input hex string.
     * @param outColor Receives the parsed color on success.
     * @return true if parsing succeeded.
     */
    static bool hexToColor(const std::string& hex, RGBAColor& outColor);

    /**
     * @brief Convert an EditorTool enum to its lowercase string name.
     * @param tool The tool enum value.
     * @return Lowercase name, e.g. "brush", "eraser".
     */
    static std::string toolToString(EditorTool tool);

    /**
     * @brief Parse a string into an EditorTool enum.
     * @param name Case-insensitive tool name.
     * @param outTool Receives the parsed tool on success.
     * @return true if the name was recognized.
     */
    static bool stringToTool(const std::string& name, EditorTool& outTool);

private:
    ToolState m_state;  ///< Current tool state.
};

}  // namespace vde::tools
