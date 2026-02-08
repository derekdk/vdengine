# VDE API Reference

This document provides a reference for the public API of the Vulkan Display Engine.

## Core Header

Include all VDE functionality with a single header:

```cpp
#include <vde/Core.h>
```

Or include individual components as needed.

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
