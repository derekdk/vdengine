# Scripted Input — Implementation Plan

> Companion to [SCRIPTED_INPUT_REVIEW.md](SCRIPTED_INPUT_REVIEW.md). This plan implements the **Phase 1** recommendations from the review: input replay, CLI integration, and smoke testing infrastructure.

## Design Decisions

The following decisions were resolved during the review and are codified here:

| Decision | Resolution | Rationale |
|---|---|---|
| Command syntax | Verb-arg (space-separated) | Matches xdotool/AutoHotkey convention; simpler to parse and write than angle-bracket tags |
| File extension | `.vdescript` | Distinct from generic `.txt`; enables tooling/file-association |
| Namespace for helpers | `vde::` (not `vde::examples::`) | Shared by examples and tools; lives alongside `Game` |
| Logging prefix | `[VDE:InputScript]` | Matches engine subsystem convention |
| Modifier key syntax | `ctrl+`, `shift+`, `alt+` prefix | Matches xdotool/browser convention; composable with `+` separator |
| Mouse coordinate space | Window-relative pixels | Matches GLFW callback coordinate space |
| Loop syntax | `label NAME` / `loop NAME COUNT` | Named labels avoid nesting complexity; count=0 means infinite |
| Input source priority | API call > CLI arg > env var | Standard convention; CLI always wins over env |
| Error handling | Fail-fast; abort with line number | Prevents confusing partial script executions |
| Script end behavior | App continues running after last command | Only `exit` terminates; allows setup-then-interact scripts |
| Tool batch processing | Not in scope — use proper CLI args | Keyboard simulation for batch ops is fragile |

## Scope

**In scope (Phase 1):**
- Script parser and command execution engine
- Verb-arg command syntax (space-separated, no angle brackets)
- Keyboard commands: `wait startup`, `wait N`, `press KEY`, `keydown KEY`, `keyup KEY`, `exit`
- Modifier keys: `press ctrl+A`, `keydown shift+W`, `press ctrl+shift+S`
- Mouse commands: `click X Y`, `click right X Y`, `mousedown X Y`, `mouseup X Y`, `mousemove X Y`, `scroll X Y DELTA`
- Loop control: `label NAME`, `loop NAME COUNT`
- `--input-script` CLI argument support on `Game`
- `VDE_INPUT_SCRIPT` environment variable fallback (lowest priority)
- `Game::setInputScriptFile()` direct API (highest priority)
- Updated `runExample()` / `runTool()` to accept `argc`/`argv`
- All 20 example `main()` + 1 tool `main()` updated to pass `argc`/`argv`
- Smoke test `.vdescript` scripts for each example
- Unit tests for the script parser
- `screenshot PATH` command (save frame to PNG via stb_image_write)
- Script file extension: `.vdescript`
- Console logging with `[VDE:InputScript]` prefix

**Deferred (Phase 2 — separate proposal):**
- `extractpixels`, `hashpixels`, `comparepixels`
- GPU-to-CPU pixel readback infrastructure
- Hash computation / visual regression framework
- Gamepad scripting
- Conditional logic, variables

---

## Work Items

### WI-1: InputScript Engine (Core)

**Files to create:**
- `include/vde/api/InputScript.h` — public types and free functions
- `src/api/InputScript.cpp` — parser, state machine, command execution

**Types:**

```cpp
// include/vde/api/InputScript.h
#pragma once

#include <memory>
#include <string>

namespace vde {

class Game; // forward

/// Opaque state for a running input script
struct InputScriptState;

/// Parse --input-script from command-line arguments
std::string getInputScriptArg(int argc, char** argv);

/// Configure game with input script from CLI args  
void configureInputScriptFromArgs(Game& game, int argc, char** argv);

} // namespace vde
```

**Internal types (in .cpp or private header):**

```cpp
enum class CommandType {
    WaitStartup,   // wait startup
    WaitMs,        // wait 500
    Press,         // press A          (keydown + keyup, with optional modifiers)
    KeyDown,       // keydown W        (with optional modifiers)
    KeyUp,         // keyup W          (with optional modifiers)
    Click,         // click 400 300    (left-click at position)
    ClickRight,    // click right 400 300
    MouseDown,     // mousedown 400 300
    MouseUp,       // mouseup 400 300
    MouseMove,     // mousemove 400 300
    Scroll,        // scroll 400 300 -3
    Screenshot,    // screenshot path.png
    Label,         // label loop_start
    Loop,          // loop loop_start 5
    Exit           // exit
};

struct ScriptCommand {
    CommandType type;
    int keyCode = 0;              // for Press/KeyDown/KeyUp
    int modifiers = 0;            // bitmask: MOD_CTRL | MOD_SHIFT | MOD_ALT
    double waitMs = 0.0;          // for WaitMs
    double mouseX = 0.0;          // for Click/MouseDown/MouseUp/MouseMove/Scroll
    double mouseY = 0.0;          // for Click/MouseDown/MouseUp/MouseMove/Scroll
    double scrollDelta = 0.0;     // for Scroll
    std::string argument;         // for Screenshot path, Label name, Loop target
    int loopCount = 0;            // for Loop (0 = infinite)
    int lineNumber = 0;           // source line for error messages
};

// Modifier bitmask constants
constexpr int MOD_CTRL  = 1 << 0;
constexpr int MOD_SHIFT = 1 << 1;
constexpr int MOD_ALT   = 1 << 2;

struct LabelState {
    size_t commandIndex = 0;      // index of the label command
    int remainingIterations = -1; // -1 = not yet entered, 0 = infinite
};

struct InputScriptState {
    std::vector<ScriptCommand> commands;
    size_t currentCommand = 0;
    double waitAccumulator = 0.0;
    bool startupComplete = false;
    bool finished = false;
    std::string scriptPath;
    uint64_t frameNumber = 0;     // for screenshot frame tracking
    std::unordered_map<std::string, LabelState> labels;  // label name → state
};
```

**Command syntax reference:**

All commands use a verb-arg format with space-separated arguments. Commands are case-insensitive. Comments use `#` or `//`.

```text
# Timing
wait startup                     # wait for first frame render
wait 500                         # wait 500ms
wait 2s                          # wait 2 seconds (optional suffix)

# Keyboard — bare key names (no quotes needed)
press A                          # keydown + keyup for A
press SPACE                      # keydown + keyup for Space
keydown W                        # hold W
keyup W                          # release W

# Keyboard — with modifiers (+ separator, order doesn't matter)
press ctrl+S                     # Ctrl+S
press ctrl+shift+Z               # Ctrl+Shift+Z
press alt+F4                     # Alt+F4
keydown shift+W                  # hold Shift+W

# Mouse — window-relative pixel coordinates
click 400 300                    # left-click at (400, 300)
click right 400 300              # right-click at (400, 300)
mousedown 400 300                # press left button at (400, 300)
mouseup 500 300                  # release left button at (500, 300)
mousemove 640 360                # move cursor to (640, 360)
scroll 400 300 -3                # scroll down 3 units at (400, 300)
scroll 400 300 5                 # scroll up 5 units at (400, 300)

# Loops — named labels with counted repetition
label my_loop                    # define a jump target
press A
wait 200
loop my_loop 5                   # jump back to label, 5 total iterations
loop my_loop 0                   # jump back to label, infinite (until exit)

# Capture
screenshot output.png            # save current frame to PNG

# Control
exit                             # quit the application
```

**Modifier key handling:**
- Modifiers are parsed from the `+`-separated key argument: `ctrl+shift+A` → modifiers=`MOD_CTRL|MOD_SHIFT`, key=`A`
- The key itself is always the last token after splitting on `+`
- Modifier names are case-insensitive: `Ctrl`, `CTRL`, `ctrl` all match
- Supported modifiers: `ctrl`, `shift`, `alt`
- When executing a modified press: `onKeyPress(key, modifiers)` / `onKeyRelease(key, modifiers)` — modifiers are passed through to the handler
- If the engine's `InputHandler` doesn't support a modifiers parameter, fall back to synthesizing individual modifier keydown/keyup events around the main key

**Mouse input handling:**
- Coordinates are window-relative pixels matching GLFW's callback coordinate space
- `click X Y` synthesizes: `onMouseMove(X, Y)` → `onMouseButton(LEFT, PRESS)` → next-frame `onMouseButton(LEFT, RELEASE)`
- `click right X Y` uses `RIGHT` button instead of `LEFT`
- `mousedown`/`mouseup` give explicit control for drag sequences
- `scroll X Y DELTA` maps to `onMouseScroll(X, Y, DELTA)` — positive = up, negative = down
- Mouse events are dispatched through the same handler resolution as keyboard: focused scene handler → global input handler

**Loop handling:**
- `label NAME` records the command index for the given label name; does not execute anything
- `loop NAME COUNT` jumps the command pointer back to the command after `label NAME`
- COUNT is the total number of iterations. On first encounter, `remainingIterations` is set to COUNT. Each time `loop` is reached, remaining is decremented. When remaining reaches 0, execution falls through.
- COUNT=0 means infinite loop (never decrements, always jumps). Only `exit` or external termination stops infinite loops.
- Labels must be defined before the `loop` command that references them (forward references are parse errors)
- Nested loops are supported by using different label names:
  ```text
  label outer
    label inner
    press A
    wait 100
    loop inner 3
  press B
  wait 200
  loop outer 2
  ```

**Parser responsibilities:**
- Read file line by line
- Strip comments (`#`, `//`) and blank lines
- Split each line into verb + arguments by whitespace
- Case-insensitive verb matching
- Map key names to `vde::KEY_*` constants (bare names, no quotes)
- Parse modifier prefixes from `+`-separated key arguments
- Parse mouse coordinates as doubles
- Parse scroll delta as signed double
- Resolve label references and validate no forward-reference loops
- Handle named keys: `SPACE`, `ESC`/`ESCAPE`, `ENTER`/`RETURN`, `TAB`, `BACKSPACE`, `DELETE`, `INSERT`, `HOME`, `END`, `LEFT`, `RIGHT`, `UP`, `DOWN`, `PGUP`/`PAGEUP`, `PGDN`/`PAGEDOWN`, `F1`–`F12`
- Single character key names map directly: `A` → `KEY_A`, `1` → `KEY_1`
- Optional `KEY_` prefix stripping: `KEY_SPACE` and `SPACE` are equivalent
- Report errors with line numbers

**Execution responsibilities (called from `Game::run()`):**
- Track current command index
- `WaitStartup`: set flag on first frame, advance
- `WaitMs`: accumulate `deltaTime`, advance when elapsed
- `Press`: call `handler->onKeyPress(key, modifiers)` then `handler->onKeyRelease(key, modifiers)`
- `KeyDown`: call `handler->onKeyPress(key, modifiers)`
- `KeyUp`: call `handler->onKeyRelease(key, modifiers)`
- `Click`/`ClickRight`: move cursor, press button, schedule release on next frame
- `MouseDown`/`MouseUp`: dispatch mouse button event at position
- `MouseMove`: dispatch cursor position event
- `Scroll`: dispatch scroll event at position
- `Screenshot`: capture swapchain to PNG with frame number in filename (WI-5)
- `Label`: no-op at execution time, advance
- `Loop`: check remaining iterations, jump or fall through
- `Exit`: call `Game::quit()`
- Handler resolution: focused scene handler → global input handler (matching existing dispatch logic)
- When all commands are consumed, do nothing (app continues running; only `exit` terminates)
- Frame counter incremented each frame for screenshot ordering

**Logging:**
- All console output uses the `[VDE:InputScript]` prefix
- Script load: `[VDE:InputScript] Loaded 12 commands from path/to/script.vdescript`
- Command execution: `[VDE:InputScript] press A` (debug-level, optional)
- Modifier commands: `[VDE:InputScript] press ctrl+shift+S` (debug-level, optional)
- Mouse commands: `[VDE:InputScript] click 400 300` (debug-level, optional)
- Loop commands: `[VDE:InputScript] loop my_loop iteration 3/5` (debug-level, optional)
- Errors: `[VDE:InputScript] Error at line 5: Invalid key name 'XYZ'`

**Error handling:**
- Parse errors abort loading the entire script (fail-fast)
- Log error with source line number and the offending text
- `loadInputScript()` returns false; `Game::initialize()` logs warning but continues (no script replay)
- File-not-found logs error to stderr and continues without a script
- Undefined label in `loop` command is a parse error
- Invalid mouse coordinates (non-numeric) are parse errors
- Unknown modifier name is a parse error

**CMake:** Add both files to `VDE_PUBLIC_HEADERS` and `VDE_SOURCES` in root `CMakeLists.txt`.

---

### WI-2: Game Class Integration

**Files to modify:**
- `include/vde/api/Game.h`
- `src/api/Game.cpp`

**Changes to `Game.h`:**

```cpp
// New #include
#include "InputScript.h"

class Game {
public:
    // New public methods
    void setInputScriptFile(const std::string& scriptPath);
    const std::string& getInputScriptFile() const;

private:
    // New private methods
    void loadInputScript();       // called in initialize()
    void processInputScript();    // called each frame in run()

    // New members
    std::string m_inputScriptFile;
    std::unique_ptr<InputScriptState> m_inputScriptState;
};
```

**Changes to `Game.cpp`:**

1. **In `initialize()`** — after all setup, before returning:
   ```cpp
   // Discovery order: explicit API call > CLI arg > environment variable
   // (CLI arg is applied before initialize via configureInputScriptFromArgs)
   if (m_inputScriptFile.empty()) {
       const char* envScript = std::getenv("VDE_INPUT_SCRIPT");
       if (envScript && envScript[0] != '\0') {
           m_inputScriptFile = envScript;
       }
   }
   if (!m_inputScriptFile.empty()) {
       loadInputScript();
   }
   ```

2. **In `run()`** — insert `processInputScript()` after `processInput()`:
   ```cpp
   processInput();
   processInputScript();  // ← new
   ```

3. **New method `processInputScript()`** — delegates to `InputScriptState` logic:
   - If no script loaded, return immediately
   - Process commands using `deltaTime`, dispatch to resolved input handler
   - Keyboard commands dispatch with modifier bitmask
   - Mouse commands dispatch cursor position and button events
   - Loop commands manage label jump state
   - On `Exit` command, call `quit()`

4. **In `quit()`** or destructor — clean up `m_inputScriptState`.

---

### WI-3: ExampleBase.h and ToolBase.h Updates

**Files to modify:**
- `examples/ExampleBase.h`
- `tools/ToolBase.h`

**ExampleBase.h changes:**

Update `runExample()` template to accept `argc`/`argv`:

```cpp
template <typename TGame>
int runExample(TGame& game, const std::string& gameName,
               uint32_t width = 1280, uint32_t height = 720,
               int argc = 0, char** argv = nullptr) {
    // Configure input script from CLI args (vde:: namespace, not vde::examples::)
    if (argc > 0 && argv != nullptr) {
        vde::configureInputScriptFromArgs(game, argc, argv);
    }

    // ... existing initialization and run logic ...
}
```

Add `#include <vde/api/InputScript.h>` to includes.

Default arguments (`argc = 0, argv = nullptr`) ensure backward compatibility — existing examples without `argc/argv` continue to compile.

**Important:** The helper functions `configureInputScriptFromArgs()` and `getInputScriptArg()` live in the `vde::` namespace (not `vde::examples::` or `vde::tools::`), since they operate on the shared `Game` class. `ExampleBase.h` and `ToolBase.h` simply call through to `vde::configureInputScriptFromArgs()`.

**ToolBase.h changes:** Same pattern for `runTool()`. Tool input scripting is intended for **smoke testing only** ("does it launch and render without crashing?"). Batch tool operations should use proper CLI arguments, not keyboard replay.

---

### WI-4: Update All Example and Tool `main()` Functions

**21 files to modify** (20 examples + 1 tool):

Each `main()` changes from:
```cpp
int main() {
    MyGame game;
    return vde::examples::runExample(game, "Name", 1280, 720);
}
```

To:
```cpp
int main(int argc, char** argv) {
    MyGame game;
    return vde::examples::runExample(game, "Name", 1280, 720, argc, argv);
}
```

**Complete list:**

| # | Path |
|---|---|
| 1 | `examples/asteroids_demo/main.cpp` |
| 2 | `examples/audio_demo/main.cpp` |
| 3 | `examples/breakout_demo/main.cpp` |
| 4 | `examples/four_scene_3d_demo/main.cpp` |
| 5 | `examples/geometry_repl/main.cpp` |
| 6 | `examples/hex_prism_stacks_demo/main.cpp` |
| 7 | `examples/imgui_demo/main.cpp` |
| 8 | `examples/materials_lighting_demo/main.cpp` |
| 9 | `examples/multi_scene_demo/main.cpp` |
| 10 | `examples/parallel_physics_demo/main.cpp` |
| 11 | `examples/physics_audio_demo/main.cpp` |
| 12 | `examples/physics_demo/main.cpp` |
| 13 | `examples/quad_viewport_demo/main.cpp` |
| 14 | `examples/resource_demo/main.cpp` |
| 15 | `examples/sidescroller/main.cpp` |
| 16 | `examples/simple_game/main.cpp` |
| 17 | `examples/sprite_demo/main.cpp` |
| 18 | `examples/triangle/main.cpp` |
| 19 | `examples/wireframe_viewer/main.cpp` |
| 20 | `examples/world_bounds_demo/main.cpp` |
| 21 | `tools/vlauncher/main.cpp` |

`tools/geometry_repl/main.cpp` already has `argc/argv` — just needs `configureInputScriptFromArgs` call added.

---

### WI-5: Screenshot Command (Optional, Lightweight)

**Rationale:** Capturing a PNG is cheap to implement (stb_image_write is already a dependency) and provides manual visual verification without the complexity of pixel hashing.

**Command syntax:**
```text
screenshot output.png
screenshot screenshots/test.png
```

**Frame number insertion:**
Screenshots automatically include the frame number at which they were captured, enabling chronological ordering during review. The frame number is inserted before the file extension:

- `screenshot output.png` → `output_frame_0042.png` (captured at frame 42)
- `screenshot screenshots/test.png` → `screenshots/test_frame_0137.png` (captured at frame 137)

Frame numbers are zero-padded to 6 digits to ensure proper lexicographic sorting.

**Implementation:**
- Track frame counter in `InputScriptState` (incremented each frame in `processInputScript()`)
- In `Game`, add a method to read the current swapchain image to CPU memory
- Use `stbi_write_png()` to save with frame-numbered filename
- This reuses existing Vulkan image copy patterns in the codebase
- Parse the screenshot path, insert `_frame_NNNNNN` before the extension

**This can be deferred** if pixel readback proves complex. The input replay system is fully useful without it.

---

### WI-6: Unit Tests for Script Parser

**File to create:** `tests/InputScript_test.cpp`

**Test cases (no GPU needed):**

```
# Timing
ParsesWaitStartup
ParsesWaitMs
ParsesWaitSecondsSuffix

# Keyboard — basic
ParsesPressCharacterKey  
ParsesPressNamedKey
ParsesPressWithKeyPrefix
ParsesKeyDown
ParsesKeyUp

# Keyboard — modifiers
ParsesCtrlModifier
ParsesShiftModifier
ParsesAltModifier
ParsesMultipleModifiers
ModifierOrderDoesNotMatter
UnknownModifierReportsError

# Mouse
ParsesClick
ParsesClickRight
ParsesMouseDown
ParsesMouseUp
ParsesMouseMove
ParsesScroll
ParsesScrollNegativeDelta
InvalidMouseCoordsReportsError

# Loops
ParsesLabel
ParsesLoop
ParsesLoopInfinite
UndefinedLabelReportsError
NestedLoopsWithDifferentLabels

# Control
ParsesExit
ParsesQuit
ParsesScreenshot

# Syntax
IgnoresComments
IgnoresBlankLines
CaseInsensitive
InvalidVerbReportsError
MalformedCommandReportsError
```

**Register in `tests/CMakeLists.txt`** by adding to `VDE_TEST_SOURCES`.

These tests validate the parser logic against the `ScriptCommand` output — pure CPU, no window or Vulkan context needed.

---

### WI-7: Smoke Test Scripts

**Directory:** `smoketests/scripts/` (keep separate from build scripts)

**File extension:** `.vdescript` — distinct from generic `.txt`, enables tooling and file-type association.

**Universal smoke test** (`smoketests/scripts/smoke_test.vdescript`):
```text
# Universal smoke test
# Verifies example launches, renders, and exits cleanly
wait startup
wait 3000
exit
```

**Per-example scripts** (optional, for examples with specific key bindings):
```text
# smoketests/scripts/smoke_physics_demo.vdescript
wait startup
wait 1000
press 1
wait 500
press 2
wait 500
wait 2000
exit
```

**Script demonstrating modifiers, mouse, and loops:**
```text
# smoketests/scripts/smoke_interactive_demo.vdescript
wait startup
wait 500

# Click in the viewport, then use modifier keys
click 640 360
wait 200
press ctrl+S
wait 500

# Drag sequence
mousedown 100 100
wait 100
mousemove 300 300
wait 100
mouseup 300 300
wait 200

# Repeat a key sequence 3 times
label key_loop
press A
wait 200
press B
wait 200
loop key_loop 3

wait 1000
exit
```

**Update `scripts/tmp_run_examples_check.ps1`** to use `--input-script smoketests/scripts/smoke_test.vdescript` instead of relying on auto-timeout.

---

### WI-8: Documentation Updates

1. **Rewrite `docs/SCRIPTED_INPUT_API.md`** — remove pixel hashing sections, reduce redundancy, focus on implemented Phase 1 features
2. **Update `API-DOC.md`** — add InputScript section to the API reference
3. **Update `docs/GETTING_STARTED.md`** — mention `--input-script` as a testing option
4. **Update `README.md`** — update any example commands to show `--input-script` usage

---

## Implementation Order

```
Phase 1a — Core (can be built and tested independently)
├── WI-1: InputScript parser + state machine
├── WI-6: Unit tests for parser
│
Phase 1b — Integration (requires WI-1)  
├── WI-2: Game class integration
├── WI-3: ExampleBase.h / ToolBase.h updates
├── WI-4: All main() function updates
│
Phase 1c — Infrastructure (requires WI-2)
├── WI-7: Smoke test scripts
├── WI-8: Documentation updates
│
Phase 1d — Optional enhancement
└── WI-5: Screenshot command
```

**Suggested PR structure:**
1. **PR 1:** WI-1 + WI-6 (parser + tests) — can be reviewed and merged independently
2. **PR 2:** WI-2 + WI-3 + WI-4 (integration + all main() updates) — builds on PR 1
3. **PR 3:** WI-7 + WI-8 (scripts + docs) — builds on PR 2
4. **PR 4:** WI-5 (screenshot) — independent enhancement

---

## Effort Estimates

| Work Item | Complexity | Estimated Effort |
|---|---|---|
| WI-1: InputScript engine | Medium–High | 5–7 hours |
| WI-2: Game integration | Medium | 2–3 hours |
| WI-3: Base class updates | Low | 30 min |
| WI-4: 21 main() updates | Low (mechanical) | 1 hour |
| WI-5: Screenshot command | Medium | 2–3 hours |
| WI-6: Parser unit tests | Medium | 2–3 hours |
| WI-7: Smoke test scripts | Low | 1 hour |
| WI-8: Documentation | Low | 1–2 hours |
| **Total (excluding screenshot)** | | **~13–18 hours** |

---

## Verification Criteria

After implementation, the following should work:

```powershell
# 1. Build succeeds
./scripts/build.ps1

# 2. Parser tests pass
./scripts/build-and-test.ps1 -Filter "InputScript*"

# 3. Example runs with input script and exits cleanly
./build_ninja/examples/vde_physics_demo.exe --input-script smoketests/scripts/smoke_test.vdescript
# Exit code: 0
# Console shows: [VDE:InputScript] Loaded N commands from smoketests/scripts/smoke_test.vdescript

# 4. Environment variable works (lowest priority)
$env:VDE_INPUT_SCRIPT = "smoketests/scripts/smoke_test.vdescript"
./build_ninja/examples/vde_physics_demo.exe
# Exit code: 0

# 5. CLI arg overrides env var
$env:VDE_INPUT_SCRIPT = "smoketests/scripts/other.vdescript"
./build_ninja/examples/vde_physics_demo.exe --input-script smoketests/scripts/smoke_test.vdescript
# Uses smoke_test.vdescript, not other.vdescript

# 6. All examples pass smoke test
./scripts/tmp_run_examples_check.ps1
# All pass

# 7. Invalid script reports error gracefully
./build_ninja/examples/vde_physics_demo.exe --input-script nonexistent.vdescript
# Logs: [VDE:InputScript] Error: unable to open script file 'nonexistent.vdescript'
# App still runs (no script replay), exits normally

# 8. Parse error is reported with line number
# Given a script with a bad command on line 5:
# Logs: [VDE:InputScript] Error at line 5: unrecognized command 'foobar'
# Script not loaded; app runs without replay

# 9. Modifier keys work
# Script: press ctrl+S
# Console shows: [VDE:InputScript] press ctrl+S

# 10. Mouse input works
# Script: click 400 300
# Console shows: [VDE:InputScript] click 400 300

# 11. Loops work
# Script with: label test / press A / wait 100 / loop test 3
# Console shows: [VDE:InputScript] loop test iteration 1/3 ... 2/3 ... 3/3
```

---

## Dependencies

- **stb_image_write** (for WI-5 screenshot) — already in `third_party/`
- **GoogleTest** — already configured for unit tests
- **No new third-party dependencies** for the core input replay system

---

## Open Questions

1. **Should `press` dispatch to ImGui as well?** If debug UI is active, scripted keys may need to go through GLFW's input path rather than directly calling `InputHandler`. This needs investigation during WI-2. If ImGui is consuming input, scripted presses routed only to `InputHandler` may be silently ignored by ImGui overlays. Mouse commands have the same concern — `click` may need to feed through GLFW's cursor callback for ImGui hit-testing to work.

2. **Timing precision:** Waits use frame delta time. On a 60fps display, minimum granularity is ~16ms. This is sufficient for smoke testing. Sub-frame interpolation is not planned — if finer control becomes necessary, it can be proposed as a Phase 2 enhancement.

3. **Modifier state leaking:** If a script uses `keydown shift+W` and the script ends or errors before `keyup shift+W`, the modifier state may leak. Consider resetting all pressed keys/modifiers when the script finishes or encounters an error.

4. **Mouse coordinate scaling:** On high-DPI displays, GLFW content scale may differ from window coordinates. Decide whether script coordinates are in screen pixels or content-scale-adjusted pixels. Recommendation: use raw window coordinates (matching GLFW callbacks) and document this.

5. **Infinite loop safety:** `loop NAME 0` runs forever. Consider adding a maximum iteration guard (e.g., 100,000) to prevent accidental hangs, with an override flag for intentionally infinite scripts.
