#pragma once

/**
 * @file Texture.h
 * @brief Vulkan texture management including image, image view, and sampler.
 */

#include <vulkan/vulkan.h>

#include <cstdint>
#include <string>

namespace vde {

/**
 * @brief Manages a Vulkan texture including image, image view, and sampler.
 *
 * This class handles the complete lifecycle of a texture:
 * - Loading image data from file
 * - Creating VkImage with appropriate format
 * - Uploading via staging buffer with layout transitions
 * - Creating VkImageView for shader access
 * - Creating VkSampler with configurable filtering
 *
 * Supports move semantics for efficient resource transfer.
 */
class Texture {
  public:
    Texture() = default;
    ~Texture();

    // Prevent copying
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    // Allow moving
    Texture(Texture&& other) noexcept;
    Texture& operator=(Texture&& other) noexcept;

    /**
     * @brief Load a texture from an image file.
     *
     * Loads the image, creates VkImage/VkImageView/VkSampler, and uploads data.
     * Uses staging buffer pattern with proper layout transitions.
     *
     * @param filepath Path to the image file (PNG, JPEG, BMP, etc.)
     * @param device Logical device handle
     * @param physicalDevice Physical device handle (for memory allocation)
     * @param commandPool Command pool for transfer operations
     * @param graphicsQueue Queue for submitting transfer commands
     * @return true if successful, false on failure
     */
    bool loadFromFile(const std::string& filepath, VkDevice device, VkPhysicalDevice physicalDevice,
                      VkCommandPool commandPool, VkQueue graphicsQueue);

    /**
     * @brief Create a texture from raw pixel data.
     *
     * Creates a texture from RGBA pixel data in memory.
     *
     * @param pixels Pointer to RGBA pixel data (4 bytes per pixel)
     * @param width Width of the image in pixels
     * @param height Height of the image in pixels
     * @param device Logical device handle
     * @param physicalDevice Physical device handle (for memory allocation)
     * @param commandPool Command pool for transfer operations
     * @param graphicsQueue Queue for submitting transfer commands
     * @return true if successful, false on failure
     */
    bool createFromData(const uint8_t* pixels, uint32_t width, uint32_t height, VkDevice device,
                        VkPhysicalDevice physicalDevice, VkCommandPool commandPool,
                        VkQueue graphicsQueue);

    /**
     * @brief Clean up all Vulkan resources.
     *
     * Destroys sampler, image view, image, and frees memory.
     * Safe to call multiple times.
     */
    void cleanup();

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

  private:
    VkDevice m_device = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkCommandPool m_commandPool = VK_NULL_HANDLE;
    VkQueue m_graphicsQueue = VK_NULL_HANDLE;

    VkImage m_image = VK_NULL_HANDLE;
    VkDeviceMemory m_imageMemory = VK_NULL_HANDLE;
    VkImageView m_imageView = VK_NULL_HANDLE;
    VkSampler m_sampler = VK_NULL_HANDLE;

    uint32_t m_width = 0;
    uint32_t m_height = 0;

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
