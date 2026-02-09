# Dear ImGui Integration Demo

This example demonstrates how to integrate [Dear ImGui](https://github.com/ocornut/imgui) with the VDE engine as an **application-side overlay**. ImGui is NOT part of the VDE engine core; it is fetched and linked only by this example.

## Architecture

### Why Application-Side?

ImGui is kept **outside** the VDE engine for several reasons:

1. **Optional dependency** - Not every VDE application needs debug UI
2. **Minimal engine footprint** - VDE focuses on rendering primitives, not UI frameworks
3. **Render pass compatibility** - ImGui renders into VDE's existing render pass as an overlay
4. **Flexibility** - Applications choose their own UI solution (ImGui, Nuklear, custom, etc.)

### How It Works

```
VulkanContext::drawFrame()
  ├── beginRenderPass (CLEAR)
  ├── Scene::render()           ← VDE entities drawn here
  ├── Game::onRender()          ← ImGui overlay rendered here
  └── endRenderPass
```

The example hooks into `Game::onRender()`, which is called **inside the active render pass** after all scene entities have been drawn. ImGui's Vulkan backend records draw commands into the same command buffer, layering UI elements on top of the 3D scene.

## Integration Pattern

### 1. CMake Setup

The [CMakeLists.txt](CMakeLists.txt) fetches ImGui via `FetchContent` and builds it as a static library with GLFW + Vulkan backends:

```cmake
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.91.8-docking
)

add_library(imgui_backend STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    # ... other ImGui sources
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
)
```

### 2. Game Class Integration

Your `Game` subclass manages ImGui's lifecycle:

```cpp
class MyGame : public vde::Game {
public:
    void onStart() override {
        // Initialize ImGui with VDE's Vulkan context
        ImGui::CreateContext();
        ImGui_ImplGlfw_InitForVulkan(getWindow()->getHandle(), true);
        
        ImGui_ImplVulkan_InitInfo info{};
        info.Instance = getVulkanContext()->getInstance();
        info.PhysicalDevice = getVulkanContext()->getPhysicalDevice();
        info.Device = getVulkanContext()->getDevice();
        // ... (see main.cpp for full init)
        ImGui_ImplVulkan_Init(&info);
    }
    
    void onRender() override {
        // This is called INSIDE the render pass
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        // Build your UI
        ImGui::Begin("My Window");
        ImGui::Text("FPS: %.1f", getFPS());
        ImGui::End();
        
        // Record ImGui draw commands
        ImGui::Render();
        VkCommandBuffer cmd = getVulkanContext()->getCurrentCommandBuffer();
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
    }
    
    void onShutdown() override {
        vkDeviceWaitIdle(getVulkanContext()->getDevice());
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }
};
```

### 3. VDE Resources Needed

ImGui's Vulkan backend needs these VulkanContext accessors:

| Method | Purpose |
|--------|---------|
| `getInstance()` | Vulkan instance handle |
| `getPhysicalDevice()` | GPU handle |
| `getDevice()` | Logical device |
| `getGraphicsQueue()` | Command submission queue |
| `getGraphicsQueueFamily()` | Queue family index |
| `getRenderPass()` | Compatible render pass for pipeline creation |
| `getCurrentCommandBuffer()` | Active command buffer for draw commands |

All of these are already exposed by `VulkanContext` as of VDE's current API.

## Features Demonstrated

This example shows:

- **Entity inspectors** - Real-time sliders/drag controls for position, rotation, scale, and color
- **Lighting editor** - Adjust ambient color and directional light intensity
- **Stats window** - FPS, frame count, delta time, entity count
- **ImGui Demo Window** - Full ImGui widget showcase (toggleable)
- **Input coexistence** - ImGui captures mouse while VDE's InputHandler still receives keyboard events

## Building & Running

```bash
# Configure (if not already done)
cmake -S . -B build

# Build the ImGui demo
cmake --build build --target vde_imgui_demo --config Debug

# Run
./build/examples/imgui_demo/Debug/vde_imgui_demo.exe   # Windows
./build/examples/imgui_demo/vde_imgui_demo             # Linux/Mac
```

## Controls

- **Mouse** - Interact with ImGui panels (drag sliders, click buttons, etc.)
- **F11** - Toggle fullscreen
- **F** - Mark test as failed (reports incorrect visuals)
- **ESC** - Exit early
- Auto-closes after 60 seconds

## Notes

- **Input handling**: ImGui installs GLFW callbacks via `ImGui_ImplGlfw_InitForVulkan(..., install_callbacks=true)`. These chain with existing callbacks, so VDE's `InputHandler` continues to work.
- **Descriptor pool**: ImGui gets its own `VkDescriptorPool` separate from VDE's internal pools.
- **Render pass**: ImGui uses VDE's existing color-only render pass (no depth attachment needed).
- **Performance**: ImGui adds minimal overhead (~0.1ms for typical debug UIs).

## Extending

To add ImGui to your own VDE application:

1. Copy the CMake setup from [CMakeLists.txt](CMakeLists.txt)
2. Copy the init/render/cleanup pattern from [main.cpp](main.cpp)
3. Customize the UI in your `onRender()` override

For production, you might want to create a reusable `vde::ImGuiOverlay` helper class that wraps this pattern.
