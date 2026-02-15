---
name: writing-tools
description: Guide for writing asset creation tools in VDE. Use this when creating interactive or scriptable tools for content creation.
---

# Writing Asset Creation Tools

This skill describes the standard pattern for creating asset creation tools in VDE. Tools are more complex than examples and support both interactive GUI mode and scriptable batch mode for automation.

## When to use this skill

- Creating an asset creation tool (geometry editor, material editor, etc.)
- Building interactive tools with REPL interfaces
- Implementing batch/automation capabilities via script files
- Creating content pipelines for VDE games

## Tools vs Examples

**Examples:**
- Simple demonstrations of engine features
- Auto-terminating with test pass/fail
- Located in `examples/` directory
- Single file implementations where possible
- Focus on showcasing specific APIs

**Tools:**
- Asset creation and content pipelines
- Persistent, does not auto-terminate
- Located in `tools/` directory  
- Multi-file implementations organized by concern
- Support both interactive and scriptable modes
- Export assets to standard formats

## Standard Tool Structure

All VDE tools follow this pattern:

### Directory Structure

```
tools/
  ToolBase.h                    # Shared base classes (like ExampleBase.h)
  my_tool/
    CMakeLists.txt              # Tool-specific build config
    main.cpp                    # Entry point with mode selection
    MyToolScene.h               # Scene header
    MyToolScene.cpp             # Scene implementation
    [Additional component files as needed]
    README.md                   # Tool documentation
```

### File Organization

**Separate by Concern:**
- `main.cpp` - Entry point, command-line parsing, mode selection
- `[Tool]Scene.h/cpp` - Scene implementation and command execution
- Additional headers for data structures (e.g., `GeometryObject.h`)
- Keep files focused on specific responsibilities

**Example from geometry_repl:**
```
tools/geometry_repl/
  main.cpp              # Entry point, script handling
  GeometryReplScene.h   # Scene declaration
  GeometryReplScene.cpp # Scene implementation, command handlers
  GeometryObject.h      # Geometry data structure
  CMakeLists.txt
  README.md
```

## Using ToolBase.h

All tools use `ToolBase.h` which provides:

### 1. BaseToolInputHandler

```cpp
#include "../ToolBase.h"

// Use as-is or extend
class MyToolInputHandler : public vde::tools::BaseToolInputHandler {
  public:
    void onKeyPress(int key) override {
        BaseToolInputHandler::onKeyPress(key);  // ESC, F1, F11
        // Add custom keys here
    }
};
```

**Built-in features:**
- ESC - Exit tool
- F1 - Toggle UI visibility
- F11 - Toggle fullscreen
- Mouse rotation and zoom for OrbitCamera

### 2. BaseToolScene

```cpp
class MyToolScene : public vde::tools::BaseToolScene {
  public:
    explicit MyToolScene(ToolMode mode = ToolMode::INTERACTIVE);

    void onEnter() override {
        // Set up camera, lighting, scene objects
        setCamera(new vde::OrbitCamera(...));
        
        addConsoleMessage("Welcome to My Tool");
    }

    // REQUIRED: Implement command execution
    void executeCommand(const std::string& cmdLine) override {
        std::istringstream iss(cmdLine);
        std::string cmd;
        iss >> cmd;

        if (cmd == "help") {
            cmdHelp();
        } else if (cmd == "create") {
            cmdCreate(iss);
        }
        // ... more commands
    }

    // REQUIRED: Tool metadata
    std::string getToolName() const override {
        return "My Tool";
    }

    std::string getToolDescription() const override {
        return "Brief description of what the tool does";
    }

    // OPTIONAL: Custom ImGui UI
    void drawDebugUI() override {
        // Draw your custom UI panels here
        // Use getConsoleLog(), addConsoleMessage(), etc.
    }

  private:
    void cmdHelp() {
        addConsoleMessage("COMMANDS:");
        addConsoleMessage("  help - Show this help");
        // ... more commands
    }

    void cmdCreate(std::istringstream& iss) {
        // Parse arguments and execute command
    }
};
```

**Key methods:**
- `executeCommand(cmdLine)` - Handle REPL commands
- `addConsoleMessage(msg)` - Log to console
- `getConsoleLog()` - Access message history
- `processScriptFile(filename)` - Execute script file

### 3. BaseToolGame

```cpp
class MyToolGame : public vde::tools::BaseToolGame<MyToolInputHandler, MyToolScene> {
  public:
    MyToolGame(ToolMode mode) : BaseToolGame(mode) {}
};
```

**Features:**
- Automatic ImGui initialization (interactive mode)
- Scene lifecycle management
- Camera controls integration
- Exit code handling

**Note:** `runTool(...)` automatically sets the process working directory to the executable directory on startup. This keeps relative asset/shader paths working when tools are launched from outside their output folder.

## Scriptability

**Every tool must support script mode** for batch processing and automation.

### Main Entry Point Pattern

```cpp
int main(int argc, char** argv) {
    ToolMode mode = ToolMode::INTERACTIVE;
    std::string scriptFile;

    // Parse arguments
    if (argc > 1) {
        scriptFile = argv[1];
        mode = ToolMode::SCRIPT;
    }

    MyToolGame tool(mode);

    if (mode == ToolMode::INTERACTIVE) {
        // Full GUI mode
        float dpiScale = vde::Window::getPrimaryMonitorDPIScale();
        uint32_t width = static_cast<uint32_t>(1400 * dpiScale);
        uint32_t height = static_cast<uint32_t>(800 * dpiScale);
        
        // Note: Passing argc/argv enables --input-script support for automated testing
        return runTool(tool, "My Tool Name", width, height, argc, argv);
    } else {
        // Script mode - minimal window, no validation
        vde::GameSettings settings;
        settings.windowWidth = 800;
        settings.windowHeight = 600;
        settings.windowTitle = "My Tool (Script Mode)";
        settings.enableValidation = false;

        if (!tool.initialize(settings)) {
            std::cerr << "Failed to initialize tool\n";
            return 1;
        }

        tool.run();
        return tool.getExitCode();
    }
}
```

### Tool Game Script Support

```cpp
class MyToolGame : public BaseToolGame<MyToolInputHandler, MyToolScene> {
  public:
    MyToolGame(ToolMode mode, const std::string& scriptFile = "")
        : BaseToolGame(mode), m_scriptFile(scriptFile) {}

    void onStart() override {
        BaseToolGame::onStart();

        // Execute script in batch mode
        if (m_toolMode == ToolMode::SCRIPT && !m_scriptFile.empty()) {
            auto* scene = getToolScene();
            if (scene && !scene->processScriptFile(m_scriptFile)) {
                m_exitCode = 1;
            }
            quit();  // Exit after script execution
        }
    }

  private:
    std::string m_scriptFile;
};
```

### Script File Format

```
# My Tool Script
# Lines beginning with # are comments
# Empty lines are ignored

# Create objects
create triangle polygon
addpoint triangle 0 0 0
addpoint triangle 1 0 0
addpoint triangle 0.5 1 0

# Set properties
setcolor triangle 1.0 0.0 0.0

# Export assets
export triangle output/triangle.obj
```

### Input Script Automation

In addition to tool-specific script commands, tools support VDE's **scripted-input** system for UI automation and testing. This is orthogonal to the tool's own command script mode:

- **Tool scripts** (above) define _what objects to create_ (geometry, materials, etc.)
- **Input scripts** (`.vdescript` files) define _how to interact with the UI_ (clicks, keys, etc.)

Example use case: Automate testing of the tool's UI by clicking buttons and verifying console output.

See the **scripted-input** skill for details on `--input-script` support and creating smoke tests.

## CMakeLists.txt Template

```cmake
# My Tool
# Brief description

add_executable(vde_my_tool
    main.cpp
    MyToolScene.cpp
    MyToolScene.h
    # Add more source files
)

target_link_libraries(vde_my_tool PRIVATE
    vde
    imgui_backend  # Always link ImGui for tools
)

add_dependencies(vde_my_tool copy_tool_shaders)

# Copy shaders to output directory
add_custom_command(TARGET vde_my_tool POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/shaders
        $<TARGET_FILE_DIR:vde_my_tool>/shaders
    COMMENT "Copying shaders to tool directory..."
)

# If tool needs additional assets:
# add_custom_command(TARGET vde_my_tool POST_BUILD
#     COMMAND ${CMAKE_COMMAND} -E copy_directory
#         ${CMAKE_CURRENT_SOURCE_DIR}/assets
#         $<TARGET_FILE_DIR:vde_my_tool>/assets
# )
```

## Command Design Patterns

### Simple Commands (no arguments)

```cpp
void cmdHelp() {
    addConsoleMessage("Available commands:");
    addConsoleMessage("  command1 - Description");
}
```

### Commands with Arguments

```cpp
void cmdCreate(std::istringstream& iss) {
    std::string name, type;
    iss >> name >> type;

    if (name.empty() || type.empty()) {
        addConsoleMessage("ERROR: Usage: create <name> <type>");
        return;
    }

    // Validate and execute
    if (type != "polygon" && type != "line") {
        addConsoleMessage("ERROR: Invalid type. Use: polygon, line");
        return;
    }

    // Do the work
    m_objects[name] = createObject(type);
    addConsoleMessage("Created " + type + " '" + name + "'");
}
```

### Commands with File I/O

```cpp
void cmdExport(std::istringstream& iss) {
    std::string name, filename;
    iss >> name >> filename;

    if (name.empty() || filename.empty()) {
        addConsoleMessage("ERROR: Usage: export <name> <filename>");
        return;
    }

    auto it = m_objects.find(name);
    if (it == m_objects.end()) {
        addConsoleMessage("ERROR: Object '" + name + "' not found");
        return;
    }

    if (it->second.exportToFile(filename)) {
        addConsoleMessage("Exported '" + name + "' to " + filename);
    } else {
        addConsoleMessage("ERROR: Export failed");
    }
}
```

## ImGui UI Patterns

### Console Window

```cpp
void drawDebugUI() override {
    float scale = getGame()->getDPIScale();

    // Console
    ImGui::SetNextWindowPos(ImVec2(10 * scale, 10 * scale), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(600 * scale, 400 * scale), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Tool Console")) {
        // Output area
        ImVec2 consoleSize = ImVec2(0, -ImGui::GetFrameHeightWithSpacing() * 2);
        if (ImGui::BeginChild("Output", consoleSize, true)) {
            for (const auto& msg : getConsoleLog()) {
                ImGui::TextWrapped("%s", msg.c_str());
            }
            if (shouldScrollToBottom()) {
                ImGui::SetScrollHereY(1.0f);
            }
        }
        ImGui::EndChild();

        // Input area
        ImGui::Separator();
        ImGui::Text(">");
        ImGui::SameLine();

        static char buffer[256] = {0};
        if (ImGui::InputText("##input", buffer, sizeof(buffer),
                            ImGuiInputTextFlags_EnterReturnsTrue)) {
            std::string cmd(buffer);
            if (!cmd.empty()) {
                addConsoleMessage("> " + cmd);
                executeCommand(cmd);
                buffer[0] = '\0';
            }
            ImGui::SetKeyboardFocusHere(-1);
        }
    }
    ImGui::End();
}
```

### Inspector Panel

```cpp
ImGui::SetNextWindowPos(ImVec2(620 * scale, 10 * scale), ImGuiCond_FirstUseEver);
ImGui::SetNextWindowSize(ImVec2(300 * scale, 400 * scale), ImGuiCond_FirstUseEver);

if (ImGui::Begin("Inspector")) {
    for (auto& [name, object] : m_objects) {
        if (ImGui::CollapsingHeader(name.c_str())) {
            ImGui::Indent();
            
            // Properties
            ImGui::Text("Type: %s", object.getTypeName());
            
            // Interactive controls
            if (ImGui::ColorEdit3(("Color##" + name).c_str(), &object.color.x)) {
                object.updateColor();
            }
            
            if (ImGui::Button(("Delete##" + name).c_str())) {
                cmdClear(name);
            }
            
            ImGui::Unindent();
        }
    }
}
ImGui::End();
```

## Testing Tools

### Interactive Mode Testing
1. **Interactive Mode:**
   ```bash
   ./vde_my_tool
   ```
   - Verify UI renders correctly
   - Test all commands via REPL
   - Check mouse camera controls
   - Test F1, F11, ESC keys

### Script Mode Testing
2. **Script Mode:**
   ```bash
   ./vde_my_tool test_script.txt
   ```
   - Verify batch execution works
   - Check output files are created
   - Verify console output is readable
   - Test error handling

### Smoke Testing (Automated Input)
Tools can also use the **scripted-input** system for UI automation and smoke testing. This is separate from tool command scripts:

- **Tool scripts** define _what to create_ (geometry, materials, etc.)
- **Input scripts** (`.vdescript`) define _how to interact with UI_ (clicks, keys)

**Create a smoke test for your tool:**
1. Create `scripts/input/smoke_<tool_name>.vdescript`
2. Test key UI interactions, command execution, etc.
3. Add to smoke test runner (see writing-examples skill)

**Example:**
```vdescript
# smoke_my_tool.vdescript
wait startup
wait 500
press F1          # Toggle UI
wait 500
click 640 360     # Click in viewport
wait 2s
exit
```

**Note:** Input scripts test the UI interaction layer, while tool command scripts test the batch processing layer. Both are important for comprehensive testing.

### Example Scripts
3. **Create Example Scripts:**
   - Provide sample scripts in tool directory or README
   - Cover common use cases
   - Include comments explaining steps

## Documentation

Create a `README.md` for each tool with:

1. **Overview** - What the tool does
2. **Usage** - How to run (interactive and script modes)
3. **Commands** - Full command reference with examples
4. **Controls** - UI controls (mouse, keyboard)
5. **Examples** - Sample workflows and scripts
6. **Output Formats** - File formats and specifications

See `tools/geometry_repl/README.md` for a complete example.

## Best Practices

1. **Always support script mode** - Essential for automation
2. **Validate all input** - Check arguments before processing
3. **Provide clear error messages** - Help users fix problems
4. **Log all actions** - Use `addConsoleMessage()` liberally
5. **Handle file I/O errors** - Check file operations, report failures
6. **Separate concerns** - Use multiple files for complex tools
7. **Document commands** - Include help command with full reference
8. **Scale UI with DPI** - Use `getDPIScale()` for window sizes
9. **Test both modes** - Ensure interactive and script modes work
10. **Version output files** - Include tool name/version in exports

## Common Patterns

### Object Management

```cpp
private:
    std::map<std::string, MyObject> m_objects;

    void cmdCreate(std::istringstream& iss) {
        std::string name;
        iss >> name;
        
        if (m_objects.find(name) != m_objects.end()) {
            addConsoleMessage("ERROR: Object already exists");
            return;
        }
        
        m_objects[name] = MyObject();
        addConsoleMessage("Created object '" + name + "'");
    }

    void cmdDelete(std::istringstream& iss) {
        std::string name;
        iss >> name;
        
        auto it = m_objects.find(name);
        if (it == m_objects.end()) {
            addConsoleMessage("ERROR: Object not found");
            return;
        }
        
        // Clean up scene entities if needed
        if (it->second.entity) {
            removeEntity(it->second.entity->getId());
        }
        
        m_objects.erase(it);
        addConsoleMessage("Deleted '" + name + "'");
    }
```

### Scene Visualization

```cpp
void updateSceneObject(const std::string& name) {
    auto& obj = m_objects[name];
    
    if (!obj.entity) {
        // Create entity
        obj.entity = addEntity<vde::MeshEntity>();
        obj.entity->setName(name);
    }
    
    // Update mesh and properties
    auto mesh = obj.createMesh();
    obj.entity->setMesh(mesh);
    obj.entity->setColor(obj.color);
    obj.entity->setPosition(obj.position);
}
```

## Adding Tools to the Build

In `tools/CMakeLists.txt`:

```cmake
# Add your tool subdirectory
add_subdirectory(my_tool)
```

Tool will automatically build when `VDE_BUILD_TOOLS` is enabled (default ON).

## Example: Complete Minimal Tool

See `tools/geometry_repl/` for a complete, production-ready tool implementation demonstrating all these patterns.
