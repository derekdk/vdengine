/**
 * @file ToolPalette.cpp
 * @brief Implementation of ToolPalette mouse-to-command translation.
 */

#include "ToolPalette.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <sstream>

namespace vde {
namespace tools {

// =============================================================================
// Mouse event handlers
// =============================================================================

std::string ToolPalette::onCanvasMouseDown(uint32_t /*canvasId*/, int pixelX, int pixelY) {
    std::string hexColor = colorToHex(m_state.color);

    switch (m_state.activeTool) {
    case EditorTool::Brush:
        return "set " + std::to_string(pixelX) + " " + std::to_string(pixelY) + " " + hexColor;

    case EditorTool::Eraser:
        return "set " + std::to_string(pixelX) + " " + std::to_string(pixelY) + " #00000000";

    case EditorTool::ColorPicker:
        return "pick " + std::to_string(pixelX) + " " + std::to_string(pixelY);

    case EditorTool::Fill:
        return "floodfill " + std::to_string(pixelX) + " " + std::to_string(pixelY) + " with " +
               hexColor;

    case EditorTool::Line:
    case EditorTool::Rect:
    case EditorTool::Circle:
        // Start shape drawing
        m_state.drawingShape = true;
        m_state.shapeStartX = pixelX;
        m_state.shapeStartY = pixelY;
        return "";  // No command until mouse up

    default:
        return "";
    }
}

std::string ToolPalette::onCanvasMouseDrag(uint32_t /*canvasId*/, int pixelX, int pixelY) {
    switch (m_state.activeTool) {
    case EditorTool::Brush: {
        std::string hexColor = colorToHex(m_state.color);
        return "set " + std::to_string(pixelX) + " " + std::to_string(pixelY) + " " + hexColor;
    }

    case EditorTool::Eraser:
        return "set " + std::to_string(pixelX) + " " + std::to_string(pixelY) + " #00000000";

    default:
        // Shapes, color picker, fill — no drag action
        return "";
    }
}

std::string ToolPalette::onCanvasMouseUp(uint32_t /*canvasId*/, int pixelX, int pixelY) {
    if (!m_state.drawingShape) {
        return "";
    }

    m_state.drawingShape = false;
    std::string hexColor = colorToHex(m_state.color);

    switch (m_state.activeTool) {
    case EditorTool::Line:
        return "draw line " + std::to_string(m_state.shapeStartX) + " " +
               std::to_string(m_state.shapeStartY) + " to " + std::to_string(pixelX) + " " +
               std::to_string(pixelY) + " with " + hexColor;

    case EditorTool::Rect: {
        std::string fillStr = m_state.fillShape ? " filled" : " outline";
        return "draw rect " + std::to_string(m_state.shapeStartX) + " " +
               std::to_string(m_state.shapeStartY) + " to " + std::to_string(pixelX) + " " +
               std::to_string(pixelY) + " with " + hexColor + fillStr;
    }

    case EditorTool::Circle: {
        int dx = pixelX - m_state.shapeStartX;
        int dy = pixelY - m_state.shapeStartY;
        int r = static_cast<int>(std::sqrt(dx * dx + dy * dy));
        std::string fillStr = m_state.fillShape ? " filled" : " outline";
        return "draw circle " + std::to_string(m_state.shapeStartX) + " " +
               std::to_string(m_state.shapeStartY) + " radius " + std::to_string(r) + " with " +
               hexColor + fillStr;
    }

    default:
        return "";
    }
}

// =============================================================================
// State management
// =============================================================================

void ToolPalette::setTool(EditorTool tool) {
    // Cancel any in-progress shape when switching tools
    m_state.drawingShape = false;
    m_state.activeTool = tool;
}

// =============================================================================
// Color conversion helpers
// =============================================================================

std::string ToolPalette::colorToHex(RGBAColor color) {
    char buf[12];
    std::snprintf(buf, sizeof(buf), "#%02X%02X%02X%02X", color.r, color.g, color.b, color.a);
    return {buf};
}

bool ToolPalette::hexToColor(const std::string& hex, RGBAColor& outColor) {
    std::string h = hex;
    if (!h.empty() && h[0] == '#') {
        h = h.substr(1);
    }

    if (h.size() != 6 && h.size() != 8) {
        return false;
    }

    uint32_t value = 0;
    const char* begin = h.data();
    const char* end = begin + h.size();
    const auto [ptr, ec] = std::from_chars(begin, end, value, 16);
    if (ec != std::errc{} || ptr != end) {
        return false;
    }

    if (h.size() == 6) {
        // #RRGGBB — alpha defaults to 255
        outColor = {static_cast<uint8_t>(value >> 16), static_cast<uint8_t>(value >> 8),
                    static_cast<uint8_t>(value), 255};
    } else {
        // #RRGGBBAA
        outColor = {static_cast<uint8_t>(value >> 24), static_cast<uint8_t>(value >> 16),
                    static_cast<uint8_t>(value >> 8), static_cast<uint8_t>(value)};
    }
    return true;
}

std::string ToolPalette::toolToString(EditorTool tool) {
    switch (tool) {
    case EditorTool::Brush:
        return "brush";
    case EditorTool::Eraser:
        return "eraser";
    case EditorTool::ColorPicker:
        return "colorpicker";
    case EditorTool::Fill:
        return "fill";
    case EditorTool::Line:
        return "line";
    case EditorTool::Rect:
        return "rect";
    case EditorTool::Circle:
        return "circle";
    default:
        return "brush";
    }
}

bool ToolPalette::stringToTool(const std::string& name, EditorTool& outTool) {
    std::string lower = name;
    std::ranges::transform(lower, lower.begin(), ::tolower);

    if (lower == "brush") {
        outTool = EditorTool::Brush;
    } else if (lower == "eraser") {
        outTool = EditorTool::Eraser;
    } else if (lower == "colorpicker" || lower == "picker") {
        outTool = EditorTool::ColorPicker;
    } else if (lower == "fill") {
        outTool = EditorTool::Fill;
    } else if (lower == "line") {
        outTool = EditorTool::Line;
    } else if (lower == "rect" || lower == "rectangle") {
        outTool = EditorTool::Rect;
    } else if (lower == "circle") {
        outTool = EditorTool::Circle;
    } else {
        return false;
    }
    return true;
}

}  // namespace tools
}  // namespace vde
