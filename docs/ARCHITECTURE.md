# VDE Architecture

This document describes the high-level architecture and design decisions of the Vulkan Display Engine.

## Design Goals

1. **Simplicity** - Abstract Vulkan's complexity without hiding necessary control
2. **Reusability** - Generic engine usable across different game types
3. **Performance** - Minimal overhead, efficient GPU utilization
4. **Testability** - Components should be unit-testable where possible
5. **Incremental Adoption** - Low-level and high-level APIs coexist; advanced features are opt-in

## System Overview

VDE has a two-layer architecture:

```
┌─────────────────────────────────────────────────────────────────────────┐
│                           Application                                    │
├─────────────────────────────────────────────────────────────────────────┤
│                         Game API Layer                                   │
│  ┌──────────┐  ┌────────────┐  ┌─────────────────────────────────────┐  │
│  │   Game   │  │ Scheduler  │  │ SceneManager                        │  │
│  │          │  │ TaskGraph  │  │  ┌───────────┐   ┌───────────┐     │  │
│  │ Input    │  │ ThreadPool │  │  │ Scene A   │   │ Scene B   │     │  │
│  │ Timing   │  └────────────┘  │  │ Entities  │   │ Entities  │     │  │
│  │ Settings │                  │  │ Camera    │   │ Camera    │     │  │
│  └──────────┘                  │  │ Viewport  │   │ Viewport  │     │  │
│                                │  │ Physics   │   │ Physics   │     │  │
│  ┌──────────┐  ┌────────────┐  │  │ LightBox  │   │ LightBox  │     │  │
│  │ Resource │  │   Audio    │  │  └───────────┘   └───────────┘     │  │
│  │ Manager  │  │  Manager   │  └─────────────────────────────────────┘  │
│  └──────────┘  └────────────┘                                           │
├─────────────────────────────────────────────────────────────────────────┤
│                      Low-Level Rendering Layer                           │
│  ┌─────────┐  ┌──────────────┐  ┌────────┐  ┌─────────┐  ┌──────────┐ │
│  │ Window  │  │VulkanContext │  │ Camera │  │Texture  │  │  Shaders │ │
│  └────┬────┘  └──────┬───────┘  └────────┘  └─────────┘  └──────────┘ │
│       │              │                                                  │
│  ┌────┴────┐  ┌──────┴─────────────────────────────────────────────┐   │
│  │  GLFW   │  │    BufferUtils · DescriptorManager · UniformBuffer │   │
│  └─────────┘  └────────────────────────────────────────────────────┘   │
├─────────────────────────────────────────────────────────────────────────┤
│                          Vulkan Driver                                   │
└─────────────────────────────────────────────────────────────────────────┘
```

## Game API Layer

The Game API provides a high-level framework for building games. Most users interact exclusively with this layer.

### Game

Central manager for the game loop, scenes, input, and subsystems.

- Owns `Window`, `VulkanContext`, `Scheduler`, `ResourceManager`
- Manages named scenes with stack-based overlays
- Runs the main loop: `processInput() → scheduler.execute() → drawFrame()`
- Supports callbacks for subclassing (`onStart`, `onUpdate`, `onRender`, `onShutdown`)

**Dependencies**: Window, VulkanContext, Scheduler, ResourceManager, AudioManager

### Scheduler

Task-graph scheduler that replaces the hard-coded game loop body with a declarative graph.

- Tasks have phases (Input → GameLogic → Audio → Physics → PostPhysics → PreRender → Render)
- Topological sort with phase-based tiebreaking
- Optional multi-threaded execution via `ThreadPool` for independent tasks (e.g., per-scene physics)
- Graph is rebuilt automatically when scenes change

**Dependencies**: ThreadPool (optional)

### Scene

A game scene/state managing entities, resources, camera, lighting, and world bounds.

- Entity management (add/remove/query by ID, name, type, or physics body)
- Per-scene camera, lighting (`LightBox`), and background color
- Optional `ViewportRect` for split-screen rendering
- Optional `PhysicsScene` for 2D physics simulation (opt-in)
- Phase callbacks for 3-phase update model (GameLogic → Audio → Visual)
- Audio event queue for phase-decoupled sound playback
- Background update support (`setContinueInBackground`)

**Dependencies**: Game, Entity, GameCamera, LightBox, PhysicsScene (optional)

### Entity System

Game objects with transform (position, rotation, scale) and visual representation.

| Class | Description |
|-------|-------------|
| `Entity` | Base class with transform, visibility, lifecycle |
| `MeshEntity` | 3D mesh with optional material and texture |
| `SpriteEntity` | 2D sprite with texture, UV rect, anchor point |
| `PhysicsEntity` | Mixin for physics-body binding with interpolated sync |
| `PhysicsSpriteEntity` | SpriteEntity + PhysicsEntity |
| `PhysicsMeshEntity` | MeshEntity + PhysicsEntity |

### Physics System

2D physics simulation with fixed-timestep accumulator. Fully opt-in per scene.

- AABB broad-phase collision detection with impulse-based resolution
- Body types: Static, Dynamic, Kinematic; sensor bodies for triggers
- Collision callbacks (global and per-body, begin/end)
- Raycast and AABB spatial queries
- Automatic PostPhysics transform sync with interpolation
- Thread-safe: each `PhysicsScene` uses pimpl with no shared mutable state

### Audio System

Cross-platform audio using miniaudio (WAV, MP3, OGG, FLAC).

- `AudioManager` singleton for playback and mixing
- Separate volume channels: Master, Music, SFX
- 3D spatial audio with listener position/orientation
- `AudioEvent` queue decouples game logic from audio calls
- Scene convenience methods: `playSFX()`, `playSFXAt()`

### Multi-Scene & Split-Screen

- `SceneGroup` activates multiple scenes simultaneously
- `ViewportRect` assigns sub-regions of the window to scenes (normalized 0-1 coordinates)
- Independent cameras per viewport with per-scene UBO updates
- Input focus routing: keyboard to focused scene, mouse to viewport under cursor

## Low-Level Rendering Layer

Direct Vulkan abstractions for advanced rendering needs.

### Window (Window.h/cpp)
- GLFW window creation and management
- Input event polling, resize handling
- Vulkan surface creation, fullscreen toggle, resolution presets

### VulkanContext (VulkanContext.h/cpp)
Core Vulkan infrastructure:
- Instance, device, swapchain, render pass, framebuffers
- Command pool/buffers, synchronization primitives
- Frame rendering orchestration via render callback

### Camera (Camera.h/cpp)
Low-level 3D camera:
- Orbital controls (pitch, yaw, distance)
- View/projection matrix generation with Vulkan Y-flip
- Pan, zoom, translate operations

### Texture (Texture.h/cpp)
Vulkan texture management (inherits from `Resource`):
- Two-phase loading: CPU load via stb_image, GPU upload via staging buffer
- Image/view/sampler creation and cleanup

### BufferUtils (BufferUtils.h/cpp)
Static utilities for Vulkan buffer operations:
- Memory type finding, buffer creation, staging copies
- Device-local and mapped buffer helpers

### ShaderCompiler / ShaderCache
Runtime GLSL → SPIR-V compilation with disk-based caching.

### HexGeometry / HexPrismMesh
Specialized hexagonal mesh generation.

## Data Flow

### Game Loop Sequence (via Scheduler)

```
1. Game::run() starts main loop
2. Each frame:
   ├── processInput()              (GLFW poll + dispatch)
   ├── scheduler.execute()         (Task graph)
   │   ├── Input phase tasks
   │   ├── GameLogic phase tasks   (per-scene update or 3-phase callbacks)
   │   ├── Audio phase tasks       (per-scene audio queue drain + AudioManager::update)
   │   ├── Physics phase tasks     (per-scene physics step — parallelizable)
   │   ├── PostPhysics tasks       (sync physics transforms to entities)
   │   ├── PreRender tasks         (camera apply, clear color)
   │   └── Render task             (per-scene viewport/camera/scissor + entity render)
   └── drawFrame()                 (Vulkan submit + present)
```

### Multi-Viewport Render Sequence

```
For each scene in active SceneGroup:
  1. Compute VkViewport + VkRect2D from scene's ViewportRect
  2. vkCmdSetViewport / vkCmdSetScissor
  3. Update UBO with scene's camera view/projection
  4. Update lighting UBO from scene's LightBox
  5. scene->render() (iterates entities)
```

## Memory Management

### Ownership Model

| Object | Owner | Mechanism |
|--------|-------|-----------|
| Window, VulkanContext | Game | unique_ptr |
| Scene | Game | unique_ptr (map) |
| Entity | Scene | shared_ptr (vector) |
| Resource (Mesh, Texture) | Scene or ResourceManager | shared_ptr / weak_ptr cache |
| GameCamera, LightBox | Scene | unique_ptr |
| PhysicsScene | Scene | unique_ptr |
| InputHandler | External | raw pointer (not owned) |

### Resource Lifecycle
- Resources loaded CPU-side first, GPU upload deferred to first use
- `ResourceManager` uses weak_ptr caching for automatic deduplication
- GPU resources cleaned up on shutdown or when last reference drops

## Threading Model

- **Default**: Single-threaded. All Vulkan commands on main thread.
- **Opt-in parallelism**: `Scheduler::setWorkerThreadCount(N)` enables thread pool.
  - Only physics tasks (per-scene `step()`) are dispatched to workers.
  - All other tasks (input, rendering, audio) remain on the main thread.
  - `PhysicsScene` is thread-safe via pimpl with no shared mutable state.
- **ThreadPool**: Fixed-size worker pool with `submit()` / `waitAll()`.
  - Level-based frontier dispatch: all ready tasks submitted together, then waited on.
  - Degrades to inline execution with threadCount == 0.

## Test Coverage

604 unit tests across 24 test files covering:

| Category | Tests | Files |
|----------|-------|-------|
| Core rendering | Camera, Texture, Shader, HexGeometry, Types, Window | 7 |
| Game API | Entity, Scene, GameCamera, LightBox, Material, Mesh, Sprite, WorldBounds, WorldUnits, CameraBounds, ResourceManager | 11 |
| Multi-scene | Scheduler, SceneGroup, ViewportRect, AudioEvent | 4 |
| Physics | PhysicsScene, PhysicsEntity | 2 |
| Threading | ThreadPool | 1 |

## Examples

14 example programs demonstrating engine features:

| Example | Demonstrates |
|---------|-------------|
| `triangle` | Basic Vulkan rendering |
| `simple_game` | Game API basics |
| `sprite_demo` | 2D sprite rendering |
| `world_bounds_demo` | World coordinate system |
| `materials_lighting_demo` | Materials & multi-light rendering |
| `resource_demo` | Resource management |
| `audio_demo` | Audio system |
| `sidescroller` | 2D platformer mechanics |
| `wireframe_viewer` | Shape rendering |
| `multi_scene_demo` | Scene management & optional viewports |
| `quad_viewport_demo` | True 4-quadrant split-screen |
| `physics_demo` | Physics simulation |
| `parallel_physics_demo` | Multi-threaded physics |
| `physics_audio_demo` | Physics → game logic → audio pipeline |
