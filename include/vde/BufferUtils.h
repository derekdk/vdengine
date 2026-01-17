#pragma once

/**
 * @file BufferUtils.h
 * @brief Static utility class for Vulkan buffer operations
 * 
 * Provides convenience functions for common buffer operations including
 * memory type finding, buffer creation, and data transfer.
 */

#include <vulkan/vulkan.h>
#include <cstdint>
#include <stdexcept>

namespace vde {

/**
 * @brief Static utility class for Vulkan buffer operations.
 * 
 * Provides convenience functions for common buffer operations:
 * - Memory type finding
 * - Generic buffer creation
 * - Device-local buffer creation with staging
 * - Persistently mapped buffer creation
 * - Buffer-to-buffer copy
 */
class BufferUtils {
public:
    /**
     * @brief Initialize BufferUtils with required Vulkan handles.
     * Must be called before using any other BufferUtils functions.
     * 
     * @param device Logical device handle
     * @param physicalDevice Physical device handle
     * @param commandPool Command pool for transfer operations
     * @param graphicsQueue Queue for submitting transfer commands
     */
    static void init(VkDevice device, VkPhysicalDevice physicalDevice,
                     VkCommandPool commandPool, VkQueue graphicsQueue);
    
    /**
     * @brief Check if BufferUtils has been initialized.
     * @return true if init() has been called with valid handles
     */
    static bool isInitialized();
    
    /**
     * @brief Reset BufferUtils state (for cleanup/testing).
     */
    static void reset();
    
    /**
     * @brief Find a memory type that satisfies the given requirements.
     * 
     * @param typeFilter Bit field of suitable memory types from VkMemoryRequirements
     * @param properties Required memory property flags
     * @return Index of a suitable memory type
     * @throws std::runtime_error if no suitable memory type is found
     */
    static uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    
    /**
     * @brief Create a buffer with the specified properties.
     * 
     * @param size Size of the buffer in bytes
     * @param usage Buffer usage flags (VK_BUFFER_USAGE_*)
     * @param properties Memory property flags (VK_MEMORY_PROPERTY_*)
     * @param buffer Output buffer handle
     * @param bufferMemory Output memory handle
     * @throws std::runtime_error on failure
     */
    static void createBuffer(VkDeviceSize size,
                            VkBufferUsageFlags usage,
                            VkMemoryPropertyFlags properties,
                            VkBuffer& buffer,
                            VkDeviceMemory& bufferMemory);
    
    /**
     * @brief Copy data between buffers using a one-time command buffer.
     * 
     * @param srcBuffer Source buffer
     * @param dstBuffer Destination buffer
     * @param size Number of bytes to copy
     */
    static void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
    
    /**
     * @brief Begin a single-time command buffer for one-shot operations.
     * 
     * Used for operations like buffer copies, image layout transitions, etc.
     * Must be paired with endSingleTimeCommands().
     * 
     * @return Command buffer ready for recording commands
     * @throws std::runtime_error if BufferUtils not initialized or allocation fails
     */
    static VkCommandBuffer beginSingleTimeCommands();
    
    /**
     * @brief End, submit, and free a single-time command buffer.
     * 
     * Submits the command buffer, waits for completion, and frees the buffer.
     * Must be paired with beginSingleTimeCommands().
     * 
     * @param commandBuffer Command buffer to submit and free
     */
    static void endSingleTimeCommands(VkCommandBuffer commandBuffer);
    
    /**
     * @brief Create a device-local buffer and upload data via staging buffer.
     * 
     * This is the preferred method for creating vertex/index buffers that
     * will be read frequently by the GPU but rarely updated by the CPU.
     * 
     * @param data Pointer to data to upload
     * @param size Size of data in bytes
     * @param usage Buffer usage flags (TRANSFER_DST is added automatically)
     * @param buffer Output buffer handle
     * @param bufferMemory Output memory handle
     * @throws std::runtime_error on failure
     */
    static void createDeviceLocalBuffer(const void* data, VkDeviceSize size,
                                        VkBufferUsageFlags usage,
                                        VkBuffer& buffer,
                                        VkDeviceMemory& bufferMemory);
    
    /**
     * @brief Create a host-visible buffer with persistent mapping.
     * 
     * Useful for uniform buffers that are updated every frame.
     * The buffer remains mapped for the lifetime of the allocation.
     * 
     * @param size Size of the buffer in bytes
     * @param usage Buffer usage flags
     * @param buffer Output buffer handle
     * @param bufferMemory Output memory handle
     * @param mappedMemory Output pointer to mapped memory
     * @throws std::runtime_error on failure
     */
    static void createMappedBuffer(VkDeviceSize size,
                                   VkBufferUsageFlags usage,
                                   VkBuffer& buffer,
                                   VkDeviceMemory& bufferMemory,
                                   void** mappedMemory);

    // Accessors for testing
    static VkDevice getDevice() { return s_device; }
    static VkPhysicalDevice getPhysicalDevice() { return s_physicalDevice; }
    static VkCommandPool getCommandPool() { return s_commandPool; }
    static VkQueue getGraphicsQueue() { return s_graphicsQueue; }

private:
    static VkDevice s_device;
    static VkPhysicalDevice s_physicalDevice;
    static VkCommandPool s_commandPool;
    static VkQueue s_graphicsQueue;
};

} // namespace vde
