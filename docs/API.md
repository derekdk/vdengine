# VDE API Reference

This document provides a reference for the public API of the Vulkan Display Engine.

VDE has a two-layer architecture:

1. **Low-Level Rendering Layer** — Direct Vulkan abstractions (`Window`, `VulkanContext`, `Camera`, `Texture`, etc.)
2. **Game API Layer** — High-level game framework (`Game`, `Scene`, `Entity`, physics, audio, etc.)

Most users should start with the [Game API](#game-api-layer). The low-level layer is available for advanced rendering needs.

## Core Headers

Include all Game API functionality with a single header:

```cpp
#include <vde/api/GameAPI.h>
```

Or include the low-level rendering core:

```cpp
#include <vde/Core.h>
```

---

# Low-Level Rendering Layer

---

## vde::Window

**Header**: `<vde/Window.h>`

Manages a GLFW window with Vulkan support.

### Constructor

```cpp
Window(uint32_t width, uint32_t height, const char* title);
```

Creates a window with the specified dimensions and title.

### Methods

| Method | Description |
|--------|-------------|
| `bool shouldClose() const` | Returns true if window close was requested |
| `void pollEvents()` | Polls for window events |
| `GLFWwindow* getHandle() const` | Returns underlying GLFW handle |
| `uint32_t getWidth() const` | Current width in pixels |
| `uint32_t getHeight() const` | Current height in pixels |
| `void setResolution(uint32_t width, uint32_t height)` | Set window resolution |
| `void setFullscreen(bool fullscreen)` | Toggle fullscreen mode |
| `bool isFullscreen() const` | Check fullscreen state |
| `void setResizeCallback(ResizeCallback)` | Set window resize callback |
| `static const Resolution& getResolution(size_t index)` | Get predefined resolution by index |
| `static size_t getResolutionCount()` | Get number of predefined resolutions |
| `static const Resolution* getResolutions()` | Get array of predefined resolutions |

### Types

```cpp
struct Resolution {
    uint32_t width;
    uint32_t height;
    const char* name;
};

using ResizeCallback = std::function<void(uint32_t, uint32_t)>;
```

---

## vde::VulkanContext

**Header**: `<vde/VulkanContext.h>`

Core Vulkan context managing instance, device, and swapchain.

### Methods

| Method | Description |
|--------|-------------|
| `void initialize(Window* window)` | Initialize Vulkan with the given window (throws on failure) |
| `void cleanup()` | Destroy all Vulkan resources |
| `void recreateSwapchain(uint32_t width, uint32_t height)` | Recreate swapchain after resize |
| `void drawFrame()` | Render a single frame |
| `void setRenderCallback(RenderCallback)` | Set the per-frame render callback |
| `void setClearColor(const glm::vec4&)` | Set the clear color |
| `const glm::vec4& getClearColor() const` | Get the current clear color |

### Accessors

| Method | Returns |
|--------|---------|
| `VkInstance getInstance() const` | Vulkan instance handle |
| `VkPhysicalDevice getPhysicalDevice()` | Physical device handle |
| `VkDevice getDevice()` | Logical device handle |
| `VkQueue getGraphicsQueue()` | Graphics queue |
| `VkQueue getPresentQueue()` | Present queue |
| `uint32_t getGraphicsQueueFamily()` | Graphics queue family index |
| `VkRenderPass getRenderPass()` | Main render pass |
| `VkCommandPool getCommandPool()` | Command pool for graphics |
| `VkExtent2D getSwapChainExtent()` | Swapchain dimensions |
| `uint32_t getCurrentFrame()` | Current frame-in-flight index |
| `VkCommandBuffer getCurrentCommandBuffer() const` | Command buffer for current frame |
| `VkBuffer getCurrentUniformBuffer() const` | Uniform buffer for current frame |
| `VkDescriptorSet getCurrentUBODescriptorSet() const` | UBO descriptor set for current frame |
| `Camera& getCamera()` | Engine camera used for rendering |
| `DescriptorManager& getDescriptorManager()` | Descriptor manager instance |

### Types

```cpp
using RenderCallback = std::function<void(VkCommandBuffer)>;
```

---

## vde::Camera

**Header**: `<vde/Camera.h>`

3D camera with orbital controls and projection management.

### Setup Methods

| Method | Description |
|--------|-------------|
| `void setPosition(const glm::vec3&)` | Set camera position |
| `void setTarget(const glm::vec3&)` | Set look-at target |
| `void setUp(const glm::vec3&)` | Set up direction |
| `void setFromPitchYaw(float dist, float pitch, float yaw, const glm::vec3& target)` | Set orbital parameters |

### Movement Methods

| Method | Description |
|--------|-------------|
| `void translate(const glm::vec3&)` | Move camera and target |
| `void pan(float dx, float dy)` | Pan in view plane |
| `void zoom(float delta)` | Zoom toward/away from target |

### Projection Methods

| Method | Description |
|--------|-------------|
| `void setPerspective(float fov, float aspect, float near, float far)` | Set projection parameters |
| `void setOrthographic(float left, float right, float bottom, float top, float near, float far)` | Set orthographic projection |
| `bool isOrthographic() const` | Check if orthographic projection is active |
| `void setAspectRatio(float aspect)` | Update aspect ratio |
| `void setFOV(float fov)` | Update field of view |

### Matrix Accessors

| Method | Returns |
|--------|---------|
| `glm::mat4 getViewMatrix()` | View matrix |
| `glm::mat4 getProjectionMatrix()` | Projection matrix (Vulkan Y-flip) |
| `glm::mat4 getViewProjectionMatrix()` | Combined VP matrix |

### Projection Accessors

| Method | Returns |
|--------|---------|
| `float getFOV() const` | Field of view in degrees |
| `float getAspectRatio() const` | Aspect ratio |
| `float getNearPlane() const` | Near clipping plane |
| `float getFarPlane() const` | Far clipping plane |

### Constants

```cpp
static constexpr float MIN_DISTANCE = 1.0f;
static constexpr float MAX_DISTANCE = 100.0f;
static constexpr float MIN_PITCH = 5.0f;
static constexpr float MAX_PITCH = 89.0f;
```

---

## vde::Texture

**Header**: `<vde/Texture.h>`

Vulkan texture loading and management.

### Methods

| Method | Description |
|--------|-------------|
| `bool loadFromFile(const std::string& path)` | Load texture data into CPU memory |
| `bool loadFromData(const uint8_t* pixels, uint32_t width, uint32_t height)` | Load texture from raw pixel data |
| `bool uploadToGPU(VulkanContext* context)` | Create GPU objects and upload data |
| `bool isOnGPU() const` | Check if texture is uploaded to GPU |
| `void freeGPUResources(VkDevice device)` | Free GPU objects (keep CPU data) |
| `void cleanup()` | Destroy CPU and GPU resources |
| `bool isValid() const` | Check if image, view, and sampler are created |
| `bool loadFromFile(const std::string& path, VkDevice, VkPhysicalDevice, VkCommandPool, VkQueue)` | Legacy one-step load/upload |
| `bool createFromData(const uint8_t* pixels, uint32_t width, uint32_t height, VkDevice, VkPhysicalDevice, VkCommandPool, VkQueue)` | Legacy one-step load/upload |

### Accessors

| Method | Returns |
|--------|---------|
| `VkImage getImage()` | Vulkan image handle |
| `VkImageView getImageView()` | Image view handle |
| `VkSampler getSampler()` | Sampler handle |
| `uint32_t getWidth()` | Texture width |
| `uint32_t getHeight()` | Texture height |

---

## vde::BufferUtils

**Header**: `<vde/BufferUtils.h>`

Static utility class for Vulkan buffer operations.

### Initialization

```cpp
static void init(VkDevice, VkPhysicalDevice, VkCommandPool, VkQueue);
static bool isInitialized();
static void reset();
```

### Buffer Operations

```cpp
static uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

static void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                         VkMemoryPropertyFlags properties,
                         VkBuffer& buffer, VkDeviceMemory& memory);

static void copyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size);

static VkCommandBuffer beginSingleTimeCommands();
static void endSingleTimeCommands(VkCommandBuffer cmd);

static void createDeviceLocalBuffer(const void* data, VkDeviceSize size,
                                    VkBufferUsageFlags usage,
                                    VkBuffer& buffer, VkDeviceMemory& memory);

static void createMappedBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                               VkBuffer& buffer, VkDeviceMemory& memory,
                               void** mappedMemory);
```

---

## vde::ShaderCache

**Header**: `<vde/ShaderCache.h>`

Shader compilation and caching.

### Constructor

```cpp
ShaderCache(const std::string& cacheDirectory = "cache/shaders");
```

### Methods

| Method | Description |
|--------|-------------|
| `bool initialize()` | Initialize cache and load manifest |
| `std::vector<uint32_t> loadShader(const std::string& path, std::optional<ShaderStage> stage = std::nullopt)` | Load from cache or compile |
| `std::vector<uint32_t> reloadShader(const std::string& path)` | Force recompile a shader |
| `bool hasSourceChanged(const std::string& path) const` | Check if source differs from cache |
| `void invalidate(const std::string& path)` | Invalidate a cached shader |
| `void clearCache()` | Clear all cached files |
| `bool saveManifest()` | Save cache manifest to disk |
| `std::vector<std::string> hotReload()` | Recompile changed shaders |
| `void setEnabled(bool enabled)` | Enable or disable caching |
| `bool isEnabled() const` | Check if caching is enabled |
| `bool isInitialized() const` | Check if cache is initialized |
| `size_t getCacheEntryCount() const` | Number of cache entries |
| `size_t getCacheHits() const` | Cache hit count |
| `size_t getCacheMisses() const` | Cache miss count |
| `const std::string& getLastError() const` | Last error message |
| `const std::string& getCacheDirectory() const` | Cache directory path |

---

## vde::HexGeometry

**Header**: `<vde/HexGeometry.h>`

Hexagon mesh generation.

### Constructor

```cpp
HexGeometry(float size = 1.0f, HexOrientation orientation = HexOrientation::FlatTop);
```

### Methods

| Method | Description |
|--------|-------------|
| `HexMesh generateHex(const glm::vec3& center = glm::vec3(0))` | Generate hex mesh |
| `std::vector<glm::vec3> getCornerPositions(const glm::vec3& center)` | Get corner positions |
| `float getSize()` | Outer radius |
| `float getWidth()` | Width (tip to tip or flat to flat) |
| `float getHeight()` | Height |

### Types

```cpp
enum class HexOrientation {
    FlatTop,    // Flat edge at top
    PointyTop   // Point at top
};

struct HexMesh {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
};
```

---

## vde::Types

**Header**: `<vde/Types.h>`

Common data structures.

### Vertex

```cpp
struct Vertex {
    glm::vec3 position;
    glm::vec3 color;
    glm::vec2 texCoord;
    
    static VkVertexInputBindingDescription getBindingDescription();
    static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions();
};
```

### UniformBufferObject

```cpp
struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};
```

### Lighting and Material GPU Types

```cpp
constexpr uint32_t MAX_LIGHTS = 8;

struct GPULight {
    glm::vec4 positionAndType;
    glm::vec4 directionAndRange;
    glm::vec4 colorAndIntensity;
    glm::vec4 spotParams;
};

struct LightingUBO {
    glm::vec4 ambientColorAndIntensity;
    glm::ivec4 lightCounts;
    GPULight lights[MAX_LIGHTS];
};

struct MaterialPushConstants {
    glm::vec4 albedo;
    glm::vec4 emission;
    float roughness;
    float metallic;
    float normalStrength;
    float padding;
};
```

---

## Version Information

```cpp
namespace vde {
    struct Version {
        static constexpr int MAJOR = 0;
        static constexpr int MINOR = 1;
        static constexpr int PATCH = 0;

        static const char* getString();
    };
}
```

---

## Error Handling

Most VDE functions throw `std::runtime_error` on failure:

```cpp
try {
    context.initialize(&window);
} catch (const std::runtime_error& e) {
    std::cerr << "Initialization failed: " << e.what() << std::endl;
}
```

Functions that may fail gracefully return `bool`:

```cpp
if (!texture.loadFromFile("missing.png")) {
    // Handle missing texture
}
```

---

# Game API Layer

The Game API provides a high-level framework for building games on top of the Vulkan rendering layer. It manages scenes, entities, resources, input, audio, physics, and multi-scene scheduling.

---

## vde::Game

**Header**: `<vde/api/Game.h>`

Main game class managing the game loop, scenes, and all engine subsystems.

### Basic Usage

```cpp
vde::Game game;
vde::GameSettings settings;
settings.gameName = "My Game";
settings.setWindowSize(1280, 720);

game.initialize(settings);
game.addScene("main", new MainScene());
game.setActiveScene("main");
game.run();
```

### Initialization & Lifecycle

| Method | Description |
|--------|-------------|
| `bool initialize(const GameSettings&)` | Initialize engine with settings |
| `void shutdown()` | Shutdown engine and release resources |
| `bool isInitialized() const` | Check if engine is initialized |
| `void run()` | Run the main game loop (blocks until quit) |
| `void quit()` | Request the game to exit |
| `bool isRunning() const` | Check if game loop is running |

### Scene Management

| Method | Description |
|--------|-------------|
| `void addScene(const std::string& name, Scene*)` | Add a scene (Game takes ownership) |
| `void addScene(const std::string& name, std::unique_ptr<Scene>)` | Add a scene (unique_ptr) |
| `void removeScene(const std::string& name)` | Remove a scene by name |
| `Scene* getScene(const std::string& name)` | Get a scene by name |
| `void setActiveScene(const std::string& name)` | Set the active scene |
| `Scene* getActiveScene()` | Get the currently active scene |
| `void setActiveSceneGroup(const SceneGroup&)` | Activate multiple scenes simultaneously |
| `const SceneGroup& getActiveSceneGroup() const` | Get current active scene group |
| `void pushScene(const std::string& name)` | Push a scene onto the stack |
| `void popScene()` | Pop scene and return to previous |

### Input Focus (Split-Screen)

| Method | Description |
|--------|-------------|
| `void setFocusedScene(const std::string& name)` | Set which scene receives keyboard input |
| `Scene* getFocusedScene()` | Get the focused scene |
| `Scene* getSceneAtScreenPosition(double x, double y)` | Get scene under mouse cursor |

### Scheduler

| Method | Description |
|--------|-------------|
| `Scheduler& getScheduler()` | Get the task scheduler |

### Input

| Method | Description |
|--------|-------------|
| `void setInputHandler(InputHandler*)` | Set global input handler (not owned) |
| `InputHandler* getInputHandler()` | Get global input handler |

### Timing

| Method | Description |
|--------|-------------|
| `float getDeltaTime() const` | Time since last frame (seconds) |
| `double getTotalTime() const` | Total time since start (seconds) |
| `float getFPS() const` | Current frames per second |
| `uint64_t getFrameCount() const` | Current frame number |

### Window & Settings

| Method | Description |
|--------|-------------|
| `Window* getWindow()` | Get the game window |
| `VulkanContext* getVulkanContext()` | Get the Vulkan context |
| `ResourceManager& getResourceManager()` | Get global resource manager |
| `const GameSettings& getSettings() const` | Get current settings |
| `void applyDisplaySettings(const DisplaySettings&)` | Apply display settings |
| `void applyGraphicsSettings(const GraphicsSettings&)` | Apply graphics settings |

### Callbacks

| Method | Description |
|--------|-------------|
| `void setResizeCallback(std::function<void(uint32_t, uint32_t)>)` | Window resize callback |
| `void setFocusCallback(std::function<void(bool)>)` | Window focus callback |

### Virtual Methods (for subclassing)

| Method | Description |
|--------|-------------|
| `virtual void onStart()` | Called before game loop starts |
| `virtual void onUpdate(float dt)` | Called every frame before scene update |
| `virtual void onRender()` | Called every frame after scene render |
| `virtual void onShutdown()` | Called during shutdown |

---

## vde::Scene

**Header**: `<vde/api/Scene.h>`

Represents a game scene/state managing entities, resources, camera, lighting, and world bounds.

### Lifecycle

| Method | Description |
|--------|-------------|
| `virtual void onEnter()` | Called when scene becomes active |
| `virtual void onExit()` | Called when scene is deactivated |
| `virtual void onPause()` | Called when another scene is pushed |
| `virtual void onResume()` | Called when returned to top of stack |
| `virtual void update(float deltaTime)` | Update scene (calls entity updates) |
| `virtual void render()` | Render scene entities |

### Phase Callbacks (Opt-In)

When enabled, the scheduler splits `update()` into three ordered phases:

| Method | Description |
|--------|-------------|
| `void enablePhaseCallbacks()` | Enable 3-phase update model |
| `bool usesPhaseCallbacks() const` | Check if phase callbacks enabled |
| `virtual void updateGameLogic(float dt)` | GameLogic phase (AI, input, spawning) |
| `virtual void updateAudio(float dt)` | Audio phase (drains audio event queue) |
| `virtual void updateVisuals(float dt)` | Visual phase (animation, particles) |

### Audio Event Queue

| Method | Description |
|--------|-------------|
| `void queueAudioEvent(const AudioEvent&)` | Queue audio event for Audio phase |
| `void playSFX(shared_ptr<AudioClip>, volume, pitch, loop)` | Queue a PlaySFX event |
| `void playSFXAt(shared_ptr<AudioClip>, x, y, z, volume, pitch)` | Queue positional audio |
| `size_t getAudioEventQueueSize() const` | Pending audio event count |

### Entity Management

| Method | Description |
|--------|-------------|
| `template<T, Args...> shared_ptr<T> addEntity(Args&&...)` | Create and add entity |
| `EntityId addEntity(Entity::Ref)` | Add existing entity |
| `Entity* getEntity(EntityId)` | Get entity by ID |
| `Entity* getEntityByName(const std::string&)` | Get entity by name |
| `Entity* getEntityByPhysicsBody(PhysicsBodyId)` | Find entity owning a physics body |
| `template<T> vector<shared_ptr<T>> getEntitiesOfType()` | Get all entities of type |
| `void removeEntity(EntityId)` | Remove entity by ID |
| `void clearEntities()` | Remove all entities |
| `const vector<Entity::Ref>& getEntities() const` | Get all entities |

### Resource Management

| Method | Description |
|--------|-------------|
| `template<T> ResourceId addResource(const std::string& path)` | Add resource from file |
| `template<T> ResourceId addResource(ResourcePtr<T>)` | Add pre-created resource |
| `template<T> T* getResource(ResourceId)` | Get resource by ID |
| `void removeResource(ResourceId)` | Remove resource |

### Camera & Lighting

| Method | Description |
|--------|-------------|
| `void setCamera(std::unique_ptr<GameCamera>)` | Set scene camera |
| `void setCamera(GameCamera*)` | Set scene camera (takes ownership) |
| `GameCamera* getCamera()` | Get scene camera |
| `void setLightBox(std::unique_ptr<LightBox>)` | Set lighting configuration |
| `void setLightBox(LightBox*)` | Set lighting (takes ownership) |
| `LightBox* getLightBox()` | Get lighting configuration |
| `const LightBox& getEffectiveLighting() const` | Get effective lighting (default if none set) |

### Background & Priority

| Method | Description |
|--------|-------------|
| `void setContinueInBackground(bool)` | Enable updates when scene is not active |
| `bool getContinueInBackground() const` | Check background update state |
| `void setUpdatePriority(int)` | Set update order priority (lower = first) |
| `int getUpdatePriority() const` | Get update priority |

### Viewport

| Method | Description |
|--------|-------------|
| `void setViewportRect(const ViewportRect&)` | Set viewport sub-region (normalized 0-1) |
| `const ViewportRect& getViewportRect() const` | Get viewport rect (default: full window) |

### Physics

| Method | Description |
|--------|-------------|
| `void enablePhysics(const PhysicsConfig&)` | Enable physics for this scene |
| `void disablePhysics()` | Disable physics |
| `bool hasPhysics() const` | Check if physics is enabled |
| `PhysicsScene* getPhysicsScene()` | Get physics scene (nullptr if disabled) |

### Input & Misc

| Method | Description |
|--------|-------------|
| `void setInputHandler(InputHandler*)` | Set scene input handler (not owned) |
| `InputHandler* getInputHandler()` | Get input handler (falls back to Game's) |
| `Game* getGame()` | Get owning Game |
| `void setBackgroundColor(const Color&)` | Set clear/background color |
| `const Color& getBackgroundColor() const` | Get background color |

### World Bounds

| Method | Description |
|--------|-------------|
| `void setWorldBounds(const WorldBounds&)` | Set 3D world bounds |
| `const WorldBounds& getWorldBounds() const` | Get world bounds |
| `bool is2D() const` | Check if scene is 2D |
| `void setCameraBounds2D(const CameraBounds2D&)` | Set 2D camera bounds |
| `CameraBounds2D& getCameraBounds2D()` | Get 2D camera bounds |

---

## vde::Entity

**Header**: `<vde/api/Entity.h>`

Base class for all game entities with transform (position, rotation, scale).

### Core

| Method | Description |
|--------|-------------|
| `EntityId getId() const` | Unique entity ID |
| `const std::string& getName() const` | Entity name |
| `void setName(const std::string&)` | Set entity name |

### Transform

| Method | Description |
|--------|-------------|
| `void setPosition(float x, float y, float z)` | Set position |
| `void setPosition(const Position&)` | Set position (struct) |
| `void setPosition(const glm::vec3&)` | Set position (vec3) |
| `const Position& getPosition() const` | Get position |
| `void setRotation(float pitch, float yaw, float roll)` | Set rotation (degrees) |
| `const Rotation& getRotation() const` | Get rotation |
| `void setScale(float uniform)` | Set uniform scale |
| `void setScale(float x, float y, float z)` | Set non-uniform scale |
| `const Scale& getScale() const` | Get scale |
| `const Transform& getTransform() const` | Get full transform |
| `void setTransform(const Transform&)` | Set full transform |
| `glm::mat4 getModelMatrix() const` | Get model matrix for rendering |

### Visibility & Lifecycle

| Method | Description |
|--------|-------------|
| `void setVisible(bool)` | Set visibility |
| `bool isVisible() const` | Check visibility |
| `virtual void onAttach(Scene*)` | Called when added to scene |
| `virtual void onDetach()` | Called when removed from scene |
| `virtual void update(float dt)` | Per-frame update |
| `virtual void render()` | Per-frame render |

---

## vde::MeshEntity

**Header**: `<vde/api/Entity.h>`

Entity that renders a 3D mesh with optional material and texture.

### Methods

| Method | Description |
|--------|-------------|
| `void setMesh(shared_ptr<Mesh>)` | Set mesh directly |
| `shared_ptr<Mesh> getMesh() const` | Get mesh |
| `void setMeshId(ResourceId)` | Set mesh by resource ID |
| `void setTexture(shared_ptr<Texture>)` | Set texture directly |
| `void setTextureId(ResourceId)` | Set texture by resource ID |
| `void setColor(const Color&)` | Set base color/tint |
| `const Color& getColor() const` | Get color |
| `void setMaterial(shared_ptr<Material>)` | Set material |
| `shared_ptr<Material> getMaterial() const` | Get material |
| `bool hasMaterial() const` | Check if material is set |

---

## vde::SpriteEntity

**Header**: `<vde/api/Entity.h>`

Entity that renders a 2D sprite with texture, color tint, UV rect, and anchor point.

### Methods

| Method | Description |
|--------|-------------|
| `void setTexture(shared_ptr<Texture>)` | Set sprite texture |
| `void setTextureId(ResourceId)` | Set texture by resource ID |
| `void setColor(const Color&)` | Set color/tint |
| `const Color& getColor() const` | Get color |
| `void setUVRect(float u, float v, float w, float h)` | Set UV sub-rectangle (sprite sheets) |
| `void setAnchor(float x, float y)` | Set anchor point (0-1; 0.5, 0.5 = center) |
| `float getAnchorX() const` | Get anchor X |
| `float getAnchorY() const` | Get anchor Y |

---

## vde::GameCamera

**Header**: `<vde/api/GameCamera.h>`

Base class for simplified game cameras wrapping the low-level `Camera`.

### Base Methods

| Method | Description |
|--------|-------------|
| `Camera& getCamera()` | Get underlying engine camera |
| `glm::mat4 getViewMatrix() const` | Get view matrix |
| `glm::mat4 getProjectionMatrix() const` | Get projection matrix |
| `glm::mat4 getViewProjectionMatrix() const` | Get combined VP matrix |
| `void setAspectRatio(float)` | Set aspect ratio |
| `void setNearPlane(float)` | Set near clip plane |
| `void setFarPlane(float)` | Set far clip plane |
| `virtual void update(float dt)` | Per-frame update |
| `void applyTo(VulkanContext&)` | Apply matrices to Vulkan context |
| `Ray screenToWorldRay(float x, float y, float w, float h) const` | Unproject screen to world ray |

### SimpleCamera

First-person style camera with direct position and direction control.

```cpp
SimpleCamera();
SimpleCamera(const Position& position, const Direction& direction);
```

| Method | Description |
|--------|-------------|
| `void setPosition(const Position&)` | Set camera position |
| `Position getPosition() const` | Get position |
| `void setDirection(const Direction&)` | Set look direction |
| `void setFieldOfView(float fov)` | Set FOV (degrees) |
| `void move(const Direction& delta)` | Move camera |
| `void rotate(float deltaPitch, float deltaYaw)` | Rotate camera |

### OrbitCamera

Third-person camera orbiting around a target point.

```cpp
OrbitCamera();
OrbitCamera(const Position& target, float distance, float pitch = 45.0f, float yaw = 0.0f);
```

| Method | Description |
|--------|-------------|
| `void setTarget(const Position&)` | Set orbit target |
| `void setDistance(float)` | Set orbit distance |
| `void setPitch(float)` | Set pitch angle (degrees) |
| `void setYaw(float)` | Set yaw angle (degrees) |
| `void setFieldOfView(float)` | Set FOV (degrees) |
| `void rotate(float deltaPitch, float deltaYaw)` | Rotate around target |
| `void zoom(float delta)` | Zoom in/out |
| `void pan(float deltaX, float deltaY)` | Pan (moves target) |
| `void setZoomLimits(float min, float max)` | Set distance limits |
| `void setPitchLimits(float min, float max)` | Set pitch limits |

### Camera2D

Orthographic camera for 2D games.

```cpp
Camera2D();
Camera2D(float width, float height);
```

| Method | Description |
|--------|-------------|
| `void setPosition(float x, float y)` | Set camera center |
| `glm::vec2 getPosition() const` | Get position |
| `void setZoom(float)` | Set zoom level (1.0 = normal) |
| `float getZoom() const` | Get zoom |
| `void setRotation(float degrees)` | Set rotation |
| `void setViewportSize(float w, float h)` | Set viewport size |
| `void move(float dx, float dy)` | Move camera |

---

## vde::Material

**Header**: `<vde/api/Material.h>`

Surface material properties for mesh rendering (Blinn-Phong lighting model).

### Constructor

```cpp
Material();
Material(const Color& albedo, float roughness, float metallic, float emission = 0.0f);
```

### Methods

| Method | Description |
|--------|-------------|
| `void setAlbedo(const Color&)` | Set base color |
| `const Color& getAlbedo() const` | Get base color |
| `void setRoughness(float)` | Set roughness (0-1) |
| `void setMetallic(float)` | Set metalness (0-1) |
| `void setEmission(float)` | Set emission intensity |
| `void setEmissionColor(const Color&)` | Set emission color |

### Factory Methods

| Factory | Description |
|---------|-------------|
| `Material::createDefault()` | Standard gray material |
| `Material::createColored(Color)` | Simple colored material |
| `Material::createMetallic(Color, roughness)` | Metallic surface |
| `Material::createEmissive(Color, intensity)` | Glowing material |
| `Material::createGlass(opacity)` | Transparent glass |

---

## vde::LightBox

**Header**: `<vde/api/LightBox.h>`

Lighting configuration for a scene. Supports up to 8 lights.

### Methods

| Method | Description |
|--------|-------------|
| `void setAmbientColor(const Color&)` | Set ambient light color |
| `void setAmbientIntensity(float)` | Set ambient intensity |
| `size_t addLight(const Light&)` | Add a light (returns index) |
| `void removeLight(size_t index)` | Remove a light |
| `void clearLights()` | Remove all lights |

### Presets

| Class | Description |
|-------|-------------|
| `SimpleColorLightBox(Color)` | Single-color ambient with directional light |
| `ThreePointLightBox()` | Classic key/fill/back 3-point lighting |

### Light Types

```cpp
struct Light {
    LightType type;     // Directional, Point, Spot
    Color color;
    float intensity;
    Position position;  // Point/Spot only
    Direction direction; // Directional/Spot only
    float range;        // Point/Spot attenuation range
    float spotInnerAngle, spotOuterAngle; // Spot only
};
```

---

## vde::InputHandler

**Header**: `<vde/api/InputHandler.h>`

Abstract interface for handling keyboard, mouse, and gamepad input. Subclass and attach to Game or Scene.

### Virtual Methods

#### Keyboard

| Method | Description |
|--------|-------------|
| `virtual void onKeyPress(int key)` | Key pressed |
| `virtual void onKeyRelease(int key)` | Key released |
| `virtual void onKeyRepeat(int key)` | Key held (repeat) |
| `virtual void onCharInput(unsigned int codepoint)` | Character input (text entry) |

#### Mouse

| Method | Description |
|--------|-------------|
| `virtual void onMouseButtonPress(int button, double x, double y)` | Mouse button pressed |
| `virtual void onMouseButtonRelease(int button, double x, double y)` | Mouse button released |
| `virtual void onMouseMove(double x, double y)` | Mouse moved |
| `virtual void onMouseScroll(double xOffset, double yOffset)` | Mouse scroll |
| `virtual void onMouseEnter()` | Mouse entered window |
| `virtual void onMouseLeave()` | Mouse left window |

#### Gamepad

| Method | Description |
|--------|-------------|
| `virtual void onGamepadConnect(int gamepadId, const char* name)` | Gamepad connected |
| `virtual void onGamepadDisconnect(int gamepadId)` | Gamepad disconnected |
| `virtual void onGamepadButtonPress(int gamepadId, int button)` | Gamepad button pressed |
| `virtual void onGamepadButtonRelease(int gamepadId, int button)` | Gamepad button released |
| `virtual void onGamepadAxis(int gamepadId, int axis, float value)` | Gamepad axis changed |

### Query Methods (Polling)

| Method | Description |
|--------|-------------|
| `bool isKeyPressed(int key) const` | Check if key is pressed |
| `bool isMouseButtonPressed(int button) const` | Check if mouse button is pressed |
| `void getMousePosition(double& x, double& y) const` | Get mouse position |
| `bool isGamepadConnected(int gamepadId) const` | Check if gamepad is connected |
| `bool isGamepadButtonPressed(int gamepadId, int button) const` | Check if gamepad button is pressed |
| `float getGamepadAxis(int gamepadId, int axis) const` | Get gamepad axis value (-1 to 1 for sticks, 0 to 1 for triggers) |
| `float getDeadZone() const` | Get axis dead zone threshold |
| `void setDeadZone(float deadZone)` | Set axis dead zone threshold (default 0.1) |

### Constants

Key, mouse button, and gamepad constants are defined in `<vde/api/KeyCodes.h>`:

**Keyboard**: `KEY_A`-`KEY_Z`, `KEY_0`-`KEY_9`, `KEY_SPACE`, `KEY_ESCAPE`, `KEY_ENTER`, arrow keys, function keys, etc.  
**Mouse**: `MOUSE_BUTTON_LEFT`, `MOUSE_BUTTON_RIGHT`, `MOUSE_BUTTON_MIDDLE`  
**Gamepads**: `JOYSTICK_1`-`JOYSTICK_16` (slots), `GAMEPAD_BUTTON_A/B/X/Y`, `GAMEPAD_BUTTON_LEFT/RIGHT_BUMPER`, `GAMEPAD_BUTTON_DPAD_*`, `GAMEPAD_AXIS_LEFT_X/Y`, `GAMEPAD_AXIS_RIGHT_X/Y`, `GAMEPAD_AXIS_LEFT/RIGHT_TRIGGER`

### Notes

- Gamepad input uses GLFW's standardized gamepad mapping (Xbox/PlayStation layout)
- Dead zone is applied to analog axes (values below threshold report as 0.0)
- State is tracked internally for polling methods
- Multiple gamepads (up to 16) are supported independently

---

## vde::ResourceManager

**Header**: `<vde/api/ResourceManager.h>`

Global resource cache with automatic deduplication using weak references.

### Methods

| Method | Description |
|--------|-------------|
| `template<T> ResourcePtr<T> load(const std::string& path)` | Load or retrieve cached resource |
| `template<T> ResourcePtr<T> add(const std::string& key, ResourcePtr<T>)` | Add pre-created resource |
| `template<T> ResourcePtr<T> get(const std::string& path)` | Get cached resource |
| `bool has(const std::string& path) const` | Check if resource is cached |
| `void remove(const std::string& path)` | Remove from cache |
| `void clear()` | Clear all cached resources |
| `size_t getCachedCount() const` | Number of cached resources |

---

## vde::Scheduler

**Header**: `<vde/api/Scheduler.h>`

Task-graph scheduler that manages per-frame execution order. Tasks are topologically sorted with phase-based tiebreaking.

### Task Phases

```cpp
enum class TaskPhase : uint8_t {
    Input = 0,        // Input processing
    GameLogic = 1,    // Scene update / game logic
    Audio = 2,        // Audio processing
    Physics = 3,      // Physics simulation
    PostPhysics = 4,  // Post-physics sync
    PreRender = 5,    // Camera apply, lights, UBO writes
    Render = 6        // Vulkan command recording
};
```

### Methods

| Method | Description |
|--------|-------------|
| `TaskId addTask(const TaskDescriptor&)` | Add a task (returns unique ID) |
| `void removeTask(TaskId)` | Remove a task |
| `void clear()` | Remove all tasks |
| `void execute()` | Execute all tasks in sorted order |
| `size_t getTaskCount() const` | Number of registered tasks |
| `bool hasTask(TaskId) const` | Check task existence |
| `std::string getTaskName(TaskId) const` | Get task name |
| `const vector<TaskId>& getLastExecutionOrder() const` | Execution order from last run |
| `void setWorkerThreadCount(size_t)` | Set parallel worker threads (0 = single-threaded) |
| `size_t getWorkerThreadCount() const` | Get worker thread count |

### TaskDescriptor

```cpp
struct TaskDescriptor {
    std::string name;                        // Human-readable name
    TaskPhase phase = TaskPhase::GameLogic;  // Execution phase (tiebreaker)
    std::function<void()> work;              // Work function
    std::vector<TaskId> dependsOn;           // Dependencies
    bool mainThreadOnly = true;              // Must run on main thread
};
```

---

## vde::SceneGroup

**Header**: `<vde/api/SceneGroup.h>`

Describes a set of scenes active simultaneously for multi-scene rendering.

### Factories

```cpp
// Simple group (all scenes get fullWindow viewport)
auto group = SceneGroup::create("gameplay", {"world", "hud", "minimap"});

// With explicit viewport layout for split-screen
auto group = SceneGroup::createWithViewports("splitscreen", {
    {"player1", ViewportRect::leftHalf()},
    {"player2", ViewportRect::rightHalf()},
});
```

### SceneGroupEntry

```cpp
struct SceneGroupEntry {
    std::string sceneName;
    ViewportRect viewport = ViewportRect::fullWindow();
};
```

---

## vde::ViewportRect

**Header**: `<vde/api/ViewportRect.h>`

Normalized viewport rectangle for split-screen rendering (origin top-left, [0,1] range).

### Fields

| Field | Description |
|-------|-------------|
| `float x, y` | Top-left corner (normalized) |
| `float width, height` | Size (normalized) |

### Static Factories

| Factory | Description |
|---------|-------------|
| `fullWindow()` | `{0, 0, 1, 1}` — entire window |
| `topLeft()` | `{0, 0, 0.5, 0.5}` |
| `topRight()` | `{0.5, 0, 0.5, 0.5}` |
| `bottomLeft()` | `{0, 0.5, 0.5, 0.5}` |
| `bottomRight()` | `{0.5, 0.5, 0.5, 0.5}` |
| `leftHalf()` | `{0, 0, 0.5, 1}` |
| `rightHalf()` | `{0.5, 0, 0.5, 1}` |
| `topHalf()` | `{0, 0, 1, 0.5}` |
| `bottomHalf()` | `{0, 0.5, 1, 0.5}` |

### Methods

| Method | Description |
|--------|-------------|
| `bool contains(float x, float y) const` | Hit-test normalized point |
| `VkViewport toVkViewport(uint32_t w, uint32_t h) const` | Convert to Vulkan viewport |
| `VkRect2D toVkScissor(uint32_t w, uint32_t h) const` | Convert to Vulkan scissor |
| `float getAspectRatio(uint32_t w, uint32_t h) const` | Compute pixel aspect ratio |

---

## vde::AudioEvent

**Header**: `<vde/api/AudioEvent.h>`

Describes an audio action queued from game logic and processed in the Audio phase.

### Types

```cpp
enum class AudioEventType : uint8_t {
    PlaySFX, PlaySFXAt, PlayMusic,
    StopSound, StopAll, PauseSound, ResumeSound, SetVolume
};
```

### Fields

| Field | Description |
|-------|-------------|
| `AudioEventType type` | Event type |
| `shared_ptr<AudioClip> clip` | Audio clip (for Play events) |
| `float volume, pitch` | Volume and pitch multipliers |
| `bool loop` | Loop flag |
| `float x, y, z` | 3D position (for PlaySFXAt) |
| `uint32_t soundId` | Sound ID (for Stop/Pause/Resume) |

### Factory Helpers

```cpp
AudioEvent::playSFX(clip, volume, pitch, loop);
AudioEvent::playSFXAt(clip, x, y, z, volume, pitch);
AudioEvent::playMusic(clip, volume, loop, fadeTime);
AudioEvent::stopSound(soundId, fadeTime);
AudioEvent::stopAll();
```

---

## vde::AudioManager

**Header**: `<vde/api/AudioManager.h>`

Singleton audio system using miniaudio. Supports music, SFX, 3D spatial audio, and volume mixing.

### Core

| Method | Description |
|--------|-------------|
| `static AudioManager& getInstance()` | Get singleton |
| `bool initialize(const AudioSettings&)` | Initialize audio engine |
| `void shutdown()` | Shutdown audio |
| `void update(float dt)` | Per-frame update |
| `bool isInitialized() const` | Check initialization |

### Playback

| Method | Description |
|--------|-------------|
| `uint32_t playSFX(shared_ptr<AudioClip>, volume, pitch, loop)` | Play sound effect |
| `uint32_t playMusic(shared_ptr<AudioClip>, volume, loop, fadeIn)` | Play music |
| `void stopSound(uint32_t id, float fadeOut)` | Stop a sound |
| `void stopAll()` | Stop all sounds |
| `void stopAllMusic()` | Stop all music |
| `void stopAllSFX()` | Stop all SFX |
| `void pauseSound(uint32_t id)` | Pause a sound |
| `void resumeSound(uint32_t id)` | Resume a sound |
| `bool isPlaying(uint32_t id) const` | Check if sound is playing |

### Volume & Mute

| Method | Description |
|--------|-------------|
| `void setMasterVolume(float)` | Set master volume (0-1) |
| `void setMusicVolume(float)` | Set music volume |
| `void setSFXVolume(float)` | Set SFX volume |
| `void setMuted(bool)` | Mute/unmute all |

### 3D Audio

| Method | Description |
|--------|-------------|
| `void setSoundPosition(uint32_t id, float x, float y, float z)` | Set sound position |
| `void setListenerPosition(float x, float y, float z)` | Set listener position |
| `void setListenerOrientation(float fx, fy, fz, ux, uy, uz)` | Set listener direction |

---

## vde::AudioClip

**Header**: `<vde/api/AudioClip.h>`

Audio resource for loading WAV, MP3, OGG, and FLAC files.

| Method | Description |
|--------|-------------|
| `bool loadFromFile(const std::string& path)` | Load audio data |
| `void setStreaming(bool)` | Enable streaming for large files |
| `bool isStreaming() const` | Check streaming state |
| `bool isLoaded() const` | Check if data is loaded |

---

## vde::PhysicsScene

**Header**: `<vde/api/PhysicsScene.h>`

2D physics simulation with fixed-timestep accumulator, AABB collision detection, and impulse-based resolution. Uses pimpl for header isolation.

### Body Management

| Method | Description |
|--------|-------------|
| `PhysicsBodyId createBody(const PhysicsBodyDef&)` | Create a physics body |
| `void destroyBody(PhysicsBodyId)` | Destroy a body |
| `PhysicsBodyState getBodyState(PhysicsBodyId) const` | Get body state |
| `PhysicsBodyDef getBodyDef(PhysicsBodyId) const` | Get body definition |
| `bool hasBody(PhysicsBodyId) const` | Check body existence |

### Forces & Velocity

| Method | Description |
|--------|-------------|
| `void applyForce(PhysicsBodyId, const glm::vec2&)` | Apply force (accumulated) |
| `void applyImpulse(PhysicsBodyId, const glm::vec2&)` | Apply instant impulse |
| `void setLinearVelocity(PhysicsBodyId, const glm::vec2&)` | Set velocity directly |
| `void setBodyPosition(PhysicsBodyId, const glm::vec2&)` | Set position directly |

### Simulation

| Method | Description |
|--------|-------------|
| `void step(float dt)` | Step simulation (fixed-timestep accumulator) |
| `float getInterpolationAlpha() const` | Get rendering interpolation alpha [0,1) |
| `int getLastStepCount() const` | Sub-steps taken in last step() |

### Collision Callbacks

| Method | Description |
|--------|-------------|
| `void setOnCollisionBegin(CollisionCallback)` | Global collision begin |
| `void setOnCollisionEnd(CollisionCallback)` | Global collision end |
| `void setBodyOnCollisionBegin(PhysicsBodyId, CollisionCallback)` | Per-body begin |
| `void setBodyOnCollisionEnd(PhysicsBodyId, CollisionCallback)` | Per-body end |

### Raycast & Spatial Queries

| Method | Description |
|--------|-------------|
| `bool raycast(origin, direction, maxDist, outHit) const` | Cast ray, get closest hit |
| `vector<PhysicsBodyId> queryAABB(min, max) const` | Get bodies in AABB region |

### Queries

| Method | Description |
|--------|-------------|
| `size_t getBodyCount() const` | Total body count |
| `size_t getActiveBodyCount() const` | Active body count |
| `void setGravity(const glm::vec2&)` | Set gravity |
| `glm::vec2 getGravity() const` | Get gravity |

---

## vde::PhysicsEntity

**Header**: `<vde/api/PhysicsEntity.h>`

Mixin class binding a visual entity to a physics body with automatic transform sync and interpolation.

### Methods

| Method | Description |
|--------|-------------|
| `PhysicsBodyId createPhysicsBody(const PhysicsBodyDef&)` | Create physics body in scene |
| `PhysicsBodyId getPhysicsBodyId() const` | Get body ID |
| `PhysicsBodyState getPhysicsState() const` | Get current body state |
| `void applyForce(const glm::vec2&)` | Apply force |
| `void applyImpulse(const glm::vec2&)` | Apply impulse |
| `void setLinearVelocity(const glm::vec2&)` | Set velocity |
| `void syncFromPhysics(float alpha)` | Copy interpolated physics position to entity |
| `void syncToPhysics()` | Copy entity position to physics body |
| `void setAutoSync(bool)` | Enable/disable automatic PostPhysics sync |
| `bool getAutoSync() const` | Check auto-sync state |

### Derived Classes

| Class | Description |
|-------|-------------|
| `PhysicsSpriteEntity` | SpriteEntity + PhysicsEntity (2D sprite driven by physics) |
| `PhysicsMeshEntity` | MeshEntity + PhysicsEntity (3D mesh driven by physics) |

---

## vde::ThreadPool

**Header**: `<vde/api/ThreadPool.h>`

Fixed-size thread pool for parallel task execution. Used by Scheduler.

### Methods

| Method | Description |
|--------|-------------|
| `explicit ThreadPool(size_t count = 0)` | Create pool (0 = inline execution) |
| `std::future<void> submit(std::function<void()>)` | Submit task |
| `void waitAll()` | Block until all tasks complete |
| `size_t getThreadCount() const` | Get worker thread count |
| `vector<thread::id> getWorkerThreadIds() const` | Get worker thread IDs |

---

## Physics Types

**Header**: `<vde/api/PhysicsTypes.h>`

### PhysicsConfig

```cpp
struct PhysicsConfig {
    float fixedTimestep = 1.0f / 60.0f;  // Fixed step (seconds)
    glm::vec2 gravity = {0.0f, -9.81f};  // Gravity vector
    int maxSubSteps = 8;                 // Spiral-of-death cap
    int iterations = 4;                  // Solver iterations
};
```

### PhysicsBodyDef

```cpp
struct PhysicsBodyDef {
    PhysicsBodyType type;    // Static, Dynamic, Kinematic
    PhysicsShape shape;      // Box, Circle, Sphere, Capsule
    glm::vec2 position;
    float rotation;
    glm::vec2 extents;       // Half-extents (box) or {radius, 0} (circle)
    float mass, friction, restitution, linearDamping;
    bool isSensor;           // Triggers callbacks but no collision response
};
```

### PhysicsBodyState

```cpp
struct PhysicsBodyState {
    glm::vec2 position;
    float rotation;
    glm::vec2 velocity;
    bool isAwake;
};
```

### CollisionEvent & RaycastHit

```cpp
struct CollisionEvent {
    PhysicsBodyId bodyA, bodyB;
    glm::vec2 contactPoint, normal;
    float depth;
};

struct RaycastHit {
    PhysicsBodyId bodyId;
    glm::vec2 point, normal;
    float distance;
};
```
