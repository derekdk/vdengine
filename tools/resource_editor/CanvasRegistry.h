#pragma once

/**
 * @file CanvasRegistry.h
 * @brief Registry that owns and manages all open canvases.
 *
 * Each canvas pairs an ImageDocument (CPU pixel data) with an optional
 * GPU texture and ImGui descriptor set for viewport rendering.
 */

#include <vulkan/vulkan.h>

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ImageDocument.h"
#include "commands/CommandTypes.h"

namespace vde {
class Texture;
}

namespace vde::tools {

/**
 * @brief A single canvas: pixel document + GPU texture + view state.
 */
struct Canvas {
    uint32_t id = 0;                                                  ///< Unique canvas ID.
    std::string name;                                                 ///< Human-readable name.
    std::unique_ptr<ImageDocument> document;                          ///< Owned pixel data.
    std::map<std::string, std::unique_ptr<ImageDocument>> resources;  ///< Named sub-resources.
    std::vector<std::string> operationHistory;                        ///< Log of commands applied.
    std::shared_ptr<vde::Texture> gpuTexture;         ///< GPU-side texture (may be null).
    VkDescriptorSet imguiTextureId = VK_NULL_HANDLE;  ///< ImGui descriptor for viewport.
    uint64_t lastUploadedGeneration = 0;              ///< Generation at last GPU upload.
    float zoomLevel = 8.0f;                           ///< Viewport zoom (pixel-art default).
    float panX = 0.0f;                                ///< Viewport horizontal pan.
    float panY = 0.0f;                                ///< Viewport vertical pan.
};

/**
 * @brief Owns all Canvas instances and provides lookup by ID or name.
 */
class CanvasRegistry {
  public:
    /**
     * @brief Create a new canvas with the given name and document.
     * @param name Unique human-readable name.
     * @param doc Owning pointer to the pixel document.
     * @return Pointer to the newly created Canvas.
     */
    Canvas* create(const std::string& name, std::unique_ptr<ImageDocument> doc);

    /**
     * @brief Look up a canvas by its numeric ID.
     * @param id Canvas ID.
     * @return Pointer to the canvas, or nullptr if not found.
     */
    Canvas* getById(uint32_t id);

    /**
     * @brief Look up a canvas by its name.
     * @param name Canvas name.
     * @return Pointer to the canvas, or nullptr if not found.
     */
    Canvas* getByName(const std::string& name);

    /**
     * @brief Resolve a string that may be a name or a numeric ID.
     * @param nameOrId Name or decimal ID string.
     * @return Pointer to the canvas, or nullptr if not found.
     */
    Canvas* resolve(const std::string& nameOrId);

    /**
     * @brief Remove a canvas by ID.
     * @param id Canvas ID to remove.
     * @return true if the canvas was found and removed.
     */
    bool remove(uint32_t id);

    /**
     * @brief Rename a canvas.
     * @param id Canvas ID.
     * @param newName New unique name.
     * @return true on success, false if ID not found or name already taken.
     */
    bool rename(uint32_t id, const std::string& newName);

    /** @brief Check whether a canvas with the given ID exists. */
    bool has(uint32_t id) const;

    /** @brief Check whether a canvas with the given name exists. */
    bool hasName(const std::string& name) const;

    /** @brief Return a sorted vector of all canvas IDs. */
    std::vector<uint32_t> getIds() const;

    /** @brief Number of canvases currently registered. */
    size_t count() const;

    /**
     * @brief Generate a name guaranteed not to collide with existing canvases.
     * @param base Desired base name (default "untitled").
     * @return Unique name, e.g. "untitled", "untitled_2", etc.
     */
    std::string generateUniqueName(const std::string& base = "untitled");

    /**
     * @brief Reference to a canvas and one of its sub-resource images.
     */
    struct ResourceRef {
        Canvas* canvas = nullptr;  ///< Owning canvas.
        ImageDocument* image =
            nullptr;  ///< Resolved image (may be the main doc or a sub-resource).
    };

    /**
     * @brief Resolve a "canvas::resource" reference.
     * @param ref String of the form "canvasName::imageName" or just "imageName".
     * @param activeCanvasId Fallback canvas when no prefix is given.
     * @return Resolved reference (both pointers may be nullptr).
     */
    ResourceRef resolveResource(const std::string& ref, uint32_t activeCanvasId);

  private:
    std::map<uint32_t, std::unique_ptr<Canvas>> m_canvases;  ///< ID -> Canvas.
    std::map<std::string, uint32_t> m_nameIndex;             ///< Name -> ID.
    uint32_t m_nextId = 1;                                   ///< Next auto-assigned ID.
    int m_nextUntitledIndex = 1;  ///< Counter for unique name generation.
};

}  // namespace vde::tools
