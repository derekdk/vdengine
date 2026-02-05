---
name: project-architecture-layout
description: Quick map of engine layout and key systems. Use this skill when you need to understand where files should be located.
---

# Project Layout

## When to use this skill
- You need to know where to place or find engine code, public headers, shaders, examples, or tests.
- You are wiring new features into the engine and want the correct header/implementation/test locations.
- You are orienting an AI agent to the repo so it can navigate modules without guessing.

## Directory map (top-level)
- include/vde/ – Public engine API (Window, VulkanContext, Camera, Texture, BufferUtils, ShaderCache, UniformBuffer, DescriptorManager, Types, helpers).
- include/vde/api/ – Game-facing API layer (Game, Scene, Entity, Mesh, CameraBounds, GameCamera, LightBox, WorldBounds, WorldUnits, input types).
- src/ – Engine implementations matching include/vde/ (VulkanContext, Window, Texture, BufferUtils, ShaderCompiler/ShaderCache, ImageLoader, HexGeometry, HexPrismMesh, DescriptorManager, UniformBuffer, ShaderHash, stb impl).
- src/api/ – Implementations for the gameplay API (Game, Scene, Entity, Mesh, GameCamera, CameraBounds, LightBox, GameTypes).
- shaders/ – GLSL sources for core examples (triangle, sprite, mesh variants).
- examples/ – Sample apps demonstrating engine usage (triangle, sprite demo, simple_game, world_bounds_demo). Each has its own CMake target.
- tests/ – Google Test suite covering engine and game API components (Camera, Window, VulkanContext-adjacent helpers, HexGeometry, Mesh, Scene, GameCamera, WorldBounds, etc.). Registered via tests/CMakeLists.txt.
- scripts/ – Build/test helpers (PowerShell build-and-test.ps1).
- docs/ – Project docs (ARCHITECTURE, API guides, getting started).
- build/, build_ninja/ – Generated build trees; do not edit.
- third_party/ – Vendored dependencies (stb) and CMake-managed externals (GLFW, GLM, glslang, GTest via FetchContent).

## Key modules and their homes
- Core: Window + VulkanContext handle GLFW surface + Vulkan instance/device/swapchain/render pass/command buffers/sync; lives in include/vde/{Window,VulkanContext}.h and src/{Window,VulkanContext}.cpp.
- Rendering helpers: Texture (image load + staging + sampler/view), BufferUtils (buffer creation/copies), DescriptorManager, UniformBuffer utilities; headers in include/vde/, impls in src/.
- Shaders: ShaderCompiler (glslang runtime compile) and ShaderCache (hash + cache) in include/vde/{ShaderCompiler,ShaderCache}.h and src/{ShaderCompiler,ShaderCache,ShaderHash}.cpp.
- Geometry: HexGeometry + HexPrismMesh generation utilities in include/vde/{HexGeometry,HexPrismMesh}.h with src counterparts.
- Math/Types: Types.h defines shared structs (vertices, uniforms); QueueFamilyIndices/SwapChainSupportDetails provide Vulkan selection helpers.
- Game API: High-level gameplay layer in include/vde/api/ with impls in src/api/ for Game, Scene, Entity, Mesh, GameCamera/Bounds, LightBox, GameTypes.
- Umbrella header: include/vde/Core.h re-exports public engine API for consumers.

## Build + test entry points
- CMake project root: CMakeLists.txt (engine + examples + tests). Generated builds live under build/ (VS solution) or build_ninja/.
- Scripted flow: scripts/build-and-test.ps1 (configure, build, run Google Tests). Accepts -BuildConfig and -Filter.
- Tests: built test binary under build/.../tests/Debug/vde_tests.exe when using the default VS generator.

## Placement guidance
- New public engine API: add header in include/vde/, implementation in src/, register target in root CMakeLists.txt, and include via Core.h if part of the main surface.
- New gameplay-facing API: add header in include/vde/api/, implementation in src/api/, register in CMake, and add coverage in tests/ with matching CMakeLists entry.
- New sample: add under examples/, update examples/CMakeLists.txt, provide shader assets in shaders/ if needed.
- New shader assets: place in shaders/, referenced by examples or engine runtime loaders.

## Notes for agents
- Namespace is vde::; prefer RAII around Vulkan handles. Check conventions skill for style details.
- Generated build directories should stay untouched; source edits belong in include/, src/, src/api/, shaders/, tests/, and examples/.