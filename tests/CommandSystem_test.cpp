/**
 * @file CommandSystem_test.cpp
 * @brief Unit tests for CommandSystem command dispatch and logging.
 */

#include "CommandSystem.h"
#include <gtest/gtest.h>

using namespace vde::tools;

// ============================================================================
// Test Fixture
// ============================================================================

class CommandSystemTest : public ::testing::Test {
  protected:
    void SetUp() override {
        cmdSys.setRegistry(&registry);

        // Create a canvas for testing canvas-targeted commands
        auto doc = std::make_unique<ImageDocument>();
        doc->createNew(8, 8);
        testCanvas = registry.create("hero", std::move(doc));
        ASSERT_NE(testCanvas, nullptr);

        cmdSys.setActiveCanvasId(testCanvas->id);
    }

    CanvasRegistry registry;
    CommandSystem cmdSys;
    Canvas* testCanvas = nullptr;
};

// ============================================================================
// Registration Tests
// ============================================================================

TEST_F(CommandSystemTest, RegisterGlobalCommand) {
    bool called = false;
    cmdSys.registerGlobalCommand("test_global", "Test global command",
                                 [&](const std::string&) { called = true; });

    EXPECT_TRUE(cmdSys.execute("test_global"));
    EXPECT_TRUE(called);
}

TEST_F(CommandSystemTest, RegisterCanvasCommand) {
    uint32_t receivedId = 0;
    std::string receivedArgs;

    cmdSys.registerCanvasCommand("test_canvas", "Test canvas command",
                                 [&](uint32_t id, const std::string& args) {
                                     receivedId = id;
                                     receivedArgs = args;
                                 });

    EXPECT_TRUE(cmdSys.execute("test_canvas some_arg"));
    EXPECT_EQ(receivedId, testCanvas->id);
    EXPECT_EQ(receivedArgs, "some_arg");
}

// ============================================================================
// Command Resolution Tests
// ============================================================================

TEST_F(CommandSystemTest, GlobalCommandTakesPriorityOverCanvas) {
    bool globalCalled = false;
    bool canvasCalled = false;

    cmdSys.registerGlobalCommand("shared", "Test",
                                 [&](const std::string&) { globalCalled = true; });
    cmdSys.registerCanvasCommand("shared", "Test",
                                 [&](uint32_t, const std::string&) { canvasCalled = true; });

    cmdSys.execute("shared");
    EXPECT_TRUE(globalCalled);
    EXPECT_FALSE(canvasCalled);
}

TEST_F(CommandSystemTest, ExplicitCanvasPrefixById) {
    uint32_t receivedId = 0;
    cmdSys.registerCanvasCommand("set", "Test",
                                 [&](uint32_t id, const std::string&) { receivedId = id; });

    std::string cmd = "@" + std::to_string(testCanvas->id) + " set 0 0";
    EXPECT_TRUE(cmdSys.execute(cmd));
    EXPECT_EQ(receivedId, testCanvas->id);
}

TEST_F(CommandSystemTest, ExplicitCanvasPrefixByName) {
    uint32_t receivedId = 0;
    cmdSys.registerCanvasCommand("set", "Test",
                                 [&](uint32_t id, const std::string&) { receivedId = id; });

    EXPECT_TRUE(cmdSys.execute("@hero set 0 0"));
    EXPECT_EQ(receivedId, testCanvas->id);
}

TEST_F(CommandSystemTest, InvalidCanvasPrefixFails) {
    cmdSys.registerCanvasCommand("set", "Test", [](uint32_t, const std::string&) {});

    EXPECT_FALSE(cmdSys.execute("@nonexistent set 0 0"));
}

TEST_F(CommandSystemTest, CanvasCommandWithoutActiveCanvasFails) {
    cmdSys.setActiveCanvasId(0);
    cmdSys.registerCanvasCommand("set", "Test", [](uint32_t, const std::string&) {});

    EXPECT_FALSE(cmdSys.execute("set 0 0"));
}

TEST_F(CommandSystemTest, UnknownCommandFails) {
    EXPECT_FALSE(cmdSys.execute("unknown_command"));
}

TEST_F(CommandSystemTest, EmptyCommandIsNoOp) {
    // Empty/whitespace commands are silently accepted (no-op)
    EXPECT_TRUE(cmdSys.execute(""));
    EXPECT_TRUE(cmdSys.execute("   "));
}

// ============================================================================
// Command Log Tests
// ============================================================================

TEST_F(CommandSystemTest, ExecuteLogsCommand) {
    cmdSys.registerGlobalCommand("logged", "Test",
                                 [&](const std::string&) { cmdSys.setLastResult("ok", true); });

    cmdSys.execute("logged");

    const auto& log = cmdSys.getLog();
    ASSERT_EQ(log.size(), 1u);
    EXPECT_EQ(log[0].commandLine, "logged");
    EXPECT_TRUE(log[0].success);
    EXPECT_EQ(log[0].result, "ok");
}

TEST_F(CommandSystemTest, LogTracksMultipleCommands) {
    cmdSys.registerGlobalCommand("a", "A", [](const std::string&) {});
    cmdSys.registerGlobalCommand("b", "B", [](const std::string&) {});

    cmdSys.execute("a");
    cmdSys.execute("b");

    EXPECT_EQ(cmdSys.getLog().size(), 2u);
}

TEST_F(CommandSystemTest, ClearLogRemovesAllEntries) {
    cmdSys.registerGlobalCommand("a", "A", [](const std::string&) {});
    cmdSys.execute("a");
    EXPECT_EQ(cmdSys.getLog().size(), 1u);

    cmdSys.clearLog();
    EXPECT_TRUE(cmdSys.getLog().empty());
}

TEST_F(CommandSystemTest, FailedCommandLoggedAsFailure) {
    cmdSys.execute("nonexistent");

    const auto& log = cmdSys.getLog();
    ASSERT_EQ(log.size(), 1u);
    EXPECT_FALSE(log[0].success);
}

// ============================================================================
// Help Text Tests
// ============================================================================

TEST_F(CommandSystemTest, HelpTextRegistered) {
    cmdSys.registerGlobalCommand("help_test", "This is the help text", [](const std::string&) {});

    EXPECT_EQ(cmdSys.getHelpText("help_test"), "This is the help text");
}

TEST_F(CommandSystemTest, HelpTextEmptyForUnknown) {
    EXPECT_TRUE(cmdSys.getHelpText("nonexistent").empty());
}

TEST_F(CommandSystemTest, GetCommandNamesReturnsAll) {
    cmdSys.registerGlobalCommand("alpha", "A", [](const std::string&) {});
    cmdSys.registerCanvasCommand("beta", "B", [](uint32_t, const std::string&) {});

    auto names = cmdSys.getCommandNames();
    EXPECT_GE(names.size(), 2u);

    bool hasAlpha = false, hasBeta = false;
    for (const auto& name : names) {
        if (name == "alpha")
            hasAlpha = true;
        if (name == "beta")
            hasBeta = true;
    }
    EXPECT_TRUE(hasAlpha);
    EXPECT_TRUE(hasBeta);
}

// ============================================================================
// SetLastResult Tests
// ============================================================================

TEST_F(CommandSystemTest, SetLastResultUpdatesLogEntry) {
    cmdSys.registerGlobalCommand("test_result", "Test", [&](const std::string&) {
        cmdSys.setLastResult("custom result message", true);
    });

    cmdSys.execute("test_result");

    const auto& log = cmdSys.getLog();
    ASSERT_GE(log.size(), 1u);
    EXPECT_EQ(log.back().result, "custom result message");
    EXPECT_TRUE(log.back().success);
}

TEST_F(CommandSystemTest, SetLastResultErrorUpdatesLogEntry) {
    cmdSys.registerGlobalCommand("test_error", "Test", [&](const std::string&) {
        cmdSys.setLastResult("something went wrong", false);
    });

    cmdSys.execute("test_error");

    const auto& log = cmdSys.getLog();
    ASSERT_GE(log.size(), 1u);
    EXPECT_EQ(log.back().result, "something went wrong");
    EXPECT_FALSE(log.back().success);
}

// ============================================================================
// Comment/Blank Line Tests
// ============================================================================

TEST_F(CommandSystemTest, CommentLinesAreIgnored) {
    // Comments should not be executed and should not appear in the log
    EXPECT_TRUE(cmdSys.execute("# this is a comment"));
    // The log should either be empty or the comment should be marked as success
}

TEST_F(CommandSystemTest, ActiveCanvasIdPersists) {
    auto doc = std::make_unique<ImageDocument>();
    doc->createNew(4, 4);
    Canvas* c2 = registry.create("second", std::move(doc));

    cmdSys.setActiveCanvasId(c2->id);
    EXPECT_EQ(cmdSys.getActiveCanvasId(), c2->id);
}
