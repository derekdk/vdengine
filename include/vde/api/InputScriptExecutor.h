#pragma once

/**
 * @file InputScriptExecutor.h
 * @brief Owns input-script state and dispatches commands each frame
 *
 * Replaces the 21+ private methods that previously lived on Game.
 * Game creates the executor, calls processFrame() once per frame, done.
 */

#include <vde/api/InputScript.h>

#include <memory>
#include <string>

namespace vde {

class ScriptEnvironment;

/**
 * @brief Executes an input script one frame at a time.
 *
 * Owns the InputScriptState and the dispatch table.
 * Game calls processFrame() once per frame; the executor handles all
 * command decoding and environmental interaction through ScriptEnvironment.
 */
class InputScriptExecutor {
  public:
    explicit InputScriptExecutor(ScriptEnvironment& env);

    /// Load and prepare a script from the given file path.
    void loadScript(const std::string& scriptPath);

    /// Advance the script by one frame.  Returns true while the script
    /// is still running.  Returns false when finished or no script loaded.
    bool processFrame(float deltaTime);

    /// True when the script has been loaded and has not yet finished.
    bool isRunning() const;

    /// True if any assertion in the script failed.
    bool hasAssertionFailure() const;

    /// Direct access for test inspection.
    InputScriptState* getState() { return m_state.get(); }
    const InputScriptState* getState() const { return m_state.get(); }

    /// Replace the internal state (for testing).
    void setState(std::unique_ptr<InputScriptState> state) { m_state = std::move(state); }

  private:
    // Command handler signature.  Returns true = keep processing this
    // frame, false = yield until next frame.
    using Handler = bool (InputScriptExecutor::*)(InputScriptState&, const ScriptCommand&);

    // Dispatch table indexed by InputCommandType.
    static const Handler s_handlers[];

    // Per-command handlers
    bool handleWaitStartup(InputScriptState& state, const ScriptCommand& cmd);
    bool handleWaitMs(InputScriptState& state, const ScriptCommand& cmd);
    bool handlePress(InputScriptState& state, const ScriptCommand& cmd);
    bool handleKeyDown(InputScriptState& state, const ScriptCommand& cmd);
    bool handleKeyUp(InputScriptState& state, const ScriptCommand& cmd);
    bool handleClick(InputScriptState& state, const ScriptCommand& cmd);
    bool handleClickRight(InputScriptState& state, const ScriptCommand& cmd);
    bool handleMouseDown(InputScriptState& state, const ScriptCommand& cmd);
    bool handleMouseUp(InputScriptState& state, const ScriptCommand& cmd);
    bool handleMouseMove(InputScriptState& state, const ScriptCommand& cmd);
    bool handleScroll(InputScriptState& state, const ScriptCommand& cmd);
    bool handleScreenshot(InputScriptState& state, const ScriptCommand& cmd);
    bool handlePrint(InputScriptState& state, const ScriptCommand& cmd);
    bool handleLabel(InputScriptState& state, const ScriptCommand& cmd);
    bool handleLoop(InputScriptState& state, const ScriptCommand& cmd);
    bool handleExit(InputScriptState& state, const ScriptCommand& cmd);
    bool handleWaitFrames(InputScriptState& state, const ScriptCommand& cmd);
    bool handleAssertSceneCount(InputScriptState& state, const ScriptCommand& cmd);
    bool handleAssertScene(InputScriptState& state, const ScriptCommand& cmd);
    bool handleCompare(InputScriptState& state, const ScriptCommand& cmd);
    bool handleSet(InputScriptState& state, const ScriptCommand& cmd);
    bool handleHoldKey(InputScriptState& state, const ScriptCommand& cmd);

    ScriptEnvironment& m_env;
    float m_deltaTime = 0.0f;
    std::unique_ptr<InputScriptState> m_state;
};

}  // namespace vde
