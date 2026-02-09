#pragma once

/**
 * @file Scene.h
 * @brief Scene management for VDE games
 *
 * Provides the Scene class for managing game states, entities,
 * resources, and rendering for a portion of the game.
 */

#include <vde/Texture.h>

#include <memory>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <vector>

#include "AudioEvent.h"
#include "CameraBounds.h"
#include "Entity.h"
#include "GameCamera.h"
#include "GameTypes.h"
#include "InputHandler.h"
#include "LightBox.h"
#include "PhysicsTypes.h"
#include "Resource.h"
#include "ViewportRect.h"
#include "WorldBounds.h"

namespace vde {

// Forward declarations
class Game;
class Mesh;
class PhysicsScene;
class Texture;

/**
 * @brief Represents a game scene/state.
 *
 * A Scene manages a collection of entities, resources, and rendering
 * settings. Games typically have multiple scenes (menu, gameplay, etc.)
 * and switch between them.
 *
 * @example
 * @code
 * class MainMenuScene : public vde::Scene {
 * public:
 *     void onEnter() override {
 *         // Setup menu UI
 *     }
 *     void update(float deltaTime) override {
 *         // Handle menu logic
 *         vde::Scene::update(deltaTime);
 *     }
 * };
 * @endcode
 */
class Scene {
  public:
    Scene();
    virtual ~Scene();

    /**
     * @brief Get the scene name.
     */
    const std::string& getName() const { return m_name; }

    // Lifecycle methods (override in subclasses)

    /**
     * @brief Called when the scene becomes active.
     */
    virtual void onEnter() {}

    /**
     * @brief Called when the scene is deactivated.
     */
    virtual void onExit() {}

    /**
     * @brief Called when the scene is paused (another scene pushed).
     */
    virtual void onPause() {}

    /**
     * @brief Called when the scene is resumed (returned to top).
     */
    virtual void onResume() {}

    /**
     * @brief Update the scene.
     * @param deltaTime Time since last update in seconds
     */
    virtual void update(float deltaTime);

    // Phase callbacks (opt-in via enablePhaseCallbacks)

    /**
     * @brief Enable phase callbacks for this scene.
     *
     * When enabled, the scheduler splits the scene's update into
     * three separate tasks:
     *   1. updateGameLogic(dt) — GameLogic phase
     *   2. updateAudio(dt)     — Audio phase
     *   3. updateVisuals(dt)   — PreRender phase (visual-only updates)
     *
     * When disabled (the default), the single `update(dt)` task is
     * used instead, preserving backwards compatibility.
     */
    void enablePhaseCallbacks() { m_usePhaseCallbacks = true; }

    /**
     * @brief Check whether phase callbacks are enabled.
     */
    bool usesPhaseCallbacks() const { return m_usePhaseCallbacks; }

    /**
     * @brief Game logic update (phase callback).
     *
     * Called during the GameLogic scheduler phase when phase callbacks
     * are enabled.  Override this to move/spawn entities, process AI,
     * read input, and queue audio events.
     *
     * The default implementation is a no-op.
     *
     * @param deltaTime Time since last update in seconds
     */
    virtual void updateGameLogic(float deltaTime);

    /**
     * @brief Audio update (phase callback).
     *
     * Called during the Audio scheduler phase when phase callbacks
     * are enabled.  The default implementation drains the audio
     * event queue (`m_audioEventQueue`) via AudioManager.
     *
     * Override to add custom audio processing, but call
     * `Scene::updateAudio(deltaTime)` to keep the queue drain.
     *
     * @param deltaTime Time since last update in seconds
     */
    virtual void updateAudio(float deltaTime);

    /**
     * @brief Visual update (phase callback).
     *
     * Called during a late GameLogic / early PreRender slot when
     * phase callbacks are enabled.  Use this for animation ticks,
     * particle systems, or anything that only affects visuals.
     *
     * The default implementation is a no-op.
     *
     * @param deltaTime Time since last update in seconds
     */
    virtual void updateVisuals(float deltaTime);

    // Audio event queue

    /**
     * @brief Queue an audio event to be processed during the Audio phase.
     * @param event The audio event to queue
     */
    void queueAudioEvent(const AudioEvent& event);
    void queueAudioEvent(AudioEvent&& event);

    /**
     * @brief Convenience: queue a PlaySFX event.
     * @param clip Audio clip to play
     * @param volume Volume multiplier (0.0–1.0)
     * @param pitch Pitch multiplier (1.0 = normal)
     * @param loop Whether to loop the sound
     */
    void playSFX(std::shared_ptr<AudioClip> clip, float volume = 1.0f, float pitch = 1.0f,
                 bool loop = false);

    /**
     * @brief Convenience: queue a positional PlaySFXAt event.
     * @param clip Audio clip to play
     * @param x X position
     * @param y Y position
     * @param z Z position
     * @param volume Volume multiplier (0.0–1.0)
     * @param pitch Pitch multiplier (1.0 = normal)
     */
    void playSFXAt(std::shared_ptr<AudioClip> clip, float x, float y, float z, float volume = 1.0f,
                   float pitch = 1.0f);

    /**
     * @brief Get the number of pending audio events in the queue.
     */
    size_t getAudioEventQueueSize() const { return m_audioEventQueue.size(); }

    /**
     * @brief Render the scene.
     */
    virtual void render();

    // Resource management

    /**
     * @brief Add a resource to this scene.
     * @tparam T Resource type (Mesh, Texture, etc.)
     * @param path Path to load the resource from
     * @return Resource ID for referencing the resource
     */
    template <typename T>
    ResourceId addResource(const std::string& path);

    /**
     * @brief Add a pre-created resource to this scene.
     * @tparam T Resource type
     * @param resource The resource to add
     * @return Resource ID
     */
    template <typename T>
    ResourceId addResource(ResourcePtr<T> resource);

    /**
     * @brief Get a resource by ID.
     * @tparam T Resource type
     * @param id Resource ID
     * @return Pointer to resource, or nullptr if not found
     */
    template <typename T>
    T* getResource(ResourceId id);

    /**
     * @brief Remove a resource by ID.
     */
    void removeResource(ResourceId id);

    // Entity management

    /**
     * @brief Add an entity to the scene.
     * @tparam T Entity type
     * @param args Constructor arguments for the entity
     * @return Shared pointer to the created entity
     */
    template <typename T, typename... Args>
    std::shared_ptr<T> addEntity(Args&&... args);

    /**
     * @brief Add an existing entity to the scene.
     * @param entity The entity to add
     * @return The entity's ID
     */
    EntityId addEntity(Entity::Ref entity);

    /**
     * @brief Get an entity by ID.
     * @param id Entity ID
     * @return Pointer to entity, or nullptr if not found
     */
    Entity* getEntity(EntityId id);

    /**
     * @brief Get an entity by name.
     * @param name Entity name
     * @return Pointer to first matching entity, or nullptr
     */
    Entity* getEntityByName(const std::string& name);

    /**
     * @brief Get all entities of a specific type.
     * @tparam T Entity type
     * @return Vector of matching entities
     */
    template <typename T>
    std::vector<std::shared_ptr<T>> getEntitiesOfType();

    /**
     * @brief Remove an entity by ID.
     */
    void removeEntity(EntityId id);

    /**
     * @brief Remove all entities.
     */
    void clearEntities();

    /**
     * @brief Get all entities.
     */
    const std::vector<Entity::Ref>& getEntities() const { return m_entities; }

    // Lighting

    /**
     * @brief Set the scene's lighting configuration.
     * @param lightBox The lighting setup to use
     * @note If no LightBox is set, a default SimpleColorLightBox with white ambient light is used.
     */
    void setLightBox(std::unique_ptr<LightBox> lightBox);
    void setLightBox(LightBox* lightBox);  // Takes ownership

    /**
     * @brief Get the scene's lighting configuration.
     * @return The LightBox, or nullptr if using default lighting.
     */
    LightBox* getLightBox() { return m_lightBox.get(); }
    const LightBox* getLightBox() const { return m_lightBox.get(); }

    /**
     * @brief Get the effective lighting (returns default if none set).
     * @return The LightBox to use for rendering.
     */
    const LightBox& getEffectiveLighting() const;

    // Camera

    /**
     * @brief Set the scene's camera.
     * @param camera The camera to use
     * @note If no camera is set, a default perspective camera is created.
     */
    void setCamera(std::unique_ptr<GameCamera> camera);
    void setCamera(GameCamera* camera);  // Takes ownership

    /**
     * @brief Get the scene's camera.
     * @return The camera, or nullptr if using default.
     */
    GameCamera* getCamera() { return m_camera.get(); }
    const GameCamera* getCamera() const { return m_camera.get(); }

    // Background & Priority

    /**
     * @brief Mark this scene for continued updates while in background.
     *
     * When true and the scene is part of an active SceneGroup, its
     * update() is called even when it is not the primary scene.
     * When true and the scene is NOT in the active group, the engine
     * will still call update() each frame (background updates).
     *
     * @param enabled Whether background updates should be enabled
     */
    void setContinueInBackground(bool enabled) { m_continueInBackground = enabled; }

    /**
     * @brief Check if background updates are enabled.
     */
    bool getContinueInBackground() const { return m_continueInBackground; }

    /**
     * @brief Set the update priority (lower values run first).
     *
     * When multiple scenes are updated in a frame, their update
     * tasks are ordered by priority (ascending).  Default is 0.
     *
     * @param priority Numeric priority (lower = earlier)
     */
    void setUpdatePriority(int priority) { m_updatePriority = priority; }

    /**
     * @brief Get the update priority.
     */
    int getUpdatePriority() const { return m_updatePriority; }

    // Viewport

    /**
     * @brief Set the viewport rectangle for this scene.
     *
     * When used in a SceneGroup, this controls which portion of the
     * window the scene renders to.  Default is fullWindow() (the
     * entire window), which preserves backwards compatibility.
     *
     * @param rect Normalized viewport rectangle (0-1 coordinates)
     */
    void setViewportRect(const ViewportRect& rect) { m_viewportRect = rect; }

    /**
     * @brief Get the viewport rectangle for this scene.
     * @return The viewport rectangle (default is fullWindow)
     */
    const ViewportRect& getViewportRect() const { return m_viewportRect; }

    // Physics

    /**
     * @brief Enable physics simulation for this scene.
     *
     * Creates a PhysicsScene with the given configuration.
     * Once enabled, the scheduler inserts Physics and PostPhysics
     * tasks for this scene.
     *
     * @param config Physics configuration
     */
    void enablePhysics(const PhysicsConfig& config = PhysicsConfig{});

    /**
     * @brief Disable physics simulation for this scene.
     *
     * Destroys the PhysicsScene and all bodies.  The scheduler
     * will no longer insert physics tasks for this scene.
     */
    void disablePhysics();

    /**
     * @brief Check if physics is enabled for this scene.
     */
    bool hasPhysics() const { return m_physicsScene != nullptr; }

    /**
     * @brief Get the physics scene (nullptr if physics not enabled).
     */
    PhysicsScene* getPhysicsScene() { return m_physicsScene.get(); }
    const PhysicsScene* getPhysicsScene() const { return m_physicsScene.get(); }

    // Input

    /**
     * @brief Set the input handler for this scene.
     * @param handler Input handler (scene does NOT take ownership)
     */
    void setInputHandler(InputHandler* handler) { m_inputHandler = handler; }

    /**
     * @brief Get the input handler.
     * Returns the scene's input handler if set, otherwise falls back to the game's input handler.
     */
    InputHandler* getInputHandler();
    const InputHandler* getInputHandler() const;

    // Game reference

    /**
     * @brief Get the game this scene belongs to.
     */
    Game* getGame() { return m_game; }
    const Game* getGame() const { return m_game; }

    // Background color

    /**
     * @brief Set the background/clear color.
     */
    void setBackgroundColor(const Color& color) { m_backgroundColor = color; }

    /**
     * @brief Get the background color.
     */
    const Color& getBackgroundColor() const { return m_backgroundColor; }

    // World Bounds

    /**
     * @brief Set the world bounds for this scene.
     *
     * World bounds define the playable area of the scene using
     * explicit meter units and cardinal directions.
     *
     * @param bounds The 3D bounds of the game world
     *
     * @example
     * @code
     * // 200m x 200m x 30m world
     * scene->setWorldBounds(WorldBounds::fromDirectionalLimits(
     *     100_m, WorldBounds::south(100_m),  // north/south
     *     WorldBounds::west(100_m), 100_m,   // west/east
     *     20_m, WorldBounds::down(10_m)      // up/down
     * ));
     * @endcode
     */
    void setWorldBounds(const WorldBounds& bounds) { m_worldBounds = bounds; }

    /**
     * @brief Get the world bounds.
     */
    const WorldBounds& getWorldBounds() const { return m_worldBounds; }
    WorldBounds& getWorldBounds() { return m_worldBounds; }

    /**
     * @brief Check if the scene is 2D (no height dimension).
     */
    bool is2D() const { return m_worldBounds.is2D(); }

    // 2D Camera Bounds

    /**
     * @brief Set 2D camera bounds for pixel-to-world coordinate mapping.
     *
     * Use this for 2D games to define how the screen maps to the world
     * and to convert between screen and world coordinates.
     *
     * @param bounds The 2D camera bounds configuration
     *
     * @example
     * @code
     * CameraBounds2D camera;
     * camera.setScreenSize(1920_px, 1080_px);
     * camera.setWorldWidth(16_m);
     * camera.centerOn(0_m, 0_m);
     * scene->setCameraBounds2D(camera);
     * @endcode
     */
    void setCameraBounds2D(const CameraBounds2D& bounds) { m_cameraBounds2D = bounds; }

    /**
     * @brief Get the 2D camera bounds.
     */
    CameraBounds2D& getCameraBounds2D() { return m_cameraBounds2D; }
    const CameraBounds2D& getCameraBounds2D() const { return m_cameraBounds2D; }

  protected:
    std::string m_name;
    Game* m_game = nullptr;

    // Entities
    std::vector<Entity::Ref> m_entities;
    std::unordered_map<EntityId, size_t> m_entityIndex;

    // Resources
    struct ResourceEntry {
        std::shared_ptr<Resource> resource;
        std::type_index type;
    };
    std::unordered_map<ResourceId, ResourceEntry> m_resources;
    ResourceId m_nextResourceId = 1;

    // Scene settings
    std::unique_ptr<LightBox> m_lightBox;
    std::unique_ptr<GameCamera> m_camera;
    InputHandler* m_inputHandler = nullptr;
    Color m_backgroundColor = Color::black();
    bool m_continueInBackground = false;
    int m_updatePriority = 0;
    ViewportRect m_viewportRect = ViewportRect::fullWindow();

    // World bounds
    WorldBounds m_worldBounds;
    CameraBounds2D m_cameraBounds2D;

    // Phase callbacks
    bool m_usePhaseCallbacks = false;

    // Audio event queue
    std::vector<AudioEvent> m_audioEventQueue;

    // Physics
    std::unique_ptr<PhysicsScene> m_physicsScene;

    friend class Game;
};

// Template implementations

template <typename T>
ResourceId Scene::addResource(const std::string& path) {
    static_assert(std::is_base_of<Resource, T>::value, "T must derive from Resource");

    auto resource = std::make_shared<T>();
    resource->m_id = m_nextResourceId++;
    resource->m_path = path;

    // Load the resource (CPU-side only)
    if constexpr (std::is_same_v<T, Mesh>) {
        resource->loadFromFile(path);
    } else if constexpr (std::is_same_v<T, Texture>) {
        resource->loadFromFile(path);
    }
    // Add other resource type loading here

    m_resources[resource->m_id] = {resource, typeid(T)};
    return resource->m_id;
}

template <typename T>
ResourceId Scene::addResource(ResourcePtr<T> resource) {
    static_assert(std::is_base_of<Resource, T>::value, "T must derive from Resource");

    if (resource->m_id == INVALID_RESOURCE_ID) {
        resource->m_id = m_nextResourceId++;
    }

    m_resources[resource->m_id] = {resource, typeid(T)};
    return resource->m_id;
}

template <typename T>
T* Scene::getResource(ResourceId id) {
    auto it = m_resources.find(id);
    if (it != m_resources.end() && it->second.type == typeid(T)) {
        return static_cast<T*>(it->second.resource.get());
    }
    return nullptr;
}

template <typename T, typename... Args>
std::shared_ptr<T> Scene::addEntity(Args&&... args) {
    static_assert(std::is_base_of<Entity, T>::value, "T must derive from Entity");

    auto entity = std::make_shared<T>(std::forward<Args>(args)...);
    m_entityIndex[entity->getId()] = m_entities.size();
    m_entities.push_back(entity);
    entity->onAttach(this);
    return entity;
}

template <typename T>
std::vector<std::shared_ptr<T>> Scene::getEntitiesOfType() {
    std::vector<std::shared_ptr<T>> result;
    for (auto& entity : m_entities) {
        if (auto typed = std::dynamic_pointer_cast<T>(entity)) {
            result.push_back(typed);
        }
    }
    return result;
}

}  // namespace vde
