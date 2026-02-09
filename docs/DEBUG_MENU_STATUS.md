# Debug Menu Integration Status

## Summary

ImGui debug menu support has been integrated into VDE ExampleBase.h. Most examples now have working debug menus accessible via **F1 key** (enabled by default).

## ✅ Examples with Working Debug Menus

These examples use `BaseExampleGame`/`BaseExampleScene` and have full debug menu support:

1. ✅ **imgui_demo** - Full custom debug UI
2. ✅ **wireframe_viewer** - Default debug UI
3. ✅ **sprite_demo** - Default debug UI
4. ✅ **world_bounds_demo** - Default debug UI
5. ✅ **physics_demo** - Default debug UI
6. ✅ **physics_audio_demo** - Default debug UI
7. ✅ **materials_lighting_demo** - Default debug UI
8. ✅ **resource_demo** - Default debug UI
9. ✅ **sidescroller** - Default debug UI
10. ✅ **asteroids_demo** - Default debug UI
11. ✅ **asteroids_physics_demo** - Default debug UI
12. ✅ **breakout_demo** - Default debug UI
13. ✅ **multi_scene_demo** - Default debug UI
14. ✅ **audio_demo** - **FIXED** - Converted to BaseExampleGame
15. ✅ **simple_game** - **FIXED** - Added manual ImGui support

## Debug UI Features

- **F1** - Toggle debug UI on/off
- Default shows: FPS, Frame count, Frame time (ms), Entity count, DPI scale
- Enabled by default
- Can be customized by overriding `drawDebugUI()` in scene classes

## ⚠️ Examples Needing Manual ImGui Integration

These 3 examples use complex custom Game classes with multiple scenes. They have ImGui includes added but still need the ImGui initialization/rendering code:

### 1. four_scene_3d_demo
- **Status:** ImGui includes added ✓
- **Needs:** ImGui init/render/cleanup code in `FourScene3DDemo` class
- **Issue:** Uses 4 custom `Focusable3DScene` classes (not BaseExampleScene)

### 2. quad_viewport_demo  
- **Status:** ImGui includes added ✓
- Status:** ImGui init/render/cleanup code in game class
- **Issue:** Uses 4 separate Scene classes (SpaceScene, ForestScene, CityScene, OceanScene)

### 3. parallel_physics_demo
- **Status:** ImGui includes added ✓
- **Needs:** ImGui init/render/cleanup code in `ParallelPhysicsGame` class
- **Issue:** Uses 2 PhysicsWorldScene + 1 CoordinatorScene with thread pool

## How to Add ImGui to Remaining Demos

For the 3 remaining demos, add the following code to their Game classes:

### Step 1: Add Member Variables

```cpp
private:
#ifdef VDE_EXAMPLE_USE_IMGUI
    VkDescriptorPool m_imguiPool = VK_NULL_HANDLE;
    bool m_imguiInitialized = false;
    
    static VkDescriptorPool createImGuiDescriptorPool(VkDevice device) {
        VkDescriptorPoolSize poolSizes[] = {{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100}};
        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        poolInfo.maxSets = 100;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = poolSizes;
        VkDescriptorPool pool = VK_NULL_HANDLE;
        vkCreateDescriptorPool(device, &poolInfo, nullptr, &pool);
        return pool;
    }
    
    void initImGui() {
        auto* ctx = getVulkanContext();
        auto* win = getWindow();
        if (!ctx || !win) return;
        
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        ImGui::StyleColorsDark();
        
        float dpiScale = getDPIScale();
        if (dpiScale > 0.0f) io.FontGlobalScale = dpiScale;
        
        ImGui_ImplGlfw_InitForVulkan(win->getHandle(), true);
        m_imguiPool = createImGuiDescriptorPool(ctx->getDevice());
        
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
        ImGui_ImplVulkan_CreateFontsTexture();
        m_imguiInitialized = true;
    }
    
    void cleanupImGui() {
        if (!m_imguiInitialized) return;
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        if (m_imguiPool != VK_NULL_HANDLE) {
            auto* ctx = getVulkanContext();
            if (ctx && ctx->getDevice()) {
                vkDestroyDescriptorPool(ctx->getDevice(), m_imguiPool, nullptr);
            }
            m_imguiPool = VK_NULL_HANDLE;
        }
        m_imguiInitialized = false;
    }
#endif
};
```

### Step 2: Update Constructor/Destructor

```cpp
~YourGameClass() override {
#ifdef VDE_EXAMPLE_USE_IMGUI
    cleanupImGui();
#endif
}
```

### Step 3: Update onStart()

```cpp
void onStart() override {
    // ... existing initialization code ...
    
#ifdef VDE_EXAMPLE_USE_IMGUI
    initImGui();
#endif
}
```

### Step 4: Add onRender() Method

```cpp
void onRender() override {
#ifdef VDE_EXAMPLE_USE_IMGUI
    if (!m_imguiInitialized) return;
    
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    
    // Draw simple debug UI
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(280, 140), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Debug Info")) {
        ImGui::Text("FPS: %.1f", getFPS());
        ImGui::Text("Frame: %llu", getFrameCount());
        ImGui::Text("Delta: %.3f ms", getDeltaTime() * 1000.0f);
        ImGui::Text("DPI Scale: %.2f", getDPIScale());
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.5f, 0.8f, 0.5f, 1.0f), "Debug Menu Active");
    }
    ImGui::End();
    
    ImGui::Render();
    auto* ctx = getVulkanContext();
    if (ctx) {
        VkCommandBuffer cmd = ctx->getCurrentCommandBuffer();
        if (cmd != VK_NULL_HANDLE) {
            ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
        }
    }
#endif
}
```

### Step 5: Update onShutdown()

```cpp
void onShutdown() override {
#ifdef VDE_EXAMPLE_USE_IMGUI
    if (getVulkanContext()) {
        vkDeviceWaitIdle(getVulkanContext()->getDevice());
    }
    cleanupImGui();
#endif
    // ... existing cleanup code ...
}
```

## Testing

To test if a demo has a working debug menu:
1. Run the demo
2. Look for debug UI overlay in top-left corner
3. Press **F1** to toggle it on/off
4. If UI doesn't appear or F1 doesn't work → debug menu not working

## Next Steps

To complete debug menu integration:
1. Apply the code above to `FourScene3DDemo`, `QuadViewportDemo`, and `ParallelPhysicsGame` classes
2. Build the project
3. Test each demo to verify debug UI appears
4. Verify F1 toggle works in each demo
