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

tests/
├── api/
│   ├── Entity_test.cpp      (Phase 1)
│   ├── GameCamera_test.cpp  (Phase 1)
│   ├── LightBox_test.cpp    (Phase 1)
│   ├── Scene_test.cpp       (Phase 1)
│   ├── Game_test.cpp        (Phase 1)
│   ├── Mesh_test.cpp        (Phase 2)
│   └── MeshEntity_test.cpp  (Phase 2)
├── Camera_test.cpp
├── ... (existing tests)
```

---

## Unit Testing Strategy

Tests validate implementation correctness at each phase. Run tests after implementing each file.

### Phase 1 Tests

#### Entity_test.cpp (~60 lines)
```cpp
class EntityTest : public ::testing::Test {
    // Test Entity base functionality (no Vulkan required)
};

TEST_F(EntityTest, DefaultConstructorGeneratesUniqueId)
TEST_F(EntityTest, SetPositionFloat3Works)
TEST_F(EntityTest, SetPositionStructWorks)
TEST_F(EntityTest, SetPositionVec3Works)
TEST_F(EntityTest, SetRotationWorks)
TEST_F(EntityTest, SetScaleUniformWorks)
TEST_F(EntityTest, SetScaleNonUniformWorks)
TEST_F(EntityTest, GetModelMatrixIdentityAtOrigin)
TEST_F(EntityTest, GetModelMatrixWithTranslation)
TEST_F(EntityTest, GetModelMatrixWithRotation)
TEST_F(EntityTest, GetModelMatrixWithScale)
TEST_F(EntityTest, GetModelMatrixCombined)  // TRS order
TEST_F(EntityTest, VisibilityDefaultTrue)
TEST_F(EntityTest, SetVisibleWorks)
```

#### GameCamera_test.cpp (~100 lines)
```cpp
class SimpleCameraTest : public ::testing::Test {};
class OrbitCameraTest : public ::testing::Test {};
class Camera2DTest : public ::testing::Test {};

// SimpleCamera
TEST_F(SimpleCameraTest, DefaultConstructor)
TEST_F(SimpleCameraTest, SetPositionUpdatesViewMatrix)
TEST_F(SimpleCameraTest, SetDirectionUpdatesViewMatrix)
TEST_F(SimpleCameraTest, SetFieldOfView)
TEST_F(SimpleCameraTest, MoveAddsToPosition)
TEST_F(SimpleCameraTest, RotateChangesPitchYaw)

// OrbitCamera
TEST_F(OrbitCameraTest, ConstructorWithTarget)
TEST_F(OrbitCameraTest, SetDistanceClampsToLimits)
TEST_F(OrbitCameraTest, SetPitchClampsToLimits)
TEST_F(OrbitCameraTest, SetYawWrapsAround)
TEST_F(OrbitCameraTest, RotateUpdatesPitchYaw)
TEST_F(OrbitCameraTest, ZoomChangesDistance)
TEST_F(OrbitCameraTest, PanMovesTarget)
TEST_F(OrbitCameraTest, CameraPositionCalculatedFromSpherical)

// Camera2D
TEST_F(Camera2DTest, DefaultConstructor)
TEST_F(Camera2DTest, SetPositionWorks)
TEST_F(Camera2DTest, SetZoomWorks)
TEST_F(Camera2DTest, SetRotationWorks)
TEST_F(Camera2DTest, ProjectionIsOrthographic)
```

#### LightBox_test.cpp (~40 lines)
```cpp
class LightBoxTest : public ::testing::Test {};

TEST_F(LightBoxTest, DefaultAmbientColor)
TEST_F(LightBoxTest, SetAmbientColor)
TEST_F(LightBoxTest, SetAmbientIntensity)
TEST_F(LightBoxTest, AddLightReturnsIndex)
TEST_F(LightBoxTest, RemoveLightWorks)
TEST_F(LightBoxTest, GetLightByIndex)
TEST_F(LightBoxTest, ClearLightsRemovesAll)
TEST_F(LightBoxTest, SimpleColorLightBoxConstructor)
TEST_F(LightBoxTest, ThreePointLightBoxHasThreeLights)
```

#### Scene_test.cpp (~80 lines)
```cpp
class SceneTest : public ::testing::Test {
    // Mock or minimal Game for testing
};

TEST_F(SceneTest, DefaultConstructor)
TEST_F(SceneTest, AddEntityReturnsPtr)
TEST_F(SceneTest, AddEntityIncrementsCount)
TEST_F(SceneTest, GetEntityById)
TEST_F(SceneTest, GetEntityByIdNotFound)
TEST_F(SceneTest, GetEntityByName)
TEST_F(SceneTest, RemoveEntityById)
TEST_F(SceneTest, ClearEntitiesRemovesAll)
TEST_F(SceneTest, SetCameraRawPointer)
TEST_F(SceneTest, SetCameraUniquePtr)
TEST_F(SceneTest, SetLightBoxRawPointer)
TEST_F(SceneTest, GetEffectiveLightingReturnsDefault)
TEST_F(SceneTest, GetEffectiveLightingReturnsCustom)
TEST_F(SceneTest, SetBackgroundColor)
TEST_F(SceneTest, UpdateCallsEntityUpdate)
TEST_F(SceneTest, SetInputHandler)
```

#### Game_test.cpp (~60 lines)
```cpp
class GameTest : public ::testing::Test {
    // Tests that don't require full Vulkan init
};

TEST_F(GameTest, DefaultConstructorNotInitialized)
TEST_F(GameTest, AddSceneStoresScene)
TEST_F(GameTest, AddSceneWithUniquePtr)
TEST_F(GameTest, GetSceneByName)
TEST_F(GameTest, GetSceneNotFound)
TEST_F(GameTest, RemoveSceneWorks)
TEST_F(GameTest, SetActiveSceneWorks)
TEST_F(GameTest, SetInputHandler)
TEST_F(GameTest, QuitSetsRunningFalse)
// Note: initialize() and run() require Vulkan, tested in integration
```

### Phase 1 Integration Test

#### GameIntegration_test.cpp (~40 lines)
```cpp
// Requires Vulkan-capable environment
class GameIntegrationTest : public ::testing::Test {
    // Skip if no Vulkan available
};

TEST_F(GameIntegrationTest, InitializeCreatesWindow)
TEST_F(GameIntegrationTest, InitializeWithSettings)
TEST_F(GameIntegrationTest, ShutdownCleansUp)
TEST_F(GameIntegrationTest, SceneLifecycleOnEnterCalled)
TEST_F(GameIntegrationTest, SceneLifecycleOnExitCalled)
TEST_F(GameIntegrationTest, SceneSwitchWorks)
```

### Phase 2 Tests

#### Mesh_test.cpp (~50 lines)
```cpp
class MeshTest : public ::testing::Test {};

TEST_F(MeshTest, DefaultConstructor)
TEST_F(MeshTest, SetDataStoresVertices)
TEST_F(MeshTest, SetDataStoresIndices)
TEST_F(MeshTest, SetDataCalculatesBounds)
TEST_F(MeshTest, CreateCubeHas36Indices)  // 6 faces * 2 tris * 3 verts
TEST_F(MeshTest, CreateCubeBoundsCorrect)
TEST_F(MeshTest, CreateSphereHasCorrectVertexCount)
TEST_F(MeshTest, CreatePlaneHas6Indices)
TEST_F(MeshTest, LoadFromFileOBJ)  // Simple cube.obj test file
```

#### MeshEntity_test.cpp (~30 lines)
```cpp
class MeshEntityTest : public ::testing::Test {};

TEST_F(MeshEntityTest, DefaultConstructor)
TEST_F(MeshEntityTest, SetMeshDirectPtr)
TEST_F(MeshEntityTest, SetMeshId)
TEST_F(MeshEntityTest, SetTextureDirectPtr)
TEST_F(MeshEntityTest, SetTextureId)
TEST_F(MeshEntityTest, SetColor)
TEST_F(MeshEntityTest, InheritsFromEntity)
```

---

## Test Execution Plan

Run tests incrementally as each file is implemented:

```
# After Entity.cpp
ctest -R Entity_test --output-on-failure

# After GameCamera.cpp
ctest -R "Camera_test|GameCamera_test" --output-on-failure

# After LightBox.cpp
ctest -R LightBox_test --output-on-failure

# After Scene.cpp
ctest -R Scene_test --output-on-failure

# After Game.cpp (Phase 1 complete)
ctest -R "Entity_test|GameCamera_test|LightBox_test|Scene_test|Game_test" --output-on-failure

# Full test suite
ctest --output-on-failure
```

### Test File Updates to CMakeLists.txt

Add to `tests/CMakeLists.txt`:
```cmake
# Game API Tests (Phase 1)
set(VDE_API_TEST_SOURCES
    api/Entity_test.cpp
    api/GameCamera_test.cpp
    api/LightBox_test.cpp
    api/Scene_test.cpp
    api/Game_test.cpp
)

# Append to VDE_TEST_SOURCES
list(APPEND VDE_TEST_SOURCES ${VDE_API_TEST_SOURCES})
```

---

## Test Success Criteria

| Phase | Milestone | Tests Passing |
|-------|-----------|---------------|
| 1.1 | Entity.cpp done | Entity_test: 14/14 |
| 1.2 | GameCamera.cpp done | GameCamera_test: 18/18 |
| 1.3 | LightBox.cpp done | LightBox_test: 9/9 |
| 1.4 | Scene.cpp done | Scene_test: 17/17 |
| 1.5 | Game.cpp done | Game_test: 9/9, all Phase 1: 67/67 |
| 1.6 | simple_game links | Example runs, shows window |
| 2.1 | Mesh.cpp done | Mesh_test: 9/9 |
| 2.2 | MeshEntity.cpp done | MeshEntity_test: 7/7 |
| 2.3 | simple_game renders | Spinning cube visible |

---

## Implementation Order (Recommended)

Each step includes implementing the source file AND its corresponding test file:

```
1. Entity.cpp + Entity_test.cpp        (~80+60 lines)  - Transform math
2. GameCamera.cpp + GameCamera_test.cpp (~180+100 lines) - Camera wrappers  
3. LightBox.cpp + LightBox_test.cpp    (~40+40 lines)  - Light management
4. Scene.cpp + Scene_test.cpp          (~120+80 lines) - Entity container + lifecycle
5. MeshEntity.cpp (stub)               (~30 lines)     - Stub for Phase 1
6. Game.cpp + Game_test.cpp            (~250+60 lines) - Main loop + initialization
7. Update CMakeLists.txt               - Add sources and tests
--- At this point, simple_game compiles and shows a window ---
--- All Phase 1 tests pass (67 tests) ---
8. Mesh.cpp + Mesh_test.cpp            (~200+50 lines) - Geometry + GPU buffers
9. MeshEntity.cpp full + test          (~70+30 lines)  - 3D rendering
10. Default pipeline                   (~150 lines)    - Shaders + pipeline creation
--- At this point, simple_game shows a spinning cube ---
--- All Phase 2 tests pass (83 tests total) ---
```

---

## Estimated Effort

| Phase | Files | Lines of Code | Test Lines | Complexity |
|-------|-------|---------------|------------|------------|
| Phase 1 | 6 src + 5 test | ~700 | ~340 | Medium |
| Phase 2 | 2+ src + 2 test | ~400 | ~80 | Medium-High |
| Phase 3 | 3+ src + tests | ~1000+ | ~200+ | High |

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
4. ✅ Add unit testing strategy to plan
5. Create `src/api/` and `tests/api/` directories
6. Implement Entity.cpp + Entity_test.cpp, run tests
7. Implement GameCamera.cpp + GameCamera_test.cpp, run tests
8. Implement LightBox.cpp + LightBox_test.cpp, run tests
9. Implement Scene.cpp + Scene_test.cpp, run tests
10. Implement MeshEntity.cpp (stub)
11. Implement Game.cpp + Game_test.cpp, run tests
12. Update CMakeLists.txt with new sources
13. Build simple_game - verify it runs and shows window
14. Run full test suite - verify 67 Phase 1 tests pass
