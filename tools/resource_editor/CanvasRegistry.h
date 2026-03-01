/**
 * @file CanvasRegistry.h
 * @brief Multi-document container with ID/name lookup for canvas management.
 *
 * CanvasRegistry manages multiple ImageDocument instances, each associated
 * with a unique numeric ID and string name. It supports ID-based and
 * name-based lookup, and holds per-canvas GPU texture and view state.
 */

#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ImageDocument.h"

namespace vde {

// Forward declarations
class Texture;

namespace tools {

/**
 * @brief Per-canvas state including document, GPU texture, and view parameters.
 *
 * Each canvas also holds a collection of named image resources (loaded or
 * composited). Resources are accessed within the owning canvas by bare name,
 * or from other canvases via the `::` double-colon accessor
 * (e.g., `hero::face_img`).
 *
 * An operation history records the ordered sequence of commands that produced
 * the canvas's current pixel state, enabling deterministic recreation and
 * DSL export.
 */
struct Canvas {
    uint32_t id = 0;
    std::string name;
    std::unique_ptr<ImageDocument> document;

    /// Named image resources (loaded or composited). Key = resource name.
    std::map<std::string, std::unique_ptr<ImageDocument>> resources;

    /// Ordered commands that produced current state (for deterministic recreation).
    std::vector<std::string> operationHistory;

    std::shared_ptr<vde::Texture> gpuTexture;         ///< nullptr until scene creates
    VkDescriptorSet imguiTextureId = VK_NULL_HANDLE;  ///< nullptr until scene creates
    uint64_t lastUploadedGeneration = 0;
    float zoomLevel = 1.0f;
    float panX = 0.0f;
    float panY = 0.0f;
};

/**
 * @brief Multi-document registry with ID/name lookup.
 *
 * Manages the lifetime and lookup of Canvas objects. Each canvas
 * has a unique monotonically-increasing ID and a unique name.
 */
class CanvasRegistry {
  public:
    CanvasRegistry() = default;
    ~CanvasRegistry() = default;

    // Non-copyable
    CanvasRegistry(const CanvasRegistry&) = delete;
    CanvasRegistry& operator=(const CanvasRegistry&) = delete;

    /**
     * @brief Create a new canvas with the given name and document.
     * @param name Unique canvas name
     * @param document The ImageDocument to own
     * @return Pointer to the created Canvas, or nullptr if name is duplicate
     */
    Canvas* create(const std::string& name, std::unique_ptr<ImageDocument> document);

    /**
     * @brief Get canvas by numeric ID.
     * @return Canvas pointer or nullptr if not found
     */
    Canvas* getById(uint32_t id);

    /**
     * @brief Get canvas by name.
     * @return Canvas pointer or nullptr if not found
     */
    Canvas* getByName(const std::string& name);

    /**
     * @brief Resolve a string that could be either a numeric ID or a name.
     * @param nameOrId String to resolve (tries numeric parse first, then name)
     * @return Canvas pointer or nullptr if not found
     */
    Canvas* resolve(const std::string& nameOrId);

    /**
     * @brief Remove a canvas by ID.
     * @return true if canvas was found and removed
     */
    bool remove(uint32_t id);

    /**
     * @brief Check if a canvas with the given ID exists.
     */
    bool has(uint32_t id) const;

    /**
     * @brief Check if a canvas with the given name exists.
     */
    bool hasName(const std::string& name) const;

    /**
     * @brief Get ordered list of all canvas IDs.
     */
    std::vector<uint32_t> getIds() const;

    /**
     * @brief Get number of canvases.
     */
    size_t count() const { return m_canvases.size(); }

    /**
     * @brief Result of resolving a `canvasname::imagename` or bare `imagename` reference.
     */
    struct ResourceRef {
        Canvas* canvas = nullptr;
        ImageDocument* image = nullptr;
    };

    /**
     * @brief Resolve a resource reference.
     *
     * Accepts bare "imagename" (searches active canvas first) or
     * "canvasname::imagename" (explicit canvas target).
     *
     * @param ref       The resource reference string
     * @param activeCanvasId  ID of the currently active canvas (for bare name fallback)
     * @return ResourceRef with canvas and image pointers, or nulls if not found
     */
    ResourceRef resolveResource(const std::string& ref, uint32_t activeCanvasId);

    /**
     * @brief Generate a unique name based on the given base string.
     * @param base Base name (default: "untitled")
     * @return Unique name like "untitled_1", "untitled_2", etc.
     */
    std::string generateUniqueName(const std::string& base = "untitled");

  private:
    uint32_t m_nextId = 1;
    std::map<uint32_t, Canvas> m_canvases;        ///< ID → Canvas
    std::map<std::string, uint32_t> m_nameIndex;  ///< name → ID
};

}  // namespace tools
}  // namespace vde
