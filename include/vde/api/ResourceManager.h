#pragma once

/**
 * @file ResourceManager.h
 * @brief Global resource management and caching for VDE
 *
 * Provides centralized resource loading, caching, and lifetime management
 * to avoid duplicate loads and enable resource sharing across scenes.
 */

#include <memory>
#include <string>
#include <typeindex>
#include <unordered_map>

#include "Resource.h"

namespace vde {

// Forward declarations
class Mesh;
class Texture;

/**
 * @brief Global resource manager for caching and sharing resources.
 *
 * ResourceManager provides centralized management of game resources with:
 * - Automatic deduplication (same path = same resource instance)
 * - Weak reference caching (resources auto-delete when unused)
 * - Type-safe resource access with compile-time checks
 * - Statistics and debugging support
 *
 * Resources are cached using weak_ptr, so they are automatically removed
 * from the cache when no longer referenced by any scene or entity.
 *
 * @example
 * @code
 * ResourceManager manager;
 *
 * // Load a texture (or get cached instance)
 * auto texture = manager.load<Texture>("assets/player.png");
 *
 * // Load the same texture again - returns the same instance
 * auto texture2 = manager.load<Texture>("assets/player.png");
 * assert(texture == texture2); // Same shared_ptr
 *
 * // Check if a resource is cached
 * if (manager.has("assets/enemy.obj")) {
 *     auto mesh = manager.get<Mesh>("assets/enemy.obj");
 * }
 * @endcode
 */
class ResourceManager {
  public:
    ResourceManager() = default;
    ~ResourceManager() = default;

    // Prevent copying
    ResourceManager(const ResourceManager&) = delete;
    ResourceManager& operator=(const ResourceManager&) = delete;

    /**
     * @brief Load or get a cached resource.
     *
     * If the resource is already cached, returns the cached instance.
     * Otherwise, creates a new resource and caches it.
     *
     * @tparam T Resource type (Mesh, Texture, etc.)
     * @param path Path to the resource file
     * @return Shared pointer to the resource, or nullptr if loading failed
     *
     * @note The resource is loaded on the CPU side only. Call uploadToGPU()
     *       separately for resources that need GPU uploads (Mesh, Texture).
     */
    template <typename T>
    ResourcePtr<T> load(const std::string& path);

    /**
     * @brief Add a pre-created resource to the cache.
     *
     * Useful for procedurally generated resources or resources created
     * from non-file sources.
     *
     * @tparam T Resource type
     * @param key Unique key for the resource (usually a path)
     * @param resource The resource to cache
     * @return Shared pointer to the resource
     */
    template <typename T>
    ResourcePtr<T> add(const std::string& key, ResourcePtr<T> resource);

    /**
     * @brief Get a cached resource by path.
     *
     * @tparam T Resource type
     * @param path Path to the resource
     * @return Shared pointer to the resource, or nullptr if not cached or wrong type
     */
    template <typename T>
    ResourcePtr<T> get(const std::string& path);

    /**
     * @brief Check if a resource is currently cached.
     *
     * @param path Path to the resource
     * @return true if the resource is in the cache and still alive
     */
    bool has(const std::string& path) const;

    /**
     * @brief Remove a resource from the cache.
     *
     * This doesn't destroy the resource if it's still referenced elsewhere,
     * but it will no longer be returned by load() or get().
     *
     * @param path Path to the resource
     */
    void remove(const std::string& path);

    /**
     * @brief Clear all cached resources.
     *
     * Removes all resources from the cache. Resources that are still
     * referenced will remain alive but won't be cached.
     */
    void clear();

    /**
     * @brief Get the number of currently cached resources.
     *
     * This counts only resources that haven't been destroyed.
     *
     * @return Number of alive cached resources
     */
    size_t getCachedCount() const;

    /**
     * @brief Estimate memory usage of cached resources (CPU-side only).
     *
     * This is an approximate count based on stored resource data.
     * Does not include GPU memory usage.
     *
     * @return Estimated bytes used by cached resources
     */
    size_t getMemoryUsage() const;

    /**
     * @brief Remove expired weak pointers from the cache.
     *
     * Called automatically by load() and get(). Can be called explicitly
     * to clean up the cache after resources are released.
     */
    void pruneExpired();

  private:
    struct CacheEntry {
        std::weak_ptr<Resource> resource;
        std::type_index type;
        size_t lastAccessTime;
        size_t estimatedSize;  // Approximate memory usage in bytes

        // Default constructor for std::unordered_map
        CacheEntry() : type(typeid(void)), lastAccessTime(0), estimatedSize(0) {}

        CacheEntry(std::weak_ptr<Resource> res, std::type_index t, size_t time = 0, size_t size = 0)
            : resource(res), type(t), lastAccessTime(time), estimatedSize(size) {}
    };

    std::unordered_map<std::string, CacheEntry> m_cache;
    size_t m_accessCounter = 0;

    /**
     * @brief Estimate memory usage of a resource.
     */
    template <typename T>
    size_t estimateResourceSize(const ResourcePtr<T>& resource) const;
};

// Template implementations

template <typename T>
ResourcePtr<T> ResourceManager::load(const std::string& path) {
    static_assert(std::is_base_of<Resource, T>::value, "T must derive from Resource");

    // Check if already cached
    auto existing = get<T>(path);
    if (existing) {
        return existing;
    }

    // Create new resource
    auto resource = std::make_shared<T>();

    // Load from file
    if (!resource->loadFromFile(path)) {
        // Loading failed
        return nullptr;
    }

    // Cache it
    m_cache[path] =
        CacheEntry(resource, typeid(T), m_accessCounter++, estimateResourceSize(resource));

    return resource;
}

template <typename T>
ResourcePtr<T> ResourceManager::add(const std::string& key, ResourcePtr<T> resource) {
    static_assert(std::is_base_of<Resource, T>::value, "T must derive from Resource");

    if (!resource) {
        return nullptr;
    }

    m_cache[key] =
        CacheEntry(resource, typeid(T), m_accessCounter++, estimateResourceSize(resource));

    return resource;
}

template <typename T>
ResourcePtr<T> ResourceManager::get(const std::string& path) {
    static_assert(std::is_base_of<Resource, T>::value, "T must derive from Resource");

    auto it = m_cache.find(path);
    if (it != m_cache.end()) {
        // Check type matches
        if (it->second.type != typeid(T)) {
            return nullptr;
        }

        // Try to lock weak_ptr
        auto resource = it->second.resource.lock();
        if (resource) {
            // Update access time
            it->second.lastAccessTime = m_accessCounter++;
            return std::static_pointer_cast<T>(resource);
        } else {
            // Resource expired, remove from cache
            m_cache.erase(it);
        }
    }

    return nullptr;
}

template <typename T>
size_t ResourceManager::estimateResourceSize(const ResourcePtr<T>& resource) const {
    if (!resource) {
        return 0;
    }

    // Rough estimates (actual size depends on content)
    // These are placeholder values for basic accounting
    if constexpr (std::is_same_v<T, Mesh>) {
        // Assume average mesh is ~100KB
        return 100 * 1024;
    } else if constexpr (std::is_same_v<T, Texture>) {
        // Estimate based on dimensions if available
        auto texture = std::static_pointer_cast<Texture>(resource);
        return static_cast<size_t>(texture->getWidth()) * texture->getHeight() * 4;
    }

    // Default estimate
    return 1024;
}

}  // namespace vde
