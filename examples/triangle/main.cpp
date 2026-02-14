/**
 * @file main.cpp
 * @brief Basic triangle rendering example for VDE.
 *
 * This example demonstrates:
 * - Low-level Vulkan rendering with VDE
 * - Creating a graphics pipeline
 * - Vertex and index buffer creation
 * - Drawing a colored triangle
 *
 * NOTE: This is a low-level example that doesn't use the Game API.
 * For most use cases, see the other examples (simple_game, sprite_demo, etc.)
 */

#include <vde/BufferUtils.h>
#include <vde/Core.h>
#include <vde/ShaderCompiler.h>
#include <vde/Types.h>

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include <array>
#include <iostream>
#include <stdexcept>

// Configuration
constexpr float AUTO_TERMINATE_SECONDS = 15.0f;

// Triangle vertices with positions and colors
const std::vector<vde::Vertex> triangleVertices = {
    // Position (x, y, z)       Color (r, g, b)       TexCoord (u, v)
    {{0.0f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.5f, 0.0f}},  // Top (red)
    {{0.5f, 0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}},   // Bottom right (green)
    {{-0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}}   // Bottom left (blue)
};

const std::vector<uint16_t> triangleIndices = {0, 1, 2};

/**
 * @brief Low-level triangle rendering application.
 */
class TriangleApp {
  public:
    void run() {
        initWindow();
        initVulkan();
        printInstructions();
        mainLoop();
        cleanup();
    }

    int getExitCode() const { return m_exitCode; }

  private:
    vde::Window* m_window = nullptr;
    vde::VulkanContext m_context;

    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_graphicsPipeline = VK_NULL_HANDLE;

    VkBuffer m_vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_vertexBufferMemory = VK_NULL_HANDLE;
    VkBuffer m_indexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_indexBufferMemory = VK_NULL_HANDLE;

    double m_startTime = 0.0;
    bool m_shouldQuit = false;
    int m_exitCode = 0;

    void initWindow() {
        m_window = new vde::Window(1280, 720, "VDE Triangle Example");

        // Set up key callback for quit
        glfwSetWindowUserPointer(m_window->getHandle(), this);
        glfwSetKeyCallback(m_window->getHandle(), [](GLFWwindow* window, int key, int scancode,
                                                     int action, int mods) {
            (void)scancode;
            (void)mods;
            auto* app = static_cast<TriangleApp*>(glfwGetWindowUserPointer(window));
            if (action == GLFW_PRESS) {
                if (key == GLFW_KEY_ESCAPE) {
                    std::cout << "User requested early exit." << std::endl;
                    app->m_shouldQuit = true;
                } else if (key == GLFW_KEY_F) {
                    std::cerr << "\n========================================" << std::endl;
                    std::cerr << "  TEST FAILED: User reported issue" << std::endl;
                    std::cerr << "  Expected: Colored triangle (red/green/blue vertices)"
                              << std::endl;
                    std::cerr << "========================================\n" << std::endl;
                    app->m_exitCode = 1;
                    app->m_shouldQuit = true;
                }
            }
        });
    }

    void printInstructions() {
        std::cout << "\n========================================" << std::endl;
        std::cout << "  VDE Example: Triangle Rendering" << std::endl;
        std::cout << "========================================\n" << std::endl;

        std::cout << "Features demonstrated:" << std::endl;
        std::cout << "  - Low-level Vulkan rendering" << std::endl;
        std::cout << "  - Graphics pipeline creation" << std::endl;
        std::cout << "  - Vertex/index buffer usage" << std::endl;
        std::cout << "  - Shader compilation from source" << std::endl;

        std::cout << "\nYou should see:" << std::endl;
        std::cout << "  - Triangle with gradient colors" << std::endl;
        std::cout << "  - Red vertex at top" << std::endl;
        std::cout << "  - Green vertex at bottom-right" << std::endl;
        std::cout << "  - Blue vertex at bottom-left" << std::endl;

        std::cout << "\nControls:" << std::endl;
        std::cout << "  F     - Fail test (if visuals are incorrect)" << std::endl;
        std::cout << "  ESC   - Exit early" << std::endl;
        std::cout << "  (Auto-closes in " << AUTO_TERMINATE_SECONDS << " seconds)\n" << std::endl;
    }

    void initVulkan() {
        m_context.initialize(m_window);

        // Set up camera for 2D rendering (looking straight down at the XY plane)
        vde::Camera& camera = m_context.getCamera();
        camera.setPosition(glm::vec3(0.0f, 0.0f, 2.0f));  // Camera at z=2
        camera.setTarget(glm::vec3(0.0f, 0.0f, 0.0f));    // Looking at origin
        camera.setPerspective(60.0f,
                              static_cast<float>(m_window->getWidth()) /
                                  static_cast<float>(m_window->getHeight()),
                              0.1f, 100.0f);

        createGraphicsPipeline();
        createVertexBuffer();
        createIndexBuffer();

        // Set up render callback
        m_context.setRenderCallback([this](VkCommandBuffer commandBuffer) {
            // Bind pipeline
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);

            // Bind vertex buffer
            VkBuffer vertexBuffers[] = {m_vertexBuffer};
            VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

            // Bind index buffer
            vkCmdBindIndexBuffer(commandBuffer, m_indexBuffer, 0, VK_INDEX_TYPE_UINT16);

            // Bind descriptor sets (UBO with matrices)
            VkDescriptorSet descriptorSet = m_context.getCurrentUBODescriptorSet();
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    m_pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

            // Draw triangle
            vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(triangleIndices.size()), 1, 0, 0,
                             0);
        });

        m_startTime = glfwGetTime();
    }

    void createGraphicsPipeline() {
        VkDevice device = m_context.getDevice();

        // Compile shaders
        vde::ShaderCompiler compiler;

        auto vertResult = compiler.compileFile("shaders/triangle.vert", vde::ShaderStage::Vertex);
        if (!vertResult.success) {
            throw std::runtime_error("Vertex shader compilation failed: " + vertResult.errorLog);
        }

        auto fragResult = compiler.compileFile("shaders/triangle.frag", vde::ShaderStage::Fragment);
        if (!fragResult.success) {
            throw std::runtime_error("Fragment shader compilation failed: " + fragResult.errorLog);
        }

        // Create shader modules
        VkShaderModuleCreateInfo vertModuleInfo{};
        vertModuleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        vertModuleInfo.codeSize = vertResult.spirv.size() * sizeof(uint32_t);
        vertModuleInfo.pCode = vertResult.spirv.data();

        VkShaderModule vertShaderModule;
        if (vkCreateShaderModule(device, &vertModuleInfo, nullptr, &vertShaderModule) !=
            VK_SUCCESS) {
            throw std::runtime_error("Failed to create vertex shader module!");
        }

        VkShaderModuleCreateInfo fragModuleInfo{};
        fragModuleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        fragModuleInfo.codeSize = fragResult.spirv.size() * sizeof(uint32_t);
        fragModuleInfo.pCode = fragResult.spirv.data();

        VkShaderModule fragShaderModule;
        if (vkCreateShaderModule(device, &fragModuleInfo, nullptr, &fragShaderModule) !=
            VK_SUCCESS) {
            vkDestroyShaderModule(device, vertShaderModule, nullptr);
            throw std::runtime_error("Failed to create fragment shader module!");
        }

        // Shader stages
        VkPipelineShaderStageCreateInfo vertStageInfo{};
        vertStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertStageInfo.module = vertShaderModule;
        vertStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo fragStageInfo{};
        fragStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragStageInfo.module = fragShaderModule;
        fragStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo shaderStages[] = {vertStageInfo, fragStageInfo};

        // Vertex input
        auto bindingDescription = vde::Vertex::getBindingDescription();
        auto attributeDescriptions = vde::Vertex::getAttributeDescriptions();

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 1;
        vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
        vertexInputInfo.vertexAttributeDescriptionCount =
            static_cast<uint32_t>(attributeDescriptions.size());
        vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

        // Input assembly
        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        // Viewport and scissor (dynamic)
        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;

        // Rasterizer
        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_NONE;  // Disable culling
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_FALSE;

        // Multisampling
        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = VK_FALSE;
        depthStencil.depthWriteEnable = VK_FALSE;
        depthStencil.depthCompareOp = VK_COMPARE_OP_ALWAYS;
        depthStencil.depthBoundsTestEnable = VK_FALSE;
        depthStencil.stencilTestEnable = VK_FALSE;

        // Color blending
        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                              VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE;

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;

        // Dynamic state
        std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT,
                                                     VK_DYNAMIC_STATE_SCISSOR};

        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        // Pipeline layout
        VkDescriptorSetLayout uboLayout = m_context.getDescriptorManager().getUniformBufferLayout();

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &uboLayout;
        pipelineLayoutInfo.pushConstantRangeCount = 0;

        if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &m_pipelineLayout) !=
            VK_SUCCESS) {
            vkDestroyShaderModule(device, fragShaderModule, nullptr);
            vkDestroyShaderModule(device, vertShaderModule, nullptr);
            throw std::runtime_error("Failed to create pipeline layout!");
        }

        // Create graphics pipeline
        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = m_pipelineLayout;
        pipelineInfo.renderPass = m_context.getRenderPass();
        pipelineInfo.subpass = 0;

        if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
                                      &m_graphicsPipeline) != VK_SUCCESS) {
            vkDestroyPipelineLayout(device, m_pipelineLayout, nullptr);
            vkDestroyShaderModule(device, fragShaderModule, nullptr);
            vkDestroyShaderModule(device, vertShaderModule, nullptr);
            throw std::runtime_error("Failed to create graphics pipeline!");
        }

        // Cleanup shader modules
        vkDestroyShaderModule(device, fragShaderModule, nullptr);
        vkDestroyShaderModule(device, vertShaderModule, nullptr);
    }

    void createVertexBuffer() {
        VkDeviceSize bufferSize = sizeof(triangleVertices[0]) * triangleVertices.size();

        // Create staging buffer
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        vde::BufferUtils::createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                           VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                       stagingBuffer, stagingBufferMemory);

        // Copy data to staging buffer
        void* data;
        vkMapMemory(m_context.getDevice(), stagingBufferMemory, 0, bufferSize, 0, &data);
        memcpy(data, triangleVertices.data(), bufferSize);
        vkUnmapMemory(m_context.getDevice(), stagingBufferMemory);

        // Create device local buffer
        vde::BufferUtils::createBuffer(
            bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_vertexBuffer, m_vertexBufferMemory);

        // Copy from staging to device local
        vde::BufferUtils::copyBuffer(stagingBuffer, m_vertexBuffer, bufferSize);

        // Cleanup staging buffer
        vkDestroyBuffer(m_context.getDevice(), stagingBuffer, nullptr);
        vkFreeMemory(m_context.getDevice(), stagingBufferMemory, nullptr);
    }

    void createIndexBuffer() {
        VkDeviceSize bufferSize = sizeof(triangleIndices[0]) * triangleIndices.size();

        // Create staging buffer
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        vde::BufferUtils::createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                           VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                       stagingBuffer, stagingBufferMemory);

        // Copy data to staging buffer
        void* data;
        vkMapMemory(m_context.getDevice(), stagingBufferMemory, 0, bufferSize, 0, &data);
        memcpy(data, triangleIndices.data(), bufferSize);
        vkUnmapMemory(m_context.getDevice(), stagingBufferMemory);

        // Create device local buffer
        vde::BufferUtils::createBuffer(
            bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_indexBuffer, m_indexBufferMemory);

        // Copy from staging to device local
        vde::BufferUtils::copyBuffer(stagingBuffer, m_indexBuffer, bufferSize);

        // Cleanup staging buffer
        vkDestroyBuffer(m_context.getDevice(), stagingBuffer, nullptr);
        vkFreeMemory(m_context.getDevice(), stagingBufferMemory, nullptr);
    }

    void mainLoop() {
        while (!m_window->shouldClose() && !m_shouldQuit) {
            m_window->pollEvents();

            // Check auto-terminate
            double elapsed = glfwGetTime() - m_startTime;
            if (elapsed >= AUTO_TERMINATE_SECONDS) {
                std::cout << "\n========================================" << std::endl;
                std::cout << "  TEST PASSED: Demo completed successfully" << std::endl;
                std::cout << "  Duration: " << elapsed << " seconds" << std::endl;
                std::cout << "========================================\n" << std::endl;
                break;
            }

            m_context.drawFrame();
        }

        vkDeviceWaitIdle(m_context.getDevice());
    }

    void cleanup() {
        VkDevice device = m_context.getDevice();

        if (m_indexBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(device, m_indexBuffer, nullptr);
            vkFreeMemory(device, m_indexBufferMemory, nullptr);
        }

        if (m_vertexBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(device, m_vertexBuffer, nullptr);
            vkFreeMemory(device, m_vertexBufferMemory, nullptr);
        }

        if (m_graphicsPipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(device, m_graphicsPipeline, nullptr);
        }

        if (m_pipelineLayout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(device, m_pipelineLayout, nullptr);
        }

        m_context.cleanup();
        delete m_window;
    }
};

int main() {
    try {
        // Initialize glslang for shader compilation
        if (!vde::initializeGlslang()) {
            std::cerr << "Failed to initialize glslang!" << std::endl;
            return 1;
        }

        TriangleApp app;
        app.run();

        // Finalize glslang
        vde::finalizeGlslang();

        return app.getExitCode();
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        vde::finalizeGlslang();
        return 1;
    }
}
