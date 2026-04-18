# VDE - Vulkan Display Engine

A lightweight, reusable Vulkan-based rendering engine designed for rapid prototyping and game development.

## Overview

VDE provides a clean abstraction over Vulkan's verbose API while maintaining flexibility for advanced use cases. The engine is organized in two layers:

**Game API (high-level)** — the recommended way to build applications:
- **Scene System**: `Game`, `Scene`, `SceneGroup` with per-scene cameras, viewports, and lifecycle
- **Entity System**: `MeshEntity`, `SpriteEntity`, `PhysicsEntity`, `TextEntity`, and more
- **Physics**: 2D rigid-body simulation with collision detection
- **Audio**: Cross-platform audio playback via miniaudio (`AudioManager`, `AudioSource`)
- **Text Rendering**: TrueType and bitmap font rendering
- **Resource Manager**: Centralized asset caching and reference counting
- **Storage**: Persistent key-value store backed by SQLite
- **Transitions**: Fade, wipe, block-fall, and circle-reveal scene transitions
- **Input Scripting**: Automated input replay for smoke testing

**Low-Level Rendering Layer** — for direct Vulkan control:
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
- **Vulkan SDK 1.3+** ([Download](https://vulkan.lunarg.com/)) — must be installed system-wide; the `VULKAN_SDK` environment variable must be set
- **C++20 compatible compiler**: Visual Studio 2022 (Windows), GCC 11+, or Clang 14+
- **Git** (for dependency fetching via CMake FetchContent)
- **Git LFS** (for asset files) — [Installation Guide](docs/GIT_LFS_SETUP.md)

> **Windows note:** The default build generator is **Ninja**, which requires the Visual Studio Developer environment to be active. Open a *Developer PowerShell for VS 2022* (or run `scripts/build.ps1`, which loads it automatically). Alternatively, use `-Generator MSBuild` with any PowerShell terminal.

### Building with the Scripts (Recommended)

VDE provides PowerShell scripts that handle all environment setup automatically:

```powershell
# Build (Ninja, Debug — default)
.\scripts\build.ps1

# Build with MSBuild (no special shell required)
.\scripts\build.ps1 -Generator MSBuild

# Release build
.\scripts\build.ps1 -Config Release

# Run unit tests (assumes a prior build)
.\scripts\test.ps1

# Build and run tests in one step
.\scripts\test.ps1 -Build

# Run specific tests
.\scripts\test.ps1 -Filter "CameraTest.*"

# Run smoke tests against all examples
.\scripts\smoke-test.ps1

# Clean rebuild
.\scripts\rebuild.ps1

# Format C++ code
.\scripts\format.ps1

# Launch VLauncher (interactive example browser)
.\run-vlauncher.ps1

# Enable local protection: block direct commits to main
.\scripts\install-hooks.ps1
```

For complete documentation see:
- `scripts/README.md` — Detailed script reference
- `.github/skills/build-tool-workflows/SKILL.md` — Complete build guide

### Building Manually

```powershell
# Ninja (default) — requires a Developer PowerShell for VS 2022
mkdir build_ninja
cd build_ninja
cmake .. -G Ninja
cmake --build .

# Run tests
.\tests\vde_tests.exe
```

```powershell
# MSBuild — works from any PowerShell terminal
mkdir build
cd build
cmake ..
cmake --build . --config Debug

# Run tests
.\tests\Debug\vde_tests.exe
```

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

### Game API (Recommended)

```cpp
#include <vde/api/GameAPI.h>

class MyScene : public vde::Scene {
public:
    void onEnter() override {
        setBackgroundColor(vde::Color(0.1f, 0.1f, 0.2f));

        auto cube = addEntity<vde::MeshEntity>();
        cube->setMesh(vde::Mesh::createCube());
        cube->setMaterial(vde::Material::createColored(vde::Color::red()));
        cube->setName("cube");
    }

    void update(float deltaTime) override {
        auto* cube = getEntityByName("cube");
        if (cube) {
            auto rot = cube->getRotation();
            cube->setRotation(rot.pitch, rot.yaw + 45.0f * deltaTime, rot.roll);
        }
        vde::Scene::update(deltaTime);
    }
};

int main() {
    vde::Game game;
    vde::GameSettings settings;
    settings.gameName = "My First VDE Game";
    settings.setWindowSize(1280, 720);

    game.initialize(settings);
    game.addScene("main", new MyScene());
    game.setActiveScene("main");
    game.run();
    return 0;
}
```

### Low-Level API

For direct Vulkan control without the Game API:

```cpp
#include <vde/Core.h>

int main() {
    vde::Window window(1280, 720, "My VDE Application");

    vde::VulkanContext context;
    if (!context.initialize(&window)) {
        return 1;
    }

    context.setRenderCallback([&](VkCommandBuffer cmd) {
        // Your rendering code here
    });

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
│   ├── Core.h             # Umbrella header (low-level API)
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
│   ├── Types.h            # Common types (Vertex, UBO)
│   └── api/               # Game API (high-level)
│       ├── GameAPI.h      # Convenience umbrella header
│       ├── Game.h         # Main game loop manager
│       ├── Scene.h        # Scene base class
│       ├── SceneGroup.h   # Multi-viewport scene groups
│       ├── Entity.h       # Entity base class
│       ├── PhysicsScene.h # 2D physics integration
│       ├── AudioManager.h # Audio playback
│       ├── TextRenderer.h # Font/text rendering
│       ├── ResourceManager.h # Asset caching
│       ├── StorageManager.h  # Persistent key-value store
│       └── ...            # Input, transitions, world units, …
├── src/                   # Implementation files
├── shaders/               # GLSL shader sources
├── tests/                 # Unit tests (Google Test)
├── examples/              # Example applications (30+)
├── tools/                 # Asset creation tools (VLauncher, geometry REPL, resource editor)
├── scripts/               # Build/test/format PowerShell scripts
└── third_party/           # Vendored dependencies (if present)
```

## Dependencies

VDE uses CMake FetchContent for most dependencies. Only the Vulkan SDK requires a manual system-wide install.

| Dependency | Version | Purpose |
|------------|---------|---------|
| **Vulkan SDK** | 1.3+ | Graphics API — **system install required** |
| **GLFW** | 3.4 | Window/input management |
| **GLM** | 1.0.1 | Mathematics library |
| **glslang** | 14.3.0 | Runtime GLSL → SPIR-V compilation |
| **stb_image** | v2.30 | Image loading |
| **miniaudio** | 0.11.21 | Cross-platform audio |
| **SQLite3** | 3.49.1 | Persistent key-value storage |
| **toml++** | v3.4.0 | TOML configuration parsing |
| **Dear ImGui** | latest | Debug UI (examples and tools only) |
| **Google Test** | 1.14.0 | Unit testing |

## Configuration Options

```cmake
# Build shared library instead of static
cmake .. -DVDE_SHARED_LIBS=ON

# Disable building tests
cmake .. -DVDE_BUILD_TESTS=OFF

# Disable building examples
cmake .. -DVDE_BUILD_EXAMPLES=OFF

# Disable building tools
cmake .. -DVDE_BUILD_TOOLS=OFF
```

## Integration

### As a Subdirectory

```cmake
cmake_minimum_required(VERSION 3.20)
project(MyGame)

add_subdirectory(path/to/vdengine)

add_executable(MyGame main.cpp)
target_link_libraries(MyGame PRIVATE vde)
```

> **Note:** `find_package(vde)` is not currently supported. Use `add_subdirectory` as shown above.

## Testing

Unit tests use Google Test:

```powershell
# Build and run all tests
.\scripts\test.ps1 -Build

# Run a specific test suite
.\scripts\test.ps1 -Filter "CameraTest.*"

# Using ctest directly (Ninja build)
cd build_ninja
ctest --output-on-failure
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

VDE ships with 34 example applications. Launch them interactively via VLauncher:

```powershell
.\run-vlauncher.ps1
```

Or run smoke tests across all examples to verify they start successfully:

```powershell
.\scripts\smoke-test.ps1
```

Notable examples:

| Example | Description |
|---------|-------------|
| `triangle` | Basic Vulkan triangle (low-level API) |
| `simple_game` | Minimal Game API usage |
| `physics_demo` | 2D rigid-body physics |
| `sprite_demo` | Sprite rendering |
| `audio_demo` | Audio playback |
| `imgui_demo` | Dear ImGui integration |
| `text_adventure_demo` | Text rendering |
| `multi_scene_demo` | Multiple scenes with scene groups |
| `quad_viewport_demo` | Split-screen viewports |
| `transition_demo` | Scene transitions |
| `asteroids_demo` | Complete Asteroids game |
| `breakout_demo` | Complete Breakout game |

## Versioning

VDE follows semantic versioning. Current version: **0.1.0**

## Acknowledgments

- [Vulkan Tutorial](https://vulkan-tutorial.com/) — Excellent Vulkan learning resource
- [GLFW](https://www.glfw.org/) — Cross-platform windowing
- [GLM](https://github.com/g-truc/glm) — OpenGL Mathematics
- [glslang](https://github.com/KhronosGroup/glslang) — Runtime shader compilation
- [miniaudio](https://miniaud.io/) — Cross-platform audio
- [Dear ImGui](https://github.com/ocornut/imgui) — Immediate mode GUI
- [Red Blob Games](https://www.redblobgames.com/grids/hexagons/) — Hexagonal grid reference
