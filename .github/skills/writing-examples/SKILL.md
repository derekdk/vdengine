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

Always build and run the example to verify it works correctly and follows the expected pattern.


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
All examples should have a smoke test script for automated verification. See the **scripted-input** skill for details on creating `.vdescript` files.

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

**Add to smoke test runner:**
Edit `scripts/smoke-test.ps1` to map your example to its smoke script:
```powershell
$smokeScriptMap = @{
  'vde_my_demo.exe' = 'smoke_my_demo.vdescript'
}
```

**Run smoke tests:**
```bash
# Run all example smoke tests
.\scripts\smoke-test.ps1

# VS Code task: "scripts: smoke-test"
```

### 1. Include the Base Header

```cpp
#include "../ExampleBase.h"
```

### 2. Input Handler (Inherit from BaseExampleInputHandler)

```cpp
class DemoInputHandler : public vde::examples::BaseExampleInputHandler {
public:
    void onKeyPress(int key) override {
        // Call base class first for ESC and F keys
        BaseExampleInputHandler::onKeyPress(key);
        
        // Add your custom keys here
        if (key == vde::KEY_SPACE) {
            m_spacePressed = true;
        }
    }
    
    bool isSpacePressed() {
        bool val = m_spacePressed;
        m_spacePressed = false;
        return val;
    }

private:
    bool m_spacePressed = false;
};
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
        auto* input = dynamic_cast<DemoInputHandler*>(getInputHandler());
        if (input) {
            // Handle your custom keys
        }
    }

protected:
    // Override these to customize the header output
    std::string getExampleName() const override {
        return "Feature Name";
    }
    
    std::vector<std::string> getFeatures() const override {
        return {
            "Feature 1 description",
            "Feature 2 description"
        };
    }
    
    std::vector<std::string> getExpectedVisuals() const override {
        return {
            "Visual element 1",
            "Visual element 2"
        };
    }
    
    std::vector<std::string> getControls() const override {
        return {
            "SPACE - Toggle something"
        };
    }
};
```

### 4. Game Class (Use BaseExampleGame Template)

```cpp
class DemoGame : public vde::examples::BaseExampleGame<DemoInputHandler, DemoScene> {
public:
    DemoGame() = default;
    
    // Optionally override onStart() if you need custom initialization
    // But make sure to call the base class version!
    void onStart() override {
        BaseExampleGame::onStart();
        // Your custom initialization...
    }
};
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

Add the example to `examples/CMakeLists.txt`:

```cmake
# [Feature] demo example demonstrating [description]
add_executable(vde_[feature]_demo
    [feature]_demo/main.cpp
)

target_link_libraries(vde_[feature]_demo PRIVATE vde)

# Copy shader files for [feature]_demo example
add_custom_command(TARGET vde_[feature]_demo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/shaders
    $<TARGET_FILE_DIR:vde_[feature]_demo>/shaders
    COMMENT "Copying shader files..."
)
```

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
