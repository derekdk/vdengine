/**
 * @file ToolPalette.h
 * @brief Mouse-to-command translation layer for the Resource Editor.
 *
 * ToolPalette tracks the current drawing tool, color, and brush size,
 * and translates mouse events on canvas into command strings for the
 * CommandSystem to execute.
 */

#pragma once

#include <cstdint>
#include <string>

#include "ImageDocument.h"

namespace vde {
namespace tools {

/**
 * @brief Available drawing tools.
 */
enum class EditorTool {
    Brush,
    Eraser,
    ColorPicker,
    Fill,
    Line,
    Rect,
    Circle,
};

/**
 * @brief Current state of the tool palette.
 */
struct ToolState {
    EditorTool activeTool = EditorTool::Brush;
    RGBAColor color = {0, 0, 0, 255};  ///< Current drawing color
    int brushSize = 1;                 ///< Brush radius (0 = single pixel)
    bool drawingShape = false;         ///< True while drawing a shape (line/rect/circle)
    int shapeStartX = 0;
    int shapeStartY = 0;
    bool fillShape = true;  ///< Fill rect/circle shapes
};

/**
 * @brief Mouse-to-command translation for canvas editing.
 *
 * Converts mouse events (down, drag, up) into command strings based
 * on the current tool, color, and brush size settings.
 */
class ToolPalette {
  public:
    ToolPalette() = default;
    ~ToolPalette() = default;

    // --- Mouse event handlers (return command strings, no @prefix) ---

    /**
     * @brief Handle mouse button press on canvas.
     * @param canvasId Canvas that was clicked
     * @param pixelX Pixel X coordinate on canvas
     * @param pixelY Pixel Y coordinate on canvas
     * @return Command string to execute (empty if no action)
     */
    std::string onCanvasMouseDown(uint32_t canvasId, int pixelX, int pixelY);

    /**
     * @brief Handle mouse drag on canvas.
     * @param canvasId Canvas being dragged on
     * @param pixelX Current pixel X coordinate
     * @param pixelY Current pixel Y coordinate
     * @return Command string to execute (empty if no action)
     */
    std::string onCanvasMouseDrag(uint32_t canvasId, int pixelX, int pixelY);

    /**
     * @brief Handle mouse button release on canvas.
     * @param canvasId Canvas where release happened
     * @param pixelX Pixel X coordinate
     * @param pixelY Pixel Y coordinate
     * @return Command string to execute (empty if no action)
     */
    std::string onCanvasMouseUp(uint32_t canvasId, int pixelX, int pixelY);

    // --- State accessors ---

    const ToolState& getState() const { return m_state; }
    ToolState& getStateMutable() { return m_state; }

    void setTool(EditorTool tool);
    void setColor(RGBAColor color) { m_state.color = color; }
    void setBrushSize(int size) { m_state.brushSize = size; }
    void setFillShape(bool fill) { m_state.fillShape = fill; }

    bool isDrawingShape() const { return m_state.drawingShape; }

    // --- Color conversion helpers ---

    /**
     * @brief Convert RGBA color to hex string (#RRGGBBAA).
     */
    static std::string colorToHex(RGBAColor color);

    /**
     * @brief Parse hex string (#RRGGBBAA or #RRGGBB) to RGBA color.
     * @return true if parsed successfully
     */
    static bool hexToColor(const std::string& hex, RGBAColor& outColor);

    /**
     * @brief Get tool name string from EditorTool enum.
     */
    static std::string toolToString(EditorTool tool);

    /**
     * @brief Parse tool name string to EditorTool enum.
     * @return true if parsed successfully
     */
    static bool stringToTool(const std::string& name, EditorTool& outTool);

  private:
    ToolState m_state;
};

}  // namespace tools
}  // namespace vde
