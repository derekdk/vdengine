/**
 * @file ImageDocument_test.cpp
 * @brief Unit tests for ImageDocument pixel data model.
 */

#include "ImageDocument.h"
#include <gtest/gtest.h>

using namespace vde::tools;

// ============================================================================
// Test Fixture
// ============================================================================

class ImageDocumentTest : public ::testing::Test {
  protected:
    void SetUp() override { doc.createNew(16, 16); }

    ImageDocument doc;
};

// ============================================================================
// Construction Tests
// ============================================================================

TEST_F(ImageDocumentTest, CreateNewSetsSize) {
    EXPECT_EQ(doc.getWidth(), 16u);
    EXPECT_EQ(doc.getHeight(), 16u);
    EXPECT_TRUE(doc.isValid());
}

TEST_F(ImageDocumentTest, CreateNewStartsTransparent) {
    RGBAColor pixel = doc.getPixel(0, 0);
    EXPECT_EQ(pixel.r, 0);
    EXPECT_EQ(pixel.g, 0);
    EXPECT_EQ(pixel.b, 0);
    EXPECT_EQ(pixel.a, 0);
}

TEST_F(ImageDocumentTest, CreateNewGenerationIsZero) {
    // createNew starts at generation 0 (incremented on first mutation)
    EXPECT_EQ(doc.getGeneration(), 0u);
}

TEST_F(ImageDocumentTest, CreateNewNotDirty) {
    EXPECT_FALSE(doc.isDirty());
}

TEST_F(ImageDocumentTest, DefaultConstructedIsInvalid) {
    ImageDocument empty;
    EXPECT_FALSE(empty.isValid());
    EXPECT_EQ(empty.getWidth(), 0u);
    EXPECT_EQ(empty.getHeight(), 0u);
}

// ============================================================================
// Pixel Access Tests
// ============================================================================

TEST_F(ImageDocumentTest, SetAndGetPixel) {
    RGBAColor red{255, 0, 0, 255};
    doc.setPixel(5, 5, red);

    RGBAColor got = doc.getPixel(5, 5);
    EXPECT_EQ(got, red);
}

TEST_F(ImageDocumentTest, SetPixelIncrementsGeneration) {
    uint64_t gen = doc.getGeneration();
    doc.setPixel(0, 0, {255, 0, 0, 255});
    EXPECT_GT(doc.getGeneration(), gen);
}

TEST_F(ImageDocumentTest, SetPixelMarksDirty) {
    doc.setPixel(0, 0, {255, 0, 0, 255});
    EXPECT_TRUE(doc.isDirty());
}

TEST_F(ImageDocumentTest, GetPixelOutOfBoundsReturnsTransparent) {
    RGBAColor pixel = doc.getPixel(-1, -1);
    EXPECT_EQ(pixel, (RGBAColor{0, 0, 0, 0}));

    pixel = doc.getPixel(100, 100);
    EXPECT_EQ(pixel, (RGBAColor{0, 0, 0, 0}));
}

TEST_F(ImageDocumentTest, SetPixelOutOfBoundsIsSafe) {
    // Should not crash
    doc.setPixel(-1, -1, {255, 0, 0, 255});
    doc.setPixel(100, 100, {255, 0, 0, 255});
    EXPECT_TRUE(true);
}

TEST_F(ImageDocumentTest, GetPixelDataNotNull) {
    EXPECT_NE(doc.getPixelData(), nullptr);
    EXPECT_NE(doc.getPixelDataMutable(), nullptr);
}

// ============================================================================
// Drawing Primitive Tests
// ============================================================================

TEST_F(ImageDocumentTest, FillSetsAllPixels) {
    RGBAColor blue{0, 0, 255, 255};
    doc.fill(blue);

    for (uint32_t y = 0; y < doc.getHeight(); ++y) {
        for (uint32_t x = 0; x < doc.getWidth(); ++x) {
            EXPECT_EQ(doc.getPixel(x, y), blue) << "at (" << x << ", " << y << ")";
        }
    }
}

TEST_F(ImageDocumentTest, DrawBrushSinglePixel) {
    RGBAColor green{0, 255, 0, 255};
    doc.drawBrush(8, 8, 0, green);

    EXPECT_EQ(doc.getPixel(8, 8), green);
    // Neighbor should still be transparent
    EXPECT_EQ(doc.getPixel(7, 8), (RGBAColor{0, 0, 0, 0}));
}

TEST_F(ImageDocumentTest, DrawBrushLargerRadius) {
    RGBAColor red{255, 0, 0, 255};
    doc.drawBrush(8, 8, 2, red);

    // Center should be colored
    EXPECT_EQ(doc.getPixel(8, 8), red);
    // Pixels within radius should be colored
    EXPECT_EQ(doc.getPixel(8, 7), red);
    EXPECT_EQ(doc.getPixel(8, 9), red);
    EXPECT_EQ(doc.getPixel(7, 8), red);
    EXPECT_EQ(doc.getPixel(9, 8), red);
}

TEST_F(ImageDocumentTest, DrawLineHorizontal) {
    RGBAColor color{255, 0, 0, 255};
    doc.drawLine(0, 5, 15, 5, color);

    for (int x = 0; x <= 15; ++x) {
        EXPECT_EQ(doc.getPixel(x, 5), color) << "at x=" << x;
    }
}

TEST_F(ImageDocumentTest, DrawLineVertical) {
    RGBAColor color{0, 255, 0, 255};
    doc.drawLine(5, 0, 5, 15, color);

    for (int y = 0; y <= 15; ++y) {
        EXPECT_EQ(doc.getPixel(5, y), color) << "at y=" << y;
    }
}

TEST_F(ImageDocumentTest, DrawRectFilled) {
    RGBAColor color{255, 255, 0, 255};
    doc.drawRect(2, 2, 4, 4, color, true);

    for (int y = 2; y < 6; ++y) {
        for (int x = 2; x < 6; ++x) {
            EXPECT_EQ(doc.getPixel(x, y), color) << "at (" << x << ", " << y << ")";
        }
    }
    // Outside should be transparent
    EXPECT_EQ(doc.getPixel(1, 2), (RGBAColor{0, 0, 0, 0}));
}

TEST_F(ImageDocumentTest, DrawRectOutline) {
    RGBAColor color{255, 0, 255, 255};
    doc.drawRect(2, 2, 5, 5, color, false);

    // Top edge
    EXPECT_EQ(doc.getPixel(2, 2), color);
    EXPECT_EQ(doc.getPixel(6, 2), color);
    // Bottom edge
    EXPECT_EQ(doc.getPixel(2, 6), color);
    EXPECT_EQ(doc.getPixel(6, 6), color);
    // Interior should be transparent
    EXPECT_EQ(doc.getPixel(4, 4), (RGBAColor{0, 0, 0, 0}));
}

TEST_F(ImageDocumentTest, DrawCircleFilled) {
    RGBAColor color{0, 255, 255, 255};
    doc.drawCircle(8, 8, 3, color, true);

    // Center should be colored
    EXPECT_EQ(doc.getPixel(8, 8), color);
    // Points at radius should be colored
    EXPECT_EQ(doc.getPixel(8, 5), color);
}

TEST_F(ImageDocumentTest, FloodFillSimple) {
    // Create a small filled area, then flood-fill it with a new color
    RGBAColor red{255, 0, 0, 255};
    RGBAColor blue{0, 0, 255, 255};

    doc.fill(red);
    doc.floodFill(0, 0, blue);

    for (uint32_t y = 0; y < doc.getHeight(); ++y) {
        for (uint32_t x = 0; x < doc.getWidth(); ++x) {
            EXPECT_EQ(doc.getPixel(x, y), blue);
        }
    }
}

TEST_F(ImageDocumentTest, FloodFillDoesNotCrossBoundary) {
    RGBAColor red{255, 0, 0, 255};
    RGBAColor blue{0, 0, 255, 255};
    RGBAColor wall{128, 128, 128, 255};

    // Draw a horizontal wall dividing the canvas
    doc.drawLine(0, 8, 15, 8, wall);

    // Fill top half
    doc.floodFill(0, 0, blue);

    // Top should be blue
    EXPECT_EQ(doc.getPixel(0, 0), blue);
    // Bottom should still be transparent
    EXPECT_EQ(doc.getPixel(0, 9), (RGBAColor{0, 0, 0, 0}));
}

TEST_F(ImageDocumentTest, FloodFillSameColorNoOp) {
    RGBAColor transparent{0, 0, 0, 0};
    uint64_t genBefore = doc.getGeneration();
    doc.floodFill(0, 0, transparent);  // Already transparent
    // Should not change generation (no-op)
    EXPECT_EQ(doc.getGeneration(), genBefore);
}

// ============================================================================
// Image Transform Tests
// ============================================================================

TEST_F(ImageDocumentTest, FlipHorizontal) {
    RGBAColor red{255, 0, 0, 255};
    doc.setPixel(0, 0, red);

    doc.flipHorizontal();

    EXPECT_EQ(doc.getPixel(15, 0), red);
    EXPECT_EQ(doc.getPixel(0, 0), (RGBAColor{0, 0, 0, 0}));
}

TEST_F(ImageDocumentTest, FlipVertical) {
    RGBAColor red{255, 0, 0, 255};
    doc.setPixel(0, 0, red);

    doc.flipVertical();

    EXPECT_EQ(doc.getPixel(0, 15), red);
    EXPECT_EQ(doc.getPixel(0, 0), (RGBAColor{0, 0, 0, 0}));
}

TEST_F(ImageDocumentTest, ResizeScalesCorrectly) {
    RGBAColor red{255, 0, 0, 255};
    doc.fill(red);

    doc.resize(8, 8);

    EXPECT_EQ(doc.getWidth(), 8u);
    EXPECT_EQ(doc.getHeight(), 8u);
    // Nearest-neighbor: all pixels should still be red
    EXPECT_EQ(doc.getPixel(0, 0), red);
    EXPECT_EQ(doc.getPixel(7, 7), red);
}

TEST_F(ImageDocumentTest, CropSubRectangle) {
    // Fill entire canvas, then set top-left corner distinct
    RGBAColor bg{100, 100, 100, 255};
    RGBAColor marker{255, 0, 0, 255};

    doc.fill(bg);
    doc.setPixel(2, 2, marker);

    doc.crop(2, 2, 4, 4);

    EXPECT_EQ(doc.getWidth(), 4u);
    EXPECT_EQ(doc.getHeight(), 4u);
    EXPECT_EQ(doc.getPixel(0, 0), marker);  // Was at (2,2), now at (0,0)
    EXPECT_EQ(doc.getPixel(1, 1), bg);
}

// ============================================================================
// Undo/Redo Tests
// ============================================================================

TEST_F(ImageDocumentTest, UndoRestoresPreviousState) {
    RGBAColor red{255, 0, 0, 255};

    doc.snapshotForUndo();
    doc.fill(red);

    EXPECT_TRUE(doc.undo());
    // Should revert to transparent
    EXPECT_EQ(doc.getPixel(0, 0), (RGBAColor{0, 0, 0, 0}));
}

TEST_F(ImageDocumentTest, RedoRestoresUndoneState) {
    RGBAColor red{255, 0, 0, 255};

    doc.snapshotForUndo();
    doc.fill(red);
    doc.undo();

    EXPECT_TRUE(doc.redo());
    EXPECT_EQ(doc.getPixel(0, 0), red);
}

TEST_F(ImageDocumentTest, UndoOnEmptyStackReturnsFalse) {
    EXPECT_FALSE(doc.undo());
}

TEST_F(ImageDocumentTest, RedoOnEmptyStackReturnsFalse) {
    EXPECT_FALSE(doc.redo());
}

TEST_F(ImageDocumentTest, UndoCountTracksCorrectly) {
    EXPECT_EQ(doc.getUndoCount(), 0u);

    doc.snapshotForUndo();
    doc.fill({255, 0, 0, 255});
    EXPECT_EQ(doc.getUndoCount(), 1u);

    doc.snapshotForUndo();
    doc.fill({0, 255, 0, 255});
    EXPECT_EQ(doc.getUndoCount(), 2u);

    doc.undo();
    EXPECT_EQ(doc.getUndoCount(), 1u);
    EXPECT_EQ(doc.getRedoCount(), 1u);
}

TEST_F(ImageDocumentTest, NewActionClearsRedoStack) {
    doc.snapshotForUndo();
    doc.fill({255, 0, 0, 255});
    doc.undo();
    EXPECT_EQ(doc.getRedoCount(), 1u);

    // New action should clear redo
    doc.snapshotForUndo();
    doc.fill({0, 255, 0, 255});
    EXPECT_EQ(doc.getRedoCount(), 0u);
}

// ============================================================================
// Persistence Tests (file path only, no actual I/O)
// ============================================================================

TEST_F(ImageDocumentTest, FilePathManagement) {
    EXPECT_TRUE(doc.getFilePath().empty());

    doc.setFilePath("test.png");
    EXPECT_EQ(doc.getFilePath(), "test.png");
}

TEST_F(ImageDocumentTest, DirtyFlagManagement) {
    EXPECT_FALSE(doc.isDirty());

    doc.markDirty();
    EXPECT_TRUE(doc.isDirty());

    doc.clearDirty();
    EXPECT_FALSE(doc.isDirty());
}

// ============================================================================
// RGBAColor Tests
// ============================================================================

TEST(RGBAColorTest, DefaultIsTransparentBlack) {
    RGBAColor c;
    EXPECT_EQ(c.r, 0);
    EXPECT_EQ(c.g, 0);
    EXPECT_EQ(c.b, 0);
    EXPECT_EQ(c.a, 255);
}

TEST(RGBAColorTest, EqualityOperator) {
    RGBAColor a{255, 0, 0, 255};
    RGBAColor b{255, 0, 0, 255};
    RGBAColor c{0, 255, 0, 255};

    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
}
