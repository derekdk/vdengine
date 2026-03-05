#pragma once

/**
 * @file TransitionManager.h
 * @brief Manages the lifecycle, render targets, and compositing of screen transitions.
 *
 * This class is internal to the engine and is owned by Game.
 * Users interact with it indirectly through Game's public transition API.
 *
 * Responsibilities:
 * - Owns two OffscreenRenderTarget instances (source + destination)
 * - Manages active Transition instance, elapsed time, and completion callback
 * - Drives progress from 0→1 and fires the completion callback
 * - Creates the fullscreen-quad graphics pipeline for compositing
 * - Records composite render pass commands into a command buffer
 */

#include <vde/OffscreenRenderTarget.h>
#include <vde/api/Transition.h>

#include <vulkan/vulkan.h>

#include <cstdint>
#include <functional>
#include <memory>

namespace vde {

class VulkanContext;

/**
 * @brief Manages the lifecycle and rendering of screen transitions.
 *
 * This class is internal to the engine and is owned by Game.
 * Users interact with it indirectly through Game's public API.
 *
 * The TransitionManager:
 * 1. Owns two offscreen render targets (source scene / destination scene)
 * 2. Drives a Transition's progress from 0→1 over the specified duration
 * 3. Fires a completion callback when the transition finishes
 * 4. Renders the composite frame using the transition's shader
 */
class TransitionManager {
  public:
    /**
     * @brief Construct a TransitionManager.
     * @param context VulkanContext that owns the device and render passes
     */
    explicit TransitionManager(VulkanContext* context);
    ~TransitionManager();

    // Non-copyable, non-movable
    TransitionManager(const TransitionManager&) = delete;
    TransitionManager& operator=(const TransitionManager&) = delete;
    TransitionManager(TransitionManager&&) = delete;
    TransitionManager& operator=(TransitionManager&&) = delete;

    /**
     * @brief Begin a transition.
     *
     * If a transition is already active, it is cancelled first.
     * If duration <= 0, the completion callback fires immediately.
     *
     * @param transition  The transition effect (TransitionManager takes ownership)
     * @param duration    Duration in seconds (must be > 0 for a visual transition)
     * @param onComplete  Callback invoked when the transition finishes
     */
    void start(std::unique_ptr<Transition> transition, float duration,
               std::function<void()> onComplete = nullptr);

    /**
     * @brief Drive the transition forward by deltaTime.
     *
     * Advances elapsed time, computes progress, and updates the
     * transition's uniforms. If progress reaches 1, the transition
     * completes and the callback fires.
     *
     * @param deltaTime Frame delta in seconds
     */
    void update(float deltaTime);

    /**
     * @brief Returns true while a transition is in progress.
     */
    bool isActive() const;

    /**
     * @brief Cancel the current transition immediately.
     *
     * The completion callback does NOT fire on cancel.
     */
    void cancel();

    /**
     * @brief Get progress [0, 1] of the current transition.
     * @return 0 if no transition is active.
     */
    float getProgress() const;

    /**
     * @brief Pause or unpause the active transition.
     *
     * While paused, update() does not advance elapsed time.
     * The transition remains active and continues to render at its
     * current progress.
     */
    void setPaused(bool paused);

    /**
     * @brief Returns true if the transition is currently paused.
     */
    bool isPaused() const;

    /**
     * @brief Advance a paused transition by exactly one frame's worth of time.
     *
     * Has no effect if the transition is not paused or not active.
     * The deltaTime used is the same value that would have been passed
     * to update() by the engine.
     *
     * @param deltaTime The frame delta to advance by (in seconds)
     */
    void stepOneFrame(float deltaTime);

    /**
     * @brief Set the playback speed multiplier for transitions.
     *
     * 1.0 = normal speed, 0.25 = quarter speed, 2.0 = double speed, etc.
     * The multiplier scales the deltaTime passed to update().
     * Must be > 0.
     *
     * @param speed Speed multiplier (default 1.0)
     */
    void setSpeed(float speed);

    /**
     * @brief Get the current playback speed multiplier.
     */
    float getSpeed() const;

    /**
     * @brief Get the current transition uniforms (for the GPU push constants).
     */
    const TransitionUniforms& getUniforms() const { return m_uniforms; }

    /**
     * @brief Get the active transition (may be null).
     */
    const Transition* getActiveTransition() const { return m_activeTransition.get(); }

    // ---- Render-target management ----

    /**
     * @brief Create / recreate offscreen render targets to match swapchain size.
     * @param width  New width in pixels
     * @param height New height in pixels
     */
    void recreateRenderTargets(uint32_t width, uint32_t height);

    /**
     * @brief Get the source offscreen render target (scene being transitioned FROM).
     */
    OffscreenRenderTarget& getSourceTarget() { return m_source; }
    const OffscreenRenderTarget& getSourceTarget() const { return m_source; }

    /**
     * @brief Get the destination offscreen render target (scene being transitioned TO).
     */
    OffscreenRenderTarget& getDestTarget() { return m_dest; }
    const OffscreenRenderTarget& getDestTarget() const { return m_dest; }

    /**
     * @brief Get the source framebuffer for rendering the source scene.
     */
    VkFramebuffer getSourceFramebuffer() const;

    /**
     * @brief Get the destination framebuffer for rendering the dest scene.
     */
    VkFramebuffer getDestFramebuffer() const;

    /**
     * @brief Render the composited transition frame.
     *
     * Binds source + dest textures, sets push constants, and draws the
     * fullscreen triangle (or delegates to custom geometry).
     *
     * This must be called within an active render pass targeting the
     * swapchain framebuffer.
     *
     * @param cmd Active command buffer
     */
    void renderComposite(VkCommandBuffer cmd);

  private:
    VulkanContext* m_context = nullptr;

    // Offscreen render targets
    OffscreenRenderTarget m_source;
    OffscreenRenderTarget m_dest;

    // Transition state
    std::unique_ptr<Transition> m_activeTransition;
    float m_duration = 0.0f;
    float m_elapsed = 0.0f;
    float m_progress = 0.0f;
    std::function<void()> m_onComplete;
    TransitionUniforms m_uniforms{};
    bool m_paused = false;
    float m_speed = 1.0f;

    // Vulkan resources for the fullscreen composite pipeline
    VkPipeline m_pipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;

    // Pipeline management
    std::string m_currentFragShaderPath;  ///< Currently compiled fragment shader

    void createDescriptorResources();
    void destroyDescriptorResources();
    void updateDescriptorSet();
    void createPipeline(const std::string& fragShaderPath);
    void destroyPipeline();
    void ensurePipeline();
};

}  // namespace vde
