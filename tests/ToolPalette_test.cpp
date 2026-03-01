/**
 * @file ToolPalette_test.cpp
 * @brief Unit tests for ToolPalette mouse-to-command translation.
 */

#include "ToolPalette.h"
#include <gtest/gtest.h>

using namespace vde::tools;

// ============================================================================
// Test Fixture
// ============================================================================

class ToolPaletteTest : public ::testing::Test {
  protected:
    void SetUp() override {
        palette.setColor({255, 0, 0, 255});
        palette.setBrushSize(1);
        palette.setTool(EditorTool::Brush);
    }

    ToolPalette palette;
};

// ============================================================================
// Color Conversion Tests
// ============================================================================

TEST(ToolPaletteStaticTest, ColorToHexBasic) {
    std::string hex = ToolPalette::colorToHex({255, 0, 0, 255});
    EXPECT_EQ(hex, "#FF0000FF");
}

TEST(ToolPaletteStaticTest, ColorToHexBlack) {
    std::string hex = ToolPalette::colorToHex({0, 0, 0, 255});
    EXPECT_EQ(hex, "#000000FF");
}

TEST(ToolPaletteStaticTest, ColorToHexTransparent) {
    std::string hex = ToolPalette::colorToHex({0, 0, 0, 0});
    EXPECT_EQ(hex, "#00000000");
}

TEST(ToolPaletteStaticTest, HexToColorBasic) {
    RGBAColor color;
    EXPECT_TRUE(ToolPalette::hexToColor("#FF0000FF", color));
    EXPECT_EQ(color.r, 255);
    EXPECT_EQ(color.g, 0);
    EXPECT_EQ(color.b, 0);
    EXPECT_EQ(color.a, 255);
}

TEST(ToolPaletteStaticTest, HexToColorWithoutAlpha) {
    RGBAColor color;
    EXPECT_TRUE(ToolPalette::hexToColor("#00FF00", color));
    EXPECT_EQ(color.r, 0);
    EXPECT_EQ(color.g, 255);
    EXPECT_EQ(color.b, 0);
    EXPECT_EQ(color.a, 255);  // Default alpha when omitted
}

TEST(ToolPaletteStaticTest, HexToColorInvalidString) {
    RGBAColor color;
    EXPECT_FALSE(ToolPalette::hexToColor("not_a_color", color));
    EXPECT_FALSE(ToolPalette::hexToColor("", color));
    EXPECT_FALSE(ToolPalette::hexToColor("#GG0000", color));
}

TEST(ToolPaletteStaticTest, ColorRoundTrip) {
    RGBAColor original{128, 64, 32, 200};
    std::string hex = ToolPalette::colorToHex(original);

    RGBAColor parsed;
    EXPECT_TRUE(ToolPalette::hexToColor(hex, parsed));
    EXPECT_EQ(parsed, original);
}

// ============================================================================
// Tool String Conversion Tests
// ============================================================================

TEST(ToolPaletteStaticTest, ToolToString) {
    EXPECT_EQ(ToolPalette::toolToString(EditorTool::Brush), "brush");
    EXPECT_EQ(ToolPalette::toolToString(EditorTool::Eraser), "eraser");
    EXPECT_EQ(ToolPalette::toolToString(EditorTool::ColorPicker), "colorpicker");
    EXPECT_EQ(ToolPalette::toolToString(EditorTool::Fill), "fill");
    EXPECT_EQ(ToolPalette::toolToString(EditorTool::Line), "line");
    EXPECT_EQ(ToolPalette::toolToString(EditorTool::Rect), "rect");
    EXPECT_EQ(ToolPalette::toolToString(EditorTool::Circle), "circle");
}

TEST(ToolPaletteStaticTest, StringToTool) {
    EditorTool tool;

    EXPECT_TRUE(ToolPalette::stringToTool("brush", tool));
    EXPECT_EQ(tool, EditorTool::Brush);

    EXPECT_TRUE(ToolPalette::stringToTool("eraser", tool));
    EXPECT_EQ(tool, EditorTool::Eraser);

    EXPECT_TRUE(ToolPalette::stringToTool("line", tool));
    EXPECT_EQ(tool, EditorTool::Line);
}

TEST(ToolPaletteStaticTest, StringToToolInvalid) {
    EditorTool tool;
    EXPECT_FALSE(ToolPalette::stringToTool("notreal", tool));
}

TEST(ToolPaletteStaticTest, ToolStringRoundTrip) {
    EditorTool tool;
    for (auto t : {EditorTool::Brush, EditorTool::Eraser, EditorTool::ColorPicker, EditorTool::Fill,
                   EditorTool::Line, EditorTool::Rect, EditorTool::Circle}) {
        std::string name = ToolPalette::toolToString(t);
        EXPECT_TRUE(ToolPalette::stringToTool(name, tool));
        EXPECT_EQ(tool, t) << "Round-trip failed for: " << name;
    }
}

// ============================================================================
// Brush Tool Mouse Event Tests
// ============================================================================

TEST_F(ToolPaletteTest, BrushMouseDownReturnsCommand) {
    std::string cmd = palette.onCanvasMouseDown(1, 5, 10);
    EXPECT_FALSE(cmd.empty());
    // Should contain "paint" with coordinates and color
    EXPECT_NE(cmd.find("paint"), std::string::npos);
}

TEST_F(ToolPaletteTest, BrushMouseDragReturnsCommand) {
    palette.onCanvasMouseDown(1, 5, 10);
    std::string cmd = palette.onCanvasMouseDrag(1, 6, 11);
    EXPECT_FALSE(cmd.empty());
    EXPECT_NE(cmd.find("paint"), std::string::npos);
}

// ============================================================================
// Eraser Tool Tests
// ============================================================================

TEST_F(ToolPaletteTest, EraserUsesTransparentColor) {
    palette.setTool(EditorTool::Eraser);
    std::string cmd = palette.onCanvasMouseDown(1, 5, 10);
    EXPECT_FALSE(cmd.empty());
    // Eraser should paint with transparent (00000000)
    EXPECT_NE(cmd.find("paint"), std::string::npos);
    EXPECT_NE(cmd.find("#00000000"), std::string::npos);
}

// ============================================================================
// ColorPicker Tool Tests
// ============================================================================

TEST_F(ToolPaletteTest, ColorPickerReturnsPickCommand) {
    palette.setTool(EditorTool::ColorPicker);
    std::string cmd = palette.onCanvasMouseDown(1, 3, 7);
    EXPECT_FALSE(cmd.empty());
    EXPECT_NE(cmd.find("pick"), std::string::npos);
}

// ============================================================================
// Fill Tool Tests
// ============================================================================

TEST_F(ToolPaletteTest, FillReturnsFillCommand) {
    palette.setTool(EditorTool::Fill);
    std::string cmd = palette.onCanvasMouseDown(1, 3, 7);
    EXPECT_FALSE(cmd.empty());
    EXPECT_NE(cmd.find("fill"), std::string::npos);
}

// ============================================================================
// Shape Tool Tests (Line, Rect, Circle)
// ============================================================================

TEST_F(ToolPaletteTest, LineToolMouseDownStartsShape) {
    palette.setTool(EditorTool::Line);
    std::string cmd = palette.onCanvasMouseDown(1, 2, 3);
    // Mouse down for shapes should NOT produce a command yet
    EXPECT_TRUE(cmd.empty());
    EXPECT_TRUE(palette.isDrawingShape());
}

TEST_F(ToolPaletteTest, LineToolMouseUpProducesCommand) {
    palette.setTool(EditorTool::Line);
    palette.onCanvasMouseDown(1, 2, 3);
    std::string cmd = palette.onCanvasMouseUp(1, 10, 12);

    EXPECT_FALSE(cmd.empty());
    EXPECT_NE(cmd.find("line"), std::string::npos);
    EXPECT_FALSE(palette.isDrawingShape());
}

TEST_F(ToolPaletteTest, RectToolMouseUpProducesCommand) {
    palette.setTool(EditorTool::Rect);
    palette.onCanvasMouseDown(1, 0, 0);
    std::string cmd = palette.onCanvasMouseUp(1, 5, 5);

    EXPECT_FALSE(cmd.empty());
    EXPECT_NE(cmd.find("rect"), std::string::npos);
}

TEST_F(ToolPaletteTest, CircleToolMouseUpProducesCommand) {
    palette.setTool(EditorTool::Circle);
    palette.onCanvasMouseDown(1, 8, 8);
    std::string cmd = palette.onCanvasMouseUp(1, 12, 8);

    EXPECT_FALSE(cmd.empty());
    EXPECT_NE(cmd.find("circle"), std::string::npos);
}

// ============================================================================
// State Management Tests
// ============================================================================

TEST_F(ToolPaletteTest, SetToolChangesState) {
    palette.setTool(EditorTool::Circle);
    EXPECT_EQ(palette.getState().activeTool, EditorTool::Circle);
}

TEST_F(ToolPaletteTest, SetColorChangesState) {
    RGBAColor blue{0, 0, 255, 128};
    palette.setColor(blue);
    EXPECT_EQ(palette.getState().color, blue);
}

TEST_F(ToolPaletteTest, SetBrushSizeChangesState) {
    palette.setBrushSize(5);
    EXPECT_EQ(palette.getState().brushSize, 5);
}

TEST_F(ToolPaletteTest, SetFillShapeChangesState) {
    palette.setFillShape(false);
    EXPECT_FALSE(palette.getState().fillShape);
    palette.setFillShape(true);
    EXPECT_TRUE(palette.getState().fillShape);
}

TEST_F(ToolPaletteTest, MutableStateAccess) {
    auto& state = palette.getStateMutable();
    state.brushSize = 42;
    EXPECT_EQ(palette.getState().brushSize, 42);
}
