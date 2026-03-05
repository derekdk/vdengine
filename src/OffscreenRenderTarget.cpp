/**
 * @file OffscreenRenderTarget.cpp
 * @brief Implementation of OffscreenRenderTarget for offscreen scene rendering.
 */

#include <vde/OffscreenRenderTarget.h>
#include <vde/VulkanContext.h>

#include <stdexcept>
#include <utility>

namespace vde {

OffscreenRenderTarget::~OffscreenRenderTarget() {
    destroy();
}

OffscreenRenderTarget::OffscreenRenderTarget(OffscreenRenderTarget&& other) noexcept
    : m_context(other.m_context), m_renderPass(other.m_renderPass),
      m_colorImage(other.m_colorImage), m_colorMemory(other.m_colorMemory),
      m_colorImageView(other.m_colorImageView), m_depthImage(other.m_depthImage),
      m_depthMemory(other.m_depthMemory), m_depthImageView(other.m_depthImageView),
      m_sampler(other.m_sampler), m_framebuffer(other.m_framebuffer), m_width(other.m_width),
      m_height(other.m_height) {
    other.m_context = nullptr;
    other.m_renderPass = VK_NULL_HANDLE;
    other.m_colorImage = VK_NULL_HANDLE;
    other.m_colorMemory = VK_NULL_HANDLE;
    other.m_colorImageView = VK_NULL_HANDLE;
    other.m_depthImage = VK_NULL_HANDLE;
    other.m_depthMemory = VK_NULL_HANDLE;
    other.m_depthImageView = VK_NULL_HANDLE;
    other.m_sampler = VK_NULL_HANDLE;
    other.m_framebuffer = VK_NULL_HANDLE;
    other.m_width = 0;
    other.m_height = 0;
}

OffscreenRenderTarget& OffscreenRenderTarget::operator=(OffscreenRenderTarget&& other) noexcept {
    if (this != &other) {
        destroy();
        m_context = other.m_context;
        m_renderPass = other.m_renderPass;
        m_colorImage = other.m_colorImage;
        m_colorMemory = other.m_colorMemory;
        m_colorImageView = other.m_colorImageView;
        m_depthImage = other.m_depthImage;
        m_depthMemory = other.m_depthMemory;
        m_depthImageView = other.m_depthImageView;
        m_sampler = other.m_sampler;
        m_framebuffer = other.m_framebuffer;
        m_width = other.m_width;
        m_height = other.m_height;

        other.m_context = nullptr;
        other.m_renderPass = VK_NULL_HANDLE;
        other.m_colorImage = VK_NULL_HANDLE;
        other.m_colorMemory = VK_NULL_HANDLE;
        other.m_colorImageView = VK_NULL_HANDLE;
        other.m_depthImage = VK_NULL_HANDLE;
        other.m_depthMemory = VK_NULL_HANDLE;
        other.m_depthImageView = VK_NULL_HANDLE;
        other.m_sampler = VK_NULL_HANDLE;
        other.m_framebuffer = VK_NULL_HANDLE;
        other.m_width = 0;
        other.m_height = 0;
    }
    return *this;
}

void OffscreenRenderTarget::create(VulkanContext* context, VkRenderPass renderPass, uint32_t width,
                                   uint32_t height) {
    if (isValid()) {
        destroy();
    }

    m_context = context;
    m_renderPass = renderPass;
    m_width = width;
    m_height = height;

    createColorResources(width, height);
    createDepthResources(width, height);
    createSampler();
    createFramebuffer();
}

void OffscreenRenderTarget::destroy() {
    if (!m_context) {
        return;
    }

    VkDevice device = m_context->getDevice();
    if (device == VK_NULL_HANDLE) {
        return;
    }

    if (m_framebuffer != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(device, m_framebuffer, nullptr);
        m_framebuffer = VK_NULL_HANDLE;
    }

    if (m_sampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, m_sampler, nullptr);
        m_sampler = VK_NULL_HANDLE;
    }

    if (m_depthImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, m_depthImageView, nullptr);
        m_depthImageView = VK_NULL_HANDLE;
    }
    if (m_depthImage != VK_NULL_HANDLE) {
        vkDestroyImage(device, m_depthImage, nullptr);
        m_depthImage = VK_NULL_HANDLE;
    }
    if (m_depthMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, m_depthMemory, nullptr);
        m_depthMemory = VK_NULL_HANDLE;
    }

    if (m_colorImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, m_colorImageView, nullptr);
        m_colorImageView = VK_NULL_HANDLE;
    }
    if (m_colorImage != VK_NULL_HANDLE) {
        vkDestroyImage(device, m_colorImage, nullptr);
        m_colorImage = VK_NULL_HANDLE;
    }
    if (m_colorMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, m_colorMemory, nullptr);
        m_colorMemory = VK_NULL_HANDLE;
    }

    m_width = 0;
    m_height = 0;
    m_context = nullptr;
    m_renderPass = VK_NULL_HANDLE;
}

void OffscreenRenderTarget::recreate(uint32_t width, uint32_t height) {
    if (!m_context || m_renderPass == VK_NULL_HANDLE) {
        throw std::runtime_error("Cannot recreate OffscreenRenderTarget: not initialized");
    }

    VulkanContext* ctx = m_context;
    VkRenderPass rp = m_renderPass;
    destroy();
    create(ctx, rp, width, height);
}

// =========================================================================
// Private resource creation
// =========================================================================

void OffscreenRenderTarget::createColorResources(uint32_t width, uint32_t height) {
    VkDevice device = m_context->getDevice();
    VkFormat colorFormat = m_context->getSwapChainImageFormat();

    // Color image: sampled + color attachment
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = colorFormat;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(device, &imageInfo, nullptr, &m_colorImage) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create offscreen color image!");
    }

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(device, m_colorImage, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;

    // Find device-local memory type
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(m_context->getPhysicalDevice(), &memProperties);
    bool found = false;
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((memReqs.memoryTypeBits & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) ==
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {
            allocInfo.memoryTypeIndex = i;
            found = true;
            break;
        }
    }
    if (!found) {
        throw std::runtime_error("Failed to find suitable memory type for offscreen color image!");
    }

    if (vkAllocateMemory(device, &allocInfo, nullptr, &m_colorMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate offscreen color image memory!");
    }

    vkBindImageMemory(device, m_colorImage, m_colorMemory, 0);

    // Color image view
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_colorImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = colorFormat;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(device, &viewInfo, nullptr, &m_colorImageView) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create offscreen color image view!");
    }
}

void OffscreenRenderTarget::createDepthResources(uint32_t width, uint32_t height) {
    VkDevice device = m_context->getDevice();
    VkFormat depthFormat = m_context->getDepthFormat();

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = depthFormat;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(device, &imageInfo, nullptr, &m_depthImage) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create offscreen depth image!");
    }

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(device, m_depthImage, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;

    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(m_context->getPhysicalDevice(), &memProperties);
    bool found = false;
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((memReqs.memoryTypeBits & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) ==
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {
            allocInfo.memoryTypeIndex = i;
            found = true;
            break;
        }
    }
    if (!found) {
        throw std::runtime_error("Failed to find suitable memory type for offscreen depth image!");
    }

    if (vkAllocateMemory(device, &allocInfo, nullptr, &m_depthMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate offscreen depth image memory!");
    }

    vkBindImageMemory(device, m_depthImage, m_depthMemory, 0);

    // Depth image view
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_depthImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = depthFormat;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(device, &viewInfo, nullptr, &m_depthImageView) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create offscreen depth image view!");
    }
}

void OffscreenRenderTarget::createSampler() {
    VkDevice device = m_context->getDevice();

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;

    if (vkCreateSampler(device, &samplerInfo, nullptr, &m_sampler) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create offscreen sampler!");
    }
}

void OffscreenRenderTarget::createFramebuffer() {
    VkDevice device = m_context->getDevice();

    VkImageView attachments[] = {m_colorImageView, m_depthImageView};

    VkFramebufferCreateInfo fbInfo{};
    fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbInfo.renderPass = m_renderPass;
    fbInfo.attachmentCount = 2;
    fbInfo.pAttachments = attachments;
    fbInfo.width = m_width;
    fbInfo.height = m_height;
    fbInfo.layers = 1;

    if (vkCreateFramebuffer(device, &fbInfo, nullptr, &m_framebuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create offscreen framebuffer!");
    }
}

}  // namespace vde
