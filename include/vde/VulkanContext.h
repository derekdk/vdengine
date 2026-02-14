#pragma once

/**
 * @file VulkanContext.h
 * @brief Core Vulkan context managing instance, device, swapchain, and rendering infrastructure.
 *
 * This class provides the foundational Vulkan setup required for rendering:
 * - Vulkan instance with optional validation layers
 * - Physical and logical device selection
 * - Swap chain management
 * - Render pass and framebuffer creation
 * - Command pool and buffer management
 * - Synchronization primitives
 *
 * Games should extend this class or compose it to add application-specific
 * rendering logic (pipelines, vertex buffers, descriptor sets, etc.)
 */

#include <vde/Camera.h>
#include <vde/DescriptorManager.h>
#include <vde/QueueFamilyIndices.h>
#include <vde/SwapChainSupportDetails.h>
#include <vde/UniformBuffer.h>

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

#include <functional>
#include <string>
#include <vector>

namespace vde {

class Window;

/**
 * @brief Callback type for custom rendering during the active render pass.
 * @param commandBuffer The active command buffer for recording draw commands
 */
using RenderCallback = std::function<void(VkCommandBuffer)>;

/**
 * @brief Core Vulkan context for rendering applications.
 *
 * Manages the Vulkan rendering infrastructure including instance creation,
 * device selection, swap chain management, and frame synchronization.
 *
 * This class handles:
 * - Vulkan instance and validation layers
 * - Physical/logical device selection
 * - Swap chain creation and recreation
 * - Render pass and framebuffer management
 * - Command buffer recording and submission
 * - Frame synchronization (semaphores, fences)
 *
 * Usage:
 * @code
 * vde::VulkanContext context;
 * context.initialize(&window);
 *
 * while (running) {
 *     context.beginFrame();
 *     // Record custom draw commands
 *     context.endFrame();
 * }
 *
 * context.cleanup();
 * @endcode
 */
class VulkanContext {
  public:
    VulkanContext();
    virtual ~VulkanContext();

    // Disable copy
    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;

    /**
     * @brief Initialize the Vulkan context.
     *
     * Creates Vulkan instance, selects devices, creates swap chain,
     * render pass, framebuffers, and synchronization objects.
     *
     * @param window Window to render to
     * @throws std::runtime_error if initialization fails
     */
    virtual void initialize(Window* window);

    /**
     * @brief Clean up all Vulkan resources.
     *
     * Destroys all Vulkan objects in correct order.
     * Safe to call multiple times.
     */
    virtual void cleanup();

    /**
     * @brief Recreate swap chain for window resize.
     *
     * Called when window is resized or swap chain becomes invalid.
     *
     * @param width New width in pixels
     * @param height New height in pixels
     */
    void recreateSwapchain(uint32_t width, uint32_t height);

    // =========================================================================
    // Accessors
    // =========================================================================

    VkInstance getInstance() const { return m_instance; }
    VkPhysicalDevice getPhysicalDevice() const { return m_physicalDevice; }
    virtual VkDevice getDevice() const { return m_device; }
    VkQueue getGraphicsQueue() const { return m_graphicsQueue; }
    VkQueue getPresentQueue() const { return m_presentQueue; }
    uint32_t getGraphicsQueueFamily() const { return m_graphicsQueueFamilyIndex; }
    VkRenderPass getRenderPass() const { return m_renderPass; }
    VkCommandPool getCommandPool() const { return m_commandPool; }
    VkExtent2D getSwapChainExtent() const { return m_swapChainExtent; }
    uint32_t getCurrentFrame() const { return m_currentFrame; }

    const std::vector<VkCommandBuffer>& getCommandBuffers() const { return m_commandBuffers; }
    const std::vector<VkSemaphore>& getImageAvailableSemaphores() const {
        return m_imageAvailableSemaphores;
    }
    const std::vector<VkSemaphore>& getRenderFinishedSemaphores() const {
        return m_renderFinishedSemaphores;
    }
    const std::vector<VkFence>& getInFlightFences() const { return m_inFlightFences; }

    Camera& getCamera() { return m_camera; }
    const Camera& getCamera() const { return m_camera; }

    DescriptorManager& getDescriptorManager() { return m_descriptorManager; }
    const DescriptorManager& getDescriptorManager() const { return m_descriptorManager; }

    /**
     * @brief Get the current frame's command buffer.
     * @return Command buffer for current frame, or VK_NULL_HANDLE if none
     */
    VkCommandBuffer getCurrentCommandBuffer() const;

    /**
     * @brief Get the current frame's uniform buffer.
     * @return Uniform buffer handle for current frame
     */
    VkBuffer getCurrentUniformBuffer() const;

    /**
     * @brief Get the current frame's UBO descriptor set.
     * @return Descriptor set for current frame's uniform buffer
     */
    VkDescriptorSet getCurrentUBODescriptorSet() const {
        if (m_uboDescriptorSets.empty())
            return VK_NULL_HANDLE;
        return m_uboDescriptorSets[m_currentFrame];
    }

    // =========================================================================
    // Rendering
    // =========================================================================

    /**
     * @brief Set callback for custom rendering during render pass.
     *
     * The callback is invoked during drawFrame() after the render pass begins.
     * Use it to record custom draw commands.
     *
     * @param callback Function to call with active command buffer
     */
    void setRenderCallback(RenderCallback callback) { m_renderCallback = std::move(callback); }

    /**
     * @brief Draw a frame.
     *
     * Acquires swap chain image, records command buffer (calling render callback),
     * submits commands, and presents the result.
     */
    virtual void drawFrame();

    /**
     * @brief Draw a frame with per-scene rendering passes.
     *
     * This supports multi-viewport rendering where each scene has its own
     * camera and viewport.  Each scene gets its own render pass with
     * appropriate UBO (view/projection) updates between passes.
     *
     * @param sceneRenderInfos Vector of per-scene render data
     */
    struct SceneRenderInfo {
        /// Camera view matrix
        glm::mat4 viewMatrix;
        /// Camera projection matrix
        glm::mat4 projMatrix;
        /// Vulkan viewport (pixel coordinates)
        VkViewport viewport;
        /// Vulkan scissor rect (pixel coordinates)
        VkRect2D scissor;
        /// Render callback for this scene
        RenderCallback renderCallback;
        /// Whether this is the first scene (uses CLEAR; others use LOAD)
        bool clearPass = false;
    };
    void drawFrameMultiScene(const std::vector<SceneRenderInfo>& sceneRenderInfos);

    // =========================================================================
    // Viewport Override
    // =========================================================================

    /**
     * @brief Set an active viewport override.
     *
     * When set, entities should use this viewport instead of computing
     * a full-window viewport.  Call clearViewportOverride() to revert.
     *
     * @param viewport The viewport to use
     * @param scissor  The scissor rect to use
     */
    void setViewportOverride(const VkViewport& viewport, const VkRect2D& scissor) {
        m_viewportOverride = viewport;
        m_scissorOverride = scissor;
        m_hasViewportOverride = true;
    }

    /**
     * @brief Clear the viewport override (revert to full window).
     */
    void clearViewportOverride() { m_hasViewportOverride = false; }

    /**
     * @brief Check if a viewport override is active.
     */
    bool hasViewportOverride() const { return m_hasViewportOverride; }

    /**
     * @brief Get the effective viewport (override if set, else full window).
     */
    VkViewport getEffectiveViewport() const {
        if (m_hasViewportOverride)
            return m_viewportOverride;
        VkViewport vp{};
        vp.x = 0.0f;
        vp.y = 0.0f;
        vp.width = static_cast<float>(m_swapChainExtent.width);
        vp.height = static_cast<float>(m_swapChainExtent.height);
        vp.minDepth = 0.0f;
        vp.maxDepth = 1.0f;
        return vp;
    }

    /**
     * @brief Get the effective scissor rect (override if set, else full window).
     */
    VkRect2D getEffectiveScissor() const {
        if (m_hasViewportOverride)
            return m_scissorOverride;
        VkRect2D sc{};
        sc.offset = {0, 0};
        sc.extent = m_swapChainExtent;
        return sc;
    }

    // =========================================================================
    // Utility
    // =========================================================================

    /**
     * @brief Read a file into a byte buffer.
     * @param filename Path to file
     * @return File contents as byte vector
     * @throws std::runtime_error if file cannot be read
     */
    std::vector<char> readFile(const std::string& filename);

    /**
     * @brief Create a shader module from SPIR-V bytecode.
     * @param code SPIR-V bytecode
     * @return Shader module handle
     * @throws std::runtime_error if creation fails
     */
    VkShaderModule createShaderModule(const std::vector<char>& code);

  protected:
    // Protected for testing subclasses
    struct MockTag {};  ///< Tag for test constructor
    VulkanContext(MockTag) : m_device(VK_NULL_HANDLE) {}

    // =========================================================================
    // Vulkan handles
    // =========================================================================

    VkInstance m_instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_debugMessenger = VK_NULL_HANDLE;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    VkQueue m_graphicsQueue = VK_NULL_HANDLE;
    VkQueue m_presentQueue = VK_NULL_HANDLE;
    uint32_t m_graphicsQueueFamilyIndex = 0;

    Window* m_window = nullptr;

    // Swap chain
    VkSwapchainKHR m_swapChain = VK_NULL_HANDLE;
    std::vector<VkImage> m_swapChainImages;
    VkFormat m_swapChainImageFormat;
    VkExtent2D m_swapChainExtent;
    std::vector<VkImageView> m_swapChainImageViews;

    // Render pass and framebuffers
    VkRenderPass m_renderPass = VK_NULL_HANDLE;
    VkRenderPass m_renderPassLoad = VK_NULL_HANDLE;  // LOAD variant for multi-scene
    std::vector<VkFramebuffer> m_swapChainFramebuffers;

    // Depth resources
    VkImage m_depthImage = VK_NULL_HANDLE;
    VkDeviceMemory m_depthImageMemory = VK_NULL_HANDLE;
    VkImageView m_depthImageView = VK_NULL_HANDLE;
    VkFormat m_depthFormat = VK_FORMAT_UNDEFINED;

    // Descriptor management
    DescriptorManager m_descriptorManager;

    // Uniform buffers
    UniformBuffer m_uniformBuffer;
    std::vector<VkDescriptorSet> m_uboDescriptorSets;

    // Camera
    Camera m_camera;

    // Command pool and buffers
    VkCommandPool m_commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> m_commandBuffers;

    // Synchronization
    // Per-frame semaphores for image acquisition
    std::vector<VkSemaphore> m_imageAvailableSemaphores;
    // Per-swapchain-image semaphores for render completion
    std::vector<VkSemaphore> m_renderFinishedSemaphores;
    std::vector<VkFence> m_inFlightFences;
    std::vector<VkFence>
        m_imagesInFlight;  // Track which fence is associated with each swapchain image
    uint32_t m_currentFrame = 0;

    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;

    // Timing
    double m_startTime = 0.0;

    // Render callback
    RenderCallback m_renderCallback;

    // Viewport override for per-scene rendering
    VkViewport m_viewportOverride{};
    VkRect2D m_scissorOverride{};
    bool m_hasViewportOverride = false;

    // Clear color (can be set by subclasses)
    glm::vec4 m_clearColor{0.1f, 0.1f, 0.15f, 1.0f};

  public:
    /**
     * @brief Set the clear color for the render pass.
     * @param color RGBA clear color
     */
    void setClearColor(const glm::vec4& color) { m_clearColor = color; }

    /**
     * @brief Get the current clear color.
     * @return RGBA clear color
     */
    const glm::vec4& getClearColor() const { return m_clearColor; }

  protected:
    // =========================================================================
    // Configuration
    // =========================================================================

#ifdef NDEBUG
    static constexpr bool kEnableValidationLayers = false;
#else
    static constexpr bool kEnableValidationLayers = true;
#endif

    const std::vector<const char*> m_validationLayers = {"VK_LAYER_KHRONOS_validation"};

    const std::vector<const char*> m_deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    // =========================================================================
    // Initialization methods
    // =========================================================================

    void createInstance();
    bool checkValidationLayerSupport();
    std::vector<const char*> getRequiredExtensions();

    void setupDebugMessenger();
    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
    static VKAPI_ATTR VkBool32 VKAPI_CALL
    debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                  VkDebugUtilsMessageTypeFlagsEXT messageType,
                  const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);

    void createSurface(Window* window);

    void pickPhysicalDevice();
    int rateDeviceSuitability(VkPhysicalDevice device);
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
    bool checkDeviceExtensionSupport(VkPhysicalDevice device);

    void createLogicalDevice();

    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
    VkSurfaceFormatKHR
    chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR
    chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, Window* window);
    void createSwapChain(Window* window);
    void createImageViews();

    void createRenderPass();
    void createDepthResources();
    void createFramebuffers();

    VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling,
                                 VkFormatFeatureFlags features);
    VkFormat findDepthFormat();
    bool hasStencilComponent(VkFormat format) const;
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

    void createDescriptorSetLayouts();
    void createUniformBuffers();
    void updateUniformBuffer(uint32_t currentFrame);

    void createCommandPool();
    void createCommandBuffers();
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);

    void createSyncObjects();

    void cleanupSwapChain();
};

}  // namespace vde
