# Getting Started with VDE

This guide will help you get up and running with the Vulkan Display Engine.

## Prerequisites

Before building VDE, ensure you have the following installed:

### Required
- **CMake 3.20+** - Build system
- **Vulkan SDK 1.3+** - [Download from LunarG](https://vulkan.lunarg.com/)
- **C++20 compatible compiler**:
  - Windows: Visual Studio 2022 or later
  - Linux: GCC 11+ or Clang 14+
  - macOS: Xcode 14+ with Clang
- **Git** - For fetching dependencies

### Recommended
- **RenderDoc** - GPU debugging and profiling
- **Visual Studio Code** with C++ extensions

## Building VDE

### Windows (PowerShell)

```powershell
# Clone the repository (or use as subdirectory)
cd vdengine

# Create build directory
mkdir build
cd build

# Configure
cmake ..

# Build
cmake --build . --config Debug

# Run tests
.\tests\Debug\vde_tests.exe
```

### Linux / macOS

```bash
cd vdengine
mkdir build && cd build
cmake ..
cmake --build .
./tests/vde_tests
```

## Your First VDE Application

Create a simple window with Vulkan rendering:

```cpp
#include <vde/Core.h>

int main() {
    // Create a window
    vde::Window window(1280, 720, "My VDE App");
    
    // Initialize Vulkan context
    vde::VulkanContext context;
    if (!context.initialize(&window)) {
        return 1;
    }
    
    // Set up a render callback
    context.setRenderCallback([](VkCommandBuffer cmd) {
        // Your rendering code here
        // This is called every frame with an active command buffer
    });
    
    // Main loop
    while (!window.shouldClose()) {
        window.pollEvents();
        context.drawFrame();
    }
    
    // Wait for GPU to finish before cleanup
    context.waitIdle();
    return 0;
}
```

### CMakeLists.txt for Your Project

```cmake
cmake_minimum_required(VERSION 3.20)
project(MyVDEApp)

# Add VDE as a subdirectory
add_subdirectory(path/to/vdengine)

# Create your executable
add_executable(MyApp main.cpp)

# Link against VDE
target_link_libraries(MyApp PRIVATE vde)
```

## Adding a Camera

```cpp
#include <vde/Core.h>

vde::Camera camera;

// Set up an orbital camera
camera.setFromPitchYaw(
    20.0f,              // Distance from target
    45.0f,              // Pitch angle (degrees)
    0.0f,               // Yaw angle (degrees)
    glm::vec3(0.0f)     // Target position
);

// Configure projection
camera.setPerspective(
    45.0f,              // Field of view (degrees)
    16.0f / 9.0f,       // Aspect ratio
    0.1f,               // Near plane
    100.0f              // Far plane
);

// In your render callback:
glm::mat4 viewProj = camera.getViewProjectionMatrix();
```

## Loading Textures

```cpp
#include <vde/Core.h>

vde::Texture texture;

// After VulkanContext initialization:
bool success = texture.loadFromFile(
    "assets/textures/sprite.png",
    context.getDevice(),
    context.getPhysicalDevice(),
    context.getCommandPool(),
    context.getGraphicsQueue()
);

if (success) {
    VkDescriptorImageInfo imageInfo = texture.getDescriptorInfo();
    // Use imageInfo in your descriptor set writes
}
```

## Generating Hex Geometry

```cpp
#include <vde/Core.h>

// Create a hex geometry generator
vde::HexGeometry hexGen(1.0f, vde::HexOrientation::FlatTop);

// Generate mesh for a single hex
vde::HexMesh mesh = hexGen.generateHex(glm::vec3(0.0f, 0.0f, 0.0f));

// mesh.vertices - vector of Vertex structs
// mesh.indices - vector of uint32_t indices
// Use these to create Vulkan vertex/index buffers
```

## Next Steps

- Explore the [examples](../examples/) folder for complete sample applications
- Read the [API Reference](API.md) for detailed documentation
- Check the [Architecture](ARCHITECTURE.md) document to understand the design

## Troubleshooting

### "Vulkan loader not found"
Ensure the Vulkan SDK is installed and `VULKAN_SDK` environment variable is set.

### "No Vulkan-capable GPU"
VDE requires a GPU with Vulkan support. Check your drivers are up to date.

### Build errors about missing headers
Run CMake configuration again - dependencies are fetched via FetchContent.

### Validation layer errors
Enable Vulkan validation layers by building in Debug mode. Check the Vulkan SDK documentation for interpreting validation messages.
