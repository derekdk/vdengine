# Input Script Executor — Design Proposal

## Problem

The `processInputScript` refactor decomposes a 367-line monolithic function into 21 private methods on `Game`. This trades one problem (a giant function) for another (a god class that grows with every new script command). Game already has ~60 private members and ~30 private methods; adding 21 more pushes it further from single-responsibility.

The script execution subsystem has a clear boundary: it reads `InputScriptState`, dispatches commands, and interacts with Game only through a narrow set of services (input delivery, scene queries, screenshots, exit). It should live in its own class.

## Design Goals

1. **Game stays thin.** Game owns the executor, calls one method per frame, done.
2. **Adding a new script command never touches Game.h.**
3. **Testable without Vulkan.** Tests inject a mock/stub for the services the executor needs.
4. **Zero overhead.** Dispatch is a function-pointer table lookup, not a virtual call chain.
5. **Consistent with existing patterns.** Game already delegates to `TransitionManager`, `Scheduler`, and `ResourceManager` the same way.

## Proposed Architecture

```
Game  ──owns──►  InputScriptExecutor  ──uses──►  ScriptEnvironment (interface)
                        │
                        ▼
                  dispatch table
              (InputCommandType → handler fn)
```

### ScriptEnvironment — the narrow contract

`InputScriptExecutor` does **not** get a `Game*`. Instead it gets a thin interface that exposes only the services script commands actually need. This prevents the executor from growing invisible coupling to the rest of Game.

```cpp
// include/vde/api/ScriptEnvironment.h

#pragma once

#include <string>

namespace vde {

class InputHandler;
class Scene;
struct SceneGroup;

/// Narrow interface that InputScriptExecutor uses to interact with the
/// engine.  Game implements this directly (no separate adapter object).
class ScriptEnvironment {
  public:
    virtual ~ScriptEnvironment() = default;

    /// Resolve the input handler for the current frame (focused scene → global).
    virtual InputHandler* resolveInputHandler() = 0;

    /// Capture the current framebuffer to a PNG file.
    virtual bool captureScreenshot(const std::string& outputPath) = 0;

    /// Look up a scene by name.
    virtual Scene* getScene(const std::string& name) = 0;

    /// Get the currently active scene group.
    virtual const SceneGroup& getActiveSceneGroup() const = 0;

    /// Get the swap chain extent (width, height) — for viewport assertions.
    virtual std::pair<uint32_t, uint32_t> getSwapChainExtent() const = 0;

    /// Set the process exit code.
    virtual void setExitCode(int code) = 0;

    /// Request engine shutdown.
    virtual void quit() = 0;
};

}  // namespace vde
```

Game implements `ScriptEnvironment` privately:
```cpp
class Game : public ScriptEnvironment { ... };
// or: Game holds a small nested GameScriptEnv that forwards to Game.
```

Private inheritance or a nested forwarder both work. Private inheritance is simpler and the vtable cost is irrelevant (one call per frame at most).

### InputScriptExecutor — owns script state and dispatch

```cpp
// include/vde/api/InputScriptExecutor.h

#pragma once

#include <memory>

#include "InputScript.h"

namespace vde {

class ScriptEnvironment;

/// Executes an input script one frame at a time.
///
/// Owns the InputScriptState and the dispatch table.
/// Game calls processFrame() once per frame; the executor handles all
/// command decoding and environmental interaction through ScriptEnvironment.
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

    /// Direct access for advanced test inspection.
    InputScriptState* getState() { return m_state.get(); }
    const InputScriptState* getState() const { return m_state.get(); }

  private:
    // Command handler signature.  Returns true = keep processing this
    // frame, false = yield until next frame.
    using Handler = bool (InputScriptExecutor::*)(InputScriptState&,
                                                   const ScriptCommand&);

    // Dispatch table indexed by InputCommandType.
    static const Handler s_handlers[];

    // Per-command handlers (private, never exposed in Game.h)
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

    ScriptEnvironment& m_env;
    float m_deltaTime = 0.0f;
    std::unique_ptr<InputScriptState> m_state;
};

}  // namespace vde
```

### Dispatch table replaces the switch

```cpp
// InputScriptExecutor.cpp

// Table indexed by static_cast<size_t>(InputCommandType).
const InputScriptExecutor::Handler InputScriptExecutor::s_handlers[] = {
    &InputScriptExecutor::handleWaitStartup,     // WaitStartup
    &InputScriptExecutor::handleWaitMs,           // WaitMs
    &InputScriptExecutor::handlePress,            // Press
    &InputScriptExecutor::handleKeyDown,          // KeyDown
    &InputScriptExecutor::handleKeyUp,            // KeyUp
    &InputScriptExecutor::handleClick,            // Click
    &InputScriptExecutor::handleClickRight,       // ClickRight
    &InputScriptExecutor::handleMouseDown,        // MouseDown
    &InputScriptExecutor::handleMouseUp,          // MouseUp
    &InputScriptExecutor::handleMouseMove,        // MouseMove
    &InputScriptExecutor::handleScroll,           // Scroll
    &InputScriptExecutor::handleScreenshot,       // Screenshot
    &InputScriptExecutor::handlePrint,            // Print
    &InputScriptExecutor::handleLabel,            // Label
    &InputScriptExecutor::handleLoop,             // Loop
    &InputScriptExecutor::handleExit,             // Exit
    &InputScriptExecutor::handleWaitFrames,       // WaitFrames
    &InputScriptExecutor::handleAssertSceneCount, // AssertSceneCount
    &InputScriptExecutor::handleAssertScene,      // AssertScene
    &InputScriptExecutor::handleCompare,          // Compare
    &InputScriptExecutor::handleSet,              // Set
};

// Compile-time safety: ensure table covers every enum value.
static_assert(std::size(InputScriptExecutor::s_handlers) ==
              static_cast<size_t>(InputCommandType::Set) + 1,
              "Dispatch table out of sync with InputCommandType enum");

bool InputScriptExecutor::processFrame(float deltaTime) {
    if (!m_state || m_state->finished) return false;

    m_deltaTime = deltaTime;
    m_state->frameNumber++;

    // Handle pending mouse release from previous frame.
    if (m_state->pendingMouseRelease) {
        m_state->pendingMouseRelease = false;
        if (auto* handler = m_env.resolveInputHandler()) {
            handler->onMouseButtonRelease(m_state->pendingMouseButton,
                                          m_state->pendingMouseX,
                                          m_state->pendingMouseY);
        }
    }

    while (m_state->currentCommand < m_state->commands.size()) {
        const auto& cmd = m_state->commands[m_state->currentCommand];
        const auto index = static_cast<size_t>(cmd.type);

        // Safety: unknown command type → advance and skip.
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
```

The `static_assert` guarantees that adding a new `InputCommandType` without a corresponding handler entry is a **compile error**, not a silent infinite loop.

### How Game changes

Game shrinks back to its pre-refactor footprint:

```cpp
// Game.h — private section changes

    // Input script  (BEFORE: 23 methods + state)
    // AFTER: one object
    std::unique_ptr<InputScriptExecutor> m_scriptExecutor;

    // Internal methods — script methods removed entirely
    void processInput();
    void loadInputScript();  // stays on Game (creates the executor)
    // ... everything else unchanged ...
```

```cpp
// Game.cpp — frame loop callsite

void Game::processInputScript() {
    if (m_scriptExecutor) {
        m_scriptExecutor->processFrame(m_deltaTime);
    }
}
```

`loadInputScript()` creates the executor:

```cpp
void Game::loadInputScript() {
    // ... existing file discovery logic ...
    m_scriptExecutor = std::make_unique<InputScriptExecutor>(*this);
    m_scriptExecutor->loadScript(m_inputScriptFile);
}
```

### Test accessors become unnecessary

The `VDE_TEST_ACCESSORS` block in Game.h is deleted entirely. Tests construct an `InputScriptExecutor` directly with a mock `ScriptEnvironment`:

```cpp
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

TEST(InputScriptExecutor, WaitFramesBlocksUntilCounterExpires) {
    MockScriptEnv env;
    InputScriptExecutor executor(env);

    auto state = std::make_unique<InputScriptState>();
    ScriptCommand waitFrames{};
    waitFrames.type = InputCommandType::WaitFrames;
    waitFrames.waitFrames = 2;
    state->commands = {waitFrames, makePressCommand(KEY_A)};
    // ... inject state, call processFrame(), check env.handler ...
}
```

No `VDE_TEST_ACCESSORS`, no `#ifdef`, no friend declarations, no mutable references to Game internals.

## File Placement

| File | Location |
|------|----------|
| `ScriptEnvironment.h` | `include/vde/api/ScriptEnvironment.h` |
| `InputScriptExecutor.h` | `include/vde/api/InputScriptExecutor.h` |
| `InputScriptExecutor.cpp` | `src/api/InputScriptExecutor.cpp` |
| `InputScriptExecutor_test.cpp` | `tests/InputScriptExecutor_test.cpp` |

## Migration Path

1. Create `ScriptEnvironment` interface.
2. Create `InputScriptExecutor` class, move handler methods from Game.cpp verbatim.
3. Make Game implement `ScriptEnvironment` (private inheritance or nested class).
4. Replace `m_inputScriptState` + 21 methods in Game with `m_scriptExecutor`.
5. Delete `VDE_TEST_ACCESSORS` block.
6. Port `GameInputScriptExecution_test.cpp` to use `MockScriptEnv` + `InputScriptExecutor` directly.
7. Build, test, smoke.

Each step compiles and passes tests independently.

## Why Not Other Approaches

| Alternative | Rejection Reason |
|-------------|------------------|
| **Virtual dispatch per command** (Command pattern) | 21 heap allocations per script load for no benefit. Commands are value types; a table lookup is O(1) with zero allocation. |
| **`std::function` dispatch table** | Captures `this` in lambdas, larger per-entry (typically 32-64 bytes vs 8 for a member-function pointer), and harder to inspect in a debugger. |
| **Free functions in anonymous namespace** | Removes methods from Game, but forces every helper to take 5+ parameters (env, state, cmd, deltaTime, ...) since they can't access executor members. Groups poorly. |
| **Keep methods on Game behind `#ifdef`** | Leaks internal detail into the public header, requires test-only compile definitions, fragile. |

## Summary of Wins

- **Game.h private section:** −21 method declarations, −6 test accessor lines, −1 state member. Net: −28 lines.
- **Separation:** Script execution is fully encapsulated. Game never sees command types.
- **Scalability:** New command = add enum value + handler method + table entry. Compile-time `static_assert` catches omissions.
- **Testability:** Pure unit tests with `MockScriptEnv`. No Vulkan, no window, no `#ifdef`.
- **Performance:** Identical or better. Table lookup replaces switch; `static_assert` prevents the missing-default infinite loop bug.
