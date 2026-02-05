#pragma once

/**
 * @file Resource.h
 * @brief Resource management for VDE games
 *
 * Provides base classes and utilities for managing game resources
 * such as textures, meshes, sounds, and other assets.
 */

#include <memory>
#include <string>
#include <typeindex>

#include "GameTypes.h"

namespace vde {

/**
 * @brief Base class for all game resources.
 *
 * Resources are assets that can be loaded and managed by scenes,
 * including textures, meshes, audio clips, etc.
 */
class Resource {
  public:
    virtual ~Resource() = default;

    /**
     * @brief Get the unique ID of this resource.
     */
    ResourceId getId() const { return m_id; }

    /**
     * @brief Get the path this resource was loaded from.
     */
    const std::string& getPath() const { return m_path; }

    /**
     * @brief Check if the resource is loaded and ready to use.
     */
    virtual bool isLoaded() const { return m_loaded; }

    /**
     * @brief Get the type name of this resource (for debugging).
     */
    virtual const char* getTypeName() const = 0;

  protected:
    Resource() = default;
    Resource(ResourceId id, const std::string& path) : m_id(id), m_path(path) {}

    ResourceId m_id = INVALID_RESOURCE_ID;
    std::string m_path;
    bool m_loaded = false;

    friend class Scene;
    friend class ResourceManager;
};

/**
 * @brief Smart pointer type for resources.
 */
template <typename T>
using ResourcePtr = std::shared_ptr<T>;

/**
 * @brief Handle to a resource that can be used without knowing the full type.
 */
class ResourceHandle {
  public:
    ResourceHandle() = default;
    ResourceHandle(ResourceId id, std::type_index type) : m_id(id), m_type(type) {}

    ResourceId getId() const { return m_id; }
    std::type_index getType() const { return m_type; }
    bool isValid() const { return m_id != INVALID_RESOURCE_ID; }

    bool operator==(const ResourceHandle& other) const {
        return m_id == other.m_id && m_type == other.m_type;
    }

    bool operator!=(const ResourceHandle& other) const { return !(*this == other); }

  private:
    ResourceId m_id = INVALID_RESOURCE_ID;
    std::type_index m_type = typeid(void);
};

}  // namespace vde
