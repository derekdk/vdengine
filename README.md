# VDE - Vulkan Display Engine

A lightweight, reusable Vulkan-based 3D rendering engine designed for rapid prototyping and game development.

## Overview

VDE provides a clean abstraction over Vulkan's verbose API while maintaining flexibility for advanced use cases. The engine handles:

- **Window Management**: Cross-platform window creation via GLFW
- **Vulkan Context**: Instance, device, swapchain, and synchronization setup
- **Shader System**: Runtime GLSL compilation with caching
- **Buffer Management**: Vertex, index, uniform buffers with staging
- **Texture Loading**: Image loading and Vulkan texture creation
- **Camera System**: Orbital and free-form camera controls
- **Hexagonal Geometry**: Hex grid mesh generation for strategy games

## Quick Start

### Prerequisites

- **CMake 3.20+**
- **Vulkan SDK 1.3+** ([Download](https://vulkan.lunarg.com/))
- **C++20 compatible compiler** (MSVC 2022, GCC 11+, Clang 14+)
- **Git** (for dependency fetching)
- **Git LFS** (for asset files) - [Installation Guide](docs/GIT_LFS_SETUP.md)

### Building

```powershell
# Clone or navigate to vdengine directory
cd vdengine

# Configure and build
mkdir build
cd build
cmake ..
cmake --build . --config Debug

# Run tests
.\tests\Debug\vde_tests.exe

# Run example
.\examples\Debug\triangle_example.exe
```

### Using the Build Scripts (Recommended)

VDE provides convenient PowerShell scripts for all build operations:

```powershell
# Quick build with MSBuild (default)
.\scripts\build.ps1

# Fast build with Ninja
.\scripts\build.ps1 -Generator Ninja

# Run all tests
.\scripts\test.ps1

# Build and test together
.\scripts\test.ps1 -Build

# Clean rebuild
.\scripts\rebuild.ps1

# Run specific tests
.\scripts\test.ps1 -Filter "CameraTest.*"

# Release build
.\scripts\build.ps1 -Config Release

# Show all available commands
.\scripts\help.ps1

# Format C++ code
.\scripts\format.ps1
```

For complete documentation, see:
- `scripts/README.md` - Detailed script usage
- `.github/skills/build-tool-workflows/SKILL.md` - Complete build guide

## Code Formatting

VDE uses clang-format to maintain consistent code style. The configuration is defined in [.clang-format](.clang-format) at the project root.

```powershell
# Format all C++ files
.\scripts\format.ps1

# Check formatting (useful for CI/pre-commit)
.\scripts\format.ps1 -Check
```

**VSCode Integration:**
- Format-on-save is enabled by default (see [.vscode/settings.json](.vscode/settings.json))
- Right-click → Format Document (or Alt+Shift+F)
- Requires clang-format in PATH (install via Visual Studio C++ clang tools or LLVM)

## Usage Example

```cpp
#include <vde/Core.h>

int main() {
    // Create window
    vde::Window window(1280, 720, "My VDE Application");
    
    // Initialize Vulkan context
    vde::VulkanContext context;
    if (!context.initialize(&window)) {
        return 1;
    }
    
    // Set up rendering callback
    context.setRenderCallback([&](VkCommandBuffer cmd) {
        // Your rendering code here
    });
    
    // Main loop
    while (!window.shouldClose()) {
        window.pollEvents();
        context.drawFrame();
    }
    
    context.waitIdle();
    return 0;
}
```

## Architecture

```
vdengine/
├── include/vde/           # Public headers
│   ├── Core.h             # Umbrella header (include this)
│   ├── Window.h           # Window management
│   ├── VulkanContext.h    # Core Vulkan setup
│   ├── Camera.h           # Camera controls
│   ├── Texture.h          # Texture management
│   ├── ShaderCache.h      # Shader compilation/caching
│   ├── BufferUtils.h      # Buffer helpers
│   ├── UniformBuffer.h    # UBO management
│   ├── DescriptorManager.h # Descriptor set management
│   ├── HexGeometry.h      # Hex grid generation
│   ├── HexPrismMesh.h     # 3D hex prism meshes
│   └── Types.h            # Common types (Vertex, UBO)
├── src/                   # Implementation files
├── shaders/               # GLSL shader sources
├── tests/                 # Unit tests (Google Test)
├── examples/              # Example applications
├── scripts/               # Build/test scripts
└── third_party/           # Vendored dependencies
```

## Core Components

### Window
Cross-platform window creation and event handling.

```cpp
vde::Window window(1920, 1080, "Game Window");
while (!window.shouldClose()) {
    window.pollEvents();
    // ...
}
```

### VulkanContext
Manages all Vulkan resources: instance, device, swapchain, render pass, command buffers.

```cpp
vde::VulkanContext context;
context.initialize(&window);
context.setRenderCallback([](VkCommandBuffer cmd) {
    // Bind pipeline, draw commands
});
context.drawFrame();
```

### Camera
Orbital camera with zoom and pan controls.

```cpp
vde::Camera camera;
camera.setPosition(glm::vec3(0, 10, 20));
camera.setTarget(glm::vec3(0, 0, 0));
camera.orbit(deltaYaw, deltaPitch);
camera.zoom(scrollDelta);
glm::mat4 viewProj = camera.getViewProjectionMatrix();
```

### ShaderCache
Compile GLSL shaders at runtime with automatic caching.

```cpp
vde::ShaderCache cache(device, "cache/");
auto vertModule = cache.getOrCompile("shaders/triangle.vert", vde::ShaderStage::Vertex);
auto fragModule = cache.getOrCompile("shaders/triangle.frag", vde::ShaderStage::Fragment);
```

### Texture
Load images and create Vulkan textures.

```cpp
vde::Texture texture;
texture.loadFromFile(context, "textures/sprite.png");
VkDescriptorImageInfo imageInfo = texture.getDescriptorInfo();
```

### HexGeometry
Generate hexagonal grid meshes for strategy games.

```cpp
auto hexMesh = vde::HexGeometry::generateFlatTop(1.0f);
// Use hexMesh.vertices and hexMesh.indices to create buffers
```

### HexPrismMesh
Generate 3D hexagonal prism meshes for terrain.

```cpp
auto prism = vde::HexPrismMeshGenerator::generate(1.0f, 0.5f, vde::HexOrientation::FlatTop);
// Creates top face, bottom face, and 6 side quads
```

## Dependencies

VDE uses CMake FetchContent for most dependencies:

| Dependency | Version | Purpose |
|------------|---------|---------|
| **Vulkan SDK** | 1.3+ | Graphics API (system install required) |
| **GLFW** | 3.x | Window/input management |
| **GLM** | 1.0+ | Mathematics library |
| **glslang** | 14.3.0 | Runtime shader compilation |
| **stb_image** | 2.30 | Image loading |
| **Google Test** | 1.14.0 | Unit testing |

## Configuration Options

CMake options for customization:

```cmake
# Build shared library instead of static
cmake .. -DVDE_SHARED_LIBS=ON

# Disable building tests
cmake .. -DVDE_BUILD_TESTS=OFF

# Disable building examples  
cmake .. -DVDE_BUILD_EXAMPLES=OFF
```

## Integration

### As a Subdirectory

```cmake
add_subdirectory(vdengine)
target_link_libraries(your_project PRIVATE vde)
```

### As an Installed Package

```cmake
find_package(vde REQUIRED)
target_link_libraries(your_project PRIVATE vde::vde)
```

## Testing

Unit tests use Google Test framework:

```powershell
# Build and run all tests
cd build
ctest -C Debug --output-on-failure

# Run specific test suite
.\tests\Debug\vde_tests.exe --gtest_filter=CameraTest.*

# Run with verbose output
.\tests\Debug\vde_tests.exe --gtest_list_tests
```

### Test Coverage

| Component | Coverage |
|-----------|----------|
| Window | Resolution, lifecycle |
| Camera | Matrices, orbital movement |
| HexGeometry | Dimensions, vertex counts |
| ShaderCache | Hash consistency |
| Types | Vertex/UBO structures |

## Examples

### Triangle Example

Basic Vulkan triangle rendering demonstrating:
- Window creation
- Vulkan context initialization
- Shader loading
- Render loop

```powershell
.\examples\Debug\triangle_example.exe
```

## Versioning

VDE follows semantic versioning. Current version: **0.1.0**

```cpp
#include <vde/Core.h>
std::cout << "VDE v" << vde::Version::major << "." 
          << vde::Version::minor << "." 
          << vde::Version::patch << std::endl;
```

## Roadmap

- [ ] ImGui integration for debug UI
- [ ] Basic 2D sprite renderer
- [ ] Texture atlasing
- [ ] Audio system (OpenAL)
- [ ] Scene graph

## License

[Your License Here]

## Acknowledgments

- [Vulkan Tutorial](https://vulkan-tutorial.com/) - Excellent Vulkan learning resource
- [GLFW](https://www.glfw.org/) - Cross-platform windowing
- [GLM](https://github.com/g-truc/glm) - OpenGL Mathematics
- [Dear ImGui](https://github.com/ocornut/imgui) - Immediate mode GUI (future integration)
- [Red Blob Games](https://www.redblobgames.com/grids/hexagons/) - Hexagonal grid reference
