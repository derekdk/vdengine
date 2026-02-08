#include <vde/DescriptorManager.h>

#include <array>
#include <stdexcept>

namespace vde {

DescriptorManager::~DescriptorManager() {
    cleanup();
}

DescriptorManager::DescriptorManager(DescriptorManager&& other) noexcept
    : m_device(other.m_device), m_uboLayout(other.m_uboLayout),
      m_samplerLayout(other.m_samplerLayout), m_descriptorPool(other.m_descriptorPool) {
    // Nullify source to prevent double cleanup
    other.m_device = VK_NULL_HANDLE;
    other.m_uboLayout = VK_NULL_HANDLE;
    other.m_samplerLayout = VK_NULL_HANDLE;
    other.m_descriptorPool = VK_NULL_HANDLE;
}

DescriptorManager& DescriptorManager::operator=(DescriptorManager&& other) noexcept {
    if (this != &other) {
        cleanup();

        m_device = other.m_device;
        m_uboLayout = other.m_uboLayout;
        m_samplerLayout = other.m_samplerLayout;
        m_descriptorPool = other.m_descriptorPool;

        other.m_device = VK_NULL_HANDLE;
        other.m_uboLayout = VK_NULL_HANDLE;
        other.m_samplerLayout = VK_NULL_HANDLE;
        other.m_descriptorPool = VK_NULL_HANDLE;
    }
    return *this;
}

void DescriptorManager::init(VkDevice device) {
    if (device == VK_NULL_HANDLE) {
        throw std::runtime_error("Cannot initialize DescriptorManager with null device!");
    }

    m_device = device;

    createUBOLayout();
    createSamplerLayout();
    createDescriptorPool();
}

void DescriptorManager::cleanup() {
    if (m_device != VK_NULL_HANDLE) {
        // Destroy pool first (it allocates from layouts)
        if (m_descriptorPool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
            m_descriptorPool = VK_NULL_HANDLE;
        }

        if (m_samplerLayout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(m_device, m_samplerLayout, nullptr);
            m_samplerLayout = VK_NULL_HANDLE;
        }

        if (m_uboLayout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(m_device, m_uboLayout, nullptr);
            m_uboLayout = VK_NULL_HANDLE;
        }
    }
}

void DescriptorManager::createUBOLayout() {
    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    uboLayoutBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &uboLayoutBinding;

    if (vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, &m_uboLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create UBO descriptor set layout!");
    }
}

void DescriptorManager::createSamplerLayout() {
    VkDescriptorSetLayoutBinding samplerLayoutBinding{};
    samplerLayoutBinding.binding = 0;
    samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBinding.descriptorCount = 1;
    samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    samplerLayoutBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &samplerLayoutBinding;

    if (vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, &m_samplerLayout) !=
        VK_SUCCESS) {
        throw std::runtime_error("Failed to create sampler descriptor set layout!");
    }
}

void DescriptorManager::createDescriptorPool() {
    std::array<VkDescriptorPoolSize, 2> poolSizes{};

    // Uniform buffers (one per frame for camera data)
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = MAX_FRAMES_IN_FLIGHT;

    // Combined image samplers (for textures)
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = MAX_TEXTURES;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = MAX_FRAMES_IN_FLIGHT + MAX_TEXTURES;

    if (vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor pool!");
    }
}

std::vector<VkDescriptorSet> DescriptorManager::allocateUBODescriptorSets() {
    if (!isInitialized()) {
        throw std::runtime_error("DescriptorManager not initialized!");
    }

    std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, m_uboLayout);

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_descriptorPool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(layouts.size());
    allocInfo.pSetLayouts = layouts.data();

    std::vector<VkDescriptorSet> descriptorSets(MAX_FRAMES_IN_FLIGHT);
    if (vkAllocateDescriptorSets(m_device, &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate UBO descriptor sets!");
    }

    return descriptorSets;
}

VkDescriptorSet DescriptorManager::allocateTextureDescriptorSet() {
    if (!isInitialized()) {
        throw std::runtime_error("DescriptorManager not initialized!");
    }

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &m_samplerLayout;

    VkDescriptorSet descriptorSet;
    if (vkAllocateDescriptorSets(m_device, &allocInfo, &descriptorSet) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate texture descriptor set!");
    }

    return descriptorSet;
}

void DescriptorManager::updateUBODescriptor(VkDescriptorSet descriptorSet, VkBuffer buffer,
                                            VkDeviceSize bufferSize) {
    if (m_device == VK_NULL_HANDLE) {
        throw std::runtime_error("DescriptorManager device not set!");
    }

    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = buffer;
    bufferInfo.offset = 0;
    bufferInfo.range = bufferSize;

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = descriptorSet;
    descriptorWrite.dstBinding = 0;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pBufferInfo = &bufferInfo;

    vkUpdateDescriptorSets(m_device, 1, &descriptorWrite, 0, nullptr);
}

void DescriptorManager::updateTextureDescriptor(VkDescriptorSet descriptorSet,
                                                VkImageView imageView, VkSampler sampler) {
    if (m_device == VK_NULL_HANDLE) {
        throw std::runtime_error("DescriptorManager device not set!");
    }

    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = imageView;
    imageInfo.sampler = sampler;

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = descriptorSet;
    descriptorWrite.dstBinding = 0;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(m_device, 1, &descriptorWrite, 0, nullptr);
}

}  // namespace vde
