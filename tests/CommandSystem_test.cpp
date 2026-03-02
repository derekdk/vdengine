/**
 * @file CommandSystem_test.cpp
 * @brief Unit tests for CommandSystem: dispatch, log recording, and real command execution.
 *
 * The test binary links AllCommands.cpp, which fires every REGISTER_COMMAND static
 * initializer and populates the CommandRegistry singleton before any test runs.
 * Tests operate on in-memory ImageDocuments â€” no GPU, window, or file-dialog code
 * is exercised.
 */

#include <fstream>
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
 * @brief Wires a CanvasRegistry + ToolPalette + CommandSystem together through an
 *        EditorContext, mirroring the initialization performed by
 *        ResourceEditorScene::onEnter() in production code.
 */
class CommandSystemTest : public ::testing::Test {
  protected:
    void SetUp() override {
        ctx.canvases = &registry;
        ctx.commands = &cmdSys;
        ctx.palette = &palette;
        cmdSys.initialize(ctx);

        // Create a default 8Ã—8 canvas for tests that need an active canvas.
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
// Log infrastructure
// ============================================================================

TEST_F(CommandSystemTest, EmptyAndWhitespaceAreSilentNoOps) {
    EXPECT_TRUE(cmdSys.execute(""));
    EXPECT_TRUE(cmdSys.execute("   \t  "));
    EXPECT_TRUE(cmdSys.getLog().empty());
}

TEST_F(CommandSystemTest, HashCommentLinesAreSkipped) {
    EXPECT_TRUE(cmdSys.execute("# this is a comment"));
    EXPECT_TRUE(cmdSys.getLog().empty());
}

TEST_F(CommandSystemTest, SlashCommentLinesAreSkipped) {
    EXPECT_TRUE(cmdSys.execute("// also a comment"));
    EXPECT_TRUE(cmdSys.getLog().empty());
}

TEST_F(CommandSystemTest, UnknownCommandLogsFailure) {
    EXPECT_FALSE(cmdSys.execute("totally_unknown_command"));

    const auto& log = cmdSys.getLog();
    ASSERT_EQ(log.size(), 1u);
    EXPECT_FALSE(log[0].success);
    EXPECT_EQ(log[0].commandLine, "totally_unknown_command");
    EXPECT_FALSE(log[0].result.empty());
}

TEST_F(CommandSystemTest, ClearResetsLogAndActiveCanvas) {
    cmdSys.execute("fill #FF0000FF");
    EXPECT_FALSE(cmdSys.getLog().empty());

    cmdSys.clear();
    EXPECT_TRUE(cmdSys.getLog().empty());
    EXPECT_EQ(cmdSys.getActiveCanvasId(), 0u);
}

TEST_F(CommandSystemTest, AddLogEntryAppendsEntry) {
    cmdSys.addLogEntry("manual", "done", true, "hero");

    const auto& log = cmdSys.getLog();
    ASSERT_EQ(log.size(), 1u);
    EXPECT_EQ(log[0].commandLine, "manual");
    EXPECT_EQ(log[0].result, "done");
    EXPECT_TRUE(log[0].success);
    EXPECT_EQ(log[0].canvasName, "hero");
}

TEST_F(CommandSystemTest, AddLogEntryFailureFlag) {
    cmdSys.addLogEntry("bad_cmd", "error msg", false);

    const auto& log = cmdSys.getLog();
    ASSERT_EQ(log.size(), 1u);
    EXPECT_FALSE(log[0].success);
    EXPECT_EQ(log[0].result, "error msg");
}

TEST_F(CommandSystemTest, LogRawInputMarksEntry) {
    cmdSys.logRawInput("raw typed text");

    const auto& log = cmdSys.getLog();
    ASSERT_EQ(log.size(), 1u);
    EXPECT_TRUE(log[0].isRawInput);
    EXPECT_EQ(log[0].commandLine, "raw typed text");
}

TEST_F(CommandSystemTest, ActiveCanvasIdRoundTrips) {
    auto doc = ImageDocument::createNew(4, 4);
    Canvas* c2 = registry.create("second", std::move(doc));
    cmdSys.setActiveCanvasId(c2->id);
    EXPECT_EQ(cmdSys.getActiveCanvasId(), c2->id);
}

TEST_F(CommandSystemTest, MultipleCommandsAccumulateInLog) {
    cmdSys.execute("fill #FF0000FF");
    cmdSys.execute("set (0, 0) #00FF00FF");
    cmdSys.execute("clear");
    EXPECT_EQ(cmdSys.getLog().size(), 3u);
}

// ============================================================================
// Real command dispatch â€” canvas drawing operations
// ============================================================================

TEST_F(CommandSystemTest, FillSucceedsAndLogsResult) {
    EXPECT_TRUE(cmdSys.execute("fill #FF0000FF"));

    const auto& log = cmdSys.getLog();
    ASSERT_EQ(log.size(), 1u);
    EXPECT_TRUE(log[0].success);
    EXPECT_EQ(log[0].commandLine, "fill #FF0000FF");
    // Result message should mention the color that was used.
    EXPECT_NE(log[0].result.find("#FF0000FF"), std::string::npos);
}

TEST_F(CommandSystemTest, FillChangesAllPixels) {
    EXPECT_TRUE(cmdSys.execute("fill #FF0000FF"));

    RGBAColor px = testCanvas->document->getPixel(0, 0);
    EXPECT_EQ(px.r, 255);
    EXPECT_EQ(px.g, 0);
    EXPECT_EQ(px.b, 0);
    EXPECT_EQ(px.a, 255);

    // Corner pixels should also be filled.
    RGBAColor corner = testCanvas->document->getPixel(7, 7);
    EXPECT_EQ(corner.r, 255);
}

TEST_F(CommandSystemTest, FillLogEntryHasCanvasName) {
    cmdSys.execute("fill #FF0000FF");

    ASSERT_FALSE(cmdSys.getLog().empty());
    EXPECT_EQ(cmdSys.getLog().back().canvasName, "hero");
}

TEST_F(CommandSystemTest, SetPixelChangesOnePixel) {
    cmdSys.execute("fill #000000FF");        // solid black background
    cmdSys.execute("set (3, 4) #00FF00FF");  // write a single green pixel

    RGBAColor px = testCanvas->document->getPixel(3, 4);
    EXPECT_EQ(px.r, 0);
    EXPECT_EQ(px.g, 255);
    EXPECT_EQ(px.b, 0);
    EXPECT_EQ(px.a, 255);

    // Neighboring pixel is unmodified.
    RGBAColor neighbor = testCanvas->document->getPixel(4, 4);
    EXPECT_EQ(neighbor.g, 0);
}

TEST_F(CommandSystemTest, SetPixelLogsResult) {
    EXPECT_TRUE(cmdSys.execute("set (1, 2) #ABCDEFFF"));

    ASSERT_FALSE(cmdSys.getLog().empty());
    EXPECT_TRUE(cmdSys.getLog().back().success);
    EXPECT_NE(cmdSys.getLog().back().result.find("(1,2)"), std::string::npos);
}

TEST_F(CommandSystemTest, ClearCommandFillsTransparent) {
    cmdSys.execute("fill #FF0000FF");  // red
    EXPECT_TRUE(cmdSys.execute("clear"));

    RGBAColor px = testCanvas->document->getPixel(0, 0);
    EXPECT_EQ(px.r, 0);
    EXPECT_EQ(px.g, 0);
    EXPECT_EQ(px.b, 0);
    EXPECT_EQ(px.a, 0);
}

TEST_F(CommandSystemTest, UndoAfterFillRestoresOriginalPixels) {
    // The canvas starts as transparent black. Fill red, then undo.
    cmdSys.execute("fill #FF0000FF");
    EXPECT_TRUE(cmdSys.execute("undo"));

    RGBAColor px = testCanvas->document->getPixel(0, 0);
    EXPECT_EQ(px.r, 0);
    EXPECT_EQ(px.g, 0);
    EXPECT_EQ(px.b, 0);
}

TEST_F(CommandSystemTest, UndoRedoRoundTrip) {
    cmdSys.execute("fill #FF0000FF");
    cmdSys.execute("undo");
    EXPECT_TRUE(cmdSys.execute("redo"));

    RGBAColor px = testCanvas->document->getPixel(0, 0);
    EXPECT_EQ(px.r, 255);
    EXPECT_EQ(px.g, 0);
    EXPECT_EQ(px.b, 0);
}

// ============================================================================
// Real command dispatch â€” global / canvas management
// ============================================================================

TEST_F(CommandSystemTest, CreateCanvasAddsToRegistry) {
    EXPECT_TRUE(cmdSys.execute("create canvas mysprite 16 16"));

    Canvas* c = registry.getByName("mysprite");
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->document->getWidth(), 16u);
    EXPECT_EQ(c->document->getHeight(), 16u);
}

TEST_F(CommandSystemTest, CreateCanvasSetsItAsActive) {
    cmdSys.execute("create canvas newone 4 4");

    Canvas* c = registry.getByName("newone");
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(cmdSys.getActiveCanvasId(), c->id);
}

TEST_F(CommandSystemTest, CreateCanvasInvalidDimensionsFails) {
    EXPECT_FALSE(cmdSys.execute("create canvas bad 0 0"));
    EXPECT_EQ(registry.getByName("bad"), nullptr);
}

TEST_F(CommandSystemTest, SelectCommandSwitchesActiveCanvas) {
    cmdSys.execute("create canvas other 4 4");  // now active
    EXPECT_TRUE(cmdSys.execute("select hero"));
    EXPECT_EQ(cmdSys.getActiveCanvasId(), testCanvas->id);
}

TEST_F(CommandSystemTest, SelectNonexistentCanvasFails) {
    EXPECT_FALSE(cmdSys.execute("select nonexistent"));

    ASSERT_FALSE(cmdSys.getLog().empty());
    EXPECT_FALSE(cmdSys.getLog().back().success);
}

TEST_F(CommandSystemTest, SelectLogsActiveCanvasName) {
    cmdSys.execute("create canvas other 4 4");
    cmdSys.execute("select hero");

    ASSERT_FALSE(cmdSys.getLog().empty());
    EXPECT_TRUE(cmdSys.getLog().back().success);
    EXPECT_NE(cmdSys.getLog().back().result.find("hero"), std::string::npos);
}

TEST_F(CommandSystemTest, CanvasPrefixByNameTargetsSpecificCanvas) {
    cmdSys.execute("create canvas second 4 4");
    Canvas* second = registry.getByName("second");
    ASSERT_NE(second, nullptr);

    // Set active back to hero, then target second via @prefix.
    cmdSys.setActiveCanvasId(testCanvas->id);
    cmdSys.execute("@second fill #FF0000FF");

    // second should be red; hero should be unchanged (transparent).
    RGBAColor secondPx = second->document->getPixel(0, 0);
    EXPECT_EQ(secondPx.r, 255);

    RGBAColor heroPx = testCanvas->document->getPixel(0, 0);
    EXPECT_EQ(heroPx.r, 0);
}

TEST_F(CommandSystemTest, CanvasPrefixByIdTargetsCorrectCanvas) {
    cmdSys.execute("create canvas second 4 4");
    Canvas* second = registry.getByName("second");
    ASSERT_NE(second, nullptr);

    cmdSys.setActiveCanvasId(testCanvas->id);
    std::string cmd = "@" + std::to_string(second->id) + " fill #0000FFFF";
    EXPECT_TRUE(cmdSys.execute(cmd));

    RGBAColor px = second->document->getPixel(0, 0);
    EXPECT_EQ(px.b, 255);

    // hero pixel untouched.
    EXPECT_EQ(testCanvas->document->getPixel(0, 0).b, 0);
}

TEST_F(CommandSystemTest, CanvasPrefixNonexistentFallsBackToActiveCanvas) {
    // When @prefix names a canvas that doesn't exist, execution silently
    // falls back to the active canvas (rather than returning an error).
    EXPECT_TRUE(cmdSys.execute("@no_canvas fill #FF0000FF"));
    // The active canvas ("hero") should have been filled.
    RGBAColor px = testCanvas->document->getPixel(0, 0);
    EXPECT_EQ(px.r, 255);
}

// ============================================================================
// Log persistence
// ============================================================================

TEST_F(CommandSystemTest, SaveFullLogWritesAllEntries) {
    cmdSys.execute("fill #FF0000FF");
    cmdSys.execute("set (1, 1) #0000FFFF");

    const std::string path = "test_cmd_log_full.txt";
    EXPECT_TRUE(cmdSys.saveFullLog(path));

    std::ifstream in(path);
    ASSERT_TRUE(in.is_open());
    std::string content((std::istreambuf_iterator<char>(in)), {});
    EXPECT_NE(content.find("fill #FF0000FF"), std::string::npos);
    EXPECT_NE(content.find("set (1, 1) #0000FFFF"), std::string::npos);

    std::remove(path.c_str());
}

TEST_F(CommandSystemTest, SaveLogRangeWritesOnlyRequestedEntries) {
    cmdSys.execute("fill #FF0000FF");        // log[0]
    cmdSys.execute("set (0, 0) #00FF00FF");  // log[1]
    cmdSys.execute("clear");                 // log[2]

    const std::string path = "test_cmd_log_range.txt";
    // Write only the first entry (range [0, 1)).
    EXPECT_TRUE(cmdSys.saveLogRange(0, 1, path));

    std::ifstream in(path);
    ASSERT_TRUE(in.is_open());
    std::string content((std::istreambuf_iterator<char>(in)), {});
    EXPECT_NE(content.find("fill #FF0000FF"), std::string::npos);
    // "clear" belongs to log[2], outside the saved range.
    EXPECT_EQ(content.find("clear"), std::string::npos);

    std::remove(path.c_str());
}

TEST_F(CommandSystemTest, SaveLogRangeInvalidStartReturnsFalse) {
    // Log is empty, so start index 5 is out of range.
    EXPECT_FALSE(cmdSys.saveLogRange(5, 10, "should_not_exist.txt"));
}
