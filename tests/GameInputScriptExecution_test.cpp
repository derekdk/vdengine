/**
 * @file GameInputScriptExecution_test.cpp
 * @brief Unit tests for InputScriptExecutor using a mock ScriptEnvironment.
 */

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <vde/api/InputHandler.h>
#include <vde/api/InputScriptExecutor.h>
#include <vde/api/ScriptEnvironment.h>

#include <vde/api/KeyCodes.h>
#include <vde/api/SceneGroup.h>

namespace vde::test {

class RecordingInputHandler : public InputHandler {
  public:
    void onKeyPress(int key) override { keyPresses.push_back(key); }
    void onKeyRelease(int key) override { keyReleases.push_back(key); }
    void onCharInput(unsigned int codepoint) override { charInputs.push_back(codepoint); }
    void onMouseButtonRelease(int /*button*/, double /*x*/, double /*y*/) override {
        mouseReleases++;
    }

    std::vector<int> keyPresses;
    std::vector<int> keyReleases;
    std::vector<unsigned int> charInputs;
    int mouseReleases = 0;
};

class MockScriptEnv : public ScriptEnvironment {
  public:
    RecordingInputHandler handler;
    SceneGroup activeGroup;

    InputHandler* resolveInputHandler() override { return &handler; }
    bool captureScreenshot(const std::string&) override { return true; }
    Scene* getScene(const std::string&) override { return nullptr; }
    const SceneGroup& getActiveSceneGroup() const override { return activeGroup; }
    std::pair<uint32_t, uint32_t> getSwapChainExtent() const override { return {1280, 720}; }
    void setExitCode(int code) override { exitCode = code; }
    void quit() override { quitCalled = true; }

    int exitCode = 0;
    bool quitCalled = false;
};

ScriptCommand makeCommand(InputCommandType type) {
    ScriptCommand cmd{};
    cmd.type = type;
    cmd.lineNumber = 1;
    return cmd;
}

ScriptCommand makePressCommand(int keyCode) {
    ScriptCommand cmd = makeCommand(InputCommandType::Press);
    cmd.keyCode = keyCode;
    return cmd;
}

TEST(InputScriptExecutor, ResolveInputHandlerPrefersFocusedSceneHandler) {
    // This test verified Game::resolveInputHandler(); with the new design
    // that method is on Game (ScriptEnvironment override), which is
    // integration-level.  Here we verify the mock wiring works.
    MockScriptEnv env;
    EXPECT_EQ(env.resolveInputHandler(), &env.handler);
}

TEST(InputScriptExecutor, WaitFramesBlocksUntilCounterExpires) {
    MockScriptEnv env;
    InputScriptExecutor executor(env);

    auto state = std::make_unique<InputScriptState>();
    ScriptCommand waitFrames = makeCommand(InputCommandType::WaitFrames);
    waitFrames.waitFrames = 2;
    state->commands = {waitFrames, makePressCommand(KEY_A)};
    executor.setState(std::move(state));

    executor.processFrame(0.016f);

    auto* s = executor.getState();
    ASSERT_NE(s, nullptr);
    EXPECT_FALSE(s->finished);
    EXPECT_EQ(s->currentCommand, 0u);
    EXPECT_EQ(s->frameWaitCounter, 1);
    EXPECT_TRUE(env.handler.keyPresses.empty());

    executor.processFrame(0.016f);

    EXPECT_TRUE(s->finished);
    EXPECT_EQ(s->currentCommand, 2u);
    EXPECT_EQ(s->frameWaitCounter, 0);
    ASSERT_EQ(env.handler.keyPresses.size(), 1u);
    ASSERT_EQ(env.handler.keyReleases.size(), 1u);
    ASSERT_EQ(env.handler.charInputs.size(), 1u);
    EXPECT_EQ(env.handler.keyPresses[0], KEY_A);
    EXPECT_EQ(env.handler.keyReleases[0], KEY_A);
    EXPECT_EQ(env.handler.charInputs[0], static_cast<unsigned int>('a'));
}

TEST(InputScriptExecutor, LoopResetsIterationsForReentry) {
    MockScriptEnv env;
    InputScriptExecutor executor(env);

    // Build a minimal script: label at 0, noop at 1, loop at 2
    auto state = std::make_unique<InputScriptState>();
    ScriptCommand label = makeCommand(InputCommandType::Label);
    label.argument = "repeat";
    ScriptCommand noop = makeCommand(InputCommandType::Print);
    noop.argument = "tick";
    ScriptCommand loop = makeCommand(InputCommandType::Loop);
    loop.argument = "repeat";
    loop.loopCount = 2;

    state->commands = {label, noop, loop};
    state->labels["repeat"].commandIndex = 0;
    executor.setState(std::move(state));

    // First frame: processes label(0), print(1), loop(2) -> jumps back
    // then label(0), print(1), loop(2) -> exhausted -> advance past loop
    // then finished.
    executor.processFrame(0.016f);

    auto* s = executor.getState();
    EXPECT_TRUE(s->finished);
    EXPECT_EQ(s->labels["repeat"].remainingIterations, -1);

    // Re-enter: reset and run again to verify re-entry works
    s->finished = false;
    s->currentCommand = 0;
    executor.processFrame(0.016f);
    EXPECT_TRUE(s->finished);
}

TEST(InputScriptExecutor, AssertSceneCountFailureSetsExitCode) {
    MockScriptEnv env;
    env.activeGroup.sceneNames = {"one", "two"};

    InputScriptExecutor executor(env);

    auto state = std::make_unique<InputScriptState>();
    ScriptCommand assertCount = makeCommand(InputCommandType::AssertSceneCount);
    assertCount.assertOp = CompareOp::Eq;
    assertCount.assertValue = 3.0;
    state->commands = {assertCount};
    executor.setState(std::move(state));

    executor.processFrame(0.016f);

    auto* s = executor.getState();
    ASSERT_NE(s, nullptr);
    EXPECT_TRUE(s->finished);
    EXPECT_TRUE(s->assertionFailed);
    EXPECT_EQ(env.exitCode, 1);
}

}  // namespace vde::test