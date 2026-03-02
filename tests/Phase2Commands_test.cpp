/**
 * @file Phase2Commands_test.cpp
 * @brief Unit tests for Phase 2 Resource Editor commands: named colors (Step 6),
 *        cross-canvas operations (Step 8), and arc/bezier drawing (Step 9).
 *
 * Uses the same EditorContext wiring as CommandSystem_test.cpp. The test binary
 * links AllCommands.cpp, which fires every REGISTER_COMMAND static initializer
 * and populates the CommandRegistry singleton before any test runs.
 * Tests operate on in-memory ImageDocuments — no GPU, window, or file-dialog
 * code is exercised.
 */

#include <string>

#include "CanvasRegistry.h"
#include "CommandSystem.h"
#include "ImageDocument.h"
#include "ToolPalette.h"
#include "commands/EditorContext.h"
#include <gtest/gtest.h>

using namespace vde::tools;

// ============================================================================
// Test Fixture
// ============================================================================

/**
 * @brief Wires a CanvasRegistry + ToolPalette + CommandSystem together through
 *        an EditorContext, mirroring the initialization performed by
 *        ResourceEditorScene::onEnter() in production code.
 */
class Phase2CommandsTest : public ::testing::Test {
  protected:
    void SetUp() override {
        ctx.canvases = &registry;
        ctx.commands = &cmdSys;
        ctx.palette = &palette;
        cmdSys.initialize(ctx);

        // Create a default 8x8 canvas for tests that need an active canvas.
        testCanvas = registry.create("hero", ImageDocument::createNew(8, 8));
        ASSERT_NE(testCanvas, nullptr);
        cmdSys.setActiveCanvasId(testCanvas->id);
    }

    CanvasRegistry registry;
    ToolPalette palette;
    CommandSystem cmdSys;
    EditorContext ctx;
    Canvas* testCanvas = nullptr;
};

// ============================================================================
// Step 6 — Named Colors
// ============================================================================

TEST_F(Phase2CommandsTest, DefineColor_Success) {
    EXPECT_TRUE(cmdSys.execute("define color sky #87CEEBFF"));

    auto it = ctx.namedColors.find("sky");
    ASSERT_NE(it, ctx.namedColors.end());
    EXPECT_EQ(it->second.r, 0x87);
    EXPECT_EQ(it->second.g, 0xCE);
    EXPECT_EQ(it->second.b, 0xEB);
    EXPECT_EQ(it->second.a, 0xFF);
}

TEST_F(Phase2CommandsTest, DefineColor_UpdatesExisting) {
    EXPECT_TRUE(cmdSys.execute("define color sky #87CEEBFF"));
    EXPECT_TRUE(cmdSys.execute("define color sky #FF0000FF"));

    auto it = ctx.namedColors.find("sky");
    ASSERT_NE(it, ctx.namedColors.end());
    EXPECT_EQ(it->second.r, 255);
    EXPECT_EQ(it->second.g, 0);
    EXPECT_EQ(it->second.b, 0);
    EXPECT_EQ(it->second.a, 255);
}

TEST_F(Phase2CommandsTest, DefineColor_UsedInFill) {
    EXPECT_TRUE(cmdSys.execute("define color sky #87CEEBFF"));
    EXPECT_TRUE(cmdSys.execute("fill sky"));

    RGBAColor px = testCanvas->document->getPixel(0, 0);
    EXPECT_EQ(px.r, 0x87);
    EXPECT_EQ(px.g, 0xCE);
    EXPECT_EQ(px.b, 0xEB);
    EXPECT_EQ(px.a, 0xFF);

    // Corner pixel should also be filled.
    RGBAColor corner = testCanvas->document->getPixel(7, 7);
    EXPECT_EQ(corner.r, 0x87);
    EXPECT_EQ(corner.g, 0xCE);
}

TEST_F(Phase2CommandsTest, DefineColor_UsedInDrawLine) {
    EXPECT_TRUE(cmdSys.execute("define color sky #87CEEBFF"));
    EXPECT_TRUE(cmdSys.execute("draw line (0,0) to (7,7) with sky"));

    // At least the start pixel should be set to the named color.
    RGBAColor px = testCanvas->document->getPixel(0, 0);
    EXPECT_EQ(px.r, 0x87);
    EXPECT_EQ(px.g, 0xCE);
    EXPECT_EQ(px.b, 0xEB);
    EXPECT_EQ(px.a, 0xFF);
}

TEST_F(Phase2CommandsTest, ListColors_Empty) {
    EXPECT_TRUE(cmdSys.execute("list colors"));

    const auto& log = cmdSys.getLog();
    ASSERT_FALSE(log.empty());
    EXPECT_TRUE(log.back().success);
    EXPECT_NE(log.back().result.find("No named colors"), std::string::npos);
}

TEST_F(Phase2CommandsTest, ListColors_ShowsDefined) {
    EXPECT_TRUE(cmdSys.execute("define color sky #87CEEBFF"));
    EXPECT_TRUE(cmdSys.execute("define color fire #FF4500FF"));
    EXPECT_TRUE(cmdSys.execute("list colors"));

    const auto& log = cmdSys.getLog();
    ASSERT_FALSE(log.empty());
    EXPECT_TRUE(log.back().success);
    EXPECT_NE(log.back().result.find("sky"), std::string::npos);
    EXPECT_NE(log.back().result.find("fire"), std::string::npos);
}

TEST_F(Phase2CommandsTest, UndefineColor_Success) {
    EXPECT_TRUE(cmdSys.execute("define color sky #87CEEBFF"));
    ASSERT_FALSE(ctx.namedColors.empty());

    EXPECT_TRUE(cmdSys.execute("undefine color sky"));
    EXPECT_TRUE(ctx.namedColors.empty());
}

TEST_F(Phase2CommandsTest, UndefineColor_NotFound) {
    EXPECT_FALSE(cmdSys.execute("undefine color nonexistent"));

    const auto& log = cmdSys.getLog();
    ASSERT_FALSE(log.empty());
    EXPECT_FALSE(log.back().success);
}

TEST_F(Phase2CommandsTest, UndefineColor_NoLongerResolves) {
    EXPECT_TRUE(cmdSys.execute("define color sky #87CEEBFF"));
    EXPECT_TRUE(cmdSys.execute("fill sky"));

    // Verify fill worked.
    RGBAColor px = testCanvas->document->getPixel(0, 0);
    EXPECT_EQ(px.r, 0x87);

    // Undefine the color.
    EXPECT_TRUE(cmdSys.execute("undefine color sky"));

    // Clear the canvas first so we can tell if the next fill succeeds.
    EXPECT_TRUE(cmdSys.execute("clear"));

    // Attempting to use the undefined color should fail.
    EXPECT_FALSE(cmdSys.execute("fill sky"));
}

// ============================================================================
// Step 8 — Cross-Canvas Operations
// ============================================================================

TEST_F(Phase2CommandsTest, DrawImage_FromResource) {
    // Create a 4x4 red resource and add it to the active canvas.
    auto res = ImageDocument::createNew(4, 4);
    res->fill(RGBAColor{255, 0, 0, 255});
    testCanvas->resources["myres"] = std::move(res);

    EXPECT_TRUE(cmdSys.execute("draw image myres (0, 0)"));

    // Verify that red pixels were drawn onto the canvas.
    RGBAColor px = testCanvas->document->getPixel(0, 0);
    EXPECT_EQ(px.r, 255);
    EXPECT_EQ(px.g, 0);
    EXPECT_EQ(px.b, 0);
    EXPECT_EQ(px.a, 255);

    // Check a pixel within the 4x4 region.
    RGBAColor px2 = testCanvas->document->getPixel(3, 3);
    EXPECT_EQ(px2.r, 255);
    EXPECT_EQ(px2.a, 255);

    // A pixel outside the resource area should be unchanged (transparent black).
    RGBAColor px3 = testCanvas->document->getPixel(5, 5);
    EXPECT_EQ(px3.r, 0);
    EXPECT_EQ(px3.a, 0);
}

TEST_F(Phase2CommandsTest, Rehost_TransfersResource) {
    // Create two canvases.
    Canvas* canvasA = testCanvas;  // "hero"
    Canvas* canvasB = registry.create("body", ImageDocument::createNew(8, 8));
    ASSERT_NE(canvasB, nullptr);

    // Add a resource to canvas A.
    auto res = ImageDocument::createNew(4, 4);
    res->fill(RGBAColor{0, 255, 0, 255});
    canvasA->resources["sprite"] = std::move(res);
    ASSERT_EQ(canvasA->resources.count("sprite"), 1u);

    // Transfer it from hero to body.
    EXPECT_TRUE(cmdSys.execute("rehost image sprite from hero to body"));

    // Resource should no longer be in A, and should be in B.
    EXPECT_EQ(canvasA->resources.count("sprite"), 0u);
    EXPECT_EQ(canvasB->resources.count("sprite"), 1u);

    // Verify the resource data is intact.
    RGBAColor px = canvasB->resources["sprite"]->getPixel(0, 0);
    EXPECT_EQ(px.g, 255);
}

TEST_F(Phase2CommandsTest, Copyhost_CopiesResource) {
    Canvas* canvasA = testCanvas;  // "hero"
    Canvas* canvasB = registry.create("body", ImageDocument::createNew(8, 8));
    ASSERT_NE(canvasB, nullptr);

    // Add a resource to A.
    auto res = ImageDocument::createNew(4, 4);
    res->fill(RGBAColor{0, 0, 255, 255});
    canvasA->resources["icon"] = std::move(res);

    // Copy from hero to body with a new name.
    EXPECT_TRUE(cmdSys.execute("copyhost image icon from hero to body as icon_copy"));

    // Resource should exist in both canvases under their respective names.
    EXPECT_EQ(canvasA->resources.count("icon"), 1u);
    EXPECT_EQ(canvasB->resources.count("icon_copy"), 1u);

    // Both should have blue pixels.
    RGBAColor pxA = canvasA->resources["icon"]->getPixel(0, 0);
    EXPECT_EQ(pxA.b, 255);
    RGBAColor pxB = canvasB->resources["icon_copy"]->getPixel(0, 0);
    EXPECT_EQ(pxB.b, 255);
}

TEST_F(Phase2CommandsTest, Copyhost_WithoutAs) {
    Canvas* canvasA = testCanvas;
    Canvas* canvasB = registry.create("body", ImageDocument::createNew(8, 8));
    ASSERT_NE(canvasB, nullptr);

    auto res = ImageDocument::createNew(4, 4);
    res->fill(RGBAColor{128, 64, 32, 255});
    canvasA->resources["tile"] = std::move(res);

    // Copy without 'as' — should keep same name.
    EXPECT_TRUE(cmdSys.execute("copyhost image tile from hero to body"));

    EXPECT_EQ(canvasA->resources.count("tile"), 1u);
    EXPECT_EQ(canvasB->resources.count("tile"), 1u);

    RGBAColor px = canvasB->resources["tile"]->getPixel(0, 0);
    EXPECT_EQ(px.r, 128);
    EXPECT_EQ(px.g, 64);
    EXPECT_EQ(px.b, 32);
}

TEST_F(Phase2CommandsTest, CanvasRegistryTransferResource) {
    Canvas* canvasA = testCanvas;
    Canvas* canvasB = registry.create("body", ImageDocument::createNew(8, 8));
    ASSERT_NE(canvasB, nullptr);

    auto res = ImageDocument::createNew(2, 2);
    res->fill(RGBAColor{10, 20, 30, 255});
    canvasA->resources["blob"] = std::move(res);

    EXPECT_TRUE(registry.transferResource("blob", canvasA->id, canvasB->id));

    EXPECT_EQ(canvasA->resources.count("blob"), 0u);
    EXPECT_EQ(canvasB->resources.count("blob"), 1u);

    RGBAColor px = canvasB->resources["blob"]->getPixel(0, 0);
    EXPECT_EQ(px.r, 10);
    EXPECT_EQ(px.g, 20);
    EXPECT_EQ(px.b, 30);

    // Transferring a non-existent resource should fail.
    EXPECT_FALSE(registry.transferResource("nope", canvasA->id, canvasB->id));
}

TEST_F(Phase2CommandsTest, CanvasRegistryCopyResource) {
    Canvas* canvasA = testCanvas;
    Canvas* canvasB = registry.create("body", ImageDocument::createNew(8, 8));
    ASSERT_NE(canvasB, nullptr);

    auto res = ImageDocument::createNew(2, 2);
    res->fill(RGBAColor{100, 200, 50, 255});
    canvasA->resources["gem"] = std::move(res);

    // Copy with a new name.
    EXPECT_TRUE(registry.copyResource("gem", canvasA->id, canvasB->id, "gem_copy"));

    EXPECT_EQ(canvasA->resources.count("gem"), 1u);
    EXPECT_EQ(canvasB->resources.count("gem_copy"), 1u);

    RGBAColor pxA = canvasA->resources["gem"]->getPixel(1, 1);
    RGBAColor pxB = canvasB->resources["gem_copy"]->getPixel(1, 1);
    EXPECT_EQ(pxA.r, pxB.r);
    EXPECT_EQ(pxA.g, pxB.g);
    EXPECT_EQ(pxA.b, pxB.b);
    EXPECT_EQ(pxA.a, pxB.a);

    // Copy with empty name should use original name.
    EXPECT_TRUE(registry.copyResource("gem", canvasA->id, canvasB->id, ""));
    EXPECT_EQ(canvasB->resources.count("gem"), 1u);

    // Copying a non-existent resource should fail.
    EXPECT_FALSE(registry.copyResource("nope", canvasA->id, canvasB->id));
}

// ============================================================================
// Step 9 — Arc & Bézier Drawing
// ============================================================================

TEST_F(Phase2CommandsTest, DrawArc_Success) {
    EXPECT_TRUE(cmdSys.execute("draw arc (4, 4) radius 3 from 0 to 360 with #FF0000FF"));

    const auto& log = cmdSys.getLog();
    ASSERT_FALSE(log.empty());
    EXPECT_TRUE(log.back().success);

    // At least some pixels should have been drawn.
    bool anyPixelSet = false;
    for (uint32_t y = 0; y < testCanvas->document->getHeight(); ++y) {
        for (uint32_t x = 0; x < testCanvas->document->getWidth(); ++x) {
            auto px = testCanvas->document->getPixel(x, y);
            if (px.a > 0) {
                anyPixelSet = true;
                break;
            }
        }
        if (anyPixelSet) break;
    }
    EXPECT_TRUE(anyPixelSet);
}

TEST_F(Phase2CommandsTest, DrawArc_UndoWorks) {
    uint64_t genBefore = testCanvas->document->getGeneration();

    EXPECT_TRUE(cmdSys.execute("draw arc (4, 4) radius 3 from 0 to 360 with #FF0000FF"));

    uint64_t genAfterDraw = testCanvas->document->getGeneration();
    EXPECT_GT(genAfterDraw, genBefore);

    EXPECT_TRUE(cmdSys.execute("undo"));

    uint64_t genAfterUndo = testCanvas->document->getGeneration();
    EXPECT_GT(genAfterUndo, genAfterDraw);

    // All pixels should be back to transparent black after undo.
    bool anyPixelSet = false;
    for (uint32_t y = 0; y < testCanvas->document->getHeight(); ++y) {
        for (uint32_t x = 0; x < testCanvas->document->getWidth(); ++x) {
            auto px = testCanvas->document->getPixel(x, y);
            if (px.a > 0) {
                anyPixelSet = true;
                break;
            }
        }
        if (anyPixelSet) break;
    }
    EXPECT_FALSE(anyPixelSet);
}

TEST_F(Phase2CommandsTest, DrawBezier_Success) {
    EXPECT_TRUE(
        cmdSys.execute("draw bezier (0, 0) (2, 6) (5, 6) (7, 0) with #00FF00FF"));

    const auto& log = cmdSys.getLog();
    ASSERT_FALSE(log.empty());
    EXPECT_TRUE(log.back().success);

    // At least some pixels should have been drawn.
    bool anyPixelSet = false;
    for (uint32_t y = 0; y < testCanvas->document->getHeight(); ++y) {
        for (uint32_t x = 0; x < testCanvas->document->getWidth(); ++x) {
            auto px = testCanvas->document->getPixel(x, y);
            if (px.a > 0) {
                anyPixelSet = true;
                break;
            }
        }
        if (anyPixelSet) break;
    }
    EXPECT_TRUE(anyPixelSet);
}

TEST_F(Phase2CommandsTest, DrawBezier_UndoWorks) {
    // Fill with a known color first so we can verify restoration.
    EXPECT_TRUE(cmdSys.execute("fill #000000FF"));

    EXPECT_TRUE(
        cmdSys.execute("draw bezier (0, 0) (2, 6) (5, 6) (7, 0) with #00FF00FF"));

    // Confirm at least one green pixel was drawn.
    bool hasGreen = false;
    for (uint32_t y = 0; y < testCanvas->document->getHeight(); ++y) {
        for (uint32_t x = 0; x < testCanvas->document->getWidth(); ++x) {
            auto px = testCanvas->document->getPixel(x, y);
            if (px.g == 255 && px.r == 0 && px.b == 0) {
                hasGreen = true;
                break;
            }
        }
        if (hasGreen) break;
    }
    EXPECT_TRUE(hasGreen);

    // Undo should restore all pixels to black.
    EXPECT_TRUE(cmdSys.execute("undo"));

    bool anyGreenRemains = false;
    for (uint32_t y = 0; y < testCanvas->document->getHeight(); ++y) {
        for (uint32_t x = 0; x < testCanvas->document->getWidth(); ++x) {
            auto px = testCanvas->document->getPixel(x, y);
            if (px.g == 255 && px.r == 0 && px.b == 0) {
                anyGreenRemains = true;
                break;
            }
        }
        if (anyGreenRemains) break;
    }
    EXPECT_FALSE(anyGreenRemains);
}

TEST_F(Phase2CommandsTest, ImageDocument_DrawArc) {
    auto doc = ImageDocument::createNew(16, 16);
    doc->drawArc(8, 8, 5, 0.0f, 360.0f, RGBAColor{255, 0, 0, 255}, 1);

    // Verify some red pixels were drawn.
    bool anyPixelSet = false;
    for (uint32_t y = 0; y < doc->getHeight(); ++y) {
        for (uint32_t x = 0; x < doc->getWidth(); ++x) {
            auto px = doc->getPixel(x, y);
            if (px.r == 255 && px.a == 255) {
                anyPixelSet = true;
                break;
            }
        }
        if (anyPixelSet) break;
    }
    EXPECT_TRUE(anyPixelSet);

    // The center itself (8,8) should NOT be drawn for a radius-5 outline arc.
    // (It's just the arc stroke, not filled.)
    RGBAColor center = doc->getPixel(8, 8);
    EXPECT_EQ(center.a, 0) << "Center pixel should remain empty for an outline arc";
}

TEST_F(Phase2CommandsTest, ImageDocument_DrawBezier) {
    auto doc = ImageDocument::createNew(16, 16);
    std::vector<std::pair<int, int>> points = {{0, 0}, {5, 15}, {10, 15}, {15, 0}};
    doc->drawBezier(points, RGBAColor{0, 255, 0, 255}, 1);

    // Verify some green pixels were drawn.
    bool anyPixelSet = false;
    int greenCount = 0;
    for (uint32_t y = 0; y < doc->getHeight(); ++y) {
        for (uint32_t x = 0; x < doc->getWidth(); ++x) {
            auto px = doc->getPixel(x, y);
            if (px.g == 255 && px.a == 255) {
                anyPixelSet = true;
                ++greenCount;
            }
        }
    }
    EXPECT_TRUE(anyPixelSet);

    // A cubic bezier across a 16x16 canvas should produce multiple pixels.
    EXPECT_GT(greenCount, 3) << "Expected several pixels along the Bézier curve";

    // The start point (0,0) should be on the curve.
    RGBAColor startPx = doc->getPixel(0, 0);
    EXPECT_EQ(startPx.g, 255);
    EXPECT_EQ(startPx.a, 255);
}
