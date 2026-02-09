---
name: imgui-integration
description: Guide for integrating Dear ImGui with VDE applications. Use this when adding debug UI, tools, or overlay interfaces to VDE games and examples.
---

# ImGui Integration with VDE

This skill provides complete guidance for integrating [Dear ImGui](https://github.com/ocornut/imgui) into VDE-based applications. ImGui is a popular immediate-mode GUI library perfect for debug overlays, in-game tools, and development interfaces.

## When to use this skill

- Adding debug UI to a VDE game or example
- Creating in-game development tools or editors
- Building inspector panels for entities, scenes, or engine stats
- Need overlay UI that doesn't require complex UI framework setup
- Want real-time property editing for rapid prototyping

## Core Principles

1. **Application-side integration** - ImGui is NOT part of VDE core; it's fetched and linked by your application
2. **Render pass overlay** - ImGui renders into VDE's existing render pass after scene entities
3. **Minimal overhead** - ImGui adds ~0.1ms for typical debug UIs
4. **Input coexistence** - ImGui and VDE's InputHandler work alongside each other
5. **Separate resources** - ImGui gets its own descriptor pool, independent of VDE's internals

## Architecture Overview

### Render Flow

```
VulkanContext::drawFrame()
  ├── beginRenderPass (CLEAR)
  ├── Scene::render()           ← VDE entities drawn first
  ├── Game::onRender()          ← ImGui overlay drawn here ✨
  └── endRenderPass
```

ImGui renders **inside** the same render pass as VDE entities, recording draw commands into the active command buffer. This creates a seamless overlay without additional render passes or framebuffers.

### Required VDE Resources

ImGui's Vulkan backend needs these `VulkanContext` accessors (all already exposed):

| Method | Purpose |
|--------|---------|
| `getInstance()` | Vulkan instance handle |
| `getPhysicalDevice()` | GPU handle for pipeline creation |
| `getDevice()` | Logical device for resource creation |
| `getGraphicsQueue()` | Command submission queue |
| `getGraphicsQueueFamily()` | Queue family index |
| `getRenderPass()` | Compatible render pass for ImGui pipeline |
| `getCurrentCommandBuffer()` | Active command buffer during rendering |

No VDE API modifications are needed - everything ImGui requires is already available.

## Integration Steps

### Step 1: CMake Setup

Create or modify your example's `CMakeLists.txt` to fetch ImGui and build its backends:

```cmake
include(FetchContent)

# Fetch Dear ImGui (docking branch recommended for multi-window support)
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.91.8-docking
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(imgui)

# Build ImGui as a static library with GLFW + Vulkan backends
add_library(imgui_backend STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
)

target_include_directories(imgui_backend PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

target_link_libraries(imgui_backend PUBLIC
    Vulkan::Vulkan
    glfw
)

# Your example executable
add_executable(my_example
    main.cpp
)

target_link_libraries(my_example PRIVATE
    vde
    imgui_backend  # Link ImGui
)
```

**Notes:**
- Use `v1.91.8-docking` or later for docking/viewport features
- `imgui_backend` library can be reused across multiple examples
- ImGui is kept separate from the `vde` library itself

### Step 2: Game Class Structure

Create a `Game` subclass that manages ImGui's lifecycle:

```cpp
#include <vde/api/GameAPI.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

class MyGame : public vde::Game {
public:
    MyGame() = default;
    ~MyGame() override {
        cleanupImGui();
    }
    
    void onStart() override {
        // Initialize VDE scenes/entities first
        setupScenes();
        
        // Then initialize ImGui
        initImGui();
    }
    
    void onRender() override {
        // This is called INSIDE the active render pass
        renderImGui();
    }
    
    void onShutdown() override {
        // Wait for GPU before cleanup
        if (getVulkanContext()) {
            vkDeviceWaitIdle(getVulkanContext()->getDevice());
        }
        cleanupImGui();
    }

private:
    VkDescriptorPool m_imguiPool = VK_NULL_HANDLE;
    bool m_imguiInitialized = false;
    
    void setupScenes();      // Your VDE scene setup
    void initImGui();        // ImGui initialization
    void renderImGui();      // ImGui per-frame rendering
    void cleanupImGui();     // ImGui cleanup
};
```

### Step 3: ImGui Initialization

Implement `initImGui()` to set up ImGui with VDE's Vulkan context:

```cpp
void MyGame::initImGui() {
    auto* ctx = getVulkanContext();
    auto* win = getWindow();
    if (!ctx || !win) return;
    
    // Create ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    
    // Optional: Enable features
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    // io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;  // Enable docking
    
    // Set visual style
    ImGui::StyleColorsDark();
    // ImGui::StyleColorsLight();
    // ImGui::StyleColorsClassic();
    
    // Initialize GLFW platform backend
    // install_callbacks=true lets ImGui capture input alongside VDE
    ImGui_ImplGlfw_InitForVulkan(win->getHandle(), true);
    
    // Create descriptor pool for ImGui's internal use
    m_imguiPool = createImGuiDescriptorPool(ctx->getDevice());
    
    // Initialize Vulkan renderer backend
    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.Instance = ctx->getInstance();
    initInfo.PhysicalDevice = ctx->getPhysicalDevice();
    initInfo.Device = ctx->getDevice();
    initInfo.QueueFamily = ctx->getGraphicsQueueFamily();
    initInfo.Queue = ctx->getGraphicsQueue();
    initInfo.DescriptorPool = m_imguiPool;
    initInfo.MinImageCount = 2;
    initInfo.ImageCount = 2;
    initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    initInfo.RenderPass = ctx->getRenderPass();
    initInfo.Subpass = 0;
    
    ImGui_ImplVulkan_Init(&initInfo);
    
    // Upload font textures to GPU
    ImGui_ImplVulkan_CreateFontsTexture();
    
    m_imguiInitialized = true;
}

// Helper: Create ImGui descriptor pool
static VkDescriptorPool createImGuiDescriptorPool(VkDevice device) {
    VkDescriptorPoolSize poolSizes[] = {
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100},
    };
    
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = 100;
    poolInfo.poolSizeCount = static_cast<uint32_t>(std::size(poolSizes));
    poolInfo.pPoolSizes = poolSizes;
    
    VkDescriptorPool pool = VK_NULL_HANDLE;
    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &pool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create ImGui descriptor pool!");
    }
    return pool;
}
```

### Step 4: Per-Frame Rendering

Implement `renderImGui()` to build UI and record draw commands:

```cpp
void MyGame::renderImGui() {
    if (!m_imguiInitialized) return;
    
    // Start new ImGui frame
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    
    // BUILD YOUR UI HERE
    // Example: Simple stats window
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Stats")) {
        ImGui::Text("FPS: %.1f", getFPS());
        ImGui::Text("Frame: %llu", getFrameCount());
        ImGui::Text("Delta: %.3f ms", getDeltaTime() * 1000.0f);
    }
    ImGui::End();
    
    // Finalize ImGui rendering
    ImGui::Render();
    ImDrawData* drawData = ImGui::GetDrawData();
    
    // Record draw commands into VDE's active command buffer
    auto* ctx = getVulkanContext();
    if (ctx) {
        VkCommandBuffer cmd = ctx->getCurrentCommandBuffer();
        if (cmd != VK_NULL_HANDLE) {
            ImGui_ImplVulkan_RenderDrawData(drawData, cmd);
        }
    }
}
```

### Step 5: Cleanup

Implement `cleanupImGui()` to release resources:

```cpp
void MyGame::cleanupImGui() {
    if (!m_imguiInitialized) return;
    
    // Shutdown backends
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    
    // Destroy descriptor pool
    if (m_imguiPool != VK_NULL_HANDLE) {
        auto* ctx = getVulkanContext();
        if (ctx && ctx->getDevice()) {
            vkDestroyDescriptorPool(ctx->getDevice(), m_imguiPool, nullptr);
        }
        m_imguiPool = VK_NULL_HANDLE;
    }
    
    m_imguiInitialized = false;
}
```

## Common UI Patterns

### Entity Inspector Panel

```cpp
void renderEntityInspector(vde::Scene* scene, vde::EntityId entityId) {
    auto* entity = scene->getEntity(entityId);
    if (!entity) return;
    
    ImGui::Begin("Entity Inspector");
    
    // Position control
    float pos[3] = {entity->getPosition().x, 
                    entity->getPosition().y, 
                    entity->getPosition().z};
    if (ImGui::DragFloat3("Position", pos, 0.1f)) {
        entity->setPosition(pos[0], pos[1], pos[2]);
    }
    
    // Rotation control
    float rot[3] = {entity->getRotation().pitch,
                    entity->getRotation().yaw,
                    entity->getRotation().roll};
    if (ImGui::SliderFloat3("Rotation", rot, 0.0f, 360.0f)) {
        entity->setRotation(rot[0], rot[1], rot[2]);
    }
    
    // Scale control
    float scale = entity->getScale().x;
    if (ImGui::SliderFloat("Scale", &scale, 0.1f, 5.0f)) {
        entity->setScale(scale);
    }
    
    // Color control (if MeshEntity or SpriteEntity)
    if (auto* meshEntity = dynamic_cast<vde::MeshEntity*>(entity)) {
        float color[3] = {meshEntity->getColor().r,
                         meshEntity->getColor().g,
                         meshEntity->getColor().b};
        if (ImGui::ColorEdit3("Color", color)) {
            meshEntity->setColor(vde::Color(color[0], color[1], color[2]));
        }
    }
    
    ImGui::End();
}
```

### Scene Statistics

```cpp
void renderSceneStats(vde::Game* game, vde::Scene* scene) {
    ImGui::Begin("Scene Stats");
    
    // Engine stats
    ImGui::Text("FPS: %.1f", game->getFPS());
    ImGui::Text("Frame Time: %.3f ms", game->getDeltaTime() * 1000.0f);
    ImGui::Separator();
    
    // Scene stats
    ImGui::Text("Entities: %zu", scene->getEntities().size());
    
    if (scene->hasPhysics()) {
        auto* physics = scene->getPhysicsScene();
        ImGui::Text("Physics Bodies: %zu", physics->getBodyCount());
    }
    
    // Camera info
    if (auto* camera = scene->getCamera()) {
        ImGui::Separator();
        ImGui::Text("Camera Type: %s", typeid(*camera).name());
    }
    
    ImGui::End();
}
```

### Lighting Controls

```cpp
void renderLightingControls(vde::Scene* scene) {
    auto* lightBox = scene->getLightBox();
    if (!lightBox) return;
    
    ImGui::Begin("Lighting");
    
    // Ambient light
    vde::Color ambient = lightBox->getAmbientColor();
    float ambientRgb[3] = {ambient.r, ambient.g, ambient.b};
    if (ImGui::ColorEdit3("Ambient", ambientRgb)) {
        lightBox->setAmbientColor(vde::Color(ambientRgb[0], ambientRgb[1], ambientRgb[2]));
    }
    
    float ambientIntensity = lightBox->getAmbientIntensity();
    if (ImGui::SliderFloat("Ambient Intensity", &ambientIntensity, 0.0f, 2.0f)) {
        lightBox->setAmbientIntensity(ambientIntensity);
    }
    
    // Individual lights
    for (size_t i = 0; i < lightBox->getLightCount(); ++i) {
        auto& light = lightBox->getLight(i);
        
        std::string label = "Light " + std::to_string(i);
        if (ImGui::TreeNode(label.c_str())) {
            float intensity = light.intensity;
            if (ImGui::SliderFloat("Intensity", &intensity, 0.0f, 3.0f)) {
                light.intensity = intensity;
            }
            
            float color[3] = {light.color.r, light.color.g, light.color.b};
            if (ImGui::ColorEdit3("Color", color)) {
                light.color = vde::Color(color[0], color[1], color[2]);
            }
            
            ImGui::TreePop();
        }
    }
    
    ImGui::End();
}
```

### Debug Overlay

```cpp
void renderDebugOverlay(vde::Game* game) {
    // Minimal overlay in corner
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
                            ImGuiWindowFlags_AlwaysAutoResize |
                            ImGuiWindowFlags_NoSavedSettings |
                            ImGuiWindowFlags_NoFocusOnAppearing |
                            ImGuiWindowFlags_NoNav;
    
    const float PAD = 10.0f;
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 workPos = viewport->WorkPos;
    ImVec2 windowPos = ImVec2(workPos.x + PAD, workPos.y + PAD);
    ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.35f);
    
    if (ImGui::Begin("Debug", nullptr, flags)) {
        ImGui::Text("FPS: %.1f", game->getFPS());
        ImGui::Text("Frame: %llu", game->getFrameCount());
    }
    ImGui::End();
}
```

## Best Practices

### 1. Initialize After VDE Setup

Always call `initImGui()` **after** VDE's `Game::initialize()` completes, typically in `onStart()`:

```cpp
void onStart() override {
    // Setup VDE scenes/entities first
    setupMyScenes();
    
    // Then initialize ImGui
    initImGui();
}
```

### 2. Cleanup with Device Idle

Wait for GPU to finish before destroying ImGui resources:

```cpp
void onShutdown() override {
    if (getVulkanContext()) {
        vkDeviceWaitIdle(getVulkanContext()->getDevice());
    }
    cleanupImGui();
}
```

### 3. Guard Against nullptr

Always check pointers before using VDE resources:

```cpp
void renderImGui() {
    if (!m_imguiInitialized) return;
    
    auto* ctx = getVulkanContext();
    if (!ctx) return;
    
    // ... proceed with ImGui rendering
}
```

### 4. Use Window Conditions

Set window positions/sizes only on first use to allow user repositioning:

```cpp
ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
ImGui::SetNextWindowSize(ImVec2(300, 200), ImGuiCond_FirstUseEver);
```

### 5. Store State in Game Class

Keep ImGui state (show flags, values) as member variables:

```cpp
class MyGame : public vde::Game {
private:
    bool m_showStats = true;
    bool m_showInspector = false;
    vde::EntityId m_selectedEntity = 0;
    
    void renderImGui() override {
        if (m_showStats) {
            renderStatsWindow();
        }
        if (m_showInspector) {
            renderEntityInspector(m_selectedEntity);
        }
    }
};
```

### 6. Handle Input Capture

ImGui automatically captures mouse when hovering over windows. Check `io.WantCaptureMouse` if you need custom behavior:

```cpp
ImGuiIO& io = ImGui::GetIO();
if (!io.WantCaptureMouse) {
    // Handle game mouse input
}
```

### 7. Performance Considerations

- **Minimize state changes** - Group similar controls together
- **Use TreeNode for large hierarchies** - Collapse unused sections
- **Cache computed values** - Don't recalculate every frame
- **Limit window count** - Prefer tabs or collapsing headers

## Common Pitfalls

### ❌ Forgetting to Call NewFrame

```cpp
// WRONG - Missing NewFrame calls
void renderImGui() {
    ImGui::Begin("Window");
    ImGui::End();
    ImGui::Render();  // Will assert!
}

// CORRECT
void renderImGui() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    
    ImGui::Begin("Window");
    ImGui::End();
    
    ImGui::Render();
}
```

### ❌ Rendering Outside Render Pass

```cpp
// WRONG - Calling from update()
void update(float dt) override {
    renderImGui();  // Not in render pass!
    Scene::update(dt);
}

// CORRECT - Call from onRender()
void onRender() override {
    renderImGui();  // Inside render pass ✓
}
```

### ❌ Not Waiting for Device Idle on Shutdown

```cpp
// WRONG - Cleanup while GPU may still be using resources
~MyGame() {
    cleanupImGui();  // May crash!
}

// CORRECT
void onShutdown() override {
    vkDeviceWaitIdle(getVulkanContext()->getDevice());
    cleanupImGui();
}
```

### ❌ Using VDE RenderCallback

```cpp
// WRONG - Don't use renderCallback for ImGui
m_vulkanContext->setRenderCallback([](VkCommandBuffer cmd) {
    renderImGui();  // Won't work - wrong scope
});

// CORRECT - Use Game::onRender()
void onRender() override {
    renderImGui();
}
```

### ❌ Modifying Entities Without Guards

```cpp
// WRONG - Entity might be nullptr or deleted
void renderInspector() {
    auto* entity = scene->getEntity(m_selectedId);
    entity->setPosition(newPos);  // May crash!
}

// CORRECT
void renderInspector() {
    auto* entity = scene->getEntity(m_selectedId);
    if (!entity) return;
    
    // Safe to modify
    entity->setPosition(newPos);
}
```

## Advanced Patterns

### Custom Scene Inspector

Build an inspector that lets you select and edit any entity:

```cpp
class EditorScene : public vde::Scene {
private:
    vde::EntityId m_selectedEntity = 0;
    
public:
    void renderEditor() {
        ImGui::Begin("Scene Hierarchy");
        
        // Entity list
        for (const auto& entity : getEntities()) {
            bool selected = (entity->getId() == m_selectedEntity);
            std::string label = entity->getName().empty() 
                ? "Entity " + std::to_string(entity->getId())
                : entity->getName();
            
            if (ImGui::Selectable(label.c_str(), selected)) {
                m_selectedEntity = entity->getId();
            }
        }
        
        ImGui::End();
        
        // Property editor for selected entity
        if (m_selectedEntity != 0) {
            renderEntityProperties(m_selectedEntity);
        }
    }
};
```

### Live Physics Tuning

```cpp
void renderPhysicsControls(vde::Scene* scene) {
    if (!scene->hasPhysics()) return;
    
    ImGui::Begin("Physics");
    
    auto* physics = scene->getPhysicsScene();
    auto config = physics->getConfig();
    
    // Gravity control
    float gravity[2] = {config.gravity.x, config.gravity.y};
    if (ImGui::DragFloat2("Gravity", gravity, 0.1f)) {
        config.gravity = {gravity[0], gravity[1]};
        // Note: Changing config at runtime may require scene recreation
    }
    
    // Per-body controls
    ImGui::Separator();
    ImGui::Text("Bodies:");
    
    for (const auto& entity : scene->getEntities()) {
        if (auto* physEntity = dynamic_cast<vde::PhysicsEntity*>(entity.get())) {
            if (ImGui::TreeNode(entity->getName().c_str())) {
                // Velocity display
                auto vel = physEntity->getLinearVelocity();
                ImGui::Text("Velocity: (%.2f, %.2f)", vel.x, vel.y);
                
                // Apply impulse button
                if (ImGui::Button("Impulse Up")) {
                    physEntity->applyImpulse(0.0f, 5.0f);
                }
                
                ImGui::TreePop();
            }
        }
    }
    
    ImGui::End();
}
```

### Material/Texture Editor

```cpp
void renderMaterialEditor(vde::MeshEntity* entity) {
    ImGui::Begin("Material Editor");
    
    auto* material = entity->getMaterial();
    if (material) {
        // Metallic/Roughness
        float metallic = material->getMetallic();
        if (ImGui::SliderFloat("Metallic", &metallic, 0.0f, 1.0f)) {
            material->setMetallic(metallic);
        }
        
        float roughness = material->getRoughness();
        if (ImGui::SliderFloat("Roughness", &roughness, 0.0f, 1.0f)) {
            material->setRoughness(roughness);
        }
        
        // Texture selection (example - would need texture list)
        if (ImGui::BeginCombo("Albedo Texture", "Current Texture")) {
            // List available textures
            ImGui::EndCombo();
        }
    }
    
    ImGui::End();
}
```

## Reference Example

See [examples/imgui_demo/](../../../examples/imgui_demo/) for a complete working example demonstrating:

- Full initialization/cleanup lifecycle
- Entity inspector panels
- Lighting controls
- Stats overlay
- ImGui Demo Window integration

## Summary Checklist

**CMake:**
- ✅ Fetch ImGui via FetchContent
- ✅ Build imgui_backend library with GLFW + Vulkan backends
- ✅ Link imgui_backend to your executable

**Initialization:**
- ✅ Call `initImGui()` in `Game::onStart()` after VDE setup
- ✅ Create ImGui context and configure IO
- ✅ Initialize GLFW backend with `install_callbacks=true`
- ✅ Create dedicated descriptor pool for ImGui
- ✅ Initialize Vulkan backend with VDE's context
- ✅ Upload font textures

**Rendering:**
- ✅ Call `renderImGui()` in `Game::onRender()` (inside render pass)
- ✅ Start frame with NewFrame calls
- ✅ Build UI with ImGui API
- ✅ Finalize with `ImGui::Render()`
- ✅ Record draw data with `ImGui_ImplVulkan_RenderDrawData()`

**Cleanup:**
- ✅ Wait for device idle in `onShutdown()`
- ✅ Shutdown Vulkan/GLFW backends
- ✅ Destroy ImGui context
- ✅ Destroy descriptor pool

**Best Practices:**
- ✅ Initialize after VDE setup
- ✅ Guard pointer accesses
- ✅ Use window conditions for positioning
- ✅ Store UI state in game class
- ✅ Check for nullptr before using entities
- ✅ Keep UI code separate from game logic

## Additional Resources

- **ImGui Documentation** - https://github.com/ocornut/imgui/wiki
- **Vulkan Backend Guide** - `imgui/backends/imgui_impl_vulkan.h`
- **GLFW Backend Guide** - `imgui/backends/imgui_impl_glfw.h`
- **VDE Example** - [examples/imgui_demo/](../../../examples/imgui_demo/)

When in doubt, refer to the working example and the official ImGui documentation for advanced features like docking, viewports, and custom rendering.
