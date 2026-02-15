---
name: scripted-input
description: Guide for using scripted input automation in VDE games, examples, and tools. Use this when adding test automation, smoke tests, or automated demos.
---

# VDE Scripted Input

This skill describes how to use VDE's scripted input replay system for automation, testing, and demos.

## When to use this skill

- Adding automated smoke tests to examples or games
- Creating demo scripts that showcase features
- Writing CI/CD test automation
- Implementing automated regression testing
- Creating reproducible bug reports with input replay

## Overview

VDE's input script system allows you to record and replay keyboard, mouse, and timing commands to automate interactions with your game or tool. Scripts use a simple text-based format with `.vdescript` extension.

## Quick Start

### Running with an Input Script

There are three ways to provide an input script:

1. **Command-line argument (highest priority):**
   ```bash
   vde_physics_demo.exe --input-script scripts/input/smoke_test.vdescript
   ```

2. **Environment variable (fallback):**
   ```bash
   set VDE_INPUT_SCRIPT=scripts/input/smoke_test.vdescript
   vde_physics_demo.exe
   ```

3. **API call (in code):**
   ```cpp
   game.setInputScriptFile("scripts/input/smoke_test.vdescript");
   ```

**Priority:** API call > CLI arg > environment variable

### Creating a Simple Script

Create a file with `.vdescript` extension:

```vdescript
# smoke_test.vdescript - Quick smoke test
wait startup          # Wait for first frame to render
wait 100              # Wait 100ms
press SPACE           # Press and release spacebar
wait 0.5s             # Wait 500ms (supports 's' suffix)
click 640 360         # Left-click at position
wait 2s
exit                  # Quit the application
```

## Script Command Reference

### Timing Commands

| Command | Description | Example |
|---------|-------------|---------|
| `wait startup` | Wait for first frame to render | `wait startup` |
| `wait <ms>` | Wait N milliseconds | `wait 500` |
| `wait <N>s` | Wait N seconds (with 's' suffix) | `wait 2.5s` |

### Keyboard Commands

| Command | Description | Example |
|---------|-------------|---------|
| `press <key>` | Press and release key | `press A` |
| `press <mod>+<key>` | Press with modifiers | `press ctrl+S` |
| `keydown <key>` | Hold key down | `keydown W` |
| `keyup <key>` | Release key | `keyup W` |

**Modifiers:** `ctrl`, `shift`, `alt` (combine with `+`)

**Supported keys:** A-Z, 0-9, SPACE, ENTER, ESC, TAB, arrow keys, function keys (F1-F12), numpad keys, punctuation. See `InputScript.cpp` for full key name map.

### Mouse Commands

| Command | Description | Example |
|---------|-------------|---------|
| `click <x> <y>` | Left-click at position | `click 400 300` |
| `click right <x> <y>` | Right-click at position | `click right 400 300` |
| `mousedown <x> <y>` | Press left button | `mousedown 100 100` |
| `mouseup <x> <y>` | Release left button | `mouseup 200 200` |
| `mousemove <x> <y>` | Move cursor to position | `mousemove 640 360` |
| `scroll <x> <y> <delta>` | Scroll wheel at position | `scroll 400 300 -3` |

**Coordinates:** (0,0) is top-left; window-relative pixel coordinates.

### Control Flow

| Command | Description | Example |
|---------|-------------|---------|
| `label <name>` | Define a jump target | `label loop_start` |
| `loop <label> <count>` | Jump back to label N times | `loop loop_start 5` |
| `exit` (or `quit`) | Quit the application | `exit` |

### Screenshot (Experimental)

| Command | Description | Example |
|---------|-------------|---------|
| `screenshot <path>` | Capture frame to PNG (with frame number) | `screenshot output.png` |

**Note:** Screenshot automatically injects frame number: `output.png` → `output_frame_0042.png`

**Current Status:** Filename handling implemented; pixel capture requires Vulkan readback (see TODO in `Game.cpp`).

## Comments and Formatting

- Lines starting with `#` or `//` are comments
- Empty lines are ignored
- Commands are case-insensitive (`PRESS` = `press`)
- Whitespace between tokens is flexible

## Example Scripts

### Smoke Test (Fast Exit)

```vdescript
# smoke_quick.vdescript - Minimal smoke test for CI
wait startup
wait 100
exit
```

### Interactive Demo

```vdescript
# demo_physics.vdescript - Automated physics demo
wait startup
wait 1s

# Spawn some boxes
label spawn_loop
press SPACE
wait 0.3s
loop spawn_loop 5

# Move camera around
wait 1s
keydown RIGHT
wait 2s
keyup RIGHT

# Finish
wait 3s
exit
```

### Automated UI Test

```vdescript
# test_menu.vdescript - Menu navigation test
wait startup

# Click "Start Game" button
click 640 400
wait 0.5s

# Click "Settings" button
click 640 500
wait 0.5s

# Press ESC to go back
press ESC
wait 0.5s

# Exit
exit
```

## Integration Patterns

### Adding to an Example

All VDE examples now support `--input-script` automatically via `runExample()`:

```cpp
int main(int argc, char** argv) {
    MyGameClass game;
    return vde::examples::runExample(game, "My Game", 1280, 720, argc, argv);
}
```

No additional code needed - `runExample` handles CLI parsing and script setup.

### Adding to a Tool

Tools support scripted input via `runTool()`:

```cpp
int main(int argc, char** argv) {
    MyToolClass tool(ToolMode::INTERACTIVE);
    return vde::tools::runTool(tool, "My Tool", width, height, argc, argv);
}
```

### Manual Integration

For custom integration (advanced):

```cpp
#include <vde/api/GameAPI.h>  // Includes InputScript.h

int main(int argc, char** argv) {
    MyGame game;
    
    // Configure script from CLI args
    vde::configureInputScriptFromArgs(game, argc, argv);
    
    // Or set manually
    game.setInputScriptFile("scripts/input/my_script.vdescript");
    
    // Initialize and run
    vde::GameSettings settings;
    settings.gameName = "My Game";
    game.initialize(settings);
    game.run();
    
    return game.getExitCode();
}
```

**Important:** Call `configureInputScriptFromArgs()` **before** changing the working directory (if applicable), so relative script paths resolve correctly.

## Smoke Testing Workflow

### Creating Smoke Tests

1. **Create a smoke script** in `scripts/input/<example_name>.vdescript`:
   ```vdescript
   wait startup
   wait 100
   exit
   ```

2. **Run the test:**
   ```bash
   build_ninja\examples\vde_physics_demo.exe --input-script scripts/input/smoke_physics_demo.vdescript
   ```

3. **Check exit code:** Exit code 0 = success.

### Batch Smoke Testing

Use the smoke test runner (if available):

```powershell
.\scripts\tmp_run_examples_check.ps1
```

This script runs all examples with their smoke scripts and reports results.

## Best Practices

1. **Start every script with `wait startup`** - Ensures the first frame has rendered before sending input.

2. **Use `wait` commands liberally** - Give the application time to process events and update state. Physics and animations need time.

3. **Use relative paths for scripts** - Scripts are resolved relative to the current working directory when the app is launched.

4. **Test scripts interactively first** - Run with a visible window to verify timing and coordinates before CI integration.

5. **Keep smoke tests minimal** - For CI, use `wait startup; wait 100; exit` for fast iteration.

6. **Use loops sparingly** - Long loops can make scripts hard to debug; prefer explicit commands.

7. **Add comments** - Explain what each section of the script does, especially for complex interactions.

8. **Version control your scripts** - Commit `.vdescript` files alongside your code.

## Debugging Tips

### Script Not Loading

- Check the console for `[VDE:InputScript]` messages
- Verify the script path is correct (absolute or relative to CWD)
- Ensure the file has `.vdescript` extension (optional but conventional)

### Commands Not Executing

- Add `wait startup` at the beginning
- Increase wait times - the application might need more time to initialize
- Check console for `[VDE:InputScript]` error messages

### Wrong Key Codes

- Verify key names in `InputScript.cpp::getKeyNameMap()`
- Use uppercase key names: `A` not `a`
- Use named keys for special keys: `SPACE`, `ENTER`, `ESC`

### Mouse Clicks Not Working

- Verify coordinates are within the window bounds
- Check that the window is focused (scripts don't steal focus on launch)
- Use `mousemove` before `click` to ensure cursor is in position

## Implementation Notes

### How It Works

1. **Script Loading:** On `Game::initialize()`, checks API, CLI, then env var for script path
2. **Parsing:** `parseInputScript()` reads the file and builds a command list
3. **Execution:** Each frame, `Game::processInputScript()` dispatches pending commands to the active `InputHandler`
4. **Timing:** `WaitMs` commands accumulate delta time until the wait duration is reached
5. **Exit:** The `exit` command sets a flag; `Game::run()` checks it and quits the loop

### Frame Timing

- Frame counter starts at 0 and increments each frame
- `wait startup` completes after the first frame renders
- Screenshot frame numbers correspond to the frame when the command executes

### Input Dispatch

Scripts dispatch through the normal input system (`InputHandler` methods), so script input behaves identically to real user input.

## API Reference

See [include/vde/api/InputScript.h](../../../include/vde/api/InputScript.h) for:
- `InputCommandType` enum (all command types)
- `ScriptCommand` struct (parsed command representation)
- `parseInputScript()` (file parser)
- `configureInputScriptFromArgs()` (CLI helper)

See [src/api/Game.cpp](../../../src/api/Game.cpp) for:
- `Game::setInputScriptFile()`
- `Game::loadInputScript()`
- `Game::processInputScript()` (command dispatcher)

## Future Enhancements

- **Screenshot pixel capture** - Currently logs the path; needs Vulkan swapchain readback
- **Visual regression testing** - Compare screenshots against golden images
- **Record mode** - Capture live input and save to `.vdescript` file
- **Conditional commands** - `if`/`then` based on game state
- **Variable substitution** - `${WIDTH}`, `${HEIGHT}` for dynamic coordinates

## Related Documentation

- [API-DOC.md](../../../API-DOC.md) - Full VDE API reference
- [SCRIPTED_INPUT_IMPLEMENTATION_PLAN.md](../../../docs/SCRIPTED_INPUT_IMPLEMENTATION_PLAN.md) - Implementation details and design
- [examples/](../../../examples/) - Example applications with smoke scripts
- [scripts/input/](../../../scripts/input/) - Example input scripts
