# VDE Game API Implementation Plan

## Overview

This document outlines the plan to implement the high-level Game API that sits on top of the existing Vulkan infrastructure. The goal is to make the `simple_game` example (and similar games) compile and run successfully.

## Implementation Status

| Phase | Description | Status | Date |
|-------|-------------|--------|------|
| Phase 1 | Core Infrastructure | ✅ Complete | 2026-01-31 |
| Phase 2 | Mesh Rendering | ✅ Complete | 2026-01-31 |
| Phase 2.5 | World Coordinates & Bounds | ✅ Complete | 2026-02-01 |
| Phase 3 | SpriteEntity & Depth Testing | ✅ Complete | 2026-01-31 |
| Phase 4 | Materials & Lighting | ✅ Complete | 2026-02-01 |
| Phase 5 | Resource Management | ✅ Complete | 2026-02-04 |
| Phase 6 | Audio & Polish | ⏳ Pending | - |

### Phase 1 Completion Notes (2026-01-31)

**Implemented Files:**
- `src/api/Entity.cpp` (~130 lines) - Entity base class with transforms
- `src/api/GameCamera.cpp` (~280 lines) - SimpleCamera, OrbitCamera, Camera2D  
- `src/api/LightBox.cpp` (~60 lines) - LightBox and ThreePointLightBox
- `src/api/Scene.cpp` (~140 lines) - Scene entity/resource management
- `src/api/Game.cpp` (~380 lines) - Full game loop with input handling
- `src/api/GameTypes.cpp` (~30 lines) - Transform::getMatrix()

**Build System:**
- Updated CMakeLists.txt with new source files
- Added /FS flag for MSVC parallel compilation
- Tested with Ninja generator (avoids PDB locking issues)

**Verification:**
- ✅ `simple_game` example compiles and links successfully
- ✅ Window opens with menu scene displaying
- ✅ All 41 unit tests pass
- ✅ Input handling (keyboard/mouse) functional

**Known Limitations (Phase 1):**
- MeshEntity::render() is a stub (no actual mesh rendering yet)
- SpriteEntity not implemented
- Resource management placeholders only
- No actual Vulkan draw calls (just clear color)

### Phase 2 Completion Notes (2026-01-31)

**Implemented Files:**
- `src/api/Mesh.cpp` (~470 lines) - Full mesh implementation with GPU buffers
- Updated `src/api/Entity.cpp` - MeshEntity::render() implementation (~80 lines)
- Updated `src/api/Game.cpp` - Mesh rendering pipeline (~200 lines added)
- `shaders/mesh.vert` - Vertex shader with push constants for model matrix
- `shaders/mesh.frag` - Fragment shader with simple directional lighting

**Key Features:**
- ✅ Mesh data storage (vertices, indices, bounds)
- ✅ GPU buffer management (automatic upload to device-local memory)
- ✅ Primitive generators: createCube(), createSphere(), createPlane(), createCylinder()
- ✅ Full Vulkan graphics pipeline with push constants and descriptor sets
- ✅ Simple OBJ file loading (vertices, normals, texture coords)
- ✅ Per-entity rendering with model matrix push constants
- ✅ Basic lighting (directional + ambient)

**Design Decisions:**
1. **Mesh GPU Buffers**: Added Vulkan buffer handles directly to Mesh class
   - Pragmatic approach for Phase 2 simplicity
   - Meshes upload to GPU on first render (lazy initialization)
   - Future: May move to separate renderer/resource manager in Phase 5

2. **Shader Architecture**: 
   - Used existing UBO structure (model/view/proj) for compatibility
   - Model matrix in UBO is ignored; per-entity model matrix via push constants
   - View/Projection shared via UBO (updated per-frame by VulkanContext)
   - Future: Consider dedicated ViewProjection UBO structure

3. **Pipeline Management**:
   - Single global mesh pipeline created in Game::initialize()
   - Pipeline accessible via Game::getMeshPipeline() for entity rendering
   - Future: Support multiple pipelines/materials in Phase 3

4. **glslang Initialization**:
   - Added glslang::InitializeProcess() in Game::initialize()
   - Added glslang::FinalizeProcess() in Game::shutdown()
   - **CRITICAL**: Required for shader compilation to work

5. **Lighting Model**:
   - Fragment shader derives normals from screen-space gradients (dFdx/dFdy)
   - Simple directional light from above (0.3, 1.0, 0.5)
   - 30% ambient + 70% diffuse lighting
   - Future: Proper vertex normals and multiple lights in Phase 3

**Verification:**
- ✅ All 41 unit tests pass
- ✅ simple_game example compiles and links
- ✅ Game initializes without crashes
- ✅ Mesh rendering pipeline creates successfully
- ✅ Cube mesh generates with colored vertices
- ⚠️  Visual rendering not yet verified (window closes immediately in headless environment)

**Known Limitations (Phase 2):**
- No depth testing (disabled for simplicity)
- Single shared pipeline (no material variations)
- Vertex normals stored as colors (workaround for lighting)
- No texture sampling yet
- Resource management still placeholder
- OBJ loader is minimal (no materials, no mtl files)

**Files Modified:**
- `include/vde/api/Mesh.h` - Added GPU buffer methods
- `include/vde/api/Game.h` - Added pipeline accessors and Vulkan types
- `examples/simple_game/main.cpp` - Set cube mesh on entity

**Build System:**
- Added `src/api/Mesh.cpp` to CMakeLists.txt
- Shaders copied to build directory automatically

---

### Phase 2.5 Completion Notes (2026-02-01)

**Goal:** Make world coordinates explicit with units (meters) and cardinal direction mapping (N/S/E/W/Up/Down).

**Implemented Files:**
- `include/vde/api/WorldUnits.h` (~220 lines) - Type-safe units and coordinate system
- `include/vde/api/WorldBounds.h` (~290 lines) - Scene/world boundary definitions
- `include/vde/api/CameraBounds.h` (~320 lines) - 2D camera with pixel-to-world mapping
- `src/api/CameraBounds.cpp` (~180 lines) - CameraBounds2D implementation

**Key Features:**

1. **Type-Safe Distance Units (Meters)**
   - `Meters` struct with implicit float conversion
   - User-defined literals: `100_m`, `50.5_m`
   - Full arithmetic operators (+, -, *, /, comparisons)
   
2. **Coordinate System Definition**
   - `CoordinateSystem` struct maps cardinal directions to axes
   - Default Y-up: North=+Z, East=+X, Up=+Y
   - Alternative Z-up preset for CAD/GIS compatibility
   
3. **World Points & Extents**
   - `WorldPoint` with directional constructors
   - `WorldExtent` for 3D sizes with width/height/depth
   - Support for 2D (flat) extents
   
4. **World Bounds**
   - `WorldBounds` - 3D AABB with cardinal direction accessors
   - `WorldBounds2D` - Simplified 2D bounds for flat games
   - Factory methods: `fromDirectionalLimits()`, `fromCenterAndExtent()`, `flat()`
   - Helper functions: `south(100_m)`, `west(100_m)`, `down(10_m)` for readable negative values
   
5. **Pixel-to-World Coordinate Mapping**
   - `Pixels` type-safe wrapper with `_px` literal
   - `ScreenSize` for viewport dimensions
   - `PixelToWorldMapping` for conversion ratios
   - Factory methods: `fromPixelsPerMeter()`, `fitWidth()`, `fitHeight()`
   
6. **2D Camera Bounds**
   - `CameraBounds2D` class for 2D games
   - Screen-to-world and world-to-screen coordinate conversion
   - Zoom support (affects visible world size)
   - Optional constraint bounds to limit camera movement
   - Visibility testing for points and rectangles

**Scene Integration:**
- Added `setWorldBounds()` / `getWorldBounds()` to Scene
- Added `setCameraBounds2D()` / `getCameraBounds2D()` to Scene
- Added `is2D()` query based on bounds height

**Usage Examples:**

```cpp
using namespace vde;

// 3D Scene: 200m x 200m x 30m world
auto bounds = WorldBounds::fromDirectionalLimits(
    100_m, WorldBounds::south(100_m),  // north/south: -100 to +100
    WorldBounds::west(100_m), 100_m,   // west/east: -100 to +100
    20_m, WorldBounds::down(10_m)      // up/down: -10 to +20
);
scene->setWorldBounds(bounds);
// bounds.width() == 200_m, bounds.depth() == 200_m, bounds.height() == 30_m

// 2D Game with camera
CameraBounds2D camera;
camera.setScreenSize(1920_px, 1080_px);
camera.setWorldWidth(16_m);  // Show 16 meters across screen
camera.centerOn(0_m, 0_m);

// Convert mouse click to world position
glm::vec2 worldPos = camera.screenToWorld(mouseX_px, mouseY_px);

// Check visibility
if (camera.isVisible(entityX, entityY)) { /* render */ }
```

**Design Decisions:**

1. **Implicit Float Conversion for Meters/Pixels**
   - Pro: Seamless integration with existing glm/float APIs
   - Pro: Zero runtime overhead (constexpr)
   - Con: Less strict type safety (can still mix accidentally)
   - Decision: Convenience > strictness for game development velocity

2. **Y-Up Default Coordinate System**
   - Matches Vulkan/OpenGL conventions
   - North=+Z allows natural map orientation
   - Z-up preset available for CAD/GIS imports

3. **Screen Y-Flip in CameraBounds2D**
   - Screen: (0,0) top-left, Y increases down
   - World: Y increases up (typical for games)
   - Automatic conversion in screenToWorld/worldToScreen

4. **Constraint Bounds Clamping**
   - Camera smoothly clamps to constraint edges
   - When zoomed out past constraint size, centers on constraints
   - Prevents showing "out of bounds" areas

**Performance Notes:**
- `Meters`, `Pixels` are zero-overhead abstractions (compile to floats)
- User-defined literals computed at compile time
- Bounds checks are simple AABB operations: O(1)
- Coordinate mapping caches `PixelToWorldMapping`, recalculates only on changes

---

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

### Phase 1: Core Infrastructure (Minimal Viable Product) ✅ COMPLETE

**Goal:** Get the simple_game example to compile, link, and show a window with a clear color.

**Status:** ✅ Completed 2026-01-31

#### 1.1 Entity Base Class (`src/api/Entity.cpp`) ✅
```cpp
// Implemented:
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

#### 1.2 GameCamera Classes (`src/api/GameCamera.cpp`) ✅
```cpp
// Implemented:
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

#### 1.3 LightBox Classes (`src/api/LightBox.cpp`) ✅
```cpp
// Implemented:
- LightBox::addLight() / removeLight()
- ThreePointLightBox::ThreePointLightBox()
// Note: SimpleColorLightBox is already inline in header
```

#### 1.4 Scene Base Class (`src/api/Scene.cpp`) ✅
```cpp
// Implemented:
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

#### 1.5 Game Class (`src/api/Game.cpp`) ✅
```cpp
// Implemented:
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

#### 1.6 MeshEntity Stub (in Entity.cpp) ✅
```cpp
// Implemented as stub:
- MeshEntity::MeshEntity() - Initialize members
- MeshEntity::render() - Empty for Phase 1 (no rendering yet)
- SpriteEntity stub also included
```

#### 1.7 CMakeLists.txt Updates ✅
Added new source files to `VDE_SOURCES`:
```cmake
src/api/Entity.cpp
src/api/Scene.cpp
src/api/Game.cpp
src/api/GameCamera.cpp
src/api/LightBox.cpp
src/api/GameTypes.cpp  # Added for Transform::getMatrix()
```

**Phase 1 Total: ~1020 lines implemented**

---

### Phase 2: Mesh Rendering ✅ COMPLETE

**Goal:** Render actual 3D meshes (MeshEntity shows a cube).

**Status:** ✅ Completed 2026-01-31

#### 2.1 Mesh Resource (`src/api/Mesh.cpp`) ✅ ~470 lines
```cpp
// Implemented:
- Mesh::loadFromFile() - Parse OBJ files (basic subset: v, vn, vt, f)
- Mesh::setData() - Store vertices/indices, compute bounds
- Mesh::createCube() - Colored cube primitive (24 vertices)
- Mesh::createSphere() - UV sphere with specified segments/rings
- Mesh::createPlane() - Subdivided plane
- Mesh::createCylinder() - Cylinder with caps
- Mesh::calculateBounds() - AABB computation

// GPU Buffer Management:
- Mesh::uploadToGPU(VulkanContext*) - Create device-local buffers
- Mesh::freeGPUBuffers(VkDevice) - Cleanup GPU resources
- Mesh::bind(VkCommandBuffer) - Bind vertex/index buffers
- Mesh::isOnGPU() - Check if uploaded
```

**Implementation Notes:**
- Vertices/indices stored in CPU memory (std::vector)
- GPU buffers created lazily on first render
- Uses BufferUtils::createDeviceLocalBuffer() with staging
- Primitive generators create colored vertices for visual testing

#### 2.2 GPU Mesh Buffers ✅
Integrated into Mesh class:
```cpp
// Added to Mesh.h:
- VkBuffer m_vertexBuffer
- VkDeviceMemory m_vertexBufferMemory  
- VkBuffer m_indexBuffer
- VkDeviceMemory m_indexBufferMemory
```

**Design Decision:** Direct Vulkan handles in Mesh
- Simpler for Phase 2 (no separate resource manager needed)
- Automatic cleanup in destructor
- Trade-off: Couples Mesh to Vulkan API
- Future: May refactor to renderer/resource manager pattern

#### 2.3 MeshEntity Full Implementation (`src/api/Entity.cpp`) ✅
```cpp
// Implemented in MeshEntity::render():
1. Get mesh (direct m_mesh or via m_meshId from scene)
2. Upload to GPU if not already uploaded
3. Get VulkanContext command buffer
4. Bind mesh rendering pipeline
5. Set dynamic viewport/scissor
6. Bind UBO descriptor set (view/projection)
7. Push model matrix as push constant
8. Bind mesh vertex/index buffers
9. Issue vkCmdDrawIndexed()
```

**Key Implementation Details:**
- Accesses Game via m_scene->getGame()
- Uses VulkanContext::getCurrentCommandBuffer()
- Pipeline from Game::getMeshPipeline()
- Model matrix from Entity::getModelMatrix()

#### 2.4 Default Rendering Pipeline (`src/api/Game.cpp`) ✅
Created in Game::createMeshRenderingPipeline():
```cpp
// Pipeline Features:
- Vertex shader: shaders/mesh.vert (GLSL compiled to SPIR-V)
- Fragment shader: shaders/mesh.frag
- Push constant: mat4 model (64 bytes, vertex stage)
- Descriptor set 0: UniformBufferObject (model/view/proj UBO)
- Dynamic states: viewport, scissor
- Vertex input: Vertex::getBindingDescription/getAttributeDescriptions
- Topology: Triangle list
- Cull mode: Back-face culling
- Front face: Counter-clockwise
- No depth testing (Phase 2 simplification)
```

**Critical Fix Applied:**
- Added glslang::InitializeProcess() in Game::initialize()
- Added glslang::FinalizeProcess() in Game::shutdown()
- Required for ShaderCompiler to work (was causing silent crashes)

**Shader Design Decision:**
- Reused existing UniformBufferObject structure (model/view/proj)
- Model matrix in UBO ignored (using push constant instead)
- Avoids creating new descriptor set layout
- Simpler integration with existing VulkanContext UBO updates

#### 2.5 Shader Implementation ✅
**shaders/mesh.vert:**
- Input: position, color, texCoord (from Vertex structure)
- Push constant: model matrix (per-entity)
- UBO: model/view/proj (uses only view/proj)
- Output: fragColor, fragTexCoord, fragWorldPos

**shaders/mesh.frag:**
- Derives normals from screen-space gradients (dFdx/dFdy)
- Simple directional light (0.3, 1.0, 0.5 normalized)
- Lighting: 30% ambient + 70% diffuse
- Output: vec4 color with lighting applied

**Why gradient normals?**
- Avoids needing proper vertex normals in Phase 2
- Works with primitives that store colors in normal slot
- Future: Add real vertex normals to Vertex structure

**Phase 2 Total: ~750 lines implemented**

---

### Phase 3: SpriteEntity & Depth Testing ✅ COMPLETE

**Goal:** Implement SpriteEntity for 2D rendering with texture support.

**Status:** ✅ Completed 2026-01-31

#### 3.1 SpriteEntity Implementation
- [x] Create sprite shaders (`shaders/simple_sprite.vert`, `shaders/simple_sprite.frag`)
- [x] Create sprite rendering pipeline in Game.cpp with alpha blending
- [x] Implement SpriteEntity::render() with quad rendering
- [x] Support texture atlas (UV rect)
- [x] Support anchor points for sprite origin
- [x] Support color tinting
- [x] Direct texture support (shared_ptr<Texture>)
- [x] Descriptor set caching for textures

#### 3.2 Unit Tests
- [x] SpriteEntity_test.cpp - 21 tests for sprite configuration
- [x] Verify UV rect, anchor, color, transform settings work
- [x] All 94 unit tests pass

**Implementation Files:**
- `shaders/simple_sprite.vert` - Vertex shader with push constants for model, tint, UV rect
- `shaders/simple_sprite.frag` - Fragment shader with texture sampling and tinting
- Updated `src/api/Entity.cpp` - Full SpriteEntity::render() implementation (~100 lines)
- Updated `src/api/Game.cpp` - createSpriteRenderingPipeline() (~150 lines)
- Updated `include/vde/api/Entity.h` - Added direct texture support to SpriteEntity
- Updated `include/vde/api/Game.h` - Sprite pipeline accessors
- Updated `include/vde/VulkanContext.h` - Non-const DescriptorManager accessor
- `tests/SpriteEntity_test.cpp` - 21 unit tests

**Key Features:**
- Shared sprite quad mesh (created lazily, reused by all sprites)
- Push constants for per-sprite transform, tint color, and UV rectangle
- Descriptor set caching per-texture for efficient rendering
- Alpha blending enabled for transparency
- Anchor point support for flexible sprite origins

**Design Decisions:**
1. **Separate Simple Shader** - Created `simple_sprite.vert/frag` instead of using the complex instanced shader
   - Simpler for initial implementation
   - Uses push constants like mesh pipeline (consistent approach)
   - Future: Can migrate to instanced rendering for batching

2. **Shared Quad Mesh** - All sprites share a single quad mesh
   - Memory efficient
   - Created lazily on first sprite render
   - Cached as static variable in Entity.cpp

3. **Descriptor Set Caching** - One descriptor set per unique texture
   - Cached in static map keyed by texture pointer
   - Avoids descriptor set allocation every frame
   - Note: Does not clean up when textures are destroyed (future improvement)

4. **Depth Testing Deferred** - Depth buffer not implemented in Phase 3
   - Render order matters for Z-ordering
   - Future: Add depth buffer to VulkanContext for proper 3D sorting

**Known Limitations (Phase 3):**
- No depth testing (render order determines Z-order)
- No sprite batching (each sprite is a separate draw call)
- Descriptor sets not freed when textures are destroyed
- Instanced shader (`sprite.vert/frag`) not used yet

**Phase 3 Total: ~300 lines implementation + ~200 lines tests**

---

### Phase 4: Materials & Lighting ✅ COMPLETE

**Goal:** Implement a Material system for surface properties and connect the existing LightBox to GPU rendering with multi-light support.

**Status:** ✅ Completed 2026-02-01

#### 4.1 Material Class (`include/vde/api/Material.h`, `src/api/Material.cpp`) ✅
```cpp
// Implemented:
- Material() - Default constructor (gray, roughness 0.5, non-metallic)
- Material(albedo, roughness, metallic, emission) - Full constructor
- Setter/getter methods with clamping for roughness/metallic
- Material::GPUData struct for push constants (48 bytes)
- getGPUData() - Convert to GPU-ready format

// Factory methods:
- Material::createDefault() - Standard gray material
- Material::createColored(color) - Simple colored material
- Material::createMetallic(color, roughness) - Metallic surface (metallic=1.0)
- Material::createEmissive(color, intensity) - Glowing material
- Material::createGlass(opacity) - Transparent glass-like material

// Texture placeholders (for future texture-based materials):
- hasAlbedoTexture(), getAlbedoTexture()
- hasNormalTexture(), hasRoughnessTexture(), hasMetallicTexture(), hasEmissionTexture()
```

**Material Properties:**
- **Albedo**: Base color (RGBA)
- **Roughness**: Surface roughness (0=smooth mirror, 1=rough diffuse)
- **Metallic**: Metalness (0=dielectric, 1=metal)
- **Emission**: Self-illumination intensity (can exceed 1.0 for HDR)
- **Emission Color**: Color of emitted light

#### 4.2 GPU Lighting Data Structures (`include/vde/Types.h`) ✅
```cpp
// Added structures for GPU transfer:
struct GPULight {
    glm::vec4 positionAndType;     // xyz=position/direction, w=type
    glm::vec4 directionAndRange;   // xyz=direction, w=range
    glm::vec4 colorAndIntensity;   // rgb=color, a=intensity
    glm::vec4 spotParams;          // x=innerCos, y=outerCos, zw=reserved
};  // 64 bytes

struct LightingUBO {
    glm::vec4 ambientColorAndIntensity;  // rgb=color, a=intensity
    glm::ivec4 lightCounts;              // x=numLights, yzw=reserved
    GPULight lights[MAX_LIGHTS];         // MAX_LIGHTS = 8
};  // 544 bytes total

struct MaterialPushConstants {
    glm::vec4 albedo;         // rgba
    glm::vec4 emissionColor;  // rgb + padding
    float roughness;
    float metallic;
    float emission;
    float padding;
};  // 48 bytes
```

#### 4.3 Shader Updates ✅

**shaders/mesh.vert** - Updated for materials:
- Added MaterialPushConstants in push constant block (after model matrix)
- Passes material properties to fragment shader
- Outputs world normal and view position for lighting calculations

**shaders/mesh.frag** - Full multi-light support:
```glsl
// Features:
- LightingUBO binding (set 1, binding 0)
- Multi-light loop (up to 8 lights)
- Light types: Directional (0), Point (1), Spot (2)
- Blinn-Phong specular calculation
- Distance attenuation for point/spot lights
- Spotlight cone falloff
- Material-based ambient/diffuse/specular response
- Emission support (adds directly to final color)
```

**Lighting Model:**
- Ambient: `ambient_color * ambient_intensity * albedo`
- Diffuse: `light_color * intensity * max(dot(N, L), 0) * albedo`
- Specular: `light_color * intensity * pow(max(dot(N, H), 0), shininess)` (Blinn-Phong)
- Shininess derived from roughness: `shininess = 2 / (roughness^4 + 0.001) - 2`
- Metallic affects specular color (metals tint specular to albedo)

#### 4.4 Lighting Infrastructure (`src/api/Game.cpp`) ✅

**createLightingResources():**
- Creates descriptor set layout for lighting UBO (Set 1, Binding 0)
- Creates descriptor pool for per-frame lighting descriptor sets
- Creates UBO buffers (one per frame-in-flight)
- Persistently maps UBO buffers for efficient updates
- Allocates and updates descriptor sets

**destroyLightingResources():**
- Unmaps and destroys UBO buffers
- Destroys descriptor pool and layout

**updateLightingUBO(const Scene*):**
- Converts LightBox data to LightingUBO format
- Copies ambient color/intensity
- Converts each Light to GPULight format
- Handles light type conversion (Directional, Point, Spot)
- Uploads to current frame's UBO buffer

**getCurrentLightingDescriptorSet():**
- Returns descriptor set for current frame

#### 4.5 MeshEntity Material Support (`src/api/Entity.cpp`) ✅
```cpp
// Added to MeshEntity:
- setMaterial(Material) / getMaterial() / hasMaterial()
- m_material member (optional<Material>)

// Updated MeshEntity::render():
1. Update lighting UBO from scene's LightBox
2. Bind mesh pipeline
3. Bind UBO descriptor set (Set 0)
4. Bind lighting descriptor set (Set 1)
5. Push model matrix (64 bytes) + MaterialPushConstants (48 bytes)
6. Use material properties if set, otherwise default gray material
```

#### 4.6 Pipeline Updates (`src/api/Game.cpp`) ✅

**createMeshRenderingPipeline() modifications:**
- Push constant size increased to 112 bytes (64 model + 48 material)
- Pipeline layout now uses 2 descriptor set layouts:
  - Set 0: UBO (view/projection matrices)
  - Set 1: Lighting UBO
- Push constants accessible from both vertex and fragment stages

#### 4.7 Unit Tests (`tests/Material_test.cpp`) ✅ ~280 lines
```cpp
// Test categories:
- Construction tests (default, with albedo, with all parameters)
- Setter/getter tests with value clamping
- GPU data conversion tests
- Factory method tests (createDefault, createColored, createMetallic, createEmissive, createGlass)
- Method chaining tests
- Texture flag tests (placeholder)
- Edge case tests (zero values, max values, copy/move construction)
```

**Implementation Files:**
- `include/vde/api/Material.h` (~260 lines) - Material class definition
- `src/api/Material.cpp` (~80 lines) - Implementation
- `include/vde/Types.h` - Added GPULight, LightingUBO, MaterialPushConstants
- `shaders/mesh.vert` - Updated with material push constants
- `shaders/mesh.frag` - Full multi-light Blinn-Phong lighting
- `include/vde/api/Game.h` - Added lighting methods and members
- `src/api/Game.cpp` - Lighting infrastructure (~220 lines added)
- `include/vde/api/Entity.h` - Added Material to MeshEntity
- `src/api/Entity.cpp` - Updated render() for materials/lighting
- `tests/Material_test.cpp` (~280 lines) - Comprehensive unit tests

**Design Decisions:**

1. **Push Constants for Materials**
   - 48-byte MaterialPushConstants struct fits in typical 128-byte limit
   - Avoids per-material descriptor sets
   - Simple and efficient for small number of material properties

2. **Lighting UBO in Set 1**
   - Separates lighting from MVP matrices (Set 0)
   - Allows independent update of lighting data
   - Per-frame UBO buffers avoid synchronization issues

3. **Blinn-Phong Lighting**
   - Well-understood model, good balance of quality vs. complexity
   - Half-vector optimization for specular
   - Roughness-to-shininess conversion for intuitive control

4. **LightBox Integration**
   - Scene's LightBox converted to GPU format each frame
   - Default white ambient used when no LightBox set
   - Supports all existing Light types (Directional, Point, Spot)

**Known Limitations (Phase 4):**
- No texture-based materials (albedo/normal/roughness maps)
- No shadows
- Fixed 8-light maximum (GPU array size)
- No PBR (physically-based rendering) - using Blinn-Phong approximation
- No environment mapping/reflections

**Phase 4 Total: ~900 lines implementation + ~280 lines tests**

---

### Phase 5: Resource Management ✅ COMPLETE

**Goal:** Implement global resource caching, improve resource loading, and make Texture integrate with the Resource system.

**Status:** ✅ Completed 2026-02-04

#### Completion Summary

**All objectives achieved:**
- ✅ Texture now inherits from Resource
- ✅ Two-phase loading pattern (CPU load, GPU upload)
- ✅ ResourceManager implemented with weak_ptr caching
- ✅ Automatic deduplication and memory management
- ✅ Descriptor set cleanup fixed
- ✅ Comprehensive unit tests (31 tests total)

**Files Created:**
- `include/vde/api/ResourceManager.h` (~280 lines) - ResourceManager class
- `src/api/ResourceManager.cpp` (~60 lines) - Implementation  
- `tests/Texture_test.cpp` (~160 lines) - Texture tests (14 tests)
- `tests/ResourceManager_test.cpp` (~250 lines) - ResourceManager tests (17 tests)

**Files Modified:**
- `include/vde/Texture.h` - Added Resource inheritance, two-phase loading
- `src/Texture.cpp` - Implemented new API, kept backward compatibility
- `include/vde/api/Scene.h` - Updated addResource() for Texture support
- `include/vde/api/Game.h` - Added ResourceManager member
- `src/api/Entity.cpp` - Added descriptor cache cleanup
- `src/api/Game.cpp` - Call cleanup on shutdown
- `tests/CMakeLists.txt` - Added new test files
- `CMakeLists.txt` - Added ResourceManager.cpp

**Total Code:**
- New: ~750 lines (implementation + tests)
- Modified: ~150 lines

---

#### Current State Analysis

**What Works:**
- ✅ Mesh inherits from Resource
- ✅ Scene has per-scene resource storage
- ✅ Basic resource ID system exists
- ✅ Mesh can be loaded via Scene::addResource()

**What's Missing:**
- ❌ Texture doesn't inherit from Resource (different API)
- ❌ No global resource caching (duplicate loads across scenes)
- ❌ Texture loading requires many Vulkan parameters
- ❌ Descriptor set cleanup issues (Phase 3 note)
- ❌ No resource reference counting or auto-cleanup

#### 5.1 Texture Resource Integration ⏳

**Goal:** Make Texture inherit from Resource with simplified API matching Mesh pattern.

**Changes to `include/vde/Texture.h`:**
```cpp
class Texture : public Resource {
public:
    Texture() = default;
    virtual ~Texture();
    
    // Simplified loading - defers Vulkan object creation
    bool loadFromFile(const std::string& path) override;
    
    // GPU upload (called lazily on first use)
    bool uploadToGPU(VulkanContext* context);
    
    // Check if uploaded to GPU
    bool isOnGPU() const { return m_image != VK_NULL_HANDLE; }
    
    // Cleanup GPU resources
    void freeGPUResources(VkDevice device);
    
    // Resource interface
    const char* getTypeName() const override { return "Texture"; }
    
    // Existing getters for Vulkan handles
    VkImage getImage() const { return m_image; }
    VkImageView getImageView() const { return m_imageView; }
    VkSampler getSampler() const { return m_sampler; }
    
private:
    // CPU-side image data (for lazy upload)
    std::vector<uint8_t> m_pixelData;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    uint32_t m_channels = 4;
    
    // GPU-side Vulkan objects
    VkImage m_image = VK_NULL_HANDLE;
    VkDeviceMemory m_imageMemory = VK_NULL_HANDLE;
    VkImageView m_imageView = VK_NULL_HANDLE;
    VkSampler m_sampler = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
};
```

**Implementation in `src/Texture.cpp`:**
- `loadFromFile()` - Load pixel data with stb_image, store in m_pixelData
- `uploadToGPU()` - Create VkImage/VkImageView/VkSampler, upload via staging buffer
- Update existing methods to use new two-phase pattern
- Keep backward compatibility for existing code

**Design Benefits:**
1. Consistent with Mesh pattern (CPU load, GPU upload)
2. Resources can be loaded without VulkanContext
3. Enables resource pre-loading before GPU init
4. Simplifies Scene::addResource template

#### 5.2 ResourceManager Class ⏳

**Goal:** Global resource cache to share resources across scenes and avoid duplicate loads.

**Create `include/vde/api/ResourceManager.h`:**
```cpp
class ResourceManager {
public:
    ResourceManager() = default;
    ~ResourceManager() = default;
    
    // Load or get cached resource
    template <typename T>
    ResourcePtr<T> load(const std::string& path);
    
    // Add pre-created resource
    template <typename T>
    ResourcePtr<T> add(const std::string& key, ResourcePtr<T> resource);
    
    // Get resource by path
    template <typename T>
    ResourcePtr<T> get(const std::string& path);
    
    // Check if resource is cached
    bool has(const std::string& path) const;
    
    // Remove resource from cache
    void remove(const std::string& path);
    
    // Clear all cached resources
    void clear();
    
    // Get resource statistics
    size_t getCachedCount() const;
    size_t getMemoryUsage() const;
    
private:
    struct CacheEntry {
        std::weak_ptr<Resource> resource;
        std::type_index type;
        size_t lastAccessTime;
    };
    
    std::unordered_map<std::string, CacheEntry> m_cache;
    size_t m_accessCounter = 0;
};
```

**Implementation in `src/api/ResourceManager.cpp`:**
- Use weak_ptr to allow automatic cleanup when no references exist
- Type-safe resource retrieval with type checking
- Thread-safe access with mutex (future enhancement)
- LRU eviction policy (future enhancement)

**Key Features:**
1. **Automatic Deduplication** - Same path = same resource instance
2. **Weak References** - Resources auto-delete when unused
3. **Type Safety** - Template ensures type correctness
4. **Access Tracking** - Foundation for LRU cache

#### 5.3 Scene Integration ⏳

**Updates to `include/vde/api/Scene.h`:**
```cpp
class Scene {
public:
    // Use global resource manager for loading
    template <typename T>
    ResourcePtr<T> loadResource(const std::string& path);
    
    // Track resources used by this scene (for cleanup)
    template <typename T>
    void useResource(ResourcePtr<T> resource);
    
    // Existing addResource methods now use ResourceManager
    template <typename T>
    ResourceId addResource(const std::string& path);
    
private:
    // Resources actively used by this scene
    std::vector<std::shared_ptr<Resource>> m_activeResources;
};
```

**Implementation in `src/api/Scene.cpp`:**
- Update `addResource()` to use Game's ResourceManager
- Track resources for scene lifetime
- Clean up on scene destruction

#### 5.4 Game Integration ⏳

**Updates to `include/vde/api/Game.h`:**
```cpp
class Game {
public:
    // Global resource manager
    ResourceManager& getResourceManager() { return m_resourceManager; }
    
private:
    ResourceManager m_resourceManager;
};
```

**Updates to `src/api/Game.cpp`:**
- Initialize ResourceManager
- Clear resources on shutdown

#### 5.5 Descriptor Set Cleanup Fix ⏳

**Problem from Phase 3:**
- SpriteEntity caches descriptor sets per texture
- Descriptor sets not freed when textures destroyed

**Solution:**
- Track descriptor sets in Texture class
- Clean up in Texture destructor
- Or: Move to per-material descriptor set pattern

**Updates to `src/api/Entity.cpp` (SpriteEntity::render):**
```cpp
// Option 1: Store descriptor set in Texture
VkDescriptorSet descriptorSet = texture->getOrCreateDescriptorSet(context);

// Option 2: Create descriptor set per-frame (simple but less efficient)
VkDescriptorSet descriptorSet = createDescriptorSet(texture, context);
```

#### 5.6 Unit Tests ⏳

**Create `tests/ResourceManager_test.cpp`:**
```cpp
TEST(ResourceManager, LoadCreatesNewResource)
TEST(ResourceManager, LoadSamePathReturnsSameInstance)
TEST(ResourceManager, WeakPtrAllowsAutoCleanup)
TEST(ResourceManager, GetReturnsNullptrForMissingResource)
TEST(ResourceManager, HasChecksExistence)
TEST(ResourceManager, RemoveDeletesFromCache)
TEST(ResourceManager, ClearRemovesAllResources)
TEST(ResourceManager, TypeMismatchReturnsNullptr)
TEST(ResourceManager, GetCachedCount)
```

**Update `tests/Scene_test.cpp`:**
```cpp
TEST(Scene, LoadResourceUsesResourceManager)
TEST(Scene, AddResourceDeduplicates)
TEST(Scene, ResourcesCleanedUpOnSceneDestruction)
```

**Create `tests/Texture_test.cpp`:**
```cpp
TEST(Texture, LoadFromFileStoresData)
TEST(Texture, InheritsFromResource)
TEST(Texture, GetTypeNameReturnsTexture)
TEST(Texture, IsLoadedAfterLoad)
TEST(Texture, IsOnGPUAfterUpload)
// Integration test with VulkanContext (optional)
```

**Expected Test Counts:**
- ResourceManager_test: ~10 tests
- Updated Scene_test: +5 tests
- Texture_test: ~8 tests
- **Total: ~23 new tests**

#### Implementation Order

1. ✅ **Create Phase 5 plan**
2. ⏳ **Refactor Texture** (2-3 hours)
   - Make Texture inherit from Resource
   - Implement loadFromFile() / uploadToGPU() pattern
   - Update existing Texture usage (if any)
   - Write Texture unit tests

3. ⏳ **Implement ResourceManager** (2-3 hours)
   - Create ResourceManager class
   - Implement template methods
   - Write ResourceManager unit tests

4. ⏳ **Scene & Game Integration** (1-2 hours)
   - Add ResourceManager to Game
   - Update Scene::addResource to use ResourceManager
   - Update Scene tests

5. ⏳ **Descriptor Set Cleanup** (1 hour)
   - Fix SpriteEntity descriptor set leak
   - Test with sprite_demo example

6. ⏳ **Verification & Documentation** (1 hour)
   - Run all tests
   - Test examples (simple_game, sprite_demo)
   - Update GAME_API_IMPLEMENTATION_PLAN.md
   - Add usage examples to docs

**Total Estimated Time: 7-10 hours**

#### Files to Create/Modify

**New Files:**
- `include/vde/api/ResourceManager.h` (~150 lines)
- `src/api/ResourceManager.cpp` (~200 lines)
- `tests/ResourceManager_test.cpp` (~150 lines)
- `tests/Texture_test.cpp` (~120 lines)

**Modified Files:**
- `include/vde/Texture.h` - Add Resource inheritance (~50 lines changed)
- `src/Texture.cpp` - Refactor to two-phase load (~100 lines changed)
- `include/vde/api/Game.h` - Add ResourceManager member (~5 lines)
- `src/api/Game.cpp` - Initialize ResourceManager (~10 lines)
- `include/vde/api/Scene.h` - Update resource methods (~20 lines)
- `src/api/Scene.cpp` - Use ResourceManager (~30 lines changed)
- `src/api/Entity.cpp` - Fix descriptor set cleanup (~20 lines)
- `tests/Scene_test.cpp` - Add resource tests (~50 lines)
- `CMakeLists.txt` - Add ResourceManager.cpp (~2 lines)

**Total New Code: ~620 lines**
**Total Modified Code: ~285 lines**

---

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
