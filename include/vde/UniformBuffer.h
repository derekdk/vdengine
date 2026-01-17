#pragma once

/**
 * @file UniformBuffer.h
 * @brief Per-frame uniform buffers for GPU data
 */

#include <vulkan/vulkan.h>
#include <vector>
#include <cstdint>

namespace vde {

/**
 * @brief Manages per-frame uniform buffers for GPU data.
 * 
 * Creates multiple buffers (one per frame-in-flight) to avoid
 * synchronization issues when updating data during rendering.
 * 
 * Buffers are persistently mapped for efficient CPU->GPU updates
 * without needing to map/unmap each frame.
 */
class UniformBuffer {
public:
    UniformBuffer() = default;
    ~UniformBuffer();
    
    // Prevent copying
    UniformBuffer(const UniformBuffer&) = delete;
    UniformBuffer& operator=(const UniformBuffer&) = delete;
    
    // Allow moving
    UniformBuffer(UniformBuffer&& other) noexcept;
    UniformBuffer& operator=(UniformBuffer&& other) noexcept;
    
    /**
     * @brief Create uniform buffers for each frame-in-flight.
     * 
     * @param device Logical device handle
     * @param physicalDevice Physical device handle
     * @param bufferSize Size of each buffer in bytes
     * @param count Number of buffers to create (typically MAX_FRAMES_IN_FLIGHT)
     */
    void create(VkDevice device, VkPhysicalDevice physicalDevice,
                VkDeviceSize bufferSize, uint32_t count);
    
    /**
     * @brief Clean up all buffers and memory.
     */
    void cleanup();
    
    /**
     * @brief Check if buffers have been created.
     * @return true if create() was called successfully
     */
    bool isCreated() const { return !m_buffers.empty(); }
    
    /**
     * @brief Update the buffer for a specific frame.
     * 
     * Copies data directly to the mapped memory. With HOST_COHERENT memory,
     * no explicit flush is needed.
     * 
     * @param frameIndex Index of the frame to update
     * @param data Pointer to the source data
     * @param size Size of data to copy (must not exceed bufferSize)
     */
    void update(uint32_t frameIndex, const void* data, VkDeviceSize size);
    
    /**
     * @brief Get the buffer for a specific frame.
     * @param frameIndex Index of the frame
     * @return VkBuffer handle for the specified frame
     */
    VkBuffer getBuffer(uint32_t frameIndex) const;
    
    /**
     * @brief Get all buffers.
     * @return Reference to the vector of buffer handles
     */
    const std::vector<VkBuffer>& getBuffers() const { return m_buffers; }
    
    /**
     * @brief Get the number of buffers.
     * @return Number of per-frame buffers
     */
    uint32_t getCount() const { return static_cast<uint32_t>(m_buffers.size()); }
    
    /**
     * @brief Get the size of each buffer.
     * @return Buffer size in bytes
     */
    VkDeviceSize getBufferSize() const { return m_bufferSize; }

private:
    VkDevice m_device = VK_NULL_HANDLE;
    VkDeviceSize m_bufferSize = 0;
    
    std::vector<VkBuffer> m_buffers;
    std::vector<VkDeviceMemory> m_buffersMemory;
    std::vector<void*> m_buffersMapped;  // Persistently mapped pointers
};

} // namespace vde
