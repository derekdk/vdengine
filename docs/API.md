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
| `void setResizeCallback(ResizeCallback)` | Set window resize callback |

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
| `bool initialize(Window* window)` | Initialize Vulkan with the given window |
| `void cleanup()` | Destroy all Vulkan resources |
| `void drawFrame()` | Render a single frame |
| `void waitIdle()` | Wait for GPU to finish all operations |
| `void setRenderCallback(RenderCallback)` | Set the per-frame render callback |
| `void setClearColor(const glm::vec4&)` | Set the clear color |

### Accessors

| Method | Returns |
|--------|---------|
| `VkDevice getDevice()` | Logical device handle |
| `VkPhysicalDevice getPhysicalDevice()` | Physical device handle |
| `VkRenderPass getRenderPass()` | Main render pass |
| `VkCommandPool getCommandPool()` | Command pool for graphics |
| `VkQueue getGraphicsQueue()` | Graphics queue |
| `VkExtent2D getSwapChainExtent()` | Swapchain dimensions |
| `uint32_t getSwapChainImageCount()` | Number of swapchain images |

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
| `void setAspectRatio(float aspect)` | Update aspect ratio |
| `void setFOV(float fov)` | Update field of view |

### Matrix Accessors

| Method | Returns |
|--------|---------|
| `glm::mat4 getViewMatrix()` | View matrix |
| `glm::mat4 getProjectionMatrix()` | Projection matrix (Vulkan Y-flip) |
| `glm::mat4 getViewProjectionMatrix()` | Combined VP matrix |

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
| `bool loadFromFile(const std::string& path, VkDevice, VkPhysicalDevice, VkCommandPool, VkQueue)` | Load texture from file |
| `void cleanup()` | Destroy texture resources |
| `VkDescriptorImageInfo getDescriptorInfo()` | Get descriptor for binding |

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
```

---

## vde::ShaderCache

**Header**: `<vde/ShaderCache.h>`

Shader compilation and caching.

### Constructor

```cpp
ShaderCache(VkDevice device, const std::string& cacheDir = "cache/");
```

### Methods

| Method | Description |
|--------|-------------|
| `VkShaderModule getOrCompile(const std::string& path, ShaderStage stage)` | Get cached or compile shader |
| `void clear()` | Clear all cached modules |

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

---

## Version Information

```cpp
namespace vde {
    struct Version {
        static constexpr int major = 0;
        static constexpr int minor = 1;
        static constexpr int patch = 0;
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
if (!texture.loadFromFile("missing.png", ...)) {
    // Handle missing texture
}
```
