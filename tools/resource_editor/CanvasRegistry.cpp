/**
 * @file CanvasRegistry.cpp
 * @brief Implementation of canvas ownership and lookup.
 */

#include "CanvasRegistry.h"

#include <algorithm>
#include <sstream>

namespace vde {
namespace tools {

// =============================================================================
// Creation / removal
// =============================================================================

Canvas* CanvasRegistry::create(const std::string& name, std::unique_ptr<ImageDocument> doc) {
    auto canvas = std::make_unique<Canvas>();
    canvas->id = m_nextId++;
    canvas->name = name;
    canvas->document = std::move(doc);

    Canvas* ptr = canvas.get();
    m_nameIndex[name] = ptr->id;
    m_canvases[ptr->id] = std::move(canvas);
    return ptr;
}

bool CanvasRegistry::remove(uint32_t id) {
    auto it = m_canvases.find(id);
    if (it == m_canvases.end()) {
        return false;
    }
    m_nameIndex.erase(it->second->name);
    m_canvases.erase(it);
    return true;
}

bool CanvasRegistry::rename(uint32_t id, const std::string& newName) {
    auto it = m_canvases.find(id);
    if (it == m_canvases.end()) {
        return false;
    }
    // Reject if the new name is already in use by a different canvas
    auto nameIt = m_nameIndex.find(newName);
    if (nameIt != m_nameIndex.end() && nameIt->second != id) {
        return false;
    }

    // Remove old name entry and insert new one
    m_nameIndex.erase(it->second->name);
    it->second->name = newName;
    m_nameIndex[newName] = id;
    return true;
}

// =============================================================================
// Lookup
// =============================================================================

Canvas* CanvasRegistry::getById(uint32_t id) {
    auto it = m_canvases.find(id);
    return (it != m_canvases.end()) ? it->second.get() : nullptr;
}

Canvas* CanvasRegistry::getByName(const std::string& name) {
    auto it = m_nameIndex.find(name);
    if (it == m_nameIndex.end()) {
        return nullptr;
    }
    return getById(it->second);
}

Canvas* CanvasRegistry::resolve(const std::string& nameOrId) {
    // Try as name first
    Canvas* byName = getByName(nameOrId);
    if (byName) {
        return byName;
    }

    // Try to parse as integer ID
    try {
        size_t pos = 0;
        unsigned long val = std::stoul(nameOrId, &pos);
        if (pos == nameOrId.size()) {
            return getById(static_cast<uint32_t>(val));
        }
    } catch (...) {
        // Not a number — fall through
    }

    return nullptr;
}

bool CanvasRegistry::has(uint32_t id) const {
    return m_canvases.find(id) != m_canvases.end();
}

bool CanvasRegistry::hasName(const std::string& name) const {
    return m_nameIndex.find(name) != m_nameIndex.end();
}

std::vector<uint32_t> CanvasRegistry::getIds() const {
    std::vector<uint32_t> ids;
    ids.reserve(m_canvases.size());
    for (const auto& [id, _] : m_canvases) {
        ids.push_back(id);
    }
    // std::map already iterates in sorted order, but be explicit
    std::sort(ids.begin(), ids.end());
    return ids;
}

size_t CanvasRegistry::count() const {
    return m_canvases.size();
}

std::string CanvasRegistry::generateUniqueName(const std::string& base) {
    // First try the base name itself
    if (!hasName(base)) {
        return base;
    }

    // Then try base_2, base_3, ...
    for (int i = m_nextUntitledIndex + 1;; ++i) {
        std::string candidate = base + "_" + std::to_string(i);
        if (!hasName(candidate)) {
            m_nextUntitledIndex = i;
            return candidate;
        }
    }
}

// =============================================================================
// Resource resolution
// =============================================================================

CanvasRegistry::ResourceRef CanvasRegistry::resolveResource(const std::string& ref,
                                                            uint32_t activeCanvasId) {
    ResourceRef result;

    // Check for "canvasName::resourceName" pattern
    auto sep = ref.find("::");
    if (sep != std::string::npos) {
        std::string canvasName = ref.substr(0, sep);
        std::string resourceName = ref.substr(sep + 2);

        Canvas* canvas = resolve(canvasName);
        if (!canvas) {
            return result;
        }
        result.canvas = canvas;

        auto resIt = canvas->resources.find(resourceName);
        if (resIt != canvas->resources.end()) {
            result.image = resIt->second.get();
        }
        return result;
    }

    // No "::" — look up in the active canvas's resources
    Canvas* active = getById(activeCanvasId);
    if (!active) {
        return result;
    }
    result.canvas = active;

    auto resIt = active->resources.find(ref);
    if (resIt != active->resources.end()) {
        result.image = resIt->second.get();
    }
    return result;
}

}  // namespace tools
}  // namespace vde
