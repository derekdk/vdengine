---
name: vulkan-patterns
description: Vulkan patterns and common tasks for the VDE engine. Use this when working with Vulkan resources, buffers, textures, or descriptors.
---

# Vulkan Patterns and Common Tasks

This skill provides essential patterns and workflows for working with Vulkan resources in the VDE engine.

## When to use this skill

- Creating or managing Vulkan resources (buffers, textures, images)
- Working with descriptor sets and descriptor layouts
- Implementing resource cleanup and RAII patterns
- Handling Vulkan errors and validation
- Optimizing Vulkan resource usage
- Understanding memory management strategies
- Performing common Vulkan operations (buffer copies, image transitions)

## Core Patterns

### RAII for Vulkan Resources

All Vulkan resources must follow RAII principles with proper cleanup in destructors.

**Pattern:**
```cpp
class ResourceManager {
public:
    ~ResourceManager() {
        cleanup();
    }
    
    // Prevent copying
    ResourceManager(const ResourceManager&) = delete;
    ResourceManager& operator=(const ResourceManager&) = delete;
    
    // Allow moving
    ResourceManager(ResourceManager&& other) noexcept
        : m_device(other.m_device)
        , m_resource(other.m_resource) {
        other.m_device = VK_NULL_HANDLE;
        other.m_resource = VK_NULL_HANDLE;
    }
    
    ResourceManager& operator=(ResourceManager&& other) noexcept {
        if (this != &other) {
            cleanup();
            m_device = other.m_device;
            m_resource = other.m_resource;
            other.m_device = VK_NULL_HANDLE;
            other.m_resource = VK_NULL_HANDLE;
        }
        return *this;
    }
    
    void cleanup() {
        if (m_device != VK_NULL_HANDLE && m_resource != VK_NULL_HANDLE) {
            vkDestroyResource(m_device, m_resource, nullptr);
            m_resource = VK_NULL_HANDLE;
        }
    }
    
private:
    VkDevice m_device = VK_NULL_HANDLE;
    VkResource m_resource = VK_NULL_HANDLE;
};
```

**Key principles:**
- Delete copy constructor and copy assignment
- Implement move constructor and move assignment
- Nullify handles after move to prevent double-free
- Always check for `VK_NULL_HANDLE` before destroying
- Destroy resources in reverse order of creation

**Examples in codebase:**
- [Texture.cpp](../../../src/Texture.cpp) - Lines 8-62 (destructor, move semantics)
- [DescriptorManager.cpp](../../../src/DescriptorManager.cpp) - Lines 6-72 (cleanup pattern)
- [VulkanContext.cpp](../../../src/VulkanContext.cpp) - Lines 24-136 (comprehensive cleanup)

### VkResult Validation

Always validate VkResult and throw on failure with descriptive messages.

**Pattern:**
```cpp
VkResult result = vkCreateBuffer(device, &bufferInfo, nullptr, &buffer);
if (result != VK_SUCCESS) {
    throw std::runtime_error("Failed to create buffer!");
}
```

**For allocation failures with cleanup:**
```cpp
if (vkAllocateMemory(device, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
    vkDestroyBuffer(device, buffer, nullptr);
    throw std::runtime_error("Failed to allocate buffer memory!");
}
```

**Examples in codebase:**
- [BufferUtils.cpp](../../../src/BufferUtils.cpp) - Lines 68-69, 82-86
- [Texture.cpp](../../../src/Texture.cpp) - Lines 246-248, 259-261
- [DescriptorManager.cpp](../../../src/DescriptorManager.cpp) - Lines 85-87, 103-105

### Initialization Validation

Utility classes with static state should validate initialization before use.

**Pattern:**
```cpp
class Utils {
public:
    static void init(VkDevice device, /*...*/) {
        s_device = device;
        // ...
    }
    
    static bool isInitialized() {
        return s_device != VK_NULL_HANDLE /* && ... */;
    }
    
    static void someOperation() {
        if (!isInitialized()) {
            throw std::runtime_error("Utils not initialized! Call init() first.");
        }
        // ... perform operation
    }
    
private:
    static VkDevice s_device;
};
```

**Example in codebase:**
- [BufferUtils.cpp](../../../src/BufferUtils.cpp) - Lines 12-31 (init/isInitialized/reset)

## Common Tasks

### Creating Buffers

Use `BufferUtils` for all buffer creation tasks.

**Initialize BufferUtils first:**
```cpp
// Called once during VulkanContext initialization
BufferUtils::init(device, physicalDevice, commandPool, graphicsQueue);
```

**Create a basic buffer:**
```cpp
VkBuffer buffer;
VkDeviceMemory bufferMemory;

BufferUtils::createBuffer(
    size,
    VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    buffer,
    bufferMemory
);
```

**Create a device-local buffer with data:**
```cpp
VkBuffer buffer;
VkDeviceMemory memory;
void* mappedMemory;

BufferUtils::createDeviceLocalBuffer(
    data,           // Source data pointer
    dataSize,       // Size in bytes
    usage,          // VK_BUFFER_USAGE_* flags
    buffer,
    memory,
    &mappedMemory   // Optional: get mapped pointer for updates
);
```

**Create a persistently mapped buffer:**
```cpp
VkBuffer buffer;
VkDeviceMemory memory;
void* mappedMemory;

BufferUtils::createMappedBuffer(
    size,
    usage,
    buffer,
    memory,
    &mappedMemory
);

// Use mappedMemory directly for updates
std::memcpy(mappedMemory, data, size);
```

**Copy between buffers:**
```cpp
BufferUtils::copyBuffer(srcBuffer, dstBuffer, size);
```

**Single-time commands:**
```cpp
VkCommandBuffer cmdBuffer = BufferUtils::beginSingleTimeCommands();
// ... record commands ...
BufferUtils::endSingleTimeCommands(cmdBuffer);
```

**Examples in codebase:**
- [BufferUtils.h](../../../include/vde/BufferUtils.h) - Full API documentation
- [VulkanContext.cpp](../../../src/VulkanContext.cpp) - Line 52 (initialization)

### Loading Textures

Use the `Texture` class for image loading and texture management.

**Load from file:**
```cpp
Texture texture;
bool success = texture.loadFromFile(
    "path/to/image.png",
    device,
    physicalDevice,
    commandPool,
    graphicsQueue
);

if (!success) {
    throw std::runtime_error("Failed to load texture!");
}

// Access texture resources
VkImageView imageView = texture.getImageView();
VkSampler sampler = texture.getSampler();
```

**Create from raw pixel data:**
```cpp
Texture texture;
bool success = texture.createFromData(
    pixelData,      // RGBA uint8_t* data
    width,
    height,
    device,
    physicalDevice,
    commandPool,
    graphicsQueue
);
```

**Key features:**
- Automatic staging buffer creation and cleanup
- Proper image layout transitions
- RAII-based resource management
- Move semantics for efficient transfers

**Examples in codebase:**
- [Texture.h](../../../include/vde/Texture.h) - Full API documentation
- [Texture.cpp](../../../src/Texture.cpp) - Lines 65-127 (loadFromFile implementation)

### Managing Descriptor Sets

Use `DescriptorManager` for descriptor set layout and allocation.

**Initialize DescriptorManager:**
```cpp
DescriptorManager descriptorManager;
descriptorManager.init(device);
```

**Get descriptor set layouts:**
```cpp
VkDescriptorSetLayout uboLayout = descriptorManager.getUBOLayout();
VkDescriptorSetLayout samplerLayout = descriptorManager.getSamplerLayout();
```

**Allocate descriptor sets:**
```cpp
// Allocate multiple UBO descriptor sets (one per frame in flight)
std::vector<VkDescriptorSet> uboSets = 
    descriptorManager.allocateUBODescriptorSets(MAX_FRAMES_IN_FLIGHT);

// Allocate a single sampler descriptor set
VkDescriptorSet samplerSet = 
    descriptorManager.allocateSamplerDescriptorSet();
```

**Update descriptor sets:**
```cpp
// Update UBO descriptor
VkDescriptorBufferInfo bufferInfo{};
bufferInfo.buffer = uniformBuffer;
bufferInfo.offset = 0;
bufferInfo.range = sizeof(UniformBufferObject);

VkWriteDescriptorSet descriptorWrite{};
descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
descriptorWrite.dstSet = descriptorSet;
descriptorWrite.dstBinding = 0;
descriptorWrite.dstArrayElement = 0;
descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
descriptorWrite.descriptorCount = 1;
descriptorWrite.pBufferInfo = &bufferInfo;

vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);

// Update texture sampler descriptor
VkDescriptorImageInfo imageInfo{};
imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
imageInfo.imageView = texture.getImageView();
imageInfo.sampler = texture.getSampler();

VkWriteDescriptorSet samplerWrite{};
samplerWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
samplerWrite.dstSet = samplerDescriptorSet;
samplerWrite.dstBinding = 0;
samplerWrite.dstArrayElement = 0;
samplerWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
samplerWrite.descriptorCount = 1;
samplerWrite.pImageInfo = &imageInfo;

vkUpdateDescriptorSets(device, 1, &samplerWrite, 0, nullptr);
```

**Examples in codebase:**
- [DescriptorManager.h](../../../include/vde/DescriptorManager.h) - Full API documentation
- [DescriptorManager.cpp](../../../src/DescriptorManager.cpp) - Implementation details

### Working with Camera

Use the `Camera` class for view and projection matrices.

**Set camera orientation:**
```cpp
Camera camera;

// Set position and rotation
camera.setFromPitchYaw(
    pitch,      // Pitch angle in radians
    yaw,        // Yaw angle in radians
    roll,       // Roll angle in radians
    target      // glm::vec3 target position
);
```

**Set projection:**
```cpp
// Perspective projection
camera.setPerspective(
    glm::radians(45.0f),  // FOV
    aspectRatio,           // width/height
    0.1f,                  // near plane
    100.0f                 // far plane
);
```

**Get matrices:**
```cpp
glm::mat4 viewMatrix = camera.getViewMatrix();
glm::mat4 projectionMatrix = camera.getProjectionMatrix();
glm::mat4 viewProjection = camera.getViewProjectionMatrix();
```

**Examples in codebase:**
- [Camera.h](../../../include/vde/Camera.h) - Full API documentation
- [Camera.cpp](../../../src/Camera.cpp) - Implementation details

## Memory Management Strategies

### Staging Buffers for Device-Local Memory

For optimal performance, use staging buffers to transfer data to device-local memory.

**Pattern:**
```cpp
// 1. Create staging buffer (host-visible, host-coherent)
VkBuffer stagingBuffer;
VkDeviceMemory stagingMemory;
BufferUtils::createBuffer(
    size,
    VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
    stagingBuffer,
    stagingMemory
);

// 2. Map and copy data to staging buffer
void* data;
vkMapMemory(device, stagingMemory, 0, size, 0, &data);
std::memcpy(data, sourceData, size);
vkUnmapMemory(device, stagingMemory);

// 3. Create device-local buffer
VkBuffer deviceBuffer;
VkDeviceMemory deviceMemory;
BufferUtils::createBuffer(
    size,
    VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    deviceBuffer,
    deviceMemory
);

// 4. Copy from staging to device-local
BufferUtils::copyBuffer(stagingBuffer, deviceBuffer, size);

// 5. Cleanup staging buffer
vkDestroyBuffer(device, stagingBuffer, nullptr);
vkFreeMemory(device, stagingMemory, nullptr);
```

**When to use:**
- Vertex buffers (static geometry)
- Index buffers
- Texture data
- Any data that won't change frequently

**Examples in codebase:**
- [BufferUtils.cpp](../../../src/BufferUtils.cpp) - Lines 135-180 (createDeviceLocalBuffer)
- [Texture.cpp](../../../src/Texture.cpp) - Lines 86-127 (image staging)

### Persistently Mapped Buffers

For frequently updated data, use persistently mapped host-visible buffers.

**When to use:**
- Uniform buffers updated every frame
- Dynamic vertex buffers
- Push constant data (though prefer actual push constants for small data)

**Pattern:**
```cpp
VkBuffer buffer;
VkDeviceMemory memory;
void* mappedMemory;

BufferUtils::createMappedBuffer(
    size,
    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
    buffer,
    memory,
    &mappedMemory
);

// Update data directly (no map/unmap needed)
std::memcpy(mappedMemory, newData, size);
```

**Examples in codebase:**
- [BufferUtils.cpp](../../../src/BufferUtils.cpp) - Lines 182-203 (createMappedBuffer)
- [UniformBuffer.cpp](../../../src/UniformBuffer.cpp) - Usage of persistently mapped buffers

## Error Handling

### Throwing Exceptions

Throw `std::runtime_error` for unrecoverable errors with descriptive messages.

**Pattern:**
```cpp
if (condition_failed) {
    throw std::runtime_error("Descriptive error message explaining what failed!");
}
```

**Examples:**
- `"Failed to create buffer!"`
- `"Failed to allocate buffer memory!"`
- `"Failed to find suitable memory type!"`
- `"BufferUtils not initialized! Call BufferUtils::init() first."`

### Returning Boolean Success

Use boolean return values for recoverable operations (e.g., optional features, file loading).

**Pattern:**
```cpp
bool loadTexture(const std::string& filepath) {
    ImageData imageData = ImageLoader::load(filepath);
    if (!imageData.isValid()) {
        return false;  // Caller can handle missing texture
    }
    // ... continue loading
    return true;
}
```

**Examples in codebase:**
- [Texture.cpp](../../../src/Texture.cpp) - Lines 65-80 (loadFromFile returns bool)

### Cleanup on Failure

Always clean up partial resources when an operation fails midway.

**Pattern:**
```cpp
if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create buffer!");
}

if (vkAllocateMemory(device, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
    vkDestroyBuffer(device, buffer, nullptr);  // Clean up buffer
    throw std::runtime_error("Failed to allocate memory!");
}
```

**Examples in codebase:**
- [BufferUtils.cpp](../../../src/BufferUtils.cpp) - Lines 82-86

## Performance Best Practices

### Minimize Per-Frame API Calls

- **Pre-allocate resources:** Create buffers, textures, and descriptor sets during initialization
- **Batch updates:** Update multiple descriptor sets with one `vkUpdateDescriptorSets` call
- **Reuse command buffers:** Record command buffers once and submit repeatedly
- **Use push constants:** For small, frequently changing data (< 128 bytes)

### Memory Preferences

1. **Device-local memory** for static buffers (best GPU performance)
2. **Host-visible + host-coherent** for frequently updated data (avoid map/unmap overhead)
3. **Staging buffers** for initial upload to device-local memory

### Command Buffer Optimization

```cpp
// Single-time commands for one-off operations
VkCommandBuffer cmd = BufferUtils::beginSingleTimeCommands();
// ... record commands ...
BufferUtils::endSingleTimeCommands(cmd);

// Pre-recorded commands for repeated use
// Record once during initialization, submit every frame
```

### Descriptor Set Strategies

- **Uniform buffers:** Use descriptor sets (binding 0)
- **Textures:** Use descriptor sets with combined image samplers
- **Push constants:** Use for per-draw data (transforms, material IDs, etc.)

**Example descriptor set layout usage:**
```cpp
// UBO layout (set 0, binding 0)
VkDescriptorSetLayoutBinding uboBinding{};
uboBinding.binding = 0;
uboBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
uboBinding.descriptorCount = 1;
uboBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

// Sampler layout (set 1, binding 0)
VkDescriptorSetLayoutBinding samplerBinding{};
samplerBinding.binding = 0;
samplerBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
samplerBinding.descriptorCount = 1;
samplerBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
```

## Debugging and Validation

### Validation Layers

Enable validation layers during development (defined in VulkanContext):

```cpp
#ifdef NDEBUG
    const bool kEnableValidationLayers = false;
#else
    const bool kEnableValidationLayers = true;
#endif
```

### Diagnostic Tools

- **RenderDoc:** Frame capture and analysis
- **Validation layers:** Catch API misuse, memory leaks, synchronization issues
- **Vulkan Configurator (vkconfig):** Configure validation layer settings
- **GPU vendor tools:** NVIDIA Nsight, AMD Radeon GPU Profiler

### Common Issues to Check

1. **Handle validation:** Always check for `VK_NULL_HANDLE` before destroying
2. **Synchronization:** Use proper semaphores and fences for async operations
3. **Memory leaks:** Ensure all `vkCreate*` calls have matching `vkDestroy*` calls
4. **Descriptor updates:** Update descriptor sets before binding them
5. **Layout transitions:** Transition image layouts before use (shader read, transfer, etc.)

## Quick Reference

### Buffer Creation Checklist

- [ ] Initialize BufferUtils once during context creation
- [ ] Choose appropriate usage flags (VERTEX, INDEX, UNIFORM, TRANSFER_SRC/DST)
- [ ] Choose appropriate memory properties (DEVICE_LOCAL, HOST_VISIBLE, HOST_COHERENT)
- [ ] For static data: use staging buffer → device-local pattern
- [ ] For dynamic data: use persistently mapped buffer
- [ ] Always validate VkResult and throw on failure
- [ ] Implement proper cleanup in destructor

### Texture Loading Checklist

- [ ] Use `Texture::loadFromFile` or `createFromData`
- [ ] Validate return value (bool success)
- [ ] Texture handles staging buffer creation/cleanup internally
- [ ] Access via `getImageView()` and `getSampler()`
- [ ] RAII cleanup handled automatically

### Descriptor Set Checklist

- [ ] Initialize DescriptorManager once
- [ ] Get appropriate layout (UBO or Sampler)
- [ ] Allocate descriptor sets (one per frame in flight for UBOs)
- [ ] Update descriptor sets with buffer/image info
- [ ] Bind descriptor sets before draw calls

## Additional Resources

- **Vulkan Tutorial:** https://vulkan-tutorial.com/
- **Vulkan Specification:** https://registry.khronos.org/vulkan/specs/
- **GLM Documentation:** https://glm.g-truc.net/
- **GLFW Documentation:** https://www.glfw.org/documentation.html
- **stb_image:** https://github.com/nothings/stb (used by ImageLoader)

## Related Skills

- [add-component](../add-component/SKILL.md) - Adding new components to the engine
- [build-tool-workflows](../build-tool-workflows/SKILL.md) - Building and testing
- [conventions](../conventions.md) - Coding conventions and style
- [architecture](../architecture/SKILL.md) - Project architecture and layout
