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

## Example Structure

All VDE examples should follow this standard pattern to enable:
- **Auto-termination** after a configurable duration
- **User verification** via 'F' key to report failures
- **Early exit** via ESC key
- **Console output** describing expected visuals
- **Clear pass/fail reporting** to command line

## Standard Pattern

### 1. Configuration Constants

```cpp
// Configuration - adjust as needed for the example
constexpr float AUTO_TERMINATE_SECONDS = 15.0f;  // Auto-close after this many seconds
```

### 2. Input Handler with Fail/Escape Support

```cpp
class DemoInputHandler : public vde::InputHandler {
public:
    void onKeyPress(int key) override {
        if (key == vde::KEY_ESCAPE) {
            m_escapePressed = true;
        }
        if (key == vde::KEY_F) {
            m_failPressed = true;
        }
    }
    
    bool isEscapePressed() {
        bool val = m_escapePressed;
        m_escapePressed = false;
        return val;
    }
    
    bool isFailPressed() {
        bool val = m_failPressed;
        m_failPressed = false;
        return val;
    }

private:
    bool m_escapePressed = false;
    bool m_failPressed = false;
};
```

### 3. Scene with Verification Logic

```cpp
class DemoScene : public vde::Scene {
public:
    void onEnter() override {
        // Print header and description
        std::cout << "\n========================================" << std::endl;
        std::cout << "  VDE Example: [Feature Name]" << std::endl;
        std::cout << "========================================\n" << std::endl;
        
        // Describe what features are demonstrated
        std::cout << "Features demonstrated:" << std::endl;
        std::cout << "  - Feature 1" << std::endl;
        std::cout << "  - Feature 2" << std::endl;
        
        // Describe what the user should see
        std::cout << "\nYou should see:" << std::endl;
        std::cout << "  - [Description of expected visuals]" << std::endl;
        
        // Show controls
        std::cout << "\nControls:" << std::endl;
        std::cout << "  F     - Fail test (if visuals are incorrect)" << std::endl;
        std::cout << "  ESC   - Exit early" << std::endl;
        std::cout << "  (Auto-closes in " << AUTO_TERMINATE_SECONDS << " seconds)\n" << std::endl;
        
        // Set up scene...
        m_elapsedTime = 0.0f;
    }
    
    void update(float deltaTime) override {
        Scene::update(deltaTime);
        m_elapsedTime += deltaTime;
        
        auto* input = dynamic_cast<DemoInputHandler*>(getInputHandler());
        if (input) {
            // Check for fail key
            if (input->isFailPressed()) {
                std::cerr << "\n========================================" << std::endl;
                std::cerr << "  TEST FAILED: User reported issue" << std::endl;
                std::cerr << "  Expected: [describe expected output]" << std::endl;
                std::cerr << "========================================\n" << std::endl;
                m_testFailed = true;
                if (m_game) m_game->quit();
                return;
            }
            
            // Check for escape key
            if (input->isEscapePressed()) {
                std::cout << "User requested early exit." << std::endl;
                if (m_game) m_game->quit();
                return;
            }
        }
        
        // Auto-terminate after configured time
        if (m_elapsedTime >= AUTO_TERMINATE_SECONDS) {
            std::cout << "\n========================================" << std::endl;
            std::cout << "  TEST PASSED: Demo completed successfully" << std::endl;
            std::cout << "  Duration: " << m_elapsedTime << " seconds" << std::endl;
            std::cout << "========================================\n" << std::endl;
            if (m_game) m_game->quit();
        }
    }
    
    bool didTestFail() const { return m_testFailed; }

private:
    float m_elapsedTime = 0.0f;
    bool m_testFailed = false;
};
```

### 4. Game Class with Exit Code

```cpp
class DemoGame : public vde::Game {
public:
    void onStart() override {
        // Set up input handler
        m_inputHandler = std::make_unique<DemoInputHandler>();
        setInputHandler(m_inputHandler.get());
        
        // Create scene - addScene takes ownership
        auto* scene = new DemoScene();
        m_scenePtr = scene;
        addScene("main", scene);
        setActiveScene("main");
    }
    
    void onShutdown() override {
        if (m_scenePtr && m_scenePtr->didTestFail()) {
            m_exitCode = 1;
        }
    }
    
    int getExitCode() const { return m_exitCode; }

private:
    std::unique_ptr<DemoInputHandler> m_inputHandler;
    DemoScene* m_scenePtr = nullptr;  // Non-owning reference
    int m_exitCode = 0;
};
```

### 5. Main Function

```cpp
int main() {
    DemoGame demo;
    
    vde::GameSettings settings;
    settings.gameName = "VDE [Feature] Demo";
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

1. **Clear visual descriptions**: Tell users exactly what they should see
2. **Reasonable timeout**: 15 seconds is good for most demos; increase for complex animations
3. **Descriptive failure output**: Help users report what went wrong
4. **Exit code 0 on success, 1 on failure**: Enables CI/CD integration
5. **Console output first**: Print instructions before rendering starts
6. **Keep demos focused**: One feature set per example

## Example Reference

See `examples/materials_lighting_demo/main.cpp` for a complete implementation of this pattern.
