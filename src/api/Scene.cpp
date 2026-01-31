/**
 * @file Scene.cpp
 * @brief Implementation of Scene class
 */

#include <vde/api/Scene.h>
#include <vde/api/Game.h>
#include <algorithm>
#include <stdexcept>

namespace vde {

// Default lighting instance (used when no LightBox is set)
static SimpleColorLightBox s_defaultLightBox(Color::white());

// ============================================================================
// Scene Implementation
// ============================================================================

Scene::Scene()
    : m_name("")
    , m_game(nullptr)
    , m_lightBox(nullptr)
    , m_camera(nullptr)
    , m_inputHandler(nullptr)
    , m_backgroundColor(Color::black())
    , m_nextResourceId(1)
{
}

Scene::~Scene() {
    // Clear all entities, calling their onDetach
    clearEntities();
}

void Scene::update(float deltaTime) {
    // Update all entities
    for (auto& entity : m_entities) {
        if (entity) {
            entity->update(deltaTime);
        }
    }
}

void Scene::render() {
    // Render all visible entities
    for (auto& entity : m_entities) {
        if (entity && entity->isVisible()) {
            entity->render();
        }
    }
}

EntityId Scene::addEntity(Entity::Ref entity) {
    if (!entity) {
        return INVALID_ENTITY_ID;
    }
    
    EntityId id = entity->getId();
    
    // Store the index for quick lookup
    m_entityIndex[id] = m_entities.size();
    m_entities.push_back(entity);
    
    // Notify entity it's been added
    entity->onAttach(this);
    
    return id;
}

Entity* Scene::getEntity(EntityId id) {
    auto it = m_entityIndex.find(id);
    if (it != m_entityIndex.end() && it->second < m_entities.size()) {
        return m_entities[it->second].get();
    }
    return nullptr;
}

Entity* Scene::getEntityByName(const std::string& name) {
    for (auto& entity : m_entities) {
        if (entity && entity->getName() == name) {
            return entity.get();
        }
    }
    return nullptr;
}

void Scene::removeEntity(EntityId id) {
    auto it = m_entityIndex.find(id);
    if (it == m_entityIndex.end()) {
        return;
    }
    
    size_t index = it->second;
    if (index >= m_entities.size()) {
        return;
    }
    
    // Notify entity before removal
    if (m_entities[index]) {
        m_entities[index]->onDetach();
    }
    
    // Swap with last element and pop (O(1) removal)
    if (index < m_entities.size() - 1) {
        // Update index of swapped entity
        EntityId swappedId = m_entities.back()->getId();
        m_entityIndex[swappedId] = index;
        std::swap(m_entities[index], m_entities.back());
    }
    
    m_entities.pop_back();
    m_entityIndex.erase(it);
}

void Scene::clearEntities() {
    // Notify all entities
    for (auto& entity : m_entities) {
        if (entity) {
            entity->onDetach();
        }
    }
    
    m_entities.clear();
    m_entityIndex.clear();
}

void Scene::setLightBox(std::unique_ptr<LightBox> lightBox) {
    m_lightBox = std::move(lightBox);
}

void Scene::setLightBox(LightBox* lightBox) {
    m_lightBox.reset(lightBox);
}

const LightBox& Scene::getEffectiveLighting() const {
    if (m_lightBox) {
        return *m_lightBox;
    }
    return s_defaultLightBox;
}

void Scene::setCamera(std::unique_ptr<GameCamera> camera) {
    m_camera = std::move(camera);
}

void Scene::setCamera(GameCamera* camera) {
    m_camera.reset(camera);
}

void Scene::removeResource(ResourceId id) {
    m_resources.erase(id);
}

} // namespace vde
