/**
 * @file GameInputScriptExecution_test.cpp
 * @brief Unit tests for InputScriptExecutor using a mock ScriptEnvironment.
 */

#include <vde/api/InputHandler.h>
#include <vde/api/InputScriptExecutor.h>
#include <vde/api/KeyCodes.h>
#include <vde/api/SceneGroup.h>
#include <vde/api/ScriptEnvironment.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

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
    size_t getScenesCreated() const override { return scenesCreated; }
    size_t getScenesRemoved() const override { return scenesRemoved; }

    int exitCode = 0;
    bool quitCalled = false;
    size_t scenesCreated = 0;
    size_t scenesRemoved = 0;
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

// ============================================================================
// Global assert fields: scenes_created, scenes_removed
// ============================================================================

TEST(InputScriptExecutor, AssertScenesCreatedResolvesFromEnvironment) {
    MockScriptEnv env;
    env.scenesCreated = 5;

    InputScriptExecutor executor(env);

    auto state = std::make_unique<InputScriptState>();
    ScriptCommand cmd = makeCommand(InputCommandType::AssertSceneCount);
    cmd.assertField = "scenes_created";
    cmd.assertOp = CompareOp::Eq;
    cmd.assertValue = 5.0;
    state->commands = {cmd};
    executor.setState(std::move(state));

    executor.processFrame(0.016f);

    auto* s = executor.getState();
    EXPECT_TRUE(s->finished);
    EXPECT_FALSE(s->assertionFailed);
}

TEST(InputScriptExecutor, AssertScenesRemovedResolvesFromEnvironment) {
    MockScriptEnv env;
    env.scenesRemoved = 2;

    InputScriptExecutor executor(env);

    auto state = std::make_unique<InputScriptState>();
    ScriptCommand cmd = makeCommand(InputCommandType::AssertSceneCount);
    cmd.assertField = "scenes_removed";
    cmd.assertOp = CompareOp::Ge;
    cmd.assertValue = 1.0;
    state->commands = {cmd};
    executor.setState(std::move(state));

    executor.processFrame(0.016f);

    auto* s = executor.getState();
    EXPECT_TRUE(s->finished);
    EXPECT_FALSE(s->assertionFailed);
}

TEST(InputScriptExecutor, AssertScenesCreatedFailsOnMismatch) {
    MockScriptEnv env;
    env.scenesCreated = 1;

    InputScriptExecutor executor(env);

    auto state = std::make_unique<InputScriptState>();
    ScriptCommand cmd = makeCommand(InputCommandType::AssertSceneCount);
    cmd.assertField = "scenes_created";
    cmd.assertOp = CompareOp::Ge;
    cmd.assertValue = 5.0;
    state->commands = {cmd};
    executor.setState(std::move(state));

    executor.processFrame(0.016f);

    auto* s = executor.getState();
    EXPECT_TRUE(s->assertionFailed);
    EXPECT_EQ(env.exitCode, 1);
}

// ============================================================================
// Variable references ($VAR_NAME) in assert RHS
// ============================================================================

TEST(InputScriptExecutor, AssertGlobalWithVariableReference) {
    MockScriptEnv env;
    env.activeGroup.sceneNames = {"a", "b", "c"};

    InputScriptExecutor executor(env);

    auto state = std::make_unique<InputScriptState>();
    // set EXPECTED 3
    ScriptCommand setCmd = makeCommand(InputCommandType::Set);
    setCmd.setVarName = "EXPECTED";
    setCmd.setVarValue = 3.0;
    // assert rendered_scene_count == $EXPECTED
    ScriptCommand assertCmd = makeCommand(InputCommandType::AssertSceneCount);
    assertCmd.assertField = "rendered_scene_count";
    assertCmd.assertOp = CompareOp::Eq;
    assertCmd.assertVarRef = "EXPECTED";
    state->commands = {setCmd, assertCmd};
    executor.setState(std::move(state));

    executor.processFrame(0.016f);

    auto* s = executor.getState();
    EXPECT_TRUE(s->finished);
    EXPECT_FALSE(s->assertionFailed);
}

TEST(InputScriptExecutor, AssertWithUndefinedVariableReferenceFailsGracefully) {
    MockScriptEnv env;
    env.activeGroup.sceneNames = {"a"};

    InputScriptExecutor executor(env);

    auto state = std::make_unique<InputScriptState>();
    ScriptCommand assertCmd = makeCommand(InputCommandType::AssertSceneCount);
    assertCmd.assertField = "rendered_scene_count";
    assertCmd.assertOp = CompareOp::Eq;
    assertCmd.assertVarRef = "UNDEFINED_VAR";
    state->commands = {assertCmd};
    executor.setState(std::move(state));

    executor.processFrame(0.016f);

    auto* s = executor.getState();
    EXPECT_TRUE(s->assertionFailed);
    EXPECT_EQ(env.exitCode, 1);
}

// ============================================================================
// HoldKey execution
// ============================================================================

TEST(InputScriptExecutor, HoldKeyYieldsUntilDurationThenAdvances) {
    MockScriptEnv env;
    InputScriptExecutor executor(env);

    auto state = std::make_unique<InputScriptState>();
    ScriptCommand hold = makeCommand(InputCommandType::HoldKey);
    hold.keyCode = KEY_A;
    hold.waitMs = 50.0;
    state->commands = {hold};
    executor.setState(std::move(state));

    // Frame 1 (16 ms): keydown fires once, command yields.
    executor.processFrame(0.016f);
    auto* s = executor.getState();
    ASSERT_NE(s, nullptr);
    ASSERT_EQ(env.handler.keyPresses.size(), 1u);
    EXPECT_EQ(env.handler.keyPresses[0], KEY_A);
    EXPECT_TRUE(env.handler.keyReleases.empty());
    EXPECT_EQ(s->currentCommand, 0u);

    // Frame 2 (another 16 ms, total 32 ms): still holding, no second keydown.
    executor.processFrame(0.016f);
    EXPECT_EQ(env.handler.keyPresses.size(), 1u);
    EXPECT_TRUE(env.handler.keyReleases.empty());
    EXPECT_EQ(s->currentCommand, 0u);

    // Frame 3 (another 25 ms, total 57 ms > 50 ms): keyup fires, command advances, script finishes.
    executor.processFrame(0.025f);
    ASSERT_EQ(env.handler.keyReleases.size(), 1u);
    EXPECT_EQ(env.handler.keyReleases[0], KEY_A);
    EXPECT_TRUE(s->finished);
}

TEST(InputScriptExecutor, HoldKeyEmitsModifiersAroundMainKey) {
    MockScriptEnv env;
    InputScriptExecutor executor(env);

    auto state = std::make_unique<InputScriptState>();
    ScriptCommand hold = makeCommand(InputCommandType::HoldKey);
    hold.keyCode = KEY_A;
    hold.modifiers = INPUT_SCRIPT_MOD_CTRL;
    hold.waitMs = 30.0;
    state->commands = {hold};
    executor.setState(std::move(state));

    // Frame 1: ctrl down then A down.
    executor.processFrame(0.016f);
    ASSERT_EQ(env.handler.keyPresses.size(), 2u);
    EXPECT_EQ(env.handler.keyPresses[0], KEY_LEFT_CONTROL);
    EXPECT_EQ(env.handler.keyPresses[1], KEY_A);
    EXPECT_TRUE(env.handler.keyReleases.empty());

    // Frame 2: exceed duration -> A up then ctrl up.
    executor.processFrame(0.020f);
    ASSERT_EQ(env.handler.keyReleases.size(), 2u);
    EXPECT_EQ(env.handler.keyReleases[0], KEY_A);
    EXPECT_EQ(env.handler.keyReleases[1], KEY_LEFT_CONTROL);
    EXPECT_TRUE(executor.getState()->finished);
}

TEST(InputScriptExecutor, PressWithShiftEmitsUppercase) {
    MockScriptEnv env;
    InputScriptExecutor executor(env);

    auto state = std::make_unique<InputScriptState>();
    ScriptCommand cmd = makeCommand(InputCommandType::Press);
    cmd.keyCode = KEY_A;
    cmd.modifiers = INPUT_SCRIPT_MOD_SHIFT;
    state->commands = {cmd};
    executor.setState(std::move(state));

    executor.processFrame(0.016f);

    ASSERT_EQ(env.handler.charInputs.size(), 1u);
    EXPECT_EQ(env.handler.charInputs[0], static_cast<unsigned int>('A'));
}

TEST(InputScriptExecutor, PressWithCtrlSuppressesCharInput) {
    MockScriptEnv env;
    InputScriptExecutor executor(env);

    auto state = std::make_unique<InputScriptState>();
    ScriptCommand cmd = makeCommand(InputCommandType::Press);
    cmd.keyCode = KEY_S;
    cmd.modifiers = INPUT_SCRIPT_MOD_CTRL;
    state->commands = {cmd};
    executor.setState(std::move(state));

    executor.processFrame(0.016f);

    EXPECT_TRUE(env.handler.charInputs.empty());
}

TEST(InputScriptExecutor, OverlappingKeydownWithSharedModifierKeepsModifierActive) {
    MockScriptEnv env;
    InputScriptExecutor executor(env);

    auto state = std::make_unique<InputScriptState>();
    ScriptCommand kdA = makeCommand(InputCommandType::KeyDown);
    kdA.keyCode = KEY_A;
    kdA.modifiers = INPUT_SCRIPT_MOD_CTRL;
    ScriptCommand kdB = makeCommand(InputCommandType::KeyDown);
    kdB.keyCode = KEY_B;
    kdB.modifiers = INPUT_SCRIPT_MOD_CTRL;
    ScriptCommand kuA = makeCommand(InputCommandType::KeyUp);
    kuA.keyCode = KEY_A;
    kuA.modifiers = INPUT_SCRIPT_MOD_CTRL;

    state->commands = {kdA, kdB, kuA};
    executor.setState(std::move(state));

    executor.processFrame(0.016f);

    // keydown ctrl+A: ctrl pressed once (ref 0->1), A pressed
    // keydown ctrl+B: ctrl NOT re-pressed (ref 1->2), B pressed
    // keyup ctrl+A: A released, ctrl NOT yet released (ref 2->1)
    ASSERT_EQ(env.handler.keyPresses.size(), 3u);
    EXPECT_EQ(env.handler.keyPresses[0], KEY_LEFT_CONTROL);
    EXPECT_EQ(env.handler.keyPresses[1], KEY_A);
    EXPECT_EQ(env.handler.keyPresses[2], KEY_B);

    ASSERT_EQ(env.handler.keyReleases.size(), 1u);
    EXPECT_EQ(env.handler.keyReleases[0], KEY_A);  // ctrl not yet released
}

}  // namespace vde::test
