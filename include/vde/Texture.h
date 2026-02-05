#pragma once

/**
 * @file Texture.h
 * @brief Vulkan texture management including image, image view, and sampler.
 */

#include <vde/api/Resource.h>

#include <vulkan/vulkan.h>

#include <cstdint>
#include <string>
#include <vector>

namespace vde {

// Forward declarations
class VulkanContext;

/**
 * @brief Manages a Vulkan texture including image, image view, and sampler.
 *
 * This class handles the complete lifecycle of a texture:
 * - Loading image data from file (CPU-side)
 * - Creating VkImage with appropriate format
 * - Uploading via staging buffer with layout transitions
 * - Creating VkImageView for shader access
 * - Creating VkSampler with configurable filtering
 *
 * Uses a two-phase loading pattern:
 * 1. loadFromFile() - Loads pixel data to CPU memory
 * 2. uploadToGPU() - Creates Vulkan objects and uploads data
 *
 * This allows resources to be loaded before VulkanContext initialization.
 *
 * Supports move semantics for efficient resource transfer.
 */
class Texture : public Resource {
  public:
    Texture() = default;
    virtual ~Texture();

    // Prevent copying
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    // Allow moving
    Texture(Texture&& other) noexcept;
    Texture& operator=(Texture&& other) noexcept;

    /**
     * @brief Load texture pixel data from file (CPU-side only).
     *
     * Loads the image data into CPU memory. Does not create any Vulkan objects.
     * Call uploadToGPU() later to create GPU resources.
     *
     * @param path Path to the image file (PNG, JPEG, BMP, etc.)
     * @return true if successful, false on failure
     */
    bool loadFromFile(const std::string& path);

    /**
     * @brief Load texture from raw pixel data (CPU-side only).
     *
     * Stores pixel data in CPU memory. Does not create any Vulkan objects.
     * Call uploadToGPU() later to create GPU resources.
     *
     * @param pixels Pointer to RGBA pixel data (4 bytes per pixel)
     * @param width Width of the image in pixels
     * @param height Height of the image in pixels
     * @return true if successful, false on failure
     */
    bool loadFromData(const uint8_t* pixels, uint32_t width, uint32_t height);

    /**
     * @brief Upload texture to GPU and create Vulkan objects.
     *
     * Creates VkImage, VkImageView, VkSampler and uploads pixel data via staging buffer.
     * Call this after loadFromFile() or loadFromData().
     *
     * @param context Vulkan context for device/queue access
     * @return true if successful, false if already uploaded or no data loaded
     */
    bool uploadToGPU(VulkanContext* context);

    /**
     * @brief Check if texture has been uploaded to GPU.
     */
    bool isOnGPU() const { return m_image != VK_NULL_HANDLE; }

    /**
     * @brief Free GPU resources (keeps CPU pixel data).
     */
    void freeGPUResources(VkDevice device);

    /**
     * @brief Clean up all resources (CPU and GPU).
     *
     * Destroys sampler, image view, image, frees memory, and clears pixel data.
     * Safe to call multiple times.
     */
    void cleanup();

    // Resource interface
    const char* getTypeName() const override { return "Texture"; }

    // Accessors
    VkImage getImage() const { return m_image; }
    VkImageView getImageView() const { return m_imageView; }
    VkSampler getSampler() const { return m_sampler; }
    uint32_t getWidth() const { return m_width; }
    uint32_t getHeight() const { return m_height; }

    /**
     * @brief Check if the texture is valid and ready for use.
     * @return true if image, image view, and sampler are all created
     */
    bool isValid() const {
        return m_image != VK_NULL_HANDLE && m_imageView != VK_NULL_HANDLE &&
               m_sampler != VK_NULL_HANDLE;
    }

    // Legacy API (deprecated - for backward compatibility)

    /**
     * @brief Load and upload a texture in one step (legacy API).
     *
     * @deprecated Use loadFromFile() + uploadToGPU() instead.
     */
    bool loadFromFile(const std::string& filepath, VkDevice device, VkPhysicalDevice physicalDevice,
                      VkCommandPool commandPool, VkQueue graphicsQueue);

    /**
     * @brief Create texture from data and upload in one step (legacy API).
     *
     * @deprecated Use loadFromData() + uploadToGPU() instead.
     */
    bool createFromData(const uint8_t* pixels, uint32_t width, uint32_t height, VkDevice device,
                        VkPhysicalDevice physicalDevice, VkCommandPool commandPool,
                        VkQueue graphicsQueue);

  private:
    // CPU-side data (for lazy GPU upload)
    std::vector<uint8_t> m_pixelData;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    uint32_t m_channels = 4;  // RGBA

    // GPU-side Vulkan objects
    VkDevice m_device = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkCommandPool m_commandPool = VK_NULL_HANDLE;
    VkQueue m_graphicsQueue = VK_NULL_HANDLE;

    VkImage m_image = VK_NULL_HANDLE;
    VkDeviceMemory m_imageMemory = VK_NULL_HANDLE;
    VkImageView m_imageView = VK_NULL_HANDLE;
    VkSampler m_sampler = VK_NULL_HANDLE;

    /**
     * @brief Create a VkImage with the specified properties.
     */
    void createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling,
                     VkImageUsageFlags usage, VkMemoryPropertyFlags properties);

    /**
     * @brief Create a VkImageView for shader access.
     */
    void createImageView(VkFormat format);

    /**
     * @brief Create a VkSampler with appropriate filtering settings.
     */
    void createSampler();

    /**
     * @brief Transition image layout using a pipeline barrier.
     */
    void transitionImageLayout(VkImageLayout oldLayout, VkImageLayout newLayout);

    /**
     * @brief Copy buffer contents to image.
     */
    void copyBufferToImage(VkBuffer buffer, uint32_t width, uint32_t height);

    /**
     * @brief Begin a single-time command buffer.
     */
    VkCommandBuffer beginSingleTimeCommands();

    /**
     * @brief End and submit a single-time command buffer.
     */
    void endSingleTimeCommands(VkCommandBuffer commandBuffer);
};

}  // namespace vde
