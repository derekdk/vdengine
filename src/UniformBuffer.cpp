#include <vde/BufferUtils.h>
#include <vde/UniformBuffer.h>

#include <cstring>
#include <stdexcept>

namespace vde {

UniformBuffer::~UniformBuffer() {
    cleanup();
}

UniformBuffer::UniformBuffer(UniformBuffer&& other) noexcept
    : m_device(other.m_device), m_bufferSize(other.m_bufferSize),
      m_buffers(std::move(other.m_buffers)), m_buffersMemory(std::move(other.m_buffersMemory)),
      m_buffersMapped(std::move(other.m_buffersMapped)) {
    // Nullify source
    other.m_device = VK_NULL_HANDLE;
    other.m_bufferSize = 0;
}

UniformBuffer& UniformBuffer::operator=(UniformBuffer&& other) noexcept {
    if (this != &other) {
        cleanup();

        m_device = other.m_device;
        m_bufferSize = other.m_bufferSize;
        m_buffers = std::move(other.m_buffers);
        m_buffersMemory = std::move(other.m_buffersMemory);
        m_buffersMapped = std::move(other.m_buffersMapped);

        other.m_device = VK_NULL_HANDLE;
        other.m_bufferSize = 0;
    }
    return *this;
}

void UniformBuffer::create(VkDevice device, [[maybe_unused]] VkPhysicalDevice physicalDevice,
                           VkDeviceSize bufferSize, uint32_t count) {
    if (device == VK_NULL_HANDLE) {
        throw std::runtime_error("Cannot create UniformBuffer with null device!");
    }
    if (bufferSize == 0) {
        throw std::runtime_error("Cannot create UniformBuffer with zero size!");
    }
    if (count == 0) {
        throw std::runtime_error("Cannot create UniformBuffer with zero count!");
    }

    m_device = device;
    m_bufferSize = bufferSize;

    m_buffers.resize(count);
    m_buffersMemory.resize(count);
    m_buffersMapped.resize(count);

    for (uint32_t i = 0; i < count; i++) {
        BufferUtils::createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                  m_buffers[i], m_buffersMemory[i]);

        // Persistently map the buffer
        VkResult result =
            vkMapMemory(device, m_buffersMemory[i], 0, bufferSize, 0, &m_buffersMapped[i]);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to map uniform buffer memory!");
        }
    }
}

void UniformBuffer::cleanup() {
    for (size_t i = 0; i < m_buffers.size(); i++) {
        // Unmap before destroying
        if (m_buffersMapped[i] != nullptr && m_buffersMemory[i] != VK_NULL_HANDLE) {
            vkUnmapMemory(m_device, m_buffersMemory[i]);
            m_buffersMapped[i] = nullptr;
        }

        if (m_buffers[i] != VK_NULL_HANDLE) {
            vkDestroyBuffer(m_device, m_buffers[i], nullptr);
            m_buffers[i] = VK_NULL_HANDLE;
        }

        if (m_buffersMemory[i] != VK_NULL_HANDLE) {
            vkFreeMemory(m_device, m_buffersMemory[i], nullptr);
            m_buffersMemory[i] = VK_NULL_HANDLE;
        }
    }

    m_buffers.clear();
    m_buffersMemory.clear();
    m_buffersMapped.clear();
}

void UniformBuffer::update(uint32_t frameIndex, const void* data, VkDeviceSize size) {
    if (frameIndex >= m_buffers.size()) {
        throw std::out_of_range("Frame index out of range for uniform buffer update");
    }
    if (data == nullptr) {
        throw std::invalid_argument("Cannot update uniform buffer with null data");
    }
    if (size > m_bufferSize) {
        throw std::runtime_error("Data size exceeds uniform buffer size");
    }

    memcpy(m_buffersMapped[frameIndex], data, size);
}

VkBuffer UniformBuffer::getBuffer(uint32_t frameIndex) const {
    if (frameIndex >= m_buffers.size()) {
        throw std::out_of_range("Frame index out of range for getBuffer");
    }
    return m_buffers[frameIndex];
}

}  // namespace vde
