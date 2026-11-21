#pragma once

/**
 * @file Game.h
 * @brief Main game class for VDE
 *
 * Provides the central Game class that manages the game loop,
 * scenes, input, and all engine subsystems.
 */

#include <vde/Texture.h>

#include <vulkan/vulkan.h>

#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "GameSettings.h"
#include "InputHandler.h"
#include "InputScript.h"
#include "InputScriptExecutor.h"
#include "ResourceManager.h"
#include "Scene.h"
#include "SceneGroup.h"
#include "Scheduler.h"
#include "ScriptEnvironment.h"
#include "Transition.h"
#include "TransitionManager.h"
#include "ViewportRect.h"

namespace vde {

// Forward declarations
class Window;
class VulkanContext;

/**
 * @brief Main game class that manages the game loop and scenes.
 *
 * The Game class is the entry point for VDE-based games. It handles:
 * - Engine initialization and shutdown
 * - The main game loop
 * - Scene management
 * - Input dispatching
 * - Frame timing
 *
 * @example
 * @code
 * #include <vde/api/Game.h>
 *
 * int main() {
 *     vde::Game game;
 *     vde::GameSettings settings;
 *     settings.gameName = "My Game";
 *     settings.display.windowWidth = 1280;
 *     settings.display.windowHeight = 720;
 *
 *     game.initialize(settings);
 *     game.addScene("main", std::make_unique<MainScene>());
 *     game.setActiveScene("main");
 *     game.run();
 *
 *     return 0;
 * }
 * @endcode
 */
class Game : private ScriptEnvironment {
  public:
    Game();
    ~Game() override;

    // Non-copyable, non-movable
    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;
    Game(Game&&) = delete;
    Game& operator=(Game&&) = delete;

    // Initialization

    /**
     * @brief Initialize the game engine.
     * @param settings Game configuration
     * @return true if initialization succeeded
     */
    bool initialize(const GameSettings& settings);

    /**
     * @brief Shutdown the game engine and release resources.
     */
    void shutdown();

    /**
     * @brief Check if the game is initialized.
     */
    [[nodiscard]] bool isInitialized() const { return m_initialized; }

    // Game loop

    /**
     * @brief Run the main game loop.
     *
     * This method blocks until the game exits.
     */
    void run();

    /**
     * @brief Request the game to exit.
     */
    void quit() override;

    // Input script

    /**
     * @brief Set the input script file path.
     *
     * This has the highest priority for input script discovery.
     * Priority order: API call > CLI arg > environment variable.
     *
     * @param scriptPath Path to the .vdescript file
     */
    void setInputScriptFile(const std::string& scriptPath);

    /**
     * @brief Get the configured input script file path.
     */
    [[nodiscard]] const std::string& getInputScriptFile() const;

    /**
     * @brief Check if the game is running.
     */
    [[nodiscard]] bool isRunning() const { return m_running; }

    // Scene management

    /**
     * @brief Add a scene to the game.
     * @param name Unique name for the scene
     * @param scene The scene to add (Game takes ownership)
     */
    void addScene(const std::string& name, Scene* scene);
    void addScene(const std::string& name, std::unique_ptr<Scene> scene);

    /**
     * @brief Remove a scene by name.
     * Removal requested during a scene-flow operation (for example, from onPause()
     * or onExit()), during manual transition stepping, or during a scheduler frame with
     * an active transition is queued. Otherwise removal is immediate, but scene destruction
     * may be deferred until scheduler tasks and GPU work are complete.
     * @param name Name of the scene to remove
     */
    void removeScene(const std::string& name);

    /**
     * @brief Get a scene by name.
     * @param name Scene name
     * @return Pointer to scene, or nullptr if not found
     */
    Scene* getScene(const std::string& name) override;

    /**
     * @brief Set the active scene.
     * @param name Name of the scene to activate
     *
     * Internally creates a single-scene group so that
     * setActiveSceneGroup-based scheduling works identically.
     * Clears the scene stack: paused scenes that are not the new scene
     * receive onExit().
     *
     * @note Scene-flow timing: while the game loop is running, scene-flow
     * requests (setActiveScene, setActiveSceneGroup, pushScene, popScene,
     * transitionToScene, transitionToSceneGroup, cancelTransition) are queued
     * and applied in call order at the start of the next frame.  Calls made
     * while the loop is not running (e.g. before run()) apply immediately.
     */
    void setActiveScene(const std::string& name);

    /**
     * @brief Set a group of scenes to be active simultaneously.
     *
     * The first scene in the group is the primary scene (its background
     * color is used for the initial clear, and scenes without their own
     * camera reuse its camera).  Every scene in the group is updated and
     * drawn each frame with its own camera, lighting, and viewport, in
     * group order (later scenes draw on top with a fresh depth buffer).
     * Scenes outside the group that have continueInBackground==true are
     * also updated.  Clears the scene stack.
     *
     * Follows the scene-flow timing described on setActiveScene().
     *
     * @param group The SceneGroup describing the scenes to activate
     */
    void setActiveSceneGroup(const SceneGroup& group);

    /**
     * @brief Get the currently active scene group.
     */
    [[nodiscard]] const SceneGroup& getActiveSceneGroup() const override {
        return m_activeSceneGroup;
    }

    /**
     * @brief Get the currently active scene.
     */
    Scene* getActiveScene() { return m_activeScene; }
    [[nodiscard]] const Scene* getActiveScene() const { return m_activeScene; }

    // Input focus (for split-screen)

    /**
     * @brief Set which scene receives keyboard input.
     *
     * When split-screen viewports are active, this controls which scene
     * gets keyboard events.  Defaults to the primary scene.
     *
     * @param sceneName Name of the scene to focus
     */
    void setFocusedScene(const std::string& sceneName);

    /**
     * @brief Get the currently focused scene for keyboard input.
     * @return Pointer to the focused scene, or the primary scene if none set
     */
    Scene* getFocusedScene();
    [[nodiscard]] const Scene* getFocusedScene() const;

    /**
     * @brief Get the scene whose viewport contains the given screen position.
     * @param mouseX Mouse X position in pixels
     * @param mouseY Mouse Y position in pixels
     * @return Pointer to the scene under the cursor, or nullptr
     */
    Scene* getSceneAtScreenPosition(double mouseX, double mouseY);

    /**
     * @brief Push a scene onto the scene stack.
     *
     * The current active group is paused (each scene receives onPause() and
     * stops updating unless it has continueInBackground) and stays visible,
     * frozen, underneath.  The pushed scene becomes the active group: it is
     * entered, updated, focused, and drawn on top with its own camera.
     * Use popScene() to return to the previous group.
     *
     * Pushing a scene that is already active or paused is ignored.
     * Follows the scene-flow timing described on setActiveScene().
     *
     * @param name Name of the scene to push
     */
    void pushScene(const std::string& name);

    /**
     * @brief Pop the current scene and return to the previous group.
     *
     * The current active group's scenes receive onExit(); the previous
     * group's scenes receive onResume() and start updating again.
     * Does nothing if the stack is empty.
     * Follows the scene-flow timing described on setActiveScene().
     */
    void popScene();

    /**
     * @brief Get the number of paused groups beneath the active group.
     */
    [[nodiscard]] size_t getSceneStackDepth() const { return m_sceneStack.size(); }

    /**
     * @brief Get the scenes drawn each frame, in draw order (bottom to top).
     *
     * Includes paused scenes from the scene stack (beneath) followed by the
     * active group.  During a transition this reports the source side.
     */
    [[nodiscard]] std::vector<std::string> getVisibleSceneNames() const override;

    /**
     * @brief Check whether scene-flow requests are queued for the next frame.
     */
    [[nodiscard]] bool hasPendingSceneChanges() const { return !m_sceneFlowRequests.empty(); }

    /**
     * @brief Apply queued scene-flow requests now (the frame-boundary step).
     *
     * run() calls this automatically at the start of every frame.  It is only
     * needed by code that steps frames manually through getScheduler().execute()
     * (headless drivers, tests).  Ignored while a frame is executing.
     */
    void applyPendingSceneChanges();

    // =========================================================================
    // Transitions
    // =========================================================================

    /**
     * @brief Transition to a scene with a visual effect.
     *
     * Equivalent to transitionToSceneGroup() with a single-scene group.
     * If duration <= 0, performs an instant scene switch (equivalent to setActiveScene).
     *
     * @param name       Name of the scene to transition to
     * @param transition The visual transition effect (Game takes ownership)
     * @param duration   Duration in seconds
     * @throws std::runtime_error if the scene is not registered
     */
    void transitionToScene(const std::string& name, std::unique_ptr<Transition> transition,
                           float duration);

    /**
     * @brief Transition a specific viewport region with a visual effect.
     *
     * Only the specified viewport region participates in the transition.
     * The active-group scene occupying @p region (exact viewport match, or the
     * topmost scene containing the region's center) is the source; when the
     * transition completes, the destination scene replaces it in the active
     * group with @p region as its viewport.  Other viewports keep rendering
     * and updating normally, and the scene stack is preserved.
     *
     * The destination must not already be active or paused.
     *
     * @param name       Name of the scene to transition to
     * @param transition The visual transition effect (Game takes ownership)
     * @param duration   Duration in seconds
     * @param region     Normalized viewport region for the transition
     * @throws std::runtime_error if the scene is not registered
     */
    void transitionToScene(const std::string& name, std::unique_ptr<Transition> transition,
                           float duration, const ViewportRect& region);

    /**
     * @brief Transition to a scene group with a visual effect.
     *
     * Everything currently visible (paused stack scenes plus the active
     * group) is the source; the destination group is rendered with its own
     * viewports and cameras.  Destination scenes are entered (or resumed)
     * when the transition starts and update alongside the source.  When it
     * completes the destination group becomes active, the scene stack is
     * cleared, and source scenes not in the destination receive onExit().
     * Scenes present in both groups (e.g. a HUD) stay entered throughout.
     *
     * If duration <= 0, performs an instant switch (equivalent to
     * setActiveSceneGroup).  Follows the scene-flow timing described on
     * setActiveScene().
     *
     * @param group      Destination scene group (must be non-empty)
     * @param transition The visual transition effect (Game takes ownership)
     * @param duration   Duration in seconds
     * @throws std::runtime_error if the group is empty or names an unregistered scene
     */
    void transitionToSceneGroup(const SceneGroup& group, std::unique_ptr<Transition> transition,
                                float duration);

    /**
     * @brief Returns true while a scene transition is in progress or queued to start.
     */
    [[nodiscard]] bool isTransitioning() const;

    /**
     * @brief Cancel the current transition.
     *
     * Snaps back to the source; destination scenes that the transition
     * entered receive onExit() (scenes it resumed are paused again).
     * A cancellation queued before the scheduler advances the transition skips
     * that frame's effect update and completion hook. A cancellation requested
     * from the effect's update callback also skips the completion hook.
     * Follows the scene-flow timing described on setActiveScene().
     */
    void cancelTransition();

    /**
     * @brief Get the progress [0, 1] of the current transition.
     * @return 0 if no transition is active.
     */
    [[nodiscard]] float getTransitionProgress() const;

    /**
     * @brief Pause or unpause the active transition.
     *
     * While paused, the transition freezes at its current progress
     * but continues to render. Use stepTransitionOneFrame() to
     * advance frame-by-frame while paused.
     */
    void setTransitionPaused(bool paused);

    /**
     * @brief Returns true if the transition is currently paused.
     */
    [[nodiscard]] bool isTransitionPaused() const;

    /**
     * @brief Advance a paused transition by exactly one frame.
     *
     * Has no effect if the transition is not paused or not active.
     */
    void stepTransitionOneFrame();

    /**
     * @brief Set the playback speed multiplier for transitions.
     *
     * 1.0 = normal speed, 0.25 = quarter speed, etc. Must be > 0.
     *
     * @param speed Speed multiplier (default 1.0)
     */
    void setTransitionSpeed(float speed);

    /**
     * @brief Get the current transition playback speed multiplier.
     */
    [[nodiscard]] float getTransitionSpeed() const;

    // Input handling

    // Scheduler

    /**
     * @brief Get the task scheduler.
     *
     * The scheduler manages the per-frame task graph. It is rebuilt
     * automatically when scenes change.
     */
    Scheduler& getScheduler() { return m_scheduler; }
    [[nodiscard]] const Scheduler& getScheduler() const { return m_scheduler; }

    /**
     * @brief Set the global input handler.
     * @param handler Input handler (Game does NOT take ownership)
     */
    void setInputHandler(InputHandler* handler) { m_inputHandler = handler; }

    /**
     * @brief Get the global input handler.
     */
    InputHandler* getInputHandler() { return m_inputHandler; }
    [[nodiscard]] const InputHandler* getInputHandler() const { return m_inputHandler; }

    // Timing

    /**
     * @brief Get the time since the last frame in seconds.
     */
    [[nodiscard]] float getDeltaTime() const { return m_deltaTime; }

    /**
     * @brief Get the total time since game start in seconds.
     */
    [[nodiscard]] double getTotalTime() const { return m_totalTime; }

    /**
     * @brief Get the current frames per second.
     */
    [[nodiscard]] float getFPS() const { return m_fps; }

    /**
     * @brief Get the current frame number.
     */
    [[nodiscard]] uint64_t getFrameCount() const { return m_frameCount; }

    // Window access

    /**
     * @brief Get the game window.
     */
    Window* getWindow() { return m_window.get(); }
    [[nodiscard]] const Window* getWindow() const { return m_window.get(); }

    /**
     * @brief Queue a window/OS operation to run at a scheduler-safe point.
     *
     * Operations run on the main thread during the scheduler's Input phase,
     * before scene updates and rendering begin for the frame.
     *
     * If the game is not currently running, the operation is executed
     * immediately.
     *
     * @param operation Operation that receives the window reference.
     */
    void scheduleWindowOperation(std::function<void(Window&)> operation);

    /**
     * @brief Queue a window resize operation to run at a scheduler-safe point.
     *
     * If multiple resize requests are queued before the next scheduler tick,
     * only the latest requested size is applied.
     *
     * @param width Target window width in pixels
     * @param height Target window height in pixels
     */
    void scheduleWindowResize(uint32_t width, uint32_t height);

    /**
     * @brief Queue a fullscreen toggle to run at a scheduler-safe point.
     * @param fullscreen True to enter fullscreen, false for windowed mode
     */
    void scheduleWindowFullscreen(bool fullscreen);

    /**
     * @brief Queue a resizable-window attribute change to run at a scheduler-safe point.
     * @param resizable True to allow user resize, false to lock window size
     */
    void scheduleWindowResizable(bool resizable);

    /**
     * @brief Get DPI scale factor for the window.
     * @return DPI scale (1.0 = 100%, 1.5 = 150%, 2.0 = 200%)
     *
     * Returns the content scale factor for the window's monitor,
     * which is useful for scaling UI elements on high-DPI displays.
     */
    [[nodiscard]] float getDPIScale() const;

    // Settings

    /**
     * @brief Get the current game settings.
     */
    [[nodiscard]] const GameSettings& getSettings() const { return m_settings; }

    /**
     * @brief Get the Vulkan context (for advanced rendering).
     */
    VulkanContext* getVulkanContext() { return m_vulkanContext.get(); }
    [[nodiscard]] const VulkanContext* getVulkanContext() const { return m_vulkanContext.get(); }

    /**
     * @brief Get the global resource manager.
     *
     * Use this to load and cache resources globally, sharing them across scenes.
     *
     * @example
     * @code
     * auto texture = game.getResourceManager().load<Texture>("player.png");
     * @endcode
     */
    ResourceManager& getResourceManager() { return m_resourceManager; }
    [[nodiscard]] const ResourceManager& getResourceManager() const { return m_resourceManager; }

    /**
     * @brief Apply new display settings.
     */
    void applyDisplaySettings(const DisplaySettings& settings);

    /**
     * @brief Apply new graphics settings.
     */
    void applyGraphicsSettings(const GraphicsSettings& settings);

    // Events/callbacks

    /**
     * @brief Set callback for window resize.
     */
    void setResizeCallback(std::function<void(uint32_t, uint32_t)> callback);

    /**
     * @brief Set callback for window focus change.
     */
    void setFocusCallback(std::function<void(bool)> callback);

    // Public accessors for rendering (used by MeshEntity)

    /**
     * @brief Get the mesh rendering pipeline.
     */
    [[nodiscard]] VkPipeline getMeshPipeline() const { return m_meshPipeline; }

    /**
     * @brief Get the mesh pipeline layout.
     */
    [[nodiscard]] VkPipelineLayout getMeshPipelineLayout() const { return m_meshPipelineLayout; }

    /**
     * @brief Allocate a mesh texture descriptor set (set 2).
     */
    VkDescriptorSet allocateMeshTextureDescriptorSet();

    /**
     * @brief Update a mesh texture descriptor set with texture binding.
     * @param descriptorSet Descriptor set to update
     * @param imageView Texture image view
     * @param sampler Texture sampler
     */
    void updateMeshTextureDescriptor(VkDescriptorSet descriptorSet, VkImageView imageView,
                                     VkSampler sampler);

    /**
     * @brief Get the sprite rendering pipeline.
     */
    [[nodiscard]] VkPipeline getSpritePipeline() const { return m_spritePipeline; }

    /**
     * @brief Get the sprite pipeline layout.
     */
    [[nodiscard]] VkPipelineLayout getSpritePipelineLayout() const {
        return m_spritePipelineLayout;
    }

    /**
     * @brief Get the sprite sampler.
     */
    [[nodiscard]] VkSampler getSpriteSampler() const { return m_spriteSampler; }

    /**
     * @brief Get the default white texture for sprites without textures.
     */
    [[nodiscard]] Texture* getDefaultWhiteTexture() const { return m_defaultWhiteTexture.get(); }

    /**
     * @brief Get the sprite descriptor set layout.
     */
    [[nodiscard]] VkDescriptorSetLayout getSpriteDescriptorSetLayout() const {
        return m_spriteDescriptorSetLayout;
    }

    /**
     * @brief Allocate a sprite descriptor set with both UBO and texture.
     */
    VkDescriptorSet allocateSpriteDescriptorSet();

    /**
     * @brief Update a sprite descriptor set with UBO and texture bindings.
     * @param descriptorSet The descriptor set to update
     * @param uboBuffer The uniform buffer for view/projection matrices
     * @param uboSize Size of the UBO
     * @param imageView The texture image view
     * @param sampler The texture sampler
     */
    void updateSpriteDescriptor(VkDescriptorSet descriptorSet, VkBuffer uboBuffer,
                                VkDeviceSize uboSize, VkImageView imageView, VkSampler sampler);

    // =========================================================================
    // Lighting System (Phase 4)
    // =========================================================================

    /**
     * @brief Get the lighting descriptor set layout (Set 1 for mesh pipeline).
     */
    [[nodiscard]] VkDescriptorSetLayout getLightingDescriptorSetLayout() const {
        return m_lightingDescriptorSetLayout;
    }

    /**
     * @brief Get the lighting descriptor set selected by the last updateLightingUBO() call.
     *
     * Prefer getLightingDescriptorSet(scene), which uploads and selects in one step.
     */
    [[nodiscard]] VkDescriptorSet getCurrentLightingDescriptorSet() const;

    /**
     * @brief Upload a scene's lighting into its per-frame lighting slot.
     *
     * Each scene drawn in a frame gets its own lighting slot, so scenes with
     * different LightBoxes can be drawn in the same frame.  The upload happens
     * once per scene per frame; later calls just select the scene's slot.
     *
     * @param scene Scene containing LightBox to upload (nullptr = default lighting)
     */
    void updateLightingUBO(const Scene* scene);

    /**
     * @brief Get the lighting descriptor set for @p scene in the current frame.
     *
     * Uploads the scene's lighting on first use this frame (see updateLightingUBO()).
     */
    [[nodiscard]] VkDescriptorSet getLightingDescriptorSet(const Scene* scene);

    /// Maximum number of distinct scenes with independent lighting per frame.
    static constexpr uint32_t kMaxLightingSlotsPerFrame = 16;

    // =========================================================================
    // Exit Code Management (B6 minimal — needed by Track A assertions)
    // =========================================================================

    /**
     * @brief Set the process exit code.
     *
     * Used by assertion commands to signal test failure.
     * Once set to non-zero, subsequent calls with 0 are ignored.
     *
     * @param code Exit code (0 = success, 1 = failure)
     */
    void setExitCode(int code) override;

    /**
     * @brief Get the current exit code.
     * @return 0 for success, 1 for failure
     */
    [[nodiscard]] virtual int getExitCode() const { return m_exitCode; }

    /// Get the script executor (for test inspection).
    InputScriptExecutor* getScriptExecutor() { return m_scriptExecutor.get(); }

    // =========================================================================
    // Screenshot Capture (A2)
    // =========================================================================

    /**
     * @brief Capture the current framebuffer to a PNG file.
     *
     * Reads back the most recently rendered frame from the GPU and writes
     * it to the specified path as an RGBA PNG image.
     *
     * @param outputPath Path for the output PNG file
     * @return true if capture and write succeeded
     */
    bool captureScreenshot(const std::string& outputPath) override;

  protected:
    // Virtual methods for subclassing

    /**
     * @brief Called once before the game loop starts.
     */
    virtual void onStart() {}

    /**
     * @brief Called every frame before scene update.
     */
    virtual void onUpdate([[maybe_unused]] float deltaTime) {}

    /**
     * @brief Called every frame after scene render.
     */
    virtual void onRender() {}

    /**
     * @brief Called when the game is shutting down.
     */
    virtual void onShutdown() {}

  private:
    // Initialization
    bool m_initialized = false;
    bool m_running = false;
    GameSettings m_settings;

    // Core systems
    std::unique_ptr<Window> m_window;
    std::unique_ptr<VulkanContext> m_vulkanContext;
    ResourceManager m_resourceManager;

    // Rendering infrastructure (Phase 2)
    VkPipelineLayout m_meshPipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_meshPipeline = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_meshDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_meshTextureDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_meshTextureDescriptorPool = VK_NULL_HANDLE;

    // Sprite rendering infrastructure (Phase 3)
    VkPipelineLayout m_spritePipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_spritePipeline = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_spriteDescriptorSetLayout = VK_NULL_HANDLE;
    VkSampler m_spriteSampler = VK_NULL_HANDLE;
    VkDescriptorPool m_spriteDescriptorPool = VK_NULL_HANDLE;
    std::unique_ptr<Texture> m_defaultWhiteTexture;  // 1x1 white texture for untextured sprites

    // Scene management
    std::unordered_map<std::string, std::unique_ptr<Scene>> m_scenes;
    Scene* m_activeScene = nullptr;
    SceneGroup m_activeSceneGroup;
    std::unordered_set<std::string>
        m_activeSceneNames;  ///< Scenes that have been entered and not exited

    /// Paused groups beneath the active group (bottom → top).
    std::vector<SceneGroup> m_sceneStack;
    /// Entered scenes that have received onPause() and not yet onResume().
    std::unordered_set<std::string> m_pausedSceneNames;
    /// Viewports assigned by a group's entries (released when a later group has no entry).
    std::unordered_map<std::string, ViewportRect> m_groupAssignedViewports;

    /// A queued scene-flow change (see setActiveScene() for timing rules).
    struct SceneFlowRequest {
        enum class Kind : uint8_t {
            SetGroup,
            Push,
            Pop,
            Remove,
            StartTransition,
            CancelTransition,
            CompleteTransition,
        };
        Kind kind = Kind::SetGroup;
        SceneGroup group;       ///< SetGroup / StartTransition destination
        std::string sceneName;  ///< Push / Remove / region transition destination
        std::unique_ptr<Transition> transition;
        float duration = 0.0f;
        bool hasRegion = false;
        ViewportRect region = ViewportRect::fullWindow();
        uint64_t transitionId = 0;  ///< CompleteTransition: which transition finished
    };
    std::vector<SceneFlowRequest> m_sceneFlowRequests;
    bool m_processingSceneFlow = false;
    bool m_schedulerRebuildPending = false;

    /// A scene drawn this frame and the viewport it is drawn into.
    struct RenderLayer {
        Scene* scene = nullptr;
        ViewportRect viewport = ViewportRect::fullWindow();
    };

    struct RetiredScene {
        std::unique_ptr<Scene> scene;
        std::optional<uint64_t> lastSubmission;  ///< Set after the frame that removed it.
    };
    std::deque<RetiredScene> m_retiredScenes;

    // Input focus for split-screen
    std::string m_focusedSceneName;

    // Scene creation/removal counters (for ScriptEnvironment)
    size_t m_scenesCreated = 0;
    size_t m_scenesRemoved = 0;

    // Lighting infrastructure (Phase 4)
    // Each frame-in-flight owns one buffer split into kMaxLightingSlotsPerFrame
    // aligned slots; each scene drawn in a frame is assigned its own slot.
    VkDescriptorSetLayout m_lightingDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_lightingDescriptorPool = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> m_lightingDescriptorSets;  // [frame * slots + slot]
    std::vector<VkBuffer> m_lightingUBOBuffers;             // One per frame-in-flight
    std::vector<VkDeviceMemory> m_lightingUBOMemory;        // One per frame-in-flight
    std::vector<void*> m_lightingUBOMapped;                 // Persistently mapped pointers
    VkDeviceSize m_lightingSlotStride = 0;
    std::vector<const Scene*> m_lightingSlotScenes;  ///< Scenes assigned a slot this frame
    uint32_t m_currentLightingSlot = 0;
    bool m_lightingSlotOverflowWarned = false;

    // Transition system
    std::unique_ptr<TransitionManager> m_transitionManager;
    bool m_transitionActive = false;             ///< Started and not yet completed/cancelled
    bool m_transitionCompletionPending = false;  ///< Effect finished; applied next frame
    SceneGroup m_transitionDestGroup;            ///< Active group once the transition completes
    std::string m_transitionSourceScene;         ///< Region transitions: scene being replaced
    std::string m_transitionDestScene;           ///< Region transitions: incoming scene
    std::vector<std::string> m_transitionEnteredScenes;  ///< Entered by the transition start
    std::vector<std::string> m_transitionResumedScenes;  ///< Resumed by the transition start
    uint64_t m_transitionGeneration = 0;                 ///< Incremented per started transition
    ViewportRect m_transitionViewport = ViewportRect::fullWindow();  ///< Viewport for transition
    bool m_viewportTransition = false;  ///< True when transition is viewport-scoped

    // Scheduler
    Scheduler m_scheduler;

    enum class WindowOperationKind : uint8_t {
        Generic,
        Resize,
    };

    struct PendingWindowOperation {
        WindowOperationKind kind = WindowOperationKind::Generic;
        std::function<void(Window&)> operation;
    };

    // Queued window operations (executed in scheduler Input phase on main thread).
    // Resize requests are de-duplicated so only the latest queued size is applied.
    std::vector<PendingWindowOperation> m_pendingWindowOperations;
    std::mutex m_pendingWindowOperationsMutex;

    // Input
    InputHandler* m_inputHandler = nullptr;

    // Timing
    float m_deltaTime = 0.0f;
    double m_totalTime = 0.0;
    float m_fps = 0.0f;
    uint64_t m_frameCount = 0;
    double m_lastFrameTime = 0.0;
    double m_fpsAccumulator = 0.0;
    int m_fpsFrameCount = 0;

    // Input script
    std::string m_inputScriptFile;
    std::unique_ptr<InputScriptExecutor> m_scriptExecutor;

    // Exit code (B6 minimal)
    int m_exitCode = 0;

    // Callbacks
    std::function<void(uint32_t, uint32_t)> m_resizeCallback;
    std::function<void(bool)> m_focusCallback;

    // ScriptEnvironment overrides (private, no public declaration)
    InputHandler* resolveInputHandler() override;
    [[nodiscard]] std::pair<uint32_t, uint32_t> getSwapChainExtent() const override;
    [[nodiscard]] size_t getScenesCreated() const override { return m_scenesCreated; }
    [[nodiscard]] size_t getScenesRemoved() const override { return m_scenesRemoved; }

    // Internal methods
    void processInput();
    void processInputScript();
    void loadInputScript();
    void pollGamepads();
    void updateTiming();
    void processPendingSceneChange();
    void executePendingWindowOperations();
    void setupInputCallbacks();
    void createMeshRenderingPipeline();
    void destroyMeshRenderingPipeline();
    void createSpriteRenderingPipeline();
    void destroySpriteRenderingPipeline();
    void createLightingResources();
    void destroyLightingResources();
    void beginLightingFrame();
    void rebuildSchedulerGraph();

    // Scene flow (see setActiveScene() for timing rules)
    void requestSceneFlow(SceneFlowRequest request);
    void applySceneFlowRequest(SceneFlowRequest& request);
    void applySceneGroup(const SceneGroup& group, bool keepStack);
    void applyPushScene(const std::string& name);
    void applyPopScene();
    void applyRemoveScene(const std::string& name);
    void applyStartTransition(SceneFlowRequest& request);
    void applyCancelTransition();
    void applyCompleteTransition();
    void clearTransitionState();
    void applyGroupViewports(const SceneGroup& group);
    [[nodiscard]] ViewportRect resolveGroupViewport(const SceneGroup& group,
                                                    const std::string& sceneName,
                                                    const Scene& scene) const;
    void refreshFocusDiagnostics();
    [[nodiscard]] bool sceneExists(const std::string& name) const;
    [[nodiscard]] bool shouldDeferSceneFlow() const;
    [[nodiscard]] bool hasEarlierTransitionCancellation() const;

    // Render layer collection
    [[nodiscard]] std::vector<RenderLayer> collectVisibleLayers() const;
    [[nodiscard]] std::vector<RenderLayer> collectGroupLayers(const SceneGroup& group) const;
    [[nodiscard]] std::vector<RenderLayer> collectFrameLayers() const;
    [[nodiscard]] std::vector<RenderLayer> collectStackLayers() const;

    void renderSingleViewport(Scene* scene);
    void renderLayers(const std::vector<RenderLayer>& layers);
    void renderTransition();

    // Centralized scene lifecycle helpers.
    // Always update diagnostics + m_activeSceneNames + call the scene callback.
    // Idempotent: activateScene is a no-op if already active, deactivateScene
    // is a no-op if not active.  This prevents double-enter / double-exit bugs.
    void activateScene(const std::string& name);
    void deactivateScene(const std::string& name);
};

}  // namespace vde
