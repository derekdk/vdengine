# VDE Game API Implementation Plan

## Overview

This document outlines the plan to implement the high-level Game API that sits on top of the existing Vulkan infrastructure. The goal is to make the `simple_game` example (and similar games) compile and run successfully.

## Current State

### What Exists (Low-Level Layer)
- ✅ `Window` - GLFW window management
- ✅ `VulkanContext` - Vulkan instance, device, swapchain, render pass, frame sync
- ✅ `Camera` - View/projection matrix computation
- ✅ `BufferUtils` - Vulkan buffer creation utilities
- ✅ `UniformBuffer` - UBO management
- ✅ `DescriptorManager` - Descriptor set/pool management
- ✅ `ShaderCompiler` / `ShaderCache` - GLSL→SPIR-V compilation
- ✅ `Texture` / `ImageLoader` - Image loading and Vulkan textures
- ✅ `HexGeometry` / `HexPrismMesh` - Specialized geometry

### API Changes Made (2026-01-31)
The following API improvements were made before implementation:

1. **Entity.h** - `MeshEntity` now supports both direct `shared_ptr<Mesh>` ownership AND resource IDs
   - Added `setMesh(shared_ptr<Mesh>)` / `getMesh()` for direct ownership
   - Added `setMeshId(ResourceId)` for resource-managed meshes  
   - Same pattern for textures: `setTexture(shared_ptr<Texture>)` / `setTextureId()`

2. **Scene.h** - Made LightBox and Camera optional with defaults
   - Added `getEffectiveLighting()` to return default if none set
   - Updated docs to clarify defaults are used when not explicitly set

3. **GameCamera.h** - Added `applyTo(VulkanContext&)` method
   - Allows easy transfer of camera matrices to the rendering context
   - Added `getAspectRatio()` getter
   - Fixed unused parameter warning in `update()`

4. **GameSettings.h** - Fixed parameter shadowing warning
   - Renamed `setFullscreen(bool fullscreen)` → `setFullscreen(bool fs)`

### What's Missing (High-Level Game API)
All headers in `include/vde/api/` exist but have **no implementations**.

**Current Linker Errors (19 unresolved symbols):**
```
Entity:
  - setPosition(float, float, float)
  - setRotation(const Rotation&)

MeshEntity:
  - MeshEntity()
  - render()

OrbitCamera:
  - OrbitCamera(const Position&, float, float, float)
  - setDistance(float)

Scene:
  - Scene()
  - ~Scene()
  - update(float)
  - render()
  - setCamera(GameCamera*)

Game:
  - Game()
  - ~Game()
  - initialize(const GameSettings&)
  - shutdown()
  - run()
  - quit()
  - addScene(const string&, Scene*)
  - setActiveScene(const string&)
```

---

## Implementation Phases

### Phase 1: Core Infrastructure (Minimal Viable Product)

**Goal:** Get the simple_game example to compile, link, and show a window with a clear color.

#### 1.1 Entity Base Class (`src/api/Entity.cpp`) ~80 lines
```cpp
// Implement:
- Static ID counter: EntityId Entity::s_nextId = 1;
- Entity::Entity() - Generate unique ID
- Entity::setPosition(float, float, float)
- Entity::setPosition(const Position&)
- Entity::setPosition(const glm::vec3&)
- Entity::setRotation(float, float, float)  
- Entity::setRotation(const Rotation&)
- Entity::setScale(float) / setScale(float, float, float)
- Entity::getModelMatrix() - Compute TRS matrix
```

#### 1.2 GameCamera Classes (`src/api/GameCamera.cpp`) ~180 lines
```cpp
// Implement:
- GameCamera::applyTo(VulkanContext&) - Copy matrices to context camera

- SimpleCamera::SimpleCamera()
- SimpleCamera::setPosition/setDirection
- SimpleCamera::updateVectors()

- OrbitCamera::OrbitCamera(...)
- OrbitCamera::setTarget/setDistance/setPitch/setYaw
- OrbitCamera::rotate/zoom/pan
- OrbitCamera::updateCamera() - Compute position from spherical coords

- Camera2D::Camera2D()
- Camera2D::setPosition/setZoom/setRotation
```

#### 1.3 LightBox Classes (`src/api/LightBox.cpp`) ~40 lines
```cpp
// Implement:
- LightBox::addLight() / removeLight()
- ThreePointLightBox::ThreePointLightBox()
// Note: SimpleColorLightBox is already inline in header
```

#### 1.4 Scene Base Class (`src/api/Scene.cpp`) ~120 lines
```cpp
// Implement:
- Scene::Scene() / ~Scene()
- Scene::update(float deltaTime) - Call update on all entities
- Scene::render() - Call render on all visible entities
- Scene::addEntity(Entity::Ref) - Store and call onAttach
- Scene::getEntity(EntityId) / getEntityByName()
- Scene::removeEntity(EntityId)
- Scene::clearEntities()
- Scene::setCamera(GameCamera*) / setCamera(unique_ptr)
- Scene::setLightBox(LightBox*) / setLightBox(unique_ptr)
- Scene::getEffectiveLighting() - Return default if none set
- Scene::removeResource(ResourceId)
```

#### 1.5 Game Class (`src/api/Game.cpp`) ~250 lines
```cpp
// Implement:
- Game::Game() / ~Game()
- Game::initialize(const GameSettings&)
  - Create Window with settings
  - Create VulkanContext and initialize
  - Set up input callbacks via GLFW
- Game::shutdown()
- Game::run() - Main loop
  - Poll events
  - Update timing (deltaTime, FPS)
  - Process scene changes
  - Apply scene camera to VulkanContext
  - Call active scene update()
  - Draw frame via VulkanContext (with scene render in callback)
- Game::quit() - Set m_running = false
- Game::addScene(name, Scene*)
- Game::removeScene(name)
- Game::setActiveScene(name)
- Game::pushScene/popScene
```

#### 1.6 MeshEntity Stub (`src/api/MeshEntity.cpp`) ~30 lines
```cpp
// Phase 1 stub:
- MeshEntity::MeshEntity() - Initialize members
- MeshEntity::render() - Empty for Phase 1 (no rendering yet)
```

#### 1.7 CMakeLists.txt Updates
Add new source files to `VDE_SOURCES`:
```cmake
src/api/Entity.cpp
src/api/Scene.cpp
src/api/Game.cpp
src/api/GameCamera.cpp
src/api/LightBox.cpp
src/api/MeshEntity.cpp
```

**Phase 1 Total: ~700 lines**

---

### Phase 2: Mesh Rendering

**Goal:** Render actual 3D meshes (MeshEntity shows a cube).

#### 2.1 Mesh Resource (`src/api/Mesh.cpp`) ~200 lines
```cpp
// Implement:
- Mesh::loadFromFile() - Parse OBJ files (simple subset)
- Mesh::setData() - Store vertices/indices, compute bounds
- Mesh::createCube() / createSphere() / createPlane()
- Mesh::calculateBounds()
```

#### 2.2 GPU Mesh Buffers
Add to Mesh or create MeshBuffer helper:
```cpp
- VkBuffer m_vertexBuffer, m_indexBuffer
- Upload vertex/index data to GPU via BufferUtils
- bind(VkCommandBuffer) - Bind vertex/index buffers
- cleanup() - Destroy buffers
```

#### 2.3 MeshEntity Full Implementation (`src/api/MeshEntity.cpp`)
```cpp
// Implement:
- MeshEntity::render()
  - Get Mesh (from m_mesh or via scene by m_meshId)
  - Bind mesh buffers
  - Push model matrix as push constant
  - Draw indexed
```

#### 2.4 Default Rendering Pipeline
Create a default 3D pipeline in Game:
- Standard vertex shader (MVP transform)
- Standard fragment shader (color + simple lighting)
- Pipeline layout with push constants for model matrix
- Descriptor sets for view/projection UBO

**Phase 2 Total: ~400 lines**

---

### Phase 3: Advanced Features

#### 3.1 SpriteEntity (`src/api/SpriteEntity.cpp`)
- 2D quad rendering
- Texture sampling with UV rectangles
- Sprite batching for performance

#### 3.2 Resource Manager
- Automatic texture/mesh loading
- Reference counting
- Async loading support

#### 3.3 Full Lighting System
- Upload light data to GPU (SSBO or UBO)
- Multiple lights per scene
- Shadow mapping (optional)

#### 3.4 Input System Improvements
- Gamepad support via GLFW
- Action mapping

---

## File Structure After Implementation

```
src/
├── api/
│   ├── Entity.cpp       (Phase 1)
│   ├── MeshEntity.cpp   (Phase 1 stub, Phase 2 full)
│   ├── SpriteEntity.cpp (Phase 3)
│   ├── Scene.cpp        (Phase 1)
│   ├── Game.cpp         (Phase 1)
│   ├── GameCamera.cpp   (Phase 1)
│   ├── LightBox.cpp     (Phase 1)
│   └── Mesh.cpp         (Phase 2)
├── BufferUtils.cpp
├── Camera.cpp
├── ... (existing files)
```

---

## Implementation Order (Recommended)

```
1. Entity.cpp          (~80 lines)  - Transform math
2. GameCamera.cpp      (~180 lines) - Camera wrappers  
3. LightBox.cpp        (~40 lines)  - Light management
4. Scene.cpp           (~120 lines) - Entity container + lifecycle
5. MeshEntity.cpp      (~30 lines)  - Stub for Phase 1
6. Game.cpp            (~250 lines) - Main loop + initialization
--- At this point, simple_game compiles and shows a window ---
7. Mesh.cpp            (~200 lines) - Geometry + GPU buffers
8. MeshEntity.cpp full (~70 lines)  - 3D rendering
9. Default pipeline    (~150 lines) - Shaders + pipeline creation
--- At this point, simple_game shows a spinning cube ---
```

---

## Estimated Effort

| Phase | Files | Lines of Code | Complexity |
|-------|-------|---------------|------------|
| Phase 1 | 6 | ~700 | Medium |
| Phase 2 | 2+ | ~400 | Medium-High |
| Phase 3 | 3+ | ~1000+ | High |

**Phase 1 alone** is sufficient to prove the API design works and have a running window.

---

## Key Design Decisions

1. **Game owns Window + VulkanContext** (composition, not inheritance)
2. **Scene::render()** is called inside `VulkanContext::drawFrame()`'s render callback
3. **Entities are updated/rendered per-frame** in Scene's `update()` / `render()`
4. **GPU resources** (Mesh buffers) created via existing `BufferUtils`
5. **GameCamera::applyTo()** bridges game cameras to VulkanContext
6. **LightBox is optional** - default white ambient if not set
7. **Mesh ownership is flexible** - direct shared_ptr OR resource IDs

---

## Next Steps

1. ✅ Update API headers with simplifications
2. ✅ Update simple_game example  
3. ✅ Test compile (19 linker errors as expected)
4. Create `src/api/` directory
5. Implement Phase 1 files in order
6. Update CMakeLists.txt
7. Build and iterate until simple_game runs
8. Add unit tests for new classes
