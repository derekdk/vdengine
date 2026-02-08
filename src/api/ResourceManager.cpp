/**
 * @file ResourceManager.cpp
 * @brief Implementation of ResourceManager class
 */

#include <vde/api/ResourceManager.h>

namespace vde {

bool ResourceManager::has(const std::string& path) const {
    auto it = m_cache.find(path);
    if (it != m_cache.end()) {
        // Check if still alive
        return !it->second.resource.expired();
    }
    return false;
}

void ResourceManager::remove(const std::string& path) {
    m_cache.erase(path);
}

void ResourceManager::clear() {
    m_cache.clear();
    m_accessCounter = 0;
}

size_t ResourceManager::getCachedCount() const {
    size_t count = 0;
    for (const auto& [path, entry] : m_cache) {
        if (!entry.resource.expired()) {
            ++count;
        }
    }
    return count;
}

size_t ResourceManager::getMemoryUsage() const {
    size_t totalSize = 0;
    for (const auto& [path, entry] : m_cache) {
        if (!entry.resource.expired()) {
            totalSize += entry.estimatedSize;
        }
    }
    return totalSize;
}

void ResourceManager::pruneExpired() {
    // Remove all expired entries
    for (auto it = m_cache.begin(); it != m_cache.end();) {
        if (it->second.resource.expired()) {
            it = m_cache.erase(it);
        } else {
            ++it;
        }
    }
}

}  // namespace vde
