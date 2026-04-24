---
name: writing-examples
description: Guide for writing example programs in VDE. Use this when creating demo or example applications.
---

# Writing Example Programs

This skill describes the standard pattern for creating example programs in VDE that support visual verification and automated testing.

## When to use this skill

- Creating a new example to demonstrate a VDE feature
- Writing a visual demo for testing rendering functionality
- Building interactive samples for documentation

If the work is becoming a multi-file playable application rather than a focused demonstration, stop and use `writing-games` instead.

Always build and run the example to verify it works correctly and follows the expected pattern.

## Completion

After implementing or editing an example, follow the `completing-work` skill for mandatory verification before declaring the task complete.


## Creating a New Example — Start Here

**Always use the scaffold script to create a new example.** It generates the correct folder, source file, `vde.toml`, smoke script, and render-verify scripts in one step, and appends the CMakeLists.txt entry automatically:

```powershell
.\scripts\new-example.ps1 -Name my_feature_demo
```

Optional parameters:
- `-Title "My Feature Demo"` — human-readable window title (defaults to prettified name)
- `-Sections entity,input` — canonical smoke-test sections (see `API-DOC.md`)
- `-NoRenderVerify` — skip render-verify script generation

After running the script, open `examples/my_feature_demo/main.cpp` and fill in the `TODO` sections. Everything else (CMakeLists.txt, smoke script, render-verify scripts, vde.toml) is already wired up.

## Example Structure

All VDE examples should follow this standard pattern to enable:
- **Auto-termination** after a configurable duration
- **User verification** via 'F' key to report failures
- **Early exit** via ESC key
- **Console output** describing expected visuals
- **Clear pass/fail reporting** to command line

## Standard Pattern

All examples should use the shared `ExampleBase.h` header which provides base classes and utilities to eliminate code duplication.

**Note:** `runExample(...)` automatically sets the process working directory to the executable directory on startup. This makes relative paths (for example `shaders/...`) resolve correctly even when launching from another folder.

## Testing the Example

### Manual Testing
1. Make sure the example builds and runs correctly.
2. Verify that the console output correctly describes the expected visuals and controls.
3. Run the example and have a user verify the output by pressing 'F' for failure, pressing 'ESC' to exit, or letting it auto-terminate.
4. Check that the exit code is `0` for success and `1` for failure

### Smoke Testing (Automated)
All Game API examples should have a smoke test script and `vde.toml` metadata for automated
verification. Low-level examples that do not use the Game API input-script flow, such as the
triangle example, are the exception and may be excluded from the smoke-test discovery path. See
the **scripted-input** skill for details on creating `.vdescript` files.

**Create a smoke test:**
1. Create a script in `smoketests/scripts/smoke_<example_name>.vdescript`
2. Use `wait startup` to wait for first frame
3. Add interactions specific to your example (key presses, mouse clicks, etc.)
4. Use `wait` commands to let the example run
5. End with `exit` command

**Example smoke test:**
```vdescript
# smoke_materials_demo.vdescript
wait startup
wait 500
press 1            # Switch material
wait 500
press 2            # Switch material
wait 2s
exit
```

**Register smoke test in vde.toml:**
Create a `vde.toml` file in your example's source directory. The vlauncher tool reads this file to discover which smoke scripts to run:

```toml
[smoke]
scripts = ["smoke_my_demo.vdescript"]
sections = ["entity", "input"]
```

Use `sections` to classify which canonical API areas the smoke test directly validates. Keep the
list to 1-3 identifiers, and use the canonical identifier list documented in `API-DOC.md`
under `Canonical Smoke Coverage Sections`. Use `text` for `TextEntity` and font/layout coverage;
reserve `entity` for mesh, sprite, and transform-focused coverage.

For examples with **multiple executables** in the same source folder (e.g., a folder that produces both `vde_my_demo` and `vde_my_demo_variant`), use per-target sections keyed by executable name (without `.exe`):

```toml
[smoke.vde_my_demo]
scripts = ["smoke_my_demo.vdescript"]
sections = ["entity"]

[smoke.vde_my_demo_variant]
scripts = ["smoke_my_demo_variant.vdescript"]
sections = ["physics", "input"]
```

**No extra smoke-test mapping is needed for examples:**
The smoke runner reads example metadata from `vde.toml`. Explicit script maps in `scripts/smoke-test.ps1` are for tools only.

**Run smoke tests:**
```bash
# Run all example smoke tests
.\scripts\smoke-test.ps1

# VS Code task: "scripts: smoke-test"
```

**Completion requirement:**
Do not declare a new or modified example complete after only building it or manually running it once. Before announcing completion, you must build, run unit tests, run smoke tests, and then run a subagent code review on the verified changes. Use the `completing-work` skill as the final gate.

### 1. Include the Base Header

```cpp
#include "../ExampleBase.h"
```

### 2. Input (KeyStateTracker — preferred for keyboard actions)

`KeyStateTracker` replaces per-key boolean flags with named action bindings. Embed it in your `InputHandler` subclass and forward key events to it:

```cpp
class DemoInputHandler : public vde::examples::BaseExampleInputHandler {
public:
    vde::KeyStateTracker keys;

    DemoInputHandler() {
        keys.bindHeld(vde::KEY_LEFT,  "left");
        keys.bindHeld(vde::KEY_RIGHT, "right");
        keys.bindOneShot(vde::KEY_SPACE, "action");
    }

    void onKeyPress(int key) override {
        BaseExampleInputHandler::onKeyPress(key);  // ESC / F pass-through
        keys.handlePress(key);
    }
    void onKeyRelease(int key) override { keys.handleRelease(key); }
};
```

Then query the tracker from the scene's `update()` via the usual `dynamic_cast` pattern:

```cpp
void update(float dt) override {
    BaseExampleScene::update(dt);
    auto* input = dynamic_cast<DemoInputHandler*>(getInputHandler());
    if (!input) return;
    if (input->keys.consume("action")) { /* ... */ }
    if (input->keys.isHeld("left"))    { /* ... */ }
}
```

### 3. Scene (Inherit from BaseExampleScene)

```cpp
class DemoScene : public vde::examples::BaseExampleScene {
public:
    // Constructor: set auto-terminate time (default is 15.0s)
    DemoScene() : BaseExampleScene(15.0f) {}
    
    void onEnter() override {
        // Print standard header (uses the methods below)
        printExampleHeader();
        
        // Set up your scene here...
    }
    
    void update(float deltaTime) override {
        // Call base class first (handles ESC, F, auto-terminate)
        BaseExampleScene::update(deltaTime);
        
        // Your custom update logic here...
    }

protected:
    // Override these to customize the header output
    std::string getExampleName() const override { return "Feature Name"; }
    
    std::vector<std::string> getFeatures() const override {
        return {"Feature 1 description", "Feature 2 description"};
    }
    
    std::vector<std::string> getExpectedVisuals() const override {
        return {"Visual element 1", "Visual element 2"};
    }
    
    std::vector<std::string> getControls() const override {
        return {"SPACE - Toggle something"};
    }
};
```

### 4. Game Class (Use BaseExampleGame Template)

```cpp
// Custom keyboard bindings via KeyStateTracker or manual flags:
class DemoGame : public vde::examples::BaseExampleGame<DemoInputHandler, DemoScene> {};

// No custom keyboard input needed (ESC/F/F11 only):
class DemoGame : public vde::examples::BaseExampleGame<vde::examples::BaseExampleInputHandler, DemoScene> {};
```

### 5. Main Function (Use runExample Helper)

```cpp
int main(int argc, char** argv) {
    DemoGame demo;
    return vde::examples::runExample(demo, "VDE Feature Demo", 1280, 720, argc, argv);
}
```

**Note:** Passing `argc` and `argv` to `runExample` enables:
- `--input-script <path>` CLI argument for automated testing
- Environment variable `VDE_INPUT_SCRIPT` support
- See the **scripted-input** skill for details on creating smoke tests and automated demos

### Alternative: Simple Main (Manual Setup)

```cpp
int main() {
    DemoGame demo;
    
    vde::GameSettings settings;
    settings.gameName = "VDE Feature Demo";
    settings.display.windowWidth = 1280;
    settings.display.windowHeight = 720;
    settings.display.fullscreen = false;
    
    try {
        if (!demo.initialize(settings)) {
            std::cerr << "Failed to initialize demo!" << std::endl;
            return 1;
        }
        
        demo.run();
        return demo.getExitCode();
        
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
}
```

## CMakeLists.txt Entry

The scaffold script (`scripts/new-example.ps1`) appends the CMakeLists.txt entry automatically. If you need to add a multi-file or asset-syncing example by hand, see the `writing-code` skill's CMake section and use existing examples in that file as a template.

## Best Practices

1. **Use ExampleBase.h**: Always inherit from the base classes to maintain consistency
2. **Clear visual descriptions**: Tell users exactly what they should see in `getExpectedVisuals()`
3. **Reasonable timeout**: 15 seconds is good for most demos; pass different value to BaseExampleScene constructor
4. **Descriptive failure output**: The base class handles this, but you can override `getFailureMessage()`
5. **Exit code 0 on success, 1 on failure**: BaseExampleGame handles this automatically
6. **Console output first**: Call `printExampleHeader()` at the start of `onEnter()`
7. **Keep demos focused**: One feature set per example
8. **Call base class methods**: Always call `BaseExampleScene::update(deltaTime)` and `BaseExampleInputHandler::onKeyPress(key)`

## Benefits of Using ExampleBase.h

- **No code duplication**: Common testing logic is centralized
- **Consistent output**: All examples have the same look and feel
- **Easy to add examples**: Less boilerplate code to write
- **Easy to maintain**: Changes to the testing pattern only need to be made once
- **Type safety**: Template-based BaseExampleGame ensures correct types

## Example Reference

See `examples/materials_lighting_demo/main.cpp` for a complete implementation of this pattern.
