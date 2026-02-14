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

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "GameSettings.h"
#include "InputHandler.h"
#include "ResourceManager.h"
#include "Scene.h"
#include "SceneGroup.h"
#include "Scheduler.h"
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
 *     game.addScene("main", new MainScene());
 *     game.setActiveScene("main");
 *     game.run();
 *
 *     return 0;
 * }
 * @endcode
 */
class Game {
  public:
    Game();
    virtual ~Game();

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
    bool isInitialized() const { return m_initialized; }

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
    void quit();

    /**
     * @brief Check if the game is running.
     */
    bool isRunning() const { return m_running; }

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
     * @param name Name of the scene to remove
     */
    void removeScene(const std::string& name);

    /**
     * @brief Get a scene by name.
     * @param name Scene name
     * @return Pointer to scene, or nullptr if not found
     */
    Scene* getScene(const std::string& name);

    /**
     * @brief Set the active scene.
     * @param name Name of the scene to activate
     *
     * Internally creates a single-scene group so that
     * setActiveSceneGroup-based scheduling works identically.
     */
    void setActiveScene(const std::string& name);

    /**
     * @brief Set a group of scenes to be active simultaneously.
     *
     * The first scene in the group is the primary scene (its camera
     * and background color are used for rendering).  All scenes in
     * the group receive update() calls each frame.  Scenes outside
     * the group that have continueInBackground==true also receive
     * update() calls.
     *
     * @param group The SceneGroup describing the scenes to activate
     */
    void setActiveSceneGroup(const SceneGroup& group);

    /**
     * @brief Get the currently active scene group.
     */
    const SceneGroup& getActiveSceneGroup() const { return m_activeSceneGroup; }

    /**
     * @brief Get the currently active scene.
     */
    Scene* getActiveScene() { return m_activeScene; }
    const Scene* getActiveScene() const { return m_activeScene; }

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
    const Scene* getFocusedScene() const;

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
     * The current scene is paused and the new scene becomes active.
     * Use popScene() to return to the previous scene.
     *
     * @param name Name of the scene to push
     */
    void pushScene(const std::string& name);

    /**
     * @brief Pop the current scene and return to the previous one.
     */
    void popScene();

    // Input handling

    // Scheduler

    /**
     * @brief Get the task scheduler.
     *
     * The scheduler manages the per-frame task graph. It is rebuilt
     * automatically when scenes change.
     */
    Scheduler& getScheduler() { return m_scheduler; }
    const Scheduler& getScheduler() const { return m_scheduler; }

    /**
     * @brief Set the global input handler.
     * @param handler Input handler (Game does NOT take ownership)
     */
    void setInputHandler(InputHandler* handler) { m_inputHandler = handler; }

    /**
     * @brief Get the global input handler.
     */
    InputHandler* getInputHandler() { return m_inputHandler; }
    const InputHandler* getInputHandler() const { return m_inputHandler; }

    // Timing

    /**
     * @brief Get the time since the last frame in seconds.
     */
    float getDeltaTime() const { return m_deltaTime; }

    /**
     * @brief Get the total time since game start in seconds.
     */
    double getTotalTime() const { return m_totalTime; }

    /**
     * @brief Get the current frames per second.
     */
    float getFPS() const { return m_fps; }

    /**
     * @brief Get the current frame number.
     */
    uint64_t getFrameCount() const { return m_frameCount; }

    // Window access

    /**
     * @brief Get the game window.
     */
    Window* getWindow() { return m_window.get(); }
    const Window* getWindow() const { return m_window.get(); }

    /**
     * @brief Get DPI scale factor for the window.
     * @return DPI scale (1.0 = 100%, 1.5 = 150%, 2.0 = 200%)
     *
     * Returns the content scale factor for the window's monitor,
     * which is useful for scaling UI elements on high-DPI displays.
     */
    float getDPIScale() const;

    // Settings

    /**
     * @brief Get the current game settings.
     */
    const GameSettings& getSettings() const { return m_settings; }

    /**
     * @brief Get the Vulkan context (for advanced rendering).
     */
    VulkanContext* getVulkanContext() { return m_vulkanContext.get(); }
    const VulkanContext* getVulkanContext() const { return m_vulkanContext.get(); }

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
    const ResourceManager& getResourceManager() const { return m_resourceManager; }

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
    VkPipeline getMeshPipeline() const { return m_meshPipeline; }

    /**
     * @brief Get the mesh pipeline layout.
     */
    VkPipelineLayout getMeshPipelineLayout() const { return m_meshPipelineLayout; }

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
    VkPipeline getSpritePipeline() const { return m_spritePipeline; }

    /**
     * @brief Get the sprite pipeline layout.
     */
    VkPipelineLayout getSpritePipelineLayout() const { return m_spritePipelineLayout; }

    /**
     * @brief Get the sprite sampler.
     */
    VkSampler getSpriteSampler() const { return m_spriteSampler; }

    /**
     * @brief Get the default white texture for sprites without textures.
     */
    Texture* getDefaultWhiteTexture() const { return m_defaultWhiteTexture.get(); }

    /**
     * @brief Get the sprite descriptor set layout.
     */
    VkDescriptorSetLayout getSpriteDescriptorSetLayout() const {
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
    VkDescriptorSetLayout getLightingDescriptorSetLayout() const {
        return m_lightingDescriptorSetLayout;
    }

    /**
     * @brief Get the current frame's lighting descriptor set.
     */
    VkDescriptorSet getCurrentLightingDescriptorSet() const;

    /**
     * @brief Update the lighting UBO with scene lighting data.
     * @param scene Scene containing LightBox to upload
     */
    void updateLightingUBO(const Scene* scene);

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
    std::vector<std::string> m_sceneStack;
    std::string m_pendingScene;
    bool m_sceneSwitchPending = false;
    SceneGroup m_activeSceneGroup;

    // Input focus for split-screen
    std::string m_focusedSceneName;

    // Lighting infrastructure (Phase 4)
    VkDescriptorSetLayout m_lightingDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_lightingDescriptorPool = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> m_lightingDescriptorSets;  // One per frame-in-flight
    std::vector<VkBuffer> m_lightingUBOBuffers;             // One per frame-in-flight
    std::vector<VkDeviceMemory> m_lightingUBOMemory;        // One per frame-in-flight
    std::vector<void*> m_lightingUBOMapped;                 // Persistently mapped pointers

    // Scheduler
    Scheduler m_scheduler;

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

    // Callbacks
    std::function<void(uint32_t, uint32_t)> m_resizeCallback;
    std::function<void(bool)> m_focusCallback;

    // Internal methods
    void processInput();
    void pollGamepads();
    void updateTiming();
    void processPendingSceneChange();
    void setupInputCallbacks();
    void createMeshRenderingPipeline();
    void destroyMeshRenderingPipeline();
    void createSpriteRenderingPipeline();
    void destroySpriteRenderingPipeline();
    void createLightingResources();
    void destroyLightingResources();
    void rebuildSchedulerGraph();
    void renderSingleViewport();
    void renderMultiViewport();
};

}  // namespace vde
