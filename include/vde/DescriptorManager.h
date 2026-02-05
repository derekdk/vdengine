#pragma once

/**
 * @file DescriptorManager.h
 * @brief Vulkan descriptor set layouts, pools, and sets management
 */

#include <vulkan/vulkan.h>

#include <vector>

namespace vde {

/**
 * @brief Manages Vulkan descriptor set layouts, pools, and sets.
 *
 * This class handles the creation and management of descriptor resources:
 * - Set 0: Per-frame uniform buffers (camera matrices)
 * - Set 1: Per-material textures (combined image samplers)
 *
 * Descriptor sets are organized by update frequency to minimize
 * rebinding overhead during rendering.
 */
class DescriptorManager {
  public:
    // Configuration constants
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;
    static constexpr uint32_t MAX_TEXTURES = 16;

    DescriptorManager() = default;
    ~DescriptorManager();

    // Prevent copying
    DescriptorManager(const DescriptorManager&) = delete;
    DescriptorManager& operator=(const DescriptorManager&) = delete;

    // Allow moving
    DescriptorManager(DescriptorManager&& other) noexcept;
    DescriptorManager& operator=(DescriptorManager&& other) noexcept;

    /**
     * @brief Initialize descriptor set layouts.
     * @param device Vulkan logical device handle.
     */
    void init(VkDevice device);

    /**
     * @brief Clean up all descriptor resources.
     */
    void cleanup();

    /**
     * @brief Check if the manager has been initialized.
     * @return true if layouts and pool have been created.
     */
    bool isInitialized() const {
        return m_device != VK_NULL_HANDLE && m_uboLayout != VK_NULL_HANDLE &&
               m_samplerLayout != VK_NULL_HANDLE && m_descriptorPool != VK_NULL_HANDLE;
    }

    // Layout accessors

    /**
     * @brief Get the uniform buffer descriptor set layout (Set 0).
     * @return VkDescriptorSetLayout for camera/MVP matrices.
     */
    VkDescriptorSetLayout getUniformBufferLayout() const { return m_uboLayout; }

    /**
     * @brief Get the texture sampler descriptor set layout (Set 1).
     * @return VkDescriptorSetLayout for combined image samplers.
     */
    VkDescriptorSetLayout getSamplerLayout() const { return m_samplerLayout; }

    /**
     * @brief Get the descriptor pool.
     * @return VkDescriptorPool for allocating descriptor sets.
     */
    VkDescriptorPool getPool() const { return m_descriptorPool; }

    /**
     * @brief Get all layouts in order for pipeline creation.
     * @return Vector of layouts: [UBO layout, Sampler layout]
     */
    std::vector<VkDescriptorSetLayout> getAllLayouts() const {
        return {m_uboLayout, m_samplerLayout};
    }

    // Descriptor set allocation

    /**
     * @brief Allocate UBO descriptor sets for each frame-in-flight.
     * @return Vector of allocated descriptor sets (one per frame)
     */
    std::vector<VkDescriptorSet> allocateUBODescriptorSets();

    /**
     * @brief Allocate a texture sampler descriptor set.
     * @return Allocated descriptor set for texture binding
     */
    VkDescriptorSet allocateTextureDescriptorSet();

    /**
     * @brief Update a UBO descriptor set with buffer information.
     * @param descriptorSet The descriptor set to update
     * @param buffer The uniform buffer handle
     * @param bufferSize Size of the buffer data
     */
    void updateUBODescriptor(VkDescriptorSet descriptorSet, VkBuffer buffer,
                             VkDeviceSize bufferSize);

    /**
     * @brief Update a texture descriptor set with image and sampler.
     * @param descriptorSet The descriptor set to update
     * @param imageView The image view for the texture
     * @param sampler The sampler for the texture
     */
    void updateTextureDescriptor(VkDescriptorSet descriptorSet, VkImageView imageView,
                                 VkSampler sampler);

  private:
    VkDevice m_device = VK_NULL_HANDLE;

    // Descriptor set layouts
    VkDescriptorSetLayout m_uboLayout = VK_NULL_HANDLE;      // Set 0: Uniform buffers
    VkDescriptorSetLayout m_samplerLayout = VK_NULL_HANDLE;  // Set 1: Texture samplers

    // Descriptor pool
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;

    void createUBOLayout();
    void createSamplerLayout();
    void createDescriptorPool();
};

}  // namespace vde
