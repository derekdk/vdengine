# Copilot Instructions for VDE (Vulkan Display Engine)

## Overview

VDE is a lightweight, reusable Vulkan-based 3D rendering engine designed for rapid prototyping and game development. This document provides guidelines for AI coding agents working within this codebase.

## Architecture

### Directory Structure
```
vdengine/
├── include/vde/     # Public headers (user-facing API)
├── src/             # Implementation files
├── shaders/         # GLSL shader sources
├── tests/           # Unit tests (Google Test)
├── examples/        # Example applications
├── scripts/         # Build and utility scripts
├── docs/            # Documentation
└── third_party/     # Vendored dependencies
```

### Core Components

| Component | Purpose |
|-----------|---------|
| **Window** | GLFW-based window management |
| **VulkanContext** | Core Vulkan instance, device, swapchain |
| **Camera** | 3D camera with orbital controls |
| **Texture** | Image loading and Vulkan textures |
| **BufferUtils** | Vulkan buffer operations |
| **ShaderCompiler** | Runtime GLSL→SPIR-V compilation |
| **ShaderCache** | Compiled shader caching |
| **HexGeometry** | Hexagon mesh generation |
| **HexPrismMesh** | 3D hex prism generation |

## Coding Conventions

### Naming
- **Namespace**: All code in `vde::` namespace
- **Classes**: PascalCase (`VulkanContext`, `ShaderCache`)
- **Functions**: camelCase (`createBuffer`, `getViewMatrix`)
- **Constants**: UPPER_SNAKE_CASE or `k` prefix (`MAX_FRAMES_IN_FLIGHT`, `kEnableValidationLayers`)
- **Member variables**: `m_` prefix (`m_device`, `m_window`)

### File Organization
- Headers use `#pragma once`
- Public headers in `include/vde/`
- Implementation in `src/`
- One class per file (matching names)

### Include Order
1. Corresponding header (for .cpp files)
2. VDE headers (`<vde/...>`)
3. Third-party headers (`<vulkan/vulkan.h>`, `<glm/glm.hpp>`)
4. Standard library headers (`<vector>`, `<string>`)

### Documentation
- Use Doxygen-style comments for public API
- Include `@brief`, `@param`, `@return` tags
- Document exceptions with `@throws`

## Dependencies

| Dependency | Version | Purpose |
|------------|---------|---------|
| Vulkan SDK | 1.3+ | Graphics API |
| GLFW | 3.x | Window/input |
| GLM | 1.0+ | Mathematics |
| glslang | 14.3.0 | Shader compilation |
| stb_image | 2.30 | Image loading |
| Google Test | 1.14.0 | Unit testing |

## Development Workflows

### Building
```powershell
cd vdengine
mkdir build && cd build
cmake ..
cmake --build . --config Debug
```

### Running Tests
```powershell
.\scripts\build-and-test.ps1
# Or directly:
.\build\tests\Debug\vde_tests.exe
```

### Adding a New Component

1. Create header in `include/vde/ComponentName.h`
2. Create implementation in `src/ComponentName.cpp`
3. Add to `CMakeLists.txt` sources
4. Add `#include <vde/ComponentName.h>` to `Core.h`
5. Write tests in `tests/ComponentName_test.cpp`
6. Add test file to `tests/CMakeLists.txt`

### Vulkan Patterns

- Use RAII for Vulkan resources where possible
- Check `VkResult` and throw on failure
- Use staging buffers for device-local memory
- Prefer push constants for small, frequently-changing data
- Use descriptor sets for textures and large uniform data

## Testing Guidelines

- Use Google Test framework
- Test class: `ComponentNameTest`
- Test naming: `TEST_F(ComponentNameTest, DescriptiveTestName)`
- Aim for high coverage on core logic
- Mock Vulkan calls where feasible

## Common Tasks

### Creating a Buffer
```cpp
BufferUtils::init(device, physicalDevice, commandPool, graphicsQueue);
VkBuffer buffer;
VkDeviceMemory memory;
BufferUtils::createBuffer(size, usage, properties, buffer, memory);
```

### Loading a Texture
```cpp
vde::Texture texture;
texture.loadFromFile("path/to/image.png", device, physicalDevice, commandPool, queue);
```

### Setting Up Camera
```cpp
vde::Camera camera;
camera.setFromPitchYaw(20.0f, 45.0f, 0.0f, glm::vec3(0.0f));
camera.setPerspective(45.0f, aspectRatio, 0.1f, 100.0f);
glm::mat4 vp = camera.getViewProjectionMatrix();
```

## Error Handling

- Throw `std::runtime_error` for unrecoverable errors
- Use return values for recoverable failures
- Log warnings for non-fatal issues
- Validate Vulkan results with descriptive error messages

## Performance Considerations

- Minimize Vulkan API calls per frame
- Batch similar draw calls
- Use instanced rendering for repeated geometry
- Prefer device-local memory for static buffers
- Profile with RenderDoc or Vulkan validation layers

## Resources

- [Vulkan Tutorial](https://vulkan-tutorial.com/)
- [Vulkan Specification](https://www.khronos.org/registry/vulkan/specs/1.3/html/)
- [GLM Documentation](https://github.com/g-truc/glm)
- [GLFW Documentation](https://www.glfw.org/documentation.html)
