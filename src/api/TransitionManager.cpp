/**
 * @file TransitionManager.cpp
 * @brief Implementation of TransitionManager.
 */

#include <vde/ShaderCompiler.h>
#include <vde/VulkanContext.h>
#include <vde/api/TransitionManager.h>

#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>

namespace vde {

// =========================================================================
// Construction / Destruction
// =========================================================================

TransitionManager::TransitionManager(VulkanContext* context) : m_context(context) {}

TransitionManager::~TransitionManager() {
    // Cancel active transition first (no callback)
    m_activeTransition.reset();
    m_onComplete = nullptr;

    destroyPipeline();
    destroyDescriptorResources();

    // Destroy offscreen render targets (they handle their own RAII)
    m_source.destroy();
    m_dest.destroy();
}

// =========================================================================
// Transition lifecycle
// =========================================================================

void TransitionManager::start(std::unique_ptr<Transition> transition, float duration,
                              std::function<void()> onComplete) {
    // Cancel any active transition first (no callback)
    if (m_activeTransition) {
        m_activeTransition.reset();
        // Clean up pipeline if the new transition uses a different shader
    }

    if (!transition) {
        // Null transition — treat as instant switch
        if (onComplete) {
            onComplete();
        }
        return;
    }

    // Instant switch: duration <= 0
    if (duration <= 0.0f) {
        transition->onStart();
        transition->onComplete();
        if (onComplete) {
            onComplete();
        }
        return;
    }

    m_activeTransition = std::move(transition);
    m_duration = duration;
    m_elapsed = 0.0f;
    m_progress = 0.0f;
    m_onComplete = std::move(onComplete);
    m_uniforms = TransitionUniforms{};

    m_activeTransition->onStart();
}

void TransitionManager::update(float deltaTime) {
    if (!m_activeTransition) {
        return;
    }

    m_elapsed += deltaTime;
    m_progress = std::clamp(m_elapsed / m_duration, 0.0f, 1.0f);

    // Build the update context
    TransitionUpdateContext ctx{};
    ctx.progress = m_progress;
    ctx.deltaTime = deltaTime;
    ctx.elapsed = m_elapsed;
    ctx.duration = m_duration;
    if (m_source.isValid()) {
        ctx.frameWidth = m_source.getWidth();
        ctx.frameHeight = m_source.getHeight();
    } else {
        auto extent = m_context->getSwapChainExtent();
        ctx.frameWidth = extent.width;
        ctx.frameHeight = extent.height;
    }

    // Let the transition update its uniforms
    m_activeTransition->update(ctx, m_uniforms);

    // Check for completion
    if (m_progress >= 1.0f) {
        m_activeTransition->onComplete();

        auto completionCallback = std::move(m_onComplete);
        m_activeTransition.reset();
        m_onComplete = nullptr;
        m_elapsed = 0.0f;
        m_progress = 0.0f;
        m_duration = 0.0f;

        if (completionCallback) {
            completionCallback();
        }
    }
}

bool TransitionManager::isActive() const {
    return m_activeTransition != nullptr;
}

void TransitionManager::cancel() {
    if (!m_activeTransition) {
        return;
    }

    m_activeTransition.reset();
    m_onComplete = nullptr;
    m_elapsed = 0.0f;
    m_progress = 0.0f;
    m_duration = 0.0f;
    m_uniforms = TransitionUniforms{};
}

float TransitionManager::getProgress() const {
    return m_progress;
}

// =========================================================================
// Render targets
// =========================================================================

void TransitionManager::recreateRenderTargets(uint32_t width, uint32_t height) {
    if (width == 0 || height == 0) {
        return;
    }

    VkRenderPass offscreenRP = m_context->getOffscreenRenderPass();
    if (offscreenRP == VK_NULL_HANDLE) {
        return;
    }

    if (m_source.isValid()) {
        m_source.recreate(width, height);
    } else {
        m_source.create(m_context, offscreenRP, width, height);
    }

    if (m_dest.isValid()) {
        m_dest.recreate(width, height);
    } else {
        m_dest.create(m_context, offscreenRP, width, height);
    }

    // Recreate descriptor set to point to new image views + samplers
    if (m_descriptorSet != VK_NULL_HANDLE) {
        updateDescriptorSet();
    }
}

VkFramebuffer TransitionManager::getSourceFramebuffer() const {
    return m_source.getFramebuffer();
}

VkFramebuffer TransitionManager::getDestFramebuffer() const {
    return m_dest.getFramebuffer();
}

// =========================================================================
// Descriptor resources
// =========================================================================

void TransitionManager::createDescriptorResources() {
    VkDevice device = m_context->getDevice();

    // Descriptor set layout: binding 0 = source sampler, binding 1 = dest sampler
    std::array<VkDescriptorSetLayoutBinding, 2> bindings{};
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[0].pImmutableSamplers = nullptr;

    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[1].pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &m_descriptorSetLayout) !=
        VK_SUCCESS) {
        throw std::runtime_error("Failed to create transition descriptor set layout!");
    }

    // Descriptor pool
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = 2;  // source + dest

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = 1;

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create transition descriptor pool!");
    }

    // Allocate descriptor set
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &m_descriptorSetLayout;

    if (vkAllocateDescriptorSets(device, &allocInfo, &m_descriptorSet) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate transition descriptor set!");
    }
}

void TransitionManager::destroyDescriptorResources() {
    VkDevice device = m_context ? m_context->getDevice() : VK_NULL_HANDLE;
    if (device == VK_NULL_HANDLE) {
        return;
    }

    // Descriptor sets are freed when the pool is destroyed
    m_descriptorSet = VK_NULL_HANDLE;

    if (m_descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, m_descriptorPool, nullptr);
        m_descriptorPool = VK_NULL_HANDLE;
    }

    if (m_descriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, m_descriptorSetLayout, nullptr);
        m_descriptorSetLayout = VK_NULL_HANDLE;
    }
}

void TransitionManager::updateDescriptorSet() {
    if (m_descriptorSet == VK_NULL_HANDLE || !m_source.isValid() || !m_dest.isValid()) {
        return;
    }

    VkDescriptorImageInfo sourceInfo{};
    sourceInfo.sampler = m_source.getSampler();
    sourceInfo.imageView = m_source.getColorImageView();
    sourceInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo destInfo{};
    destInfo.sampler = m_dest.getSampler();
    destInfo.imageView = m_dest.getColorImageView();
    destInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    std::array<VkWriteDescriptorSet, 2> writes{};
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = m_descriptorSet;
    writes[0].dstBinding = 0;
    writes[0].dstArrayElement = 0;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[0].descriptorCount = 1;
    writes[0].pImageInfo = &sourceInfo;

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = m_descriptorSet;
    writes[1].dstBinding = 1;
    writes[1].dstArrayElement = 0;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[1].descriptorCount = 1;
    writes[1].pImageInfo = &destInfo;

    vkUpdateDescriptorSets(m_context->getDevice(), static_cast<uint32_t>(writes.size()),
                           writes.data(), 0, nullptr);
}

// =========================================================================
// Pipeline
// =========================================================================

void TransitionManager::createPipeline(const std::string& fragShaderPath) {
    VkDevice device = m_context->getDevice();

    // Ensure descriptor resources exist
    if (m_descriptorSetLayout == VK_NULL_HANDLE) {
        createDescriptorResources();
    }

    // Compile shaders
    ShaderCompiler compiler;

    std::string vertPath = "shaders/transition_fullscreen.vert";
    auto vertResult = compiler.compileFile(vertPath, ShaderStage::Vertex);
    if (!vertResult.success) {
        std::cerr << "[TransitionManager] Vertex shader compilation failed: " << vertResult.errorLog
                  << std::endl;
        throw std::runtime_error("Failed to compile transition vertex shader: " +
                                 vertResult.errorLog);
    }

    std::string fullFragPath = "shaders/" + fragShaderPath;
    auto fragResult = compiler.compileFile(fullFragPath, ShaderStage::Fragment);
    if (!fragResult.success) {
        std::cerr << "[TransitionManager] Fragment shader compilation failed: "
                  << fragResult.errorLog << std::endl;
        throw std::runtime_error("Failed to compile transition fragment shader: " +
                                 fragResult.errorLog);
    }

    // Create shader modules
    VkShaderModule vertModule = m_context->createShaderModule(
        std::vector<char>(reinterpret_cast<char*>(vertResult.spirv.data()),
                          reinterpret_cast<char*>(vertResult.spirv.data()) +
                              vertResult.spirv.size() * sizeof(uint32_t)));

    VkShaderModule fragModule = m_context->createShaderModule(
        std::vector<char>(reinterpret_cast<char*>(fragResult.spirv.data()),
                          reinterpret_cast<char*>(fragResult.spirv.data()) +
                              fragResult.spirv.size() * sizeof(uint32_t)));

    // Shader stages
    VkPipelineShaderStageCreateInfo vertStage{};
    vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertStage.module = vertModule;
    vertStage.pName = "main";

    VkPipelineShaderStageCreateInfo fragStage{};
    fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStage.module = fragModule;
    fragStage.pName = "main";

    VkPipelineShaderStageCreateInfo stages[] = {vertStage, fragStage};

    // No vertex input (fullscreen triangle generated in shader)
    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 0;
    vertexInput.vertexAttributeDescriptionCount = 0;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // Dynamic viewport and scissor
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;  // Fullscreen triangle, no culling
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_FALSE;
    depthStencil.depthWriteEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates = dynamicStates;

    // Push constant range for TransitionUniforms
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(TransitionUniforms);

    // Pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &m_descriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &m_pipelineLayout) !=
        VK_SUCCESS) {
        vkDestroyShaderModule(device, vertModule, nullptr);
        vkDestroyShaderModule(device, fragModule, nullptr);
        throw std::runtime_error("Failed to create transition pipeline layout!");
    }

    // Graphics pipeline — renders into the swapchain render pass
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = stages;
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = m_pipelineLayout;
    pipelineInfo.renderPass = m_context->getRenderPass();  // Swapchain render pass
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline) !=
        VK_SUCCESS) {
        vkDestroyShaderModule(device, vertModule, nullptr);
        vkDestroyShaderModule(device, fragModule, nullptr);
        throw std::runtime_error("Failed to create transition graphics pipeline!");
    }

    // Clean up shader modules (no longer needed after pipeline creation)
    vkDestroyShaderModule(device, vertModule, nullptr);
    vkDestroyShaderModule(device, fragModule, nullptr);

    m_currentFragShaderPath = fragShaderPath;
}

void TransitionManager::destroyPipeline() {
    VkDevice device = m_context ? m_context->getDevice() : VK_NULL_HANDLE;
    if (device == VK_NULL_HANDLE) {
        return;
    }

    if (m_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, m_pipeline, nullptr);
        m_pipeline = VK_NULL_HANDLE;
    }

    if (m_pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, m_pipelineLayout, nullptr);
        m_pipelineLayout = VK_NULL_HANDLE;
    }

    m_currentFragShaderPath.clear();
}

void TransitionManager::ensurePipeline() {
    if (!m_activeTransition) {
        return;
    }

    std::string fragPath = m_activeTransition->getFragmentShaderPath();
    if (fragPath != m_currentFragShaderPath || m_pipeline == VK_NULL_HANDLE) {
        destroyPipeline();
        createPipeline(fragPath);
        updateDescriptorSet();
    }
}

// =========================================================================
// Compositing
// =========================================================================

void TransitionManager::renderComposite(VkCommandBuffer cmd) {
    if (!m_activeTransition || !m_source.isValid() || !m_dest.isValid()) {
        return;
    }

    // Ensure the pipeline matches the current transition's shader
    ensurePipeline();

    if (m_pipeline == VK_NULL_HANDLE || m_pipelineLayout == VK_NULL_HANDLE) {
        return;
    }

    // Check for custom geometry transitions
    if (m_activeTransition->usesCustomGeometry()) {
        TransitionUpdateContext ctx{};
        ctx.progress = m_progress;
        ctx.deltaTime = 0.0f;
        ctx.elapsed = m_elapsed;
        ctx.duration = m_duration;
        ctx.frameWidth = m_source.getWidth();
        ctx.frameHeight = m_source.getHeight();
        m_activeTransition->renderCustomGeometry(cmd, ctx);
        return;
    }

    // Set viewport and scissor to full swapchain extent
    VkExtent2D extent = m_context->getSwapChainExtent();

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(extent.width);
    viewport.height = static_cast<float>(extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = extent;

    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    // Bind pipeline
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);

    // Bind descriptor set (source + dest textures)
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1,
                            &m_descriptorSet, 0, nullptr);

    // Push transition uniforms
    vkCmdPushConstants(cmd, m_pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                       sizeof(TransitionUniforms), &m_uniforms);

    // Draw fullscreen triangle (3 vertices, no vertex buffer)
    vkCmdDraw(cmd, 3, 1, 0, 0);
}

}  // namespace vde
