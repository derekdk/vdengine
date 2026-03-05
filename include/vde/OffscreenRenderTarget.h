#pragma once

/**
 * @file OffscreenRenderTarget.h
 * @brief Offscreen render target for rendering scenes to textures.
 *
 * Provides a Vulkan image + image view + framebuffer + sampler bundle
 * that can be used to render a scene to an offscreen texture and then
 * sample that texture in a later compositing pass (e.g. screen transitions).
 */

#include <vulkan/vulkan.h>

#include <cstdint>

namespace vde {

class VulkanContext;

/**
 * @brief Self-contained offscreen render target (color + depth).
 *
 * Creates and manages the Vulkan resources needed to render a scene
 * to an offscreen texture:
 *   - Color image (sampled in a later pass)
 *   - Depth image (used during scene rendering, not sampled)
 *   - Image views for both
 *   - Framebuffer compatible with the offscreen render pass
 *   - Sampler for reading the color attachment in a fragment shader
 *
 * Supports recreation at a new resolution (e.g. on swapchain resize).
 * All Vulkan resources are cleaned up in the destructor (RAII).
 *
 * Usage:
 * @code
 * OffscreenRenderTarget target;
 * target.create(vulkanContext, 1920, 1080);
 * // ... render scene using target.getFramebuffer() ...
 * // ... sample target.getImageView() / target.getSampler() ...
 * target.destroy(); // or let destructor handle it
 * @endcode
 */
class OffscreenRenderTarget {
  public:
    OffscreenRenderTarget() = default;
    ~OffscreenRenderTarget();

    // Non-copyable
    OffscreenRenderTarget(const OffscreenRenderTarget&) = delete;
    OffscreenRenderTarget& operator=(const OffscreenRenderTarget&) = delete;

    // Movable
    OffscreenRenderTarget(OffscreenRenderTarget&& other) noexcept;
    OffscreenRenderTarget& operator=(OffscreenRenderTarget&& other) noexcept;

    /**
     * @brief Create the offscreen render target at the given resolution.
     *
     * Uses the same color format as the swapchain and includes a depth
     * attachment. The render pass must be the offscreen render pass from
     * VulkanContext (renders color to SHADER_READ_OPTIMAL).
     *
     * @param context  VulkanContext that owns the device and render pass
     * @param renderPass  The offscreen render pass to use for the framebuffer
     * @param width    Width in pixels
     * @param height   Height in pixels
     * @throws std::runtime_error if any Vulkan resource creation fails
     */
    void create(VulkanContext* context, VkRenderPass renderPass, uint32_t width, uint32_t height);

    /**
     * @brief Destroy all Vulkan resources and reset to default state.
     *
     * Safe to call multiple times or on a default-constructed instance.
     */
    void destroy();

    /**
     * @brief Recreate the render target at a new resolution.
     *
     * Equivalent to destroy() + create() with the same context and render pass.
     *
     * @param width  New width in pixels
     * @param height New height in pixels
     */
    void recreate(uint32_t width, uint32_t height);

    // ---- Accessors ----

    /** @brief Whether the render target has been successfully created. */
    bool isValid() const { return m_colorImage != VK_NULL_HANDLE; }

    /** @brief Get the color image (for layout transitions, etc.). */
    VkImage getColorImage() const { return m_colorImage; }

    /** @brief Get the color image view (for descriptor set binding). */
    VkImageView getColorImageView() const { return m_colorImageView; }

    /** @brief Get the sampler for reading the color attachment. */
    VkSampler getSampler() const { return m_sampler; }

    /** @brief Get the framebuffer (for vkCmdBeginRenderPass). */
    VkFramebuffer getFramebuffer() const { return m_framebuffer; }

    /** @brief Width in pixels. */
    uint32_t getWidth() const { return m_width; }

    /** @brief Height in pixels. */
    uint32_t getHeight() const { return m_height; }

  private:
    void createColorResources(uint32_t width, uint32_t height);
    void createDepthResources(uint32_t width, uint32_t height);
    void createSampler();
    void createFramebuffer();

    VulkanContext* m_context = nullptr;
    VkRenderPass m_renderPass = VK_NULL_HANDLE;  ///< Not owned; borrowed from VulkanContext

    // Color attachment
    VkImage m_colorImage = VK_NULL_HANDLE;
    VkDeviceMemory m_colorMemory = VK_NULL_HANDLE;
    VkImageView m_colorImageView = VK_NULL_HANDLE;

    // Depth attachment
    VkImage m_depthImage = VK_NULL_HANDLE;
    VkDeviceMemory m_depthMemory = VK_NULL_HANDLE;
    VkImageView m_depthImageView = VK_NULL_HANDLE;

    // Sampling
    VkSampler m_sampler = VK_NULL_HANDLE;

    // Framebuffer
    VkFramebuffer m_framebuffer = VK_NULL_HANDLE;

    // Dimensions
    uint32_t m_width = 0;
    uint32_t m_height = 0;
};

}  // namespace vde
