# VDE - Vulkan Display Engine

A lightweight, reusable Vulkan-based rendering engine designed for rapid prototyping and game development.

## Overview

VDE provides a clean abstraction over Vulkan's verbose API while maintaining flexibility for advanced use cases. The engine is organized in two layers:

For a current picture of implemented, partial, and planned features, start with [docs/PROJECT_STATUS.md](docs/PROJECT_STATUS.md). It is the canonical high-level status document for this repository.

**Game API (high-level)** — the recommended way to build applications:
- **Scene System**: `Game`, `Scene`, `SceneGroup` with per-scene cameras, viewports, and lifecycle
- **Entity System**: `MeshEntity`, `SpriteEntity`, `PhysicsEntity`, `TextEntity`, and more
- **Animation**: Scene-owned tweening, sprite animation clips, and animated sprite entities
- **Physics**: 2D rigid-body simulation with collision detection
- **Audio**: Cross-platform audio playback via miniaudio (`AudioManager`, `AudioSource`)
- **Text Rendering**: TrueType and bitmap font rendering
- **Resource Manager**: Centralized asset caching and reference counting
- **Storage**: Persistent key-value store backed by SQLite
- **Transitions**: Fade, wipe, block-fall, and circle-reveal scene transitions
- **Input & Automation**: Input handlers, action maps, key tracking, and scripted input replay

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
- **Vulkan SDK 1.3+ or Vulkan development packages** — Vulkan must be installed and discoverable by CMake; on Windows, the LunarG SDK is the typical option and usually sets `VULKAN_SDK` ([Download](https://vulkan.lunarg.com/))
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

# Run smoke tests against examples, games, and tools
.\scripts\smoke-test.ps1

# Run the full verification pipeline
.\scripts\verify.ps1

# Run targeted lint checks for your current git delta
.\scripts\lint.ps1 -ChangedOnly

# Run golden-image render verification
.\scripts\render-verify.ps1

# Clean rebuild
.\scripts\rebuild.ps1

# Format C++ code
.\scripts\format.ps1

# Launch VLauncher (interactive example/game/tool browser)
.\scripts\run-vlauncher.ps1

# Root shortcut for VLauncher
.\run-vlauncher.ps1

# Enable local protection: block direct commits to main
.\scripts\install-hooks.ps1
```

For complete documentation see:
- `scripts/README.md` — Detailed script reference
- `docs/GETTING_STARTED.md` — Onboarding guide and first-application walkthrough
- `docs/PROJECT_STATUS.md` — Canonical implemented/partial/planned status
- `.github/skills/build-tool-workflows/SKILL.md` — Complete build guide

### Building Manually

```powershell
# Ninja (Debug, default) — requires a Developer PowerShell for VS 2022
mkdir build_ninja
cd build_ninja
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Debug
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
├── tests/                 # Unit tests (vde_tests + vde_resource_editor_tests)
├── examples/              # 43 example directories / 44 registered example targets
├── games/                 # 2 larger playable applications
├── tools/                 # 4 tools (VLauncher, geometry REPL, hex editor, resource editor)
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
| **nlohmann/json** | v3.11.3 | Metadata and import helpers |
| **Dear ImGui** | v1.91.8-docking | Debug UI (examples, games, and tools) |
| **Google Test** | 1.14.0 | Unit testing |

## Configuration Options

```cmake
# Build shared library instead of static
cmake .. -DVDE_SHARED_LIBS=ON

# Disable building tests
cmake .. -DVDE_BUILD_TESTS=OFF

# Disable building examples
cmake .. -DVDE_BUILD_EXAMPLES=OFF

# Disable building games
cmake .. -DVDE_BUILD_GAMES=OFF

# Disable building tools
cmake .. -DVDE_BUILD_TOOLS=OFF

# Enable compiler timing instrumentation for build benchmarking
cmake .. -DVDE_TIMING=ON
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

The repo currently registers two test executables: `vde_tests` for engine and Game API coverage, and `vde_resource_editor_tests` for resource-editor domain logic.

Coverage spans rendering helpers, scenes and entities, world units and bounds, physics, audio, input and scripted input execution, transitions, text and emoji rendering, sprite sheets and animation, launcher utilities, FLIP image comparison helpers, and resource-editor command infrastructure.

For the full local gate used by the repo, run:

```powershell
.\scripts\verify.ps1
```

## Examples

VDE currently ships with 44 registered example targets across 43 example directories. They cover low-level rendering, sprites, text, transitions, physics, audio, diagnostics, storage, input automation, camera feel, and multi-scene or multi-viewport workflows. Launch them interactively via VLauncher:

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
| `camera_feel_demo` | Camera2D follow, deadzone, look-ahead, and shake workflows |
| `input_actions_demo` | Named action mapping and persisted bindings |
| `imgui_demo` | Dear ImGui integration |
| `emoji_demo` | Color emoji rendering in engine text and ImGui |
| `text_adventure_demo` | Interactive text adventure capstone |
| `multi_scene_demo` | Multiple scenes with scene groups |
| `quad_viewport_demo` | Split-screen viewports |
| `transition_demo` | Scene transitions |
| `asteroids_demo` | Classic Asteroids plus a physics-based variant |
| `vertical_shooter` | Full top-down scrolling shooter |
| `breakout_demo` | Complete Breakout-style game |

## Games and Tools

Beyond the examples, VDE also ships with 2 games and 4 tools.

**Games**
- `pong` — compact multiplayer-ready game sample with a fuller gameplay loop
- `fishing_game` — 2D pond-fishing game showing the recommended `games/` layout

**Tools**
- `vlauncher` — interactive launcher for examples, games, and tools
- `resource_editor` — asset and command-system tool with dedicated tests
- `geometry_repl` — geometry experimentation and inspection tool
- `hex_editor` — hex-grid editing workflow

## Versioning

VDE follows semantic versioning. Current version: **0.1.0**

## License

Copyright (c) 2026 Derek Kowaluk

This project is licensed under the **MIT License** — see the [LICENSE](LICENSE) file for details.
You are free to use, modify, and distribute this software, provided the copyright notice and attribution to Derek Kowaluk are preserved in all copies or substantial portions of the Software.

## Acknowledgments

- [Vulkan Tutorial](https://vulkan-tutorial.com/) — Excellent Vulkan learning resource
- [GLFW](https://www.glfw.org/) — Cross-platform windowing
- [GLM](https://github.com/g-truc/glm) — OpenGL Mathematics
- [glslang](https://github.com/KhronosGroup/glslang) — Runtime shader compilation
- [miniaudio](https://miniaud.io/) — Cross-platform audio
- [Dear ImGui](https://github.com/ocornut/imgui) — Immediate mode GUI
- [Red Blob Games](https://www.redblobgames.com/grids/hexagons/) — Hexagonal grid reference
