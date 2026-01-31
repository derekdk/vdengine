#pragma once

/**
 * @file Scene.h
 * @brief Scene management for VDE games
 * 
 * Provides the Scene class for managing game states, entities,
 * resources, and rendering for a portion of the game.
 */

#include "GameTypes.h"
#include "Entity.h"
#include "Resource.h"
#include "LightBox.h"
#include "GameCamera.h"
#include "InputHandler.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <typeindex>

namespace vde {

// Forward declarations
class Game;
class Mesh;
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
    template<typename T>
    ResourceId addResource(const std::string& path);

    /**
     * @brief Add a pre-created resource to this scene.
     * @tparam T Resource type
     * @param resource The resource to add
     * @return Resource ID
     */
    template<typename T>
    ResourceId addResource(ResourcePtr<T> resource);

    /**
     * @brief Get a resource by ID.
     * @tparam T Resource type
     * @param id Resource ID
     * @return Pointer to resource, or nullptr if not found
     */
    template<typename T>
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
    template<typename T, typename... Args>
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
    template<typename T>
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
    void setLightBox(LightBox* lightBox); // Takes ownership

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
    void setCamera(GameCamera* camera); // Takes ownership

    /**
     * @brief Get the scene's camera.
     * @return The camera, or nullptr if using default.
     */
    GameCamera* getCamera() { return m_camera.get(); }
    const GameCamera* getCamera() const { return m_camera.get(); }

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

    friend class Game;
};

// Template implementations

template<typename T>
ResourceId Scene::addResource(const std::string& path) {
    static_assert(std::is_base_of<Resource, T>::value, "T must derive from Resource");
    
    auto resource = std::make_shared<T>();
    resource->m_id = m_nextResourceId++;
    resource->m_path = path;
    
    // Load the resource
    if constexpr (std::is_same_v<T, Mesh>) {
        resource->loadFromFile(path);
    }
    // Add other resource type loading here
    
    m_resources[resource->m_id] = {resource, typeid(T)};
    return resource->m_id;
}

template<typename T>
ResourceId Scene::addResource(ResourcePtr<T> resource) {
    static_assert(std::is_base_of<Resource, T>::value, "T must derive from Resource");
    
    if (resource->m_id == INVALID_RESOURCE_ID) {
        resource->m_id = m_nextResourceId++;
    }
    
    m_resources[resource->m_id] = {resource, typeid(T)};
    return resource->m_id;
}

template<typename T>
T* Scene::getResource(ResourceId id) {
    auto it = m_resources.find(id);
    if (it != m_resources.end() && it->second.type == typeid(T)) {
        return static_cast<T*>(it->second.resource.get());
    }
    return nullptr;
}

template<typename T, typename... Args>
std::shared_ptr<T> Scene::addEntity(Args&&... args) {
    static_assert(std::is_base_of<Entity, T>::value, "T must derive from Entity");
    
    auto entity = std::make_shared<T>(std::forward<Args>(args)...);
    m_entityIndex[entity->getId()] = m_entities.size();
    m_entities.push_back(entity);
    entity->onAttach(this);
    return entity;
}

template<typename T>
std::vector<std::shared_ptr<T>> Scene::getEntitiesOfType() {
    std::vector<std::shared_ptr<T>> result;
    for (auto& entity : m_entities) {
        if (auto typed = std::dynamic_pointer_cast<T>(entity)) {
            result.push_back(typed);
        }
    }
    return result;
}

} // namespace vde
