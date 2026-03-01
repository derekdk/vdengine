/**
 * @file CanvasRegistry.cpp
 * @brief Implementation of CanvasRegistry multi-document container.
 */

#include "CanvasRegistry.h"

#include <charconv>

namespace vde {
namespace tools {

Canvas* CanvasRegistry::create(const std::string& name, std::unique_ptr<ImageDocument> document) {
    // Reject duplicate names
    if (m_nameIndex.find(name) != m_nameIndex.end()) {
        return nullptr;
    }

    uint32_t id = m_nextId++;

    Canvas canvas;
    canvas.id = id;
    canvas.name = name;
    canvas.document = std::move(document);

    auto [it, inserted] = m_canvases.emplace(id, std::move(canvas));
    if (!inserted) {
        return nullptr;
    }

    m_nameIndex[name] = id;
    return &it->second;
}

Canvas* CanvasRegistry::getById(uint32_t id) {
    auto it = m_canvases.find(id);
    if (it == m_canvases.end())
        return nullptr;
    return &it->second;
}

Canvas* CanvasRegistry::getByName(const std::string& name) {
    auto nameIt = m_nameIndex.find(name);
    if (nameIt == m_nameIndex.end())
        return nullptr;
    return getById(nameIt->second);
}

Canvas* CanvasRegistry::resolve(const std::string& nameOrId) {
    // Try numeric parse first
    uint32_t id = 0;
    auto result = std::from_chars(nameOrId.data(), nameOrId.data() + nameOrId.size(), id);
    if (result.ec == std::errc{} && result.ptr == nameOrId.data() + nameOrId.size()) {
        Canvas* canvas = getById(id);
        if (canvas)
            return canvas;
    }

    // Fall back to name lookup
    return getByName(nameOrId);
}

bool CanvasRegistry::remove(uint32_t id) {
    auto it = m_canvases.find(id);
    if (it == m_canvases.end())
        return false;

    m_nameIndex.erase(it->second.name);
    m_canvases.erase(it);
    return true;
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
    for (const auto& [id, canvas] : m_canvases) {
        ids.push_back(id);
    }
    return ids;
}

std::string CanvasRegistry::generateUniqueName(const std::string& base) {
    uint32_t suffix = 1;
    while (true) {
        std::string candidate = base + "_" + std::to_string(suffix);
        if (!hasName(candidate)) {
            return candidate;
        }
        ++suffix;
    }
}

}  // namespace tools
}  // namespace vde
