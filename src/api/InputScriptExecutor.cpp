/**
 * @file InputScriptExecutor.cpp
 * @brief Implementation of InputScriptExecutor — owns script state and dispatch
 */

#include <vde/api/InputHandler.h>
#include <vde/api/InputScriptExecutor.h>
#include <vde/api/KeyCodes.h>
#include <vde/api/Scene.h>
#include <vde/api/SceneGroup.h>
#include <vde/api/ScriptEnvironment.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

#include "FLIP.h"
#include "stb_image.h"

namespace vde {

namespace {

void emitScriptCharInput(InputHandler& handler, int keyCode, int modifiers) {
    // Control and Alt chords don't produce text input in real systems.
    if (modifiers & (INPUT_SCRIPT_MOD_CTRL | INPUT_SCRIPT_MOD_ALT)) {
        return;
    }
    if (keyCode >= KEY_A && keyCode <= KEY_Z) {
        // KEY_A..KEY_Z are ASCII 65..90 ('A'..'Z').
        // Shift produces uppercase; bare key produces lowercase (+32).
        unsigned int codepoint = (modifiers & INPUT_SCRIPT_MOD_SHIFT)
                                     ? static_cast<unsigned int>(keyCode)
                                     : static_cast<unsigned int>(keyCode + 32);
        handler.onCharInput(codepoint);
    } else if (keyCode >= KEY_SPACE && keyCode <= 126) {
        handler.onCharInput(static_cast<unsigned int>(keyCode));
    }
}

void emitModifierKeysDown(InputHandler& handler, int modifiers, InputScriptState& state) {
    if (modifiers & INPUT_SCRIPT_MOD_CTRL) {
        if (state.modifierRefCounts[KEY_LEFT_CONTROL]++ == 0) {
            handler.onKeyPress(KEY_LEFT_CONTROL);
        }
    }
    if (modifiers & INPUT_SCRIPT_MOD_SHIFT) {
        if (state.modifierRefCounts[KEY_LEFT_SHIFT]++ == 0) {
            handler.onKeyPress(KEY_LEFT_SHIFT);
        }
    }
    if (modifiers & INPUT_SCRIPT_MOD_ALT) {
        if (state.modifierRefCounts[KEY_LEFT_ALT]++ == 0) {
            handler.onKeyPress(KEY_LEFT_ALT);
        }
    }
}

void emitModifierKeysUp(InputHandler& handler, int modifiers, InputScriptState& state) {
    if (modifiers & INPUT_SCRIPT_MOD_ALT) {
        int& count = state.modifierRefCounts[KEY_LEFT_ALT];
        if (count > 0 && --count == 0) {
            handler.onKeyRelease(KEY_LEFT_ALT);
        }
    }
    if (modifiers & INPUT_SCRIPT_MOD_SHIFT) {
        int& count = state.modifierRefCounts[KEY_LEFT_SHIFT];
        if (count > 0 && --count == 0) {
            handler.onKeyRelease(KEY_LEFT_SHIFT);
        }
    }
    if (modifiers & INPUT_SCRIPT_MOD_CTRL) {
        int& count = state.modifierRefCounts[KEY_LEFT_CONTROL];
        if (count > 0 && --count == 0) {
            handler.onKeyRelease(KEY_LEFT_CONTROL);
        }
    }
}

std::string makeScreenshotFramePath(const std::string& basePath, uint64_t /*frameNumber*/) {
    // Return the path as-is so screenshot/compare can use matching paths.
    // The frame number was historically appended but no scripts depend on it.
    if (basePath.find_last_of('.') == std::string::npos) {
        return basePath + ".png";
    }
    return basePath;
}

bool isSceneInActiveGroup(const SceneGroup& group, const std::string& sceneName) {
    return std::any_of(
        group.sceneNames.begin(), group.sceneNames.end(),
        [&](const std::string& activeSceneName) { return activeSceneName == sceneName; });
}

bool tryResolveAssertSceneFieldValue(std::pair<uint32_t, uint32_t> swapExtent, Scene* targetScene,
                                     bool inActiveGroup, const ScriptCommand& cmd,
                                     double& fieldValue) {
    if (cmd.assertField == "was_rendered" || cmd.assertField == "not_blank") {
        fieldValue = (targetScene && inActiveGroup) ? 1.0 : 0.0;
        return true;
    }

    if (cmd.assertField == "draw_calls") {
        if (targetScene && inActiveGroup) {
            fieldValue = targetScene->getEntities().empty() ? 0.0 : 1.0;
        }
        return true;
    }

    if (cmd.assertField == "entities_drawn" || cmd.assertField == "entity_count") {
        if (targetScene) {
            fieldValue = static_cast<double>(targetScene->getDiagnostics().totalEntityCount);
        }
        return true;
    }

    if (cmd.assertField == "viewport_width" || cmd.assertField == "viewport_height") {
        if (!targetScene) {
            fieldValue = 0.0;
            return true;
        }

        const auto& viewport = targetScene->getViewportRect();
        fieldValue = cmd.assertField == "viewport_width"
                         ? static_cast<double>(viewport.width * swapExtent.first)
                         : static_cast<double>(viewport.height * swapExtent.second);
        return true;
    }

    // SceneDiagnostics entity type counts
    if (cmd.assertField == "mesh_entity_count") {
        if (targetScene)
            fieldValue = static_cast<double>(targetScene->getDiagnostics().meshEntityCount);
        return true;
    }
    if (cmd.assertField == "sprite_entity_count") {
        if (targetScene)
            fieldValue = static_cast<double>(targetScene->getDiagnostics().spriteEntityCount);
        return true;
    }
    if (cmd.assertField == "text_entity_count") {
        if (targetScene)
            fieldValue = static_cast<double>(targetScene->getDiagnostics().textEntityCount);
        return true;
    }
    if (cmd.assertField == "physics_entity_count") {
        if (targetScene)
            fieldValue = static_cast<double>(targetScene->getDiagnostics().physicsEntityCount);
        return true;
    }

    // SceneDiagnostics entity lifecycle counters
    if (cmd.assertField == "entities_created") {
        if (targetScene)
            fieldValue = static_cast<double>(targetScene->getDiagnostics().entitiesCreated);
        return true;
    }
    if (cmd.assertField == "entities_removed") {
        if (targetScene)
            fieldValue = static_cast<double>(targetScene->getDiagnostics().entitiesRemoved);
        return true;
    }

    // SceneDiagnostics lifecycle counters
    if (cmd.assertField == "enter_count") {
        if (targetScene)
            fieldValue = static_cast<double>(targetScene->getDiagnostics().enterCount);
        return true;
    }
    if (cmd.assertField == "exit_count") {
        if (targetScene)
            fieldValue = static_cast<double>(targetScene->getDiagnostics().exitCount);
        return true;
    }
    if (cmd.assertField == "pause_count") {
        if (targetScene)
            fieldValue = static_cast<double>(targetScene->getDiagnostics().pauseCount);
        return true;
    }
    if (cmd.assertField == "resume_count") {
        if (targetScene)
            fieldValue = static_cast<double>(targetScene->getDiagnostics().resumeCount);
        return true;
    }
    if (cmd.assertField == "is_focused") {
        if (targetScene)
            fieldValue = targetScene->getDiagnostics().isFocused ? 1.0 : 0.0;
        return true;
    }

    return false;
}

double computeFlipMeanError(const unsigned char* imageA, const unsigned char* imageB, int width,
                            int height) {
    const size_t pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height);
    std::vector<float> refLinear(pixelCount * 3);
    std::vector<float> testLinear(pixelCount * 3);

    for (size_t i = 0; i < pixelCount; ++i) {
        for (int c = 0; c < 3; ++c) {
            float srgb = static_cast<float>(imageA[i * 4 + c]) / 255.0f;
            refLinear[i * 3 + c] = FLIP::color3::sRGBToLinearRGB(srgb);

            srgb = static_cast<float>(imageB[i * 4 + c]) / 255.0f;
            testLinear[i * 3 + c] = FLIP::color3::sRGBToLinearRGB(srgb);
        }
    }

    FLIP::Parameters params;
    params.PPD = FLIP::calculatePPD(0.7f, static_cast<float>(width), 0.4f);

    float meanError = 0.0f;
    float* errorMap = nullptr;

    FLIP::evaluate(refLinear.data(), testLinear.data(), width, height, false, params, false, true,
                   meanError, &errorMap);

    // FLIP::evaluate() allocates errorMap with new[]; caller owns it
    delete[] errorMap;
    return static_cast<double>(meanError);
}

}  // namespace

// ============================================================================
// Dispatch table
// ============================================================================

const InputScriptExecutor::Handler InputScriptExecutor::s_handlers[] = {
    &InputScriptExecutor::handleWaitStartup,       // WaitStartup
    &InputScriptExecutor::handleWaitMs,            // WaitMs
    &InputScriptExecutor::handlePress,             // Press
    &InputScriptExecutor::handleKeyDown,           // KeyDown
    &InputScriptExecutor::handleKeyUp,             // KeyUp
    &InputScriptExecutor::handleClick,             // Click
    &InputScriptExecutor::handleClickRight,        // ClickRight
    &InputScriptExecutor::handleMouseDown,         // MouseDown
    &InputScriptExecutor::handleMouseUp,           // MouseUp
    &InputScriptExecutor::handleMouseMove,         // MouseMove
    &InputScriptExecutor::handleScroll,            // Scroll
    &InputScriptExecutor::handleScreenshot,        // Screenshot
    &InputScriptExecutor::handlePrint,             // Print
    &InputScriptExecutor::handleLabel,             // Label
    &InputScriptExecutor::handleLoop,              // Loop
    &InputScriptExecutor::handleExit,              // Exit
    &InputScriptExecutor::handleWaitFrames,        // WaitFrames
    &InputScriptExecutor::handleAssertSceneCount,  // AssertSceneCount
    &InputScriptExecutor::handleAssertScene,       // AssertScene
    &InputScriptExecutor::handleCompare,           // Compare
    &InputScriptExecutor::handleSet,               // Set
    &InputScriptExecutor::handleHoldKey,           // HoldKey
};

InputScriptExecutor::InputScriptExecutor(ScriptEnvironment& env) : m_env(env) {
    static_assert(std::size(s_handlers) == static_cast<size_t>(InputCommandType::HoldKey) + 1,
                  "Dispatch table out of sync with InputCommandType enum");
}

void InputScriptExecutor::loadScript(const std::string& scriptPath) {
    m_state = std::make_unique<InputScriptState>();
    std::string errorMsg;

    if (!parseInputScript(scriptPath, m_state->commands, m_state->labels, errorMsg)) {
        std::cerr << "[VDE:InputScript] " << errorMsg << std::endl;
        m_state.reset();
        return;
    }

    m_state->scriptPath = scriptPath;
    std::cout << "[VDE:InputScript] Loaded " << m_state->commands.size() << " commands from "
              << scriptPath << std::endl;
}

bool InputScriptExecutor::processFrame(float deltaTime) {
    if (!m_state || m_state->finished) {
        return false;
    }

    m_deltaTime = deltaTime;
    m_state->frameNumber++;

    // Handle pending mouse release from previous frame.
    if (m_state->pendingMouseRelease) {
        m_state->pendingMouseRelease = false;
        if (auto* handler = m_env.resolveInputHandler()) {
            handler->onMouseButtonRelease(m_state->pendingMouseButton, m_state->pendingMouseX,
                                          m_state->pendingMouseY);
        }
    }

    while (m_state->currentCommand < m_state->commands.size()) {
        const auto& cmd = m_state->commands[m_state->currentCommand];
        const auto index = static_cast<size_t>(cmd.type);

        // Safety: unknown command type -> advance and skip.
        if (index >= std::size(s_handlers)) {
            m_state->currentCommand++;
            continue;
        }

        if (!(this->*s_handlers[index])(*m_state, cmd)) {
            return true;  // Yield until next frame.
        }
    }

    // All commands consumed.
    if (m_state->assertionFailed) {
        m_env.setExitCode(1);
    }
    m_state->finished = true;
    return false;
}

bool InputScriptExecutor::isRunning() const {
    return m_state && !m_state->finished;
}

bool InputScriptExecutor::hasAssertionFailure() const {
    return m_state && m_state->assertionFailed;
}

// ============================================================================
// Command handlers
// ============================================================================

bool InputScriptExecutor::handleWaitStartup(InputScriptState& state, const ScriptCommand&) {
    if (!state.startupComplete) {
        state.startupComplete = true;
        state.currentCommand++;
        return false;
    }

    state.currentCommand++;
    return true;
}

bool InputScriptExecutor::handleWaitMs(InputScriptState& state, const ScriptCommand& cmd) {
    state.waitAccumulator += static_cast<double>(m_deltaTime) * 1000.0;
    if (state.waitAccumulator >= cmd.waitMs) {
        state.waitAccumulator = 0.0;
        state.currentCommand++;
        return true;
    }

    return false;
}

bool InputScriptExecutor::handlePress(InputScriptState& state, const ScriptCommand& cmd) {
    if (InputHandler* handler = m_env.resolveInputHandler()) {
        emitModifierKeysDown(*handler, cmd.modifiers, state);
        handler->onKeyPress(cmd.keyCode);
        handler->onKeyRelease(cmd.keyCode);
        emitScriptCharInput(*handler, cmd.keyCode, cmd.modifiers);
        emitModifierKeysUp(*handler, cmd.modifiers, state);
    }

    state.currentCommand++;
    return true;
}

bool InputScriptExecutor::handleKeyDown(InputScriptState& state, const ScriptCommand& cmd) {
    if (InputHandler* handler = m_env.resolveInputHandler()) {
        emitModifierKeysDown(*handler, cmd.modifiers, state);
        handler->onKeyPress(cmd.keyCode);
    }

    state.currentCommand++;
    return true;
}

bool InputScriptExecutor::handleKeyUp(InputScriptState& state, const ScriptCommand& cmd) {
    if (InputHandler* handler = m_env.resolveInputHandler()) {
        handler->onKeyRelease(cmd.keyCode);
        emitModifierKeysUp(*handler, cmd.modifiers, state);
    }

    state.currentCommand++;
    return true;
}

bool InputScriptExecutor::handleHoldKey(InputScriptState& state, const ScriptCommand& cmd) {
    if (!state.holdKeyActive) {
        // First entry: send keydown to simulate the user pressing and holding the key.
        // If no handler is available yet, yield without starting the timer.
        InputHandler* handler = m_env.resolveInputHandler();
        if (!handler) {
            return false;
        }
        emitModifierKeysDown(*handler, cmd.modifiers, state);
        handler->onKeyPress(cmd.keyCode);
        state.holdKeyActive = true;
    }

    state.waitAccumulator += static_cast<double>(m_deltaTime) * 1000.0;
    if (state.waitAccumulator >= cmd.waitMs) {
        // Hold duration elapsed: send keyup to release the key.
        // If handler not available, yield without releasing until it is.
        InputHandler* handler = m_env.resolveInputHandler();
        if (!handler) {
            return false;
        }
        handler->onKeyRelease(cmd.keyCode);
        emitModifierKeysUp(*handler, cmd.modifiers, state);
        state.holdKeyActive = false;
        state.waitAccumulator = 0.0;
        state.currentCommand++;
        return true;
    }

    return false;  // Yield — still holding.
}

bool InputScriptExecutor::handleClick(InputScriptState& state, const ScriptCommand& cmd) {
    if (InputHandler* handler = m_env.resolveInputHandler()) {
        handler->onMouseMove(cmd.mouseX, cmd.mouseY);
        handler->onMouseButtonPress(MOUSE_BUTTON_LEFT, cmd.mouseX, cmd.mouseY);
        state.pendingMouseRelease = true;
        state.pendingMouseButton = MOUSE_BUTTON_LEFT;
        state.pendingMouseX = cmd.mouseX;
        state.pendingMouseY = cmd.mouseY;
    }

    state.currentCommand++;
    return false;
}

bool InputScriptExecutor::handleClickRight(InputScriptState& state, const ScriptCommand& cmd) {
    if (InputHandler* handler = m_env.resolveInputHandler()) {
        handler->onMouseMove(cmd.mouseX, cmd.mouseY);
        handler->onMouseButtonPress(MOUSE_BUTTON_RIGHT, cmd.mouseX, cmd.mouseY);
        state.pendingMouseRelease = true;
        state.pendingMouseButton = MOUSE_BUTTON_RIGHT;
        state.pendingMouseX = cmd.mouseX;
        state.pendingMouseY = cmd.mouseY;
    }

    state.currentCommand++;
    return false;
}

bool InputScriptExecutor::handleMouseDown(InputScriptState& state, const ScriptCommand& cmd) {
    if (InputHandler* handler = m_env.resolveInputHandler()) {
        handler->onMouseMove(cmd.mouseX, cmd.mouseY);
        handler->onMouseButtonPress(MOUSE_BUTTON_LEFT, cmd.mouseX, cmd.mouseY);
    }

    state.currentCommand++;
    return true;
}

bool InputScriptExecutor::handleMouseUp(InputScriptState& state, const ScriptCommand& cmd) {
    if (InputHandler* handler = m_env.resolveInputHandler()) {
        handler->onMouseButtonRelease(MOUSE_BUTTON_LEFT, cmd.mouseX, cmd.mouseY);
    }

    state.currentCommand++;
    return true;
}

bool InputScriptExecutor::handleMouseMove(InputScriptState& state, const ScriptCommand& cmd) {
    if (InputHandler* handler = m_env.resolveInputHandler()) {
        handler->onMouseMove(cmd.mouseX, cmd.mouseY);
    }

    state.currentCommand++;
    return true;
}

bool InputScriptExecutor::handleScroll(InputScriptState& state, const ScriptCommand& cmd) {
    if (InputHandler* handler = m_env.resolveInputHandler()) {
        handler->onMouseMove(cmd.mouseX, cmd.mouseY);
        handler->onMouseScroll(0.0, cmd.scrollDelta);
    }

    state.currentCommand++;
    return true;
}

bool InputScriptExecutor::handleScreenshot(InputScriptState& state, const ScriptCommand& cmd) {
    m_env.captureScreenshot(makeScreenshotFramePath(cmd.argument, state.frameNumber));
    state.currentCommand++;
    return true;
}

bool InputScriptExecutor::handlePrint(InputScriptState& state, const ScriptCommand& cmd) {
    std::cout << "[VDE:InputScript] " << cmd.argument << std::endl;
    state.currentCommand++;
    return true;
}

bool InputScriptExecutor::handleLabel(InputScriptState& state, const ScriptCommand&) {
    state.currentCommand++;
    return true;
}

bool InputScriptExecutor::handleLoop(InputScriptState& state, const ScriptCommand& cmd) {
    auto labelIt = state.labels.find(cmd.argument);
    if (labelIt == state.labels.end()) {
        std::cerr << "[VDE:InputScript] Error at line " << cmd.lineNumber << ": undefined label '"
                  << cmd.argument << "'" << std::endl;
        state.finished = true;
        return false;
    }

    auto& labelState = labelIt->second;
    if (cmd.loopCount == 0) {
        state.currentCommand = labelState.commandIndex + 1;
        return true;
    }

    labelState.remainingIterations =
        labelState.remainingIterations < 0 ? cmd.loopCount - 1 : labelState.remainingIterations - 1;
    if (labelState.remainingIterations > 0) {
        state.currentCommand = labelState.commandIndex + 1;
        return true;
    }

    labelState.remainingIterations = -1;
    state.currentCommand++;
    return true;
}

bool InputScriptExecutor::handleExit(InputScriptState& state, const ScriptCommand&) {
    std::cout << "[VDE:InputScript] exit" << std::endl;
    if (state.assertionFailed) {
        m_env.setExitCode(1);
    }

    state.finished = true;
    m_env.quit();
    return false;
}

bool InputScriptExecutor::handleWaitFrames(InputScriptState& state, const ScriptCommand& cmd) {
    if (state.frameWaitCounter == 0) {
        state.frameWaitCounter = cmd.waitFrames;
    }

    state.frameWaitCounter--;
    if (state.frameWaitCounter > 0) {
        return false;
    }

    state.currentCommand++;
    return true;
}

bool InputScriptExecutor::handleAssertSceneCount(InputScriptState& state,
                                                 const ScriptCommand& cmd) {
    // Resolve assert value (may be a variable reference)
    double assertValue = cmd.assertValue;
    if (!cmd.assertVarRef.empty()) {
        auto varIt = state.variables.find(cmd.assertVarRef);
        if (varIt == state.variables.end()) {
            std::cerr << "[VDE:InputScript] ASSERT ERROR at line " << cmd.lineNumber
                      << ": undefined variable '$" << cmd.assertVarRef << "'" << std::endl;
            state.assertionFailed = true;
            state.currentCommand++;
            return true;
        }
        assertValue = varIt->second;
    }

    double count = 0.0;
    std::string fieldLabel;

    if (cmd.assertField.empty() || cmd.assertField == "rendered_scene_count") {
        count = static_cast<double>(m_env.getActiveSceneGroup().sceneNames.size());
        fieldLabel = "rendered_scene_count";
    } else if (cmd.assertField == "scenes_created") {
        count = static_cast<double>(m_env.getScenesCreated());
        fieldLabel = "scenes_created";
    } else if (cmd.assertField == "scenes_removed") {
        count = static_cast<double>(m_env.getScenesRemoved());
        fieldLabel = "scenes_removed";
    } else {
        std::cerr << "[VDE:InputScript] ASSERT ERROR at line " << cmd.lineNumber
                  << ": unknown scene count field '" << cmd.assertField << "'" << std::endl;
        state.assertionFailed = true;
        state.currentCommand++;
        return true;
    }

    if (!evaluateComparison(count, cmd.assertOp, assertValue)) {
        std::cerr << "[VDE:InputScript] ASSERT FAILED at line " << cmd.lineNumber << ": "
                  << fieldLabel << " (" << static_cast<int>(count) << ") "
                  << compareOpToString(cmd.assertOp) << " " << static_cast<int>(assertValue)
                  << std::endl;
        state.assertionFailed = true;
    }

    state.currentCommand++;
    return true;
}

bool InputScriptExecutor::handleAssertScene(InputScriptState& state, const ScriptCommand& cmd) {
    const auto& activeGroup = m_env.getActiveSceneGroup();
    Scene* targetScene = m_env.getScene(cmd.assertSceneName);
    const bool inActiveGroup = isSceneInActiveGroup(activeGroup, cmd.assertSceneName);
    double fieldValue = 0.0;

    // Resolve assert value (may be a variable reference)
    double assertValue = cmd.assertValue;
    if (!cmd.assertVarRef.empty()) {
        auto varIt = state.variables.find(cmd.assertVarRef);
        if (varIt == state.variables.end()) {
            std::cerr << "[VDE:InputScript] ASSERT ERROR at line " << cmd.lineNumber
                      << ": undefined variable '$" << cmd.assertVarRef << "'" << std::endl;
            state.assertionFailed = true;
            state.currentCommand++;
            return true;
        }
        assertValue = varIt->second;
    }

    if (!tryResolveAssertSceneFieldValue(m_env.getSwapChainExtent(), targetScene, inActiveGroup,
                                         cmd, fieldValue)) {
        std::cerr << "[VDE:InputScript] ASSERT ERROR at line " << cmd.lineNumber
                  << ": unknown field '" << cmd.assertField << "'" << std::endl;
        state.assertionFailed = true;
    } else if (!evaluateComparison(fieldValue, cmd.assertOp, assertValue)) {
        std::cerr << "[VDE:InputScript] ASSERT FAILED at line " << cmd.lineNumber << ": scene \""
                  << cmd.assertSceneName << "\" " << cmd.assertField << " (" << fieldValue << ") "
                  << compareOpToString(cmd.assertOp) << " " << assertValue << std::endl;
        state.assertionFailed = true;
    }

    state.currentCommand++;
    return true;
}

bool InputScriptExecutor::handleCompare(InputScriptState& state, const ScriptCommand& cmd) {
    std::cout << "[VDE:InputScript] compare " << cmd.argument << " vs " << cmd.comparePath
              << " (threshold " << cmd.compareThreshold << ")" << std::endl;

    int actualWidth = 0, actualHeight = 0, actualChannels = 0;
    int goldenWidth = 0, goldenHeight = 0, goldenChannels = 0;
    unsigned char* actualImage =
        stbi_load(cmd.argument.c_str(), &actualWidth, &actualHeight, &actualChannels, 4);
    unsigned char* goldenImage =
        stbi_load(cmd.comparePath.c_str(), &goldenWidth, &goldenHeight, &goldenChannels, 4);

    if (!actualImage) {
        std::cerr << "[VDE:InputScript] ASSERT FAILED at line " << cmd.lineNumber
                  << ": cannot load image '" << cmd.argument << "'" << std::endl;
        state.assertionFailed = true;
    } else if (!goldenImage) {
        std::cerr << "[VDE:InputScript] ASSERT FAILED at line " << cmd.lineNumber
                  << ": cannot load golden image '" << cmd.comparePath << "'" << std::endl;
        state.assertionFailed = true;
    } else if (actualWidth != goldenWidth || actualHeight != goldenHeight) {
        std::cerr << "[VDE:InputScript] ASSERT FAILED at line " << cmd.lineNumber
                  << ": dimension mismatch — actual (" << actualWidth << "x" << actualHeight
                  << ") vs golden (" << goldenWidth << "x" << goldenHeight << ")" << std::endl;
        state.assertionFailed = true;
    } else {
        const double error =
            computeFlipMeanError(actualImage, goldenImage, actualWidth, actualHeight);
        if (error > cmd.compareThreshold) {
            std::cerr << "[VDE:InputScript] ASSERT FAILED at line " << cmd.lineNumber
                      << ": image mismatch — FLIP " << error << " > threshold "
                      << cmd.compareThreshold << std::endl;
            state.assertionFailed = true;
        } else {
            std::cout << "[VDE:InputScript] compare PASSED (FLIP " << error
                      << " <= " << cmd.compareThreshold << ")" << std::endl;
        }
    }

    if (actualImage) {
        stbi_image_free(actualImage);
    }
    if (goldenImage) {
        stbi_image_free(goldenImage);
    }

    state.currentCommand++;
    return true;
}

bool InputScriptExecutor::handleSet(InputScriptState& state, const ScriptCommand& cmd) {
    state.variables[cmd.setVarName] = cmd.setVarValue;
    state.currentCommand++;
    return true;
}

}  // namespace vde
