#include <vde/BufferUtils.h>
#include <cstring>

namespace vde {

// Static member definitions
VkDevice BufferUtils::s_device = VK_NULL_HANDLE;
VkPhysicalDevice BufferUtils::s_physicalDevice = VK_NULL_HANDLE;
VkCommandPool BufferUtils::s_commandPool = VK_NULL_HANDLE;
VkQueue BufferUtils::s_graphicsQueue = VK_NULL_HANDLE;

void BufferUtils::init(VkDevice device, VkPhysicalDevice physicalDevice,
                       VkCommandPool commandPool, VkQueue graphicsQueue) {
    s_device = device;
    s_physicalDevice = physicalDevice;
    s_commandPool = commandPool;
    s_graphicsQueue = graphicsQueue;
}

bool BufferUtils::isInitialized() {
    return s_device != VK_NULL_HANDLE &&
           s_physicalDevice != VK_NULL_HANDLE &&
           s_commandPool != VK_NULL_HANDLE &&
           s_graphicsQueue != VK_NULL_HANDLE;
}

void BufferUtils::reset() {
    s_device = VK_NULL_HANDLE;
    s_physicalDevice = VK_NULL_HANDLE;
    s_commandPool = VK_NULL_HANDLE;
    s_graphicsQueue = VK_NULL_HANDLE;
}

uint32_t BufferUtils::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    if (s_physicalDevice == VK_NULL_HANDLE) {
        throw std::runtime_error("BufferUtils not initialized! Call BufferUtils::init() first.");
    }
    
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(s_physicalDevice, &memProperties);
    
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    
    throw std::runtime_error("Failed to find suitable memory type!");
}

void BufferUtils::createBuffer(VkDeviceSize size,
                               VkBufferUsageFlags usage,
                               VkMemoryPropertyFlags properties,
                               VkBuffer& buffer,
                               VkDeviceMemory& bufferMemory) {
    if (s_device == VK_NULL_HANDLE) {
        throw std::runtime_error("BufferUtils not initialized! Call BufferUtils::init() first.");
    }
    
    // Create buffer
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    if (vkCreateBuffer(s_device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create buffer!");
    }
    
    // Get memory requirements
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(s_device, buffer, &memRequirements);
    
    // Allocate memory
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);
    
    if (vkAllocateMemory(s_device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        // Clean up buffer before throwing
        vkDestroyBuffer(s_device, buffer, nullptr);
        buffer = VK_NULL_HANDLE;
        throw std::runtime_error("Failed to allocate buffer memory!");
    }
    
    // Bind buffer to memory
    vkBindBufferMemory(s_device, buffer, bufferMemory, 0);
}

void BufferUtils::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();
    
    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);
    
    endSingleTimeCommands(commandBuffer);
}

VkCommandBuffer BufferUtils::beginSingleTimeCommands() {
    if (s_device == VK_NULL_HANDLE || s_commandPool == VK_NULL_HANDLE) {
        throw std::runtime_error("BufferUtils not initialized! Call BufferUtils::init() first.");
    }
    
    // Allocate temporary command buffer
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = s_commandPool;
    allocInfo.commandBufferCount = 1;
    
    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(s_device, &allocInfo, &commandBuffer);
    
    // Record copy command
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    
    vkBeginCommandBuffer(commandBuffer, &beginInfo);
    
    return commandBuffer;
}

void BufferUtils::endSingleTimeCommands(VkCommandBuffer commandBuffer) {
    vkEndCommandBuffer(commandBuffer);
    
    // Submit and wait
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    
    vkQueueSubmit(s_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(s_graphicsQueue);
    
    // Free command buffer
    vkFreeCommandBuffers(s_device, s_commandPool, 1, &commandBuffer);
}

void BufferUtils::createDeviceLocalBuffer(const void* data, VkDeviceSize size,
                                          VkBufferUsageFlags usage,
                                          VkBuffer& buffer,
                                          VkDeviceMemory& bufferMemory) {
    if (data == nullptr) {
        throw std::runtime_error("Cannot create device-local buffer with null data!");
    }
    
    // Create staging buffer (host-visible)
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;
    createBuffer(size,
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingBuffer, stagingMemory);
    
    // Copy data to staging buffer
    void* mapped;
    vkMapMemory(s_device, stagingMemory, 0, size, 0, &mapped);
    memcpy(mapped, data, static_cast<size_t>(size));
    vkUnmapMemory(s_device, stagingMemory);
    
    // Create device-local buffer
    createBuffer(size,
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                 buffer, bufferMemory);
    
    // Copy from staging to device-local
    copyBuffer(stagingBuffer, buffer, size);
    
    // Clean up staging buffer
    vkDestroyBuffer(s_device, stagingBuffer, nullptr);
    vkFreeMemory(s_device, stagingMemory, nullptr);
}

void BufferUtils::createMappedBuffer(VkDeviceSize size,
                                     VkBufferUsageFlags usage,
                                     VkBuffer& buffer,
                                     VkDeviceMemory& bufferMemory,
                                     void** mappedMemory) {
    if (mappedMemory == nullptr) {
        throw std::runtime_error("mappedMemory pointer cannot be null!");
    }
    
    // Create host-visible, coherent buffer
    createBuffer(size, usage,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 buffer, bufferMemory);
    
    // Persistently map the memory
    if (vkMapMemory(s_device, bufferMemory, 0, size, 0, mappedMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to map buffer memory!");
    }
}

} // namespace vde
