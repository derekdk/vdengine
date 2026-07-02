#pragma once

/**
 * @file TileMapImport.h
 * @brief Import helpers for Tiled-authored tilemaps.
 */

#include <vde/Texture.h>
#include <vde/api/TileMap.h>

#include <glm/vec2.hpp>

#include <memory>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace vde {

class VulkanContext;

/**
 * @brief Supported typed property value imported from Tiled metadata.
 */
using TileMapImportPropertyValue = std::variant<bool, int, float, std::string>;

/**
 * @brief Object-layer record imported from a Tiled map.
 *
 * Positions are converted into TileMap local/world units using a bottom-left
 * origin so they can be used directly alongside TileMap collision output.
 */
struct ImportedTileObject {
    int id = 0;
    std::string name;
    std::string type;
    std::string layerName;
    glm::vec2 position{0.0f};
    glm::vec2 size{0.0f};
    bool point = false;
    bool visible = true;
    float rotationDegrees = 0.0f;
    TileCollisionKind collisionKind = TileCollisionKind::None;
    std::unordered_map<std::string, TileMapImportPropertyValue> properties;
};

/**
 * @brief Result of importing a Tiled map.
 */
struct ImportedTileMap {
    std::shared_ptr<TileMap> tileMap;
    std::vector<ImportedTileObject> objects;
};

/**
 * @brief Import options for Tiled -> TileMap conversion.
 */
struct TileMapImportOptions {
    float tileWidth = 1.0f;
    float tileHeight = 1.0f;
    float layerDepthStep = 0.05f;
    bool importObjectLayers = true;
};

/**
 * @brief Import a documented subset of Tiled JSON into the runtime TileMap API.
 *
 * Supported subset:
 * - finite orthogonal maps
 * - inline integer tile-layer data arrays
 * - one embedded image tileset
 * - optional object layers with point or axis-aligned rectangle objects
 * - bool/int/float/string custom properties
 *
 * Unsupported features throw actionable exceptions instead of silently falling
 * back, including infinite maps, external tilesets, layer encoding/compression,
 * multiple tilesets, and flipped or rotated tile GIDs.
 */
class TileMapImport {
  public:
    /**
     * @brief Import Tiled JSON metadata using a caller-provided tileset texture.
     * @param texture Texture that matches the embedded tileset metadata.
     * @param jsonText Tiled JSON text.
     * @param options Runtime import options.
     * @return Imported TileMap plus optional object-layer records.
     */
    static ImportedTileMap importTiledJson(const std::shared_ptr<Texture>& texture,
                                           const std::string& jsonText,
                                           const TileMapImportOptions& options = {});

    /**
     * @brief Import a Tiled JSON file and load its referenced tileset image.
     * @param context Optional Vulkan context used to upload the loaded texture.
     * @param jsonPath Path to the Tiled JSON file.
     * @param options Runtime import options.
     * @return Imported TileMap plus optional object-layer records.
     */
    static ImportedTileMap importTiledJsonFile(VulkanContext* context, const std::string& jsonPath,
                                               const TileMapImportOptions& options = {});
};

}  // namespace vde