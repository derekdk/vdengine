# VDE Architecture

This document describes the high-level architecture and design decisions of the Vulkan Display Engine.

## Design Goals

1. **Simplicity** - Abstract Vulkan's complexity without hiding necessary control
2. **Reusability** - Generic engine usable across different game types
3. **Performance** - Minimal overhead, efficient GPU utilization
4. **Testability** - Components should be unit-testable where possible

## System Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                        Application                               │
├─────────────────────────────────────────────────────────────────┤
│                           VDE API                                │
│  ┌─────────┐  ┌──────────────┐  ┌────────┐  ┌─────────────────┐ │
│  │ Window  │  │VulkanContext │  │ Camera │  │  HexGeometry    │ │
│  └────┬────┘  └──────┬───────┘  └────────┘  └─────────────────┘ │
│       │              │                                           │
│  ┌────┴────┐  ┌──────┴───────────────────────────────────────┐  │
│  │  GLFW   │  │              Vulkan Subsystems               │  │
│  └─────────┘  │  ┌──────────┐ ┌───────────┐ ┌─────────────┐  │  │
│               │  │ Texture  │ │ Shaders   │ │   Buffers   │  │  │
│               │  └──────────┘ └───────────┘ └─────────────┘  │  │
│               └──────────────────────────────────────────────┘  │
├─────────────────────────────────────────────────────────────────┤
│                        Vulkan Driver                             │
└─────────────────────────────────────────────────────────────────┘
```

## Component Responsibilities

### Window (window.h/cpp)
- GLFW window creation and management
- Input event polling
- Resize handling and callbacks
- Vulkan surface creation (via GLFW)

**Dependencies**: GLFW
**Dependents**: VulkanContext

### VulkanContext (VulkanContext.h/cpp)
Core Vulkan infrastructure management:
- Instance creation with validation layers
- Physical/logical device selection
- Swap chain management
- Render pass and framebuffers
- Command pool and buffers
- Synchronization primitives (semaphores, fences)
- Frame rendering orchestration

**Dependencies**: Window, Vulkan
**Dependents**: Application, Texture, BufferUtils

### Camera (Camera.h/cpp)
3D view and projection management:
- Position/target-based setup
- Orbital camera controls (pitch, yaw, distance)
- View matrix generation
- Projection matrix with Vulkan Y-flip correction
- Pan, zoom, translate operations

**Dependencies**: GLM
**Dependents**: Application

### Texture (Texture.h/cpp)
Vulkan texture management:
- Image loading via stb_image
- Staging buffer creation
- Image layout transitions
- Image view and sampler creation
- Descriptor info generation

**Dependencies**: ImageLoader, BufferUtils, Vulkan
**Dependents**: Application

### BufferUtils (BufferUtils.h/cpp)
Static utility class for Vulkan buffer operations:
- Memory type finding
- Buffer creation with various properties
- Buffer-to-buffer copy via staging
- Single-time command buffer utilities

**Dependencies**: Vulkan
**Dependents**: Texture, Application

### ShaderCompiler (ShaderCompiler.h/cpp)
Runtime GLSL to SPIR-V compilation:
- Uses glslang library
- Supports vertex, fragment, compute shaders
- Returns compiled SPIR-V binary

**Dependencies**: glslang
**Dependents**: ShaderCache

### ShaderCache (ShaderCache.h/cpp)
Shader compilation caching:
- Computes hash of shader source
- Caches compiled shader modules
- Loads from cache or compiles on demand

**Dependencies**: ShaderCompiler
**Dependents**: Application

### HexGeometry (HexGeometry.h/cpp)
Hexagon mesh generation:
- Flat-top and pointy-top orientations
- Vertex and index buffer generation
- UV coordinate calculation
- Corner position utilities

**Dependencies**: Types, GLM
**Dependents**: Application

### HexPrismMesh (HexPrismMesh.h/cpp)
3D hexagonal prism generation:
- Top and bottom face generation
- Side quad generation
- Normal calculation
- Height-based geometry

**Dependencies**: GLM
**Dependents**: Application

## Data Flow

### Initialization Sequence

```
1. Application creates Window
2. Application creates VulkanContext
3. VulkanContext.initialize(&window)
   ├── createInstance()
   ├── setupDebugMessenger()
   ├── createSurface(window)
   ├── pickPhysicalDevice()
   ├── createLogicalDevice()
   ├── createSwapChain()
   ├── createImageViews()
   ├── createRenderPass()
   ├── createFramebuffers()
   ├── createCommandPool()
   ├── createCommandBuffers()
   └── createSyncObjects()
4. Application sets render callback
5. Enter main loop
```

### Render Frame Sequence

```
1. window.pollEvents()
2. context.drawFrame()
   ├── vkWaitForFences()
   ├── vkAcquireNextImageKHR()
   ├── vkResetCommandBuffer()
   ├── vkBeginCommandBuffer()
   ├── vkCmdBeginRenderPass()
   ├── renderCallback(commandBuffer)  // User code
   ├── vkCmdEndRenderPass()
   ├── vkEndCommandBuffer()
   ├── vkQueueSubmit()
   └── vkQueuePresentKHR()
```

## Memory Management

### Resource Ownership
- VulkanContext owns core Vulkan resources (instance, device, swapchain)
- Texture owns its image, memory, view, and sampler
- BufferUtils is stateless (uses static initialization)
- ShaderCache owns compiled shader modules

### Cleanup Order
Resources must be destroyed in reverse creation order:
1. Application resources (textures, buffers)
2. Pipelines and descriptor sets
3. Command buffers and pools
4. Framebuffers and render pass
5. Swap chain and image views
6. Logical device
7. Debug messenger
8. Surface
9. Instance

## Threading Model

VDE is currently single-threaded:
- All Vulkan commands submitted from main thread
- No internal thread synchronization
- Applications requiring multi-threading should manage their own synchronization

## Extension Points

### Custom Rendering
Set a render callback on VulkanContext:
```cpp
context.setRenderCallback([](VkCommandBuffer cmd) {
    // Custom Vulkan rendering commands
});
```

### Custom Pipelines
Applications create their own graphics pipelines using:
- `context.getDevice()` - Logical device
- `context.getRenderPass()` - Compatible render pass
- `context.getSwapChainExtent()` - Viewport dimensions

### Subclassing
VulkanContext can be subclassed for custom initialization:
```cpp
class MyContext : public vde::VulkanContext {
protected:
    // Override initialization methods
};
```

## Future Considerations

### Planned Enhancements
- ImGui integration for debug UI
- Basic 2D sprite renderer
- Texture atlasing
- Multiple render passes
- Compute shader support

### Design Constraints
- Single window support (could add multi-window)
- Fixed render pass format (could make configurable)
- No built-in scene graph (intentional - keep engine minimal)
