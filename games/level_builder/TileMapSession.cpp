#include "TileMapSession.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string_view>

#include <nlohmann/json.hpp>

namespace {

using OrderedJson = nlohmann::ordered_json;

constexpr float kTileSize = 1.0f;
constexpr float kDefaultLayerDepthStep = 0.06f;
constexpr glm::vec2 kDefaultSpawnPoint(4.5f, 7.0f);
constexpr const char* kImportedMapPath = "assets/tiled/tilemap_demo.tmj";
constexpr const char* kOverlayFileName = "level_builder_ground.overlay.json";
constexpr const char* kOverlayFormatId = "vde.level_builder.ground_overlay";
constexpr int kOverlayFormatVersionLegacy = 1;
constexpr int kOverlayFormatVersion = 2;

std::string readTextFile(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Failed to open overlay file: " + path.string());
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

void writeTextFile(const std::filesystem::path& path, const std::string& text) {
    const std::filesystem::path parentPath = path.parent_path();
    if (!parentPath.empty()) {
        std::error_code createError;
        std::filesystem::create_directories(parentPath, createError);
        if (createError) {
            throw std::runtime_error("Failed to create overlay directory: " + parentPath.string());
        }
    }

    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("Failed to open overlay for writing: " + path.string());
    }

    output << text;
    if (!output.good()) {
        throw std::runtime_error("Failed to write overlay file: " + path.string());
    }
}

std::string requireString(const OrderedJson& object, const char* key, std::string_view context) {
    if (!object.contains(key) || !object.at(key).is_string()) {
        throw std::invalid_argument(std::string("LevelBuilder overlay ") + std::string(context) +
                                    " missing string field: " + key);
    }

    return object.at(key).get<std::string>();
}

int requireInt(const OrderedJson& object, const char* key, std::string_view context) {
    if (!object.contains(key) || !object.at(key).is_number_integer()) {
        throw std::invalid_argument(std::string("LevelBuilder overlay ") + std::string(context) +
                                    " missing integer field: " + key);
    }

    return object.at(key).get<int>();
}

// Read a float field, returning defaultValue if the field is absent or non-numeric.
float optionalFloat(const OrderedJson& object, const char* key, float defaultValue) {
    if (!object.contains(key) || !object.at(key).is_number()) {
        return defaultValue;
    }
    return object.at(key).get<float>();
}

// Read a bool field, returning defaultValue if the field is absent or non-boolean.
bool optionalBool(const OrderedJson& object, const char* key, bool defaultValue) {
    if (!object.contains(key) || !object.at(key).is_boolean()) {
        return defaultValue;
    }
    return object.at(key).get<bool>();
}

std::vector<int> parseTilesArray(const OrderedJson& tilesJson, size_t expectedCount,
                                 std::string_view context) {
    if (!tilesJson.is_array()) {
        throw std::invalid_argument(std::string("LevelBuilder overlay ") + std::string(context) +
                                    " tiles must be an array");
    }
    if (tilesJson.size() != expectedCount) {
        throw std::invalid_argument(std::string("LevelBuilder overlay ") + std::string(context) +
                                    " tile count does not match the map");
    }

    std::vector<int> tiles;
    tiles.reserve(expectedCount);
    for (const auto& tileValue : tilesJson) {
        if (!tileValue.is_number_integer()) {
            throw std::invalid_argument(std::string("LevelBuilder overlay ") +
                                        std::string(context) + " tiles must be integers");
        }
        tiles.push_back(tileValue.get<int>());
    }
    return tiles;
}

void validateTileIds(const vde::TileMap& tileMap, const std::vector<int>& tiles,
                     std::string_view context) {
    const auto tileSet = tileMap.getTileSet();
    const int spriteCount = tileSet != nullptr ? tileSet->getSpriteCount() : 0;
    for (const int tileId : tiles) {
        if (tileId < vde::TileMap::kEmptyTile ||
            (tileId >= 0 && tileSet != nullptr && tileId >= spriteCount)) {
            throw std::invalid_argument(std::string("LevelBuilder overlay ") +
                                        std::string(context) + " contains invalid tile ID " +
                                        std::to_string(tileId));
        }
    }
}

std::vector<int> captureTileMapLayerTiles(const vde::TileMap& tileMap, int layerIndex);

levelbuilder::LayerDefinition buildImportedLayerDefinition(const vde::TileMap& tileMap,
                                                           int layerIndex) {
    levelbuilder::LayerDefinition layer;
    layer.id = "layer_" + std::to_string(layerIndex);
    layer.name = tileMap.getLayerInfo(layerIndex).name;
    if (layer.name.empty()) {
        layer.name = "Layer " + std::to_string(layerIndex);
    }
    layer.tiles = captureTileMapLayerTiles(tileMap, layerIndex);
    layer.depthZ = tileMap.getLayerDepth(layerIndex);
    layer.visible = tileMap.isLayerVisible(layerIndex);
    layer.collisionEnabled = layerIndex == 0 || !tileMap.extractCollisionRects(layerIndex).empty();
    return layer;
}

bool sameLayerMetadata(const levelbuilder::LayerDefinition& lhs,
                       const levelbuilder::LayerDefinition& rhs) {
    return lhs.id == rhs.id && lhs.name == rhs.name && lhs.depthZ == rhs.depthZ &&
           lhs.visible == rhs.visible && lhs.collisionEnabled == rhs.collisionEnabled &&
           lhs.followFactorX == rhs.followFactorX && lhs.followFactorY == rhs.followFactorY &&
           lhs.scrollVelocityX == rhs.scrollVelocityX &&
           lhs.scrollVelocityY == rhs.scrollVelocityY && lhs.scrollOffsetX == rhs.scrollOffsetX &&
           lhs.scrollOffsetY == rhs.scrollOffsetY;
}

using LayerAndTiles = std::pair<levelbuilder::LayerDefinition, std::vector<int>>;

// Parse a single layer entry from the v2 layers array.
LayerAndTiles parseLayerJsonV2(const OrderedJson& layerJson, const vde::TileMap& tileMap,
                               size_t layerIndex) {
    const std::string context = "layers[" + std::to_string(layerIndex) + "]";

    levelbuilder::LayerDefinition def;
    def.id = requireString(layerJson, "id", context);
    def.name = requireString(layerJson, "name", context);
    def.depthZ = optionalFloat(layerJson, "depth_z", 0.0f);
    def.visible = optionalBool(layerJson, "visible", true);
    def.collisionEnabled = optionalBool(layerJson, "collision_enabled", layerIndex == 0);
    def.followFactorX = optionalFloat(layerJson, "follow_factor_x", 1.0f);
    def.followFactorY = optionalFloat(layerJson, "follow_factor_y", 1.0f);
    def.scrollVelocityX = optionalFloat(layerJson, "scroll_velocity_x", 0.0f);
    def.scrollVelocityY = optionalFloat(layerJson, "scroll_velocity_y", 0.0f);
    def.scrollOffsetX = optionalFloat(layerJson, "scroll_offset_x", 0.0f);
    def.scrollOffsetY = optionalFloat(layerJson, "scroll_offset_y", 0.0f);

    if (requireInt(layerJson, "columns", context) != tileMap.getColumnCount() ||
        requireInt(layerJson, "rows", context) != tileMap.getRowCount()) {
        throw std::invalid_argument("LevelBuilder overlay layer " + std::to_string(layerIndex) +
                                    " dimensions do not match the map");
    }

    if (!layerJson.contains("tiles")) {
        throw std::invalid_argument("LevelBuilder overlay layer " + std::to_string(layerIndex) +
                                    " missing tiles array");
    }

    const size_t expectedTileCount =
        static_cast<size_t>(tileMap.getColumnCount()) * static_cast<size_t>(tileMap.getRowCount());
    std::vector<int> tiles = parseTilesArray(layerJson.at("tiles"), expectedTileCount, context);
    validateTileIds(tileMap, tiles, context);
    return {std::move(def), std::move(tiles)};
}

// Parse overlay JSON (v1 or v2) into a list of LayerDefinition + tile pairs.
std::vector<LayerAndTiles> parseOverlayLayers(const OrderedJson& root, const vde::TileMap& tileMap,
                                              const std::string& sourceMapId) {
    if (!root.is_object()) {
        throw std::invalid_argument("LevelBuilder overlay root must be an object");
    }

    if (requireString(root, "format", "root") != kOverlayFormatId) {
        throw std::invalid_argument("LevelBuilder overlay format is not supported");
    }

    if (requireString(root, "base_map", "root") != sourceMapId) {
        throw std::invalid_argument("LevelBuilder overlay targets a different source map");
    }

    const int version = requireInt(root, "version", "root");

    if (version == kOverlayFormatVersionLegacy) {
        // v1 backward compatibility: single editable_layer block.
        if (!root.contains("editable_layer") || !root.at("editable_layer").is_object()) {
            throw std::invalid_argument("LevelBuilder overlay missing editable_layer object");
        }

        const OrderedJson& editableLayer = root.at("editable_layer");
        const std::string layerName = requireString(editableLayer, "name", "editable_layer");

        const std::string expectedLayerName = tileMap.getLayerInfo(0).name;
        if (layerName != expectedLayerName) {
            throw std::invalid_argument("LevelBuilder overlay targets a different editable layer");
        }

        if (requireInt(editableLayer, "columns", "editable_layer") != tileMap.getColumnCount() ||
            requireInt(editableLayer, "rows", "editable_layer") != tileMap.getRowCount()) {
            throw std::invalid_argument("LevelBuilder overlay dimensions do not match the map");
        }

        if (!editableLayer.contains("tiles") || !editableLayer.at("tiles").is_array()) {
            throw std::invalid_argument("LevelBuilder overlay missing tiles array");
        }

        const size_t expectedTileCount = static_cast<size_t>(tileMap.getColumnCount()) *
                                         static_cast<size_t>(tileMap.getRowCount());
        std::vector<int> tiles =
            parseTilesArray(editableLayer.at("tiles"), expectedTileCount, "editable_layer");
        validateTileIds(tileMap, tiles, "editable_layer");

        levelbuilder::LayerDefinition def;
        def.id = "layer_0";
        def.name = layerName;
        def.depthZ = tileMap.getLayerDepth(0);
        def.visible = tileMap.isLayerVisible(0);
        def.collisionEnabled = true;
        std::vector<LayerAndTiles> layers;
        layers.push_back({std::move(def), std::move(tiles)});
        for (int layerIndex = 1; layerIndex < tileMap.getLayerCount(); ++layerIndex) {
            levelbuilder::LayerDefinition importedLayer =
                buildImportedLayerDefinition(tileMap, layerIndex);
            std::vector<int> importedTiles = importedLayer.tiles;
            layers.push_back({std::move(importedLayer), std::move(importedTiles)});
        }
        return layers;
    }

    if (version == kOverlayFormatVersion) {
        // v2: layers array.
        if (!root.contains("layers") || !root.at("layers").is_array()) {
            throw std::invalid_argument("LevelBuilder overlay missing layers array");
        }

        const OrderedJson& layersJson = root.at("layers");
        if (layersJson.empty()) {
            throw std::invalid_argument("LevelBuilder overlay layers array is empty");
        }

        std::vector<LayerAndTiles> result;
        result.reserve(layersJson.size());
        for (size_t i = 0; i < layersJson.size(); ++i) {
            if (!layersJson.at(i).is_object()) {
                throw std::invalid_argument("LevelBuilder overlay layer " + std::to_string(i) +
                                            " must be an object");
            }
            result.push_back(parseLayerJsonV2(layersJson.at(i), tileMap, i));
        }
        return result;
    }

    throw std::invalid_argument("LevelBuilder overlay version is not supported");
}

OrderedJson buildLayerJson(const levelbuilder::LayerDefinition& layerDef,
                           const std::vector<int>& tiles, int columns, int rows) {
    return OrderedJson{{"id", layerDef.id},
                       {"name", layerDef.name},
                       {"depth_z", layerDef.depthZ},
                       {"visible", layerDef.visible},
                       {"collision_enabled", layerDef.collisionEnabled},
                       {"follow_factor_x", layerDef.followFactorX},
                       {"follow_factor_y", layerDef.followFactorY},
                       {"scroll_velocity_x", layerDef.scrollVelocityX},
                       {"scroll_velocity_y", layerDef.scrollVelocityY},
                       {"scroll_offset_x", layerDef.scrollOffsetX},
                       {"scroll_offset_y", layerDef.scrollOffsetY},
                       {"columns", columns},
                       {"rows", rows},
                       {"tiles", tiles}};
}

OrderedJson buildOverlayJson(const vde::TileMap& tileMap,
                             const std::vector<levelbuilder::LayerDefinition>& layers,
                             const std::vector<std::vector<int>>& tileData,
                             const std::string& sourceMapId) {
    const int columns = tileMap.getColumnCount();
    const int rows = tileMap.getRowCount();

    OrderedJson layersJson = OrderedJson::array();
    for (size_t i = 0; i < layers.size() && i < tileData.size(); ++i) {
        layersJson.push_back(buildLayerJson(layers[i], tileData[i], columns, rows));
    }

    return OrderedJson{{"format", kOverlayFormatId},
                       {"version", kOverlayFormatVersion},
                       {"base_map", sourceMapId},
                       {"layers", std::move(layersJson)}};
}

void applyLayerStackToTileMap(vde::TileMap& tileMap,
                              const std::vector<levelbuilder::LayerDefinition>& layers,
                              const std::vector<std::vector<int>>& tileData) {
    const std::vector<int> emptyTiles(static_cast<size_t>(tileMap.getColumnCount()) *
                                          static_cast<size_t>(tileMap.getRowCount()),
                                      vde::TileMap::kEmptyTile);
    for (int layerIndex = 0; layerIndex < tileMap.getLayerCount(); ++layerIndex) {
        if (static_cast<size_t>(layerIndex) < tileData.size()) {
            const auto& layer = layers.at(static_cast<size_t>(layerIndex));
            tileMap.loadLayerFromArray(layerIndex, tileData.at(static_cast<size_t>(layerIndex)));
            tileMap.setLayerName(layerIndex, layer.name);
            tileMap.setLayerVisible(layerIndex, layer.visible);
            tileMap.setLayerDepth(layerIndex, layer.depthZ);
        } else {
            tileMap.loadLayerFromArray(layerIndex, emptyTiles);
            tileMap.setLayerVisible(layerIndex, false);
        }
    }
}

std::vector<int> captureTileMapLayerTiles(const vde::TileMap& tileMap, int layerIndex) {
    const int columns = tileMap.getColumnCount();
    const int rows = tileMap.getRowCount();
    std::vector<int> tiles;
    tiles.reserve(static_cast<size_t>(columns) * static_cast<size_t>(rows));
    for (int row = 0; row < rows; ++row) {
        for (int column = 0; column < columns; ++column) {
            tiles.push_back(tileMap.getTile(layerIndex, column, row));
        }
    }
    return tiles;
}

glm::vec2 findSpawnPoint(const std::vector<vde::ImportedTileObject>& objects) {
    for (const auto& object : objects) {
        if (object.name == "spawn" || object.type == "spawn") {
            return object.position;
        }
    }

    std::cerr << "WARNING: LevelBuilder map is missing a spawn object; using fallback spawn "
                 "point\n";
    return kDefaultSpawnPoint;
}

}  // namespace

namespace levelbuilder {

void TileMapSession::load(vde::VulkanContext* context) {
    if (context == nullptr) {
        throw std::invalid_argument("LevelBuilder requires a valid VulkanContext to load maps");
    }

    vde::TileMapImportOptions options;
    options.tileWidth = kTileSize;
    options.tileHeight = kTileSize;
    options.layerDepthStep = kDefaultLayerDepthStep;

    auto imported = vde::TileMapImport::importTiledJsonFile(context, kImportedMapPath, options);
    if (imported.tileMap == nullptr) {
        throw std::runtime_error("LevelBuilder failed to import a tilemap");
    }

    adoptTileMap(imported.tileMap, findSpawnPoint(imported.objects), imported.objects.size(),
                 kImportedMapPath);
    (void)reloadEditableLayerOverlay();
}

void TileMapSession::adoptTileMap(const std::shared_ptr<const vde::TileMap>& tileMap,
                                  glm::vec2 spawnPoint, size_t importedObjectCount,
                                  const std::string& sourceMapId) {
    if (tileMap == nullptr) {
        throw std::invalid_argument("TileMapSession requires a valid tilemap");
    }

    m_tileMap = tileMap->clone();
    m_spawnPoint = spawnPoint;
    m_importedObjectCount = importedObjectCount;
    m_sourceMapId = sourceMapId.empty() ? std::string(kImportedMapPath) : sourceMapId;
    if (m_tileMap->getLayerCount() <= 0) {
        throw std::invalid_argument("TileMapSession requires at least one layer in the tilemap");
    }

    m_importedLayers.clear();
    m_importedLayers.reserve(static_cast<size_t>(m_tileMap->getLayerCount()));
    for (int layerIndex = 0; layerIndex < m_tileMap->getLayerCount(); ++layerIndex) {
        m_importedLayers.push_back(buildImportedLayerDefinition(*m_tileMap, layerIndex));
    }

    m_layers = m_importedLayers;
    m_savedLayers = m_layers;
    m_savedLayerTiles.clear();
    m_savedLayerTiles.reserve(m_layers.size());
    for (const auto& layer : m_layers) {
        m_savedLayerTiles.push_back(layer.tiles);
    }
    m_activeLayerIndex = 0;
    clearEditHistory();
    m_hasUnsavedChanges = false;
    if (m_overlayPath.empty()) {
        m_overlayPath = kOverlayFileName;
    }
    m_lastPersistenceStatus = "Using imported ground layer.";

    m_runtimeLayerSyncRevisions.assign(m_layers.size(), 0);
    rebuildCollisionCache();
    markRuntimeLayoutChanged();
}

void TileMapSession::setOverlayPath(std::filesystem::path overlayPath) {
    m_overlayPath =
        overlayPath.empty() ? std::filesystem::path(kOverlayFileName) : std::move(overlayPath);
}

void TileMapSession::markRuntimeChanged(std::optional<size_t> layerIndex) {
    ++m_runtimeRevision;
    if (layerIndex.has_value() && layerIndex.value() < m_runtimeLayerSyncRevisions.size()) {
        m_runtimeLayerSyncRevisions.at(layerIndex.value()) = m_runtimeRevision;
    }
}

void TileMapSession::markRuntimeLayoutChanged() {
    ++m_runtimeLayoutRevision;
    markRuntimeChanged();
    m_runtimeLayerSyncRevisions.assign(m_layers.size(), 0);
}

size_t TileMapSession::runtimeLayerSyncRevision(size_t index) const {
    if (index >= m_runtimeLayerSyncRevisions.size()) {
        return 0;
    }
    return m_runtimeLayerSyncRevisions.at(index);
}

bool TileMapSession::setMapPosition(const glm::vec3& position) {
    if (m_tileMap == nullptr) {
        return false;
    }

    const auto& currentPosition = m_tileMap->getPosition();
    if (currentPosition.x == position.x && currentPosition.y == position.y &&
        currentPosition.z == position.z) {
        return false;
    }

    m_tileMap->setPosition(position);
    rebuildCollisionCache();
    markRuntimeChanged();
    return true;
}

const LayerDefinition* TileMapSession::layerDefinition(size_t index) const {
    if (index >= m_layers.size()) {
        return nullptr;
    }
    return &m_layers[index];
}

std::optional<LayerScrollPreset> TileMapSession::layerScrollPreset(size_t index) const {
    if (index >= m_layers.size()) {
        return std::nullopt;
    }

    const LayerDefinition& layer = m_layers[index];
    if (layer.followFactorX == 1.0f && layer.followFactorY == 1.0f &&
        layer.scrollVelocityX == 0.0f && layer.scrollVelocityY == 0.0f) {
        return LayerScrollPreset::Gameplay;
    }
    if (layer.followFactorX == 0.75f && layer.followFactorY == 0.90f &&
        layer.scrollVelocityX == 0.0f && layer.scrollVelocityY == 0.0f) {
        return LayerScrollPreset::MildParallax;
    }
    if (layer.followFactorX == 0.40f && layer.followFactorY == 0.65f &&
        layer.scrollVelocityX == 0.0f && layer.scrollVelocityY == 0.0f) {
        return LayerScrollPreset::StrongParallax;
    }
    if (layer.followFactorX == 0.25f && layer.followFactorY == 0.50f &&
        layer.scrollVelocityX == 0.35f && layer.scrollVelocityY == 0.0f) {
        return LayerScrollPreset::DriftingDecorative;
    }
    return std::nullopt;
}

const char* TileMapSession::layerScrollPresetName(LayerScrollPreset preset) {
    switch (preset) {
    case LayerScrollPreset::Gameplay:
        return "Gameplay";
    case LayerScrollPreset::MildParallax:
        return "Mild Parallax";
    case LayerScrollPreset::StrongParallax:
        return "Strong Parallax";
    case LayerScrollPreset::DriftingDecorative:
        return "Drifting Decorative";
    }
    return "Custom";
}

std::optional<glm::vec3>
TileMapSession::runtimeLayerPosition(size_t index, const glm::vec2& cameraPosition,
                                     const glm::vec2& runtimeScrollOffset) const {
    if (m_tileMap == nullptr || index >= m_layers.size()) {
        return std::nullopt;
    }

    const LayerDefinition& layer = m_layers[index];
    const auto& basePosition = m_tileMap->getPosition();
    return glm::vec3(basePosition.x + layer.scrollOffsetX + runtimeScrollOffset.x +
                         cameraPosition.x * (1.0f - layer.followFactorX),
                     basePosition.y + layer.scrollOffsetY + runtimeScrollOffset.y +
                         cameraPosition.y * (1.0f - layer.followFactorY),
                     basePosition.z + layer.depthZ);
}

std::shared_ptr<vde::TileMap> TileMapSession::createRuntimeTileMap(size_t layerIndex) const {
    if (m_tileMap == nullptr || layerIndex >= m_layers.size()) {
        return nullptr;
    }

    auto runtimeTileMap =
        std::make_shared<vde::TileMap>(m_tileMap->getTileWidth(), m_tileMap->getTileHeight(),
                                       m_tileMap->getColumnCount(), m_tileMap->getRowCount());

    if (const auto tileSet = m_tileMap->getTileSet(); tileSet != nullptr) {
        runtimeTileMap->setTileSet(tileSet);
    }
    runtimeTileMap->setCulling(m_tileMap->isCullingEnabled());

    if (!syncRuntimeTileMap(layerIndex, *runtimeTileMap)) {
        return nullptr;
    }

    return runtimeTileMap;
}

bool TileMapSession::syncRuntimeTileMap(size_t layerIndex, vde::TileMap& runtimeTileMap) const {
    if (m_tileMap == nullptr || layerIndex >= m_layers.size()) {
        return false;
    }
    if (runtimeTileMap.getColumnCount() != m_tileMap->getColumnCount() ||
        runtimeTileMap.getRowCount() != m_tileMap->getRowCount()) {
        return false;
    }

    const LayerDefinition& layer = m_layers[layerIndex];
    if (const auto tileSet = m_tileMap->getTileSet();
        tileSet != nullptr && runtimeTileMap.getTileSet() != tileSet) {
        runtimeTileMap.setTileSet(tileSet);
    }

    runtimeTileMap.setLayerName(0, layer.name);
    runtimeTileMap.setLayerVisible(0, layer.visible);
    runtimeTileMap.loadLayerFromArray(0, layer.tiles);

    const auto position = runtimeLayerPosition(layerIndex, glm::vec2(0.0f));
    if (!position.has_value()) {
        return false;
    }
    runtimeTileMap.setPosition(position->x, position->y, position->z);
    return true;
}

bool TileMapSession::setActiveLayerIndex(size_t index) {
    if (index >= m_layers.size()) {
        return false;
    }
    m_activeLayerIndex = index;
    return true;
}

bool TileMapSession::setLayerVisibility(size_t index, bool visible) {
    if (index >= m_layers.size()) {
        return false;
    }
    LayerDefinition& layer = m_layers[index];
    if (layer.visible == visible) {
        return false;
    }

    layer.visible = visible;
    if (m_tileMap != nullptr && index < static_cast<size_t>(m_tileMap->getLayerCount())) {
        m_tileMap->setLayerVisible(static_cast<int>(index), visible);
    }

    refreshDirtyState();
    markRuntimeChanged(index);
    return true;
}

bool TileMapSession::toggleLayerVisibility(size_t index) {
    if (index >= m_layers.size()) {
        return false;
    }
    return setLayerVisibility(index, !m_layers[index].visible);
}

bool TileMapSession::setLayerDepthZ(size_t index, float depthZ) {
    if (index >= m_layers.size()) {
        return false;
    }
    LayerDefinition& layer = m_layers[index];
    if (layer.depthZ == depthZ) {
        return false;
    }

    layer.depthZ = depthZ;
    if (m_tileMap != nullptr && index < static_cast<size_t>(m_tileMap->getLayerCount())) {
        m_tileMap->setLayerDepth(static_cast<int>(index), depthZ);
    }

    refreshDirtyState();
    markRuntimeChanged(index);
    return true;
}

bool TileMapSession::adjustLayerDepthZ(size_t index, float deltaZ) {
    if (index >= m_layers.size() || deltaZ == 0.0f) {
        return false;
    }
    return setLayerDepthZ(index, m_layers[index].depthZ + deltaZ);
}

bool TileMapSession::setLayerScrollPreset(size_t index, LayerScrollPreset preset) {
    if (index >= m_layers.size()) {
        return false;
    }

    glm::vec2 followFactor(1.0f);
    glm::vec2 scrollVelocity(0.0f);
    switch (preset) {
    case LayerScrollPreset::Gameplay:
        followFactor = {1.0f, 1.0f};
        scrollVelocity = {0.0f, 0.0f};
        break;
    case LayerScrollPreset::MildParallax:
        followFactor = {0.75f, 0.90f};
        scrollVelocity = {0.0f, 0.0f};
        break;
    case LayerScrollPreset::StrongParallax:
        followFactor = {0.40f, 0.65f};
        scrollVelocity = {0.0f, 0.0f};
        break;
    case LayerScrollPreset::DriftingDecorative:
        followFactor = {0.25f, 0.50f};
        scrollVelocity = {0.35f, 0.0f};
        break;
    }

    LayerDefinition& layer = m_layers[index];
    if (layer.followFactorX == followFactor.x && layer.followFactorY == followFactor.y &&
        layer.scrollVelocityX == scrollVelocity.x && layer.scrollVelocityY == scrollVelocity.y) {
        return false;
    }

    layer.followFactorX = followFactor.x;
    layer.followFactorY = followFactor.y;
    layer.scrollVelocityX = scrollVelocity.x;
    layer.scrollVelocityY = scrollVelocity.y;
    refreshDirtyState();
    markRuntimeChanged();
    return true;
}

bool TileMapSession::cycleLayerScrollPreset(size_t index, int direction) {
    if (index >= m_layers.size() || direction == 0) {
        return false;
    }

    constexpr int kPresetCount = 4;
    const auto currentPreset = layerScrollPreset(index);
    const int currentIndex = currentPreset.has_value() ? static_cast<int>(currentPreset.value())
                                                       : (direction > 0 ? -1 : 0);
    const int step = direction > 0 ? 1 : -1;
    const int nextIndex = (currentIndex + step + kPresetCount) % kPresetCount;
    return setLayerScrollPreset(index, static_cast<LayerScrollPreset>(nextIndex));
}

size_t TileMapSession::addLayer(const std::string& name) {
    if (m_tileMap == nullptr) {
        return m_layers.size();
    }

    const size_t newIndex = m_layers.size();
    const size_t tileCount = static_cast<size_t>(m_tileMap->getColumnCount()) *
                             static_cast<size_t>(m_tileMap->getRowCount());

    LayerDefinition layer;
    layer.id = "layer_" + std::to_string(newIndex);
    layer.name = name.empty() ? ("Layer " + std::to_string(newIndex)) : name;
    layer.tiles.assign(tileCount, vde::TileMap::kEmptyTile);
    layer.collisionEnabled = false;
    layer.depthZ = static_cast<float>(newIndex) * kDefaultLayerDepthStep;

    m_layers.push_back(std::move(layer));
    m_hasUnsavedChanges = true;
    markRuntimeLayoutChanged();

    return newIndex;
}

glm::ivec2 TileMapSession::maxTileCoordinate() const {
    if (m_tileMap == nullptr) {
        return {0, 0};
    }

    return {std::max(0, m_tileMap->getColumnCount() - 1),
            std::max(0, m_tileMap->getRowCount() - 1)};
}

glm::ivec2 TileMapSession::clampTileCoordinate(const glm::ivec2& tileCoordinate) const {
    const glm::ivec2 maxTile = maxTileCoordinate();
    return {std::clamp(tileCoordinate.x, 0, maxTile.x), std::clamp(tileCoordinate.y, 0, maxTile.y)};
}

glm::ivec2 TileMapSession::worldToTileClamped(const glm::vec2& worldPosition) const {
    if (m_tileMap == nullptr) {
        return {0, 0};
    }

    const auto& mapPosition = m_tileMap->getPosition();
    const float localX = worldPosition.x - mapPosition.x;
    const float localY = worldPosition.y - mapPosition.y;
    const int column = static_cast<int>(std::floor(localX / m_tileMap->getTileWidth()));
    const int row = static_cast<int>(std::floor(localY / m_tileMap->getTileHeight()));
    return clampTileCoordinate({column, row});
}

glm::ivec2 TileMapSession::nearestTileToWorld(const glm::vec2& worldPosition) const {
    return worldToTileClamped(worldPosition);
}

glm::vec2 TileMapSession::tileCenterWorld(const glm::ivec2& tileCoordinate) const {
    const glm::ivec2 clampedTile = clampTileCoordinate(tileCoordinate);
    if (m_tileMap == nullptr) {
        return {0.0f, 0.0f};
    }

    const auto& mapPosition = m_tileMap->getPosition();
    return {mapPosition.x + (static_cast<float>(clampedTile.x) + 0.5f) * m_tileMap->getTileWidth(),
            mapPosition.y +
                (static_cast<float>(clampedTile.y) + 0.5f) * m_tileMap->getTileHeight()};
}

std::string TileMapSession::editableLayerName() const {
    if (m_activeLayerIndex < m_layers.size()) {
        return m_layers[m_activeLayerIndex].name;
    }
    if (m_tileMap != nullptr) {
        const std::string& name = m_tileMap->getLayerInfo(0).name;
        return name.empty() ? "Layer 0" : name;
    }
    return "Layer 0";
}

std::string TileMapSession::overlayFileName() const {
    const std::filesystem::path effectivePath =
        m_overlayPath.empty() ? std::filesystem::path(kOverlayFileName) : m_overlayPath;
    const std::filesystem::path fileName = effectivePath.filename();
    return fileName.empty() ? effectivePath.string() : fileName.string();
}

int TileMapSession::readLayerTile(size_t layerIndex, const glm::ivec2& tileCoord) const {
    if (layerIndex < m_layers.size()) {
        if (m_tileMap == nullptr) {
            return vde::TileMap::kEmptyTile;
        }
        const size_t columnCount = static_cast<size_t>(m_tileMap->getColumnCount());
        const size_t tileIndex =
            static_cast<size_t>(tileCoord.y) * columnCount + static_cast<size_t>(tileCoord.x);
        const auto& tiles = m_layers[layerIndex].tiles;
        return tileIndex < tiles.size() ? tiles[tileIndex] : vde::TileMap::kEmptyTile;
    }
    return vde::TileMap::kEmptyTile;
}

void TileMapSession::writeLayerTile(size_t layerIndex, const glm::ivec2& tileCoord, int tileId) {
    if (m_tileMap != nullptr && layerIndex < static_cast<size_t>(m_tileMap->getLayerCount())) {
        m_tileMap->setTile(static_cast<int>(layerIndex), tileCoord.x, tileCoord.y, tileId);
    }
    if (layerIndex < m_layers.size()) {
        if (m_tileMap == nullptr) {
            return;
        }
        const size_t columnCount = static_cast<size_t>(m_tileMap->getColumnCount());
        const size_t tileIndex =
            static_cast<size_t>(tileCoord.y) * columnCount + static_cast<size_t>(tileCoord.x);
        auto& tiles = m_layers[layerIndex].tiles;
        if (tileIndex < tiles.size()) {
            tiles[tileIndex] = tileId;
        }
    }
}

int TileMapSession::editableTileId(const glm::ivec2& tileCoordinate) const {
    if (m_tileMap == nullptr) {
        return vde::TileMap::kEmptyTile;
    }

    const glm::ivec2 clampedTile = clampTileCoordinate(tileCoordinate);
    return readLayerTile(m_activeLayerIndex, clampedTile);
}

std::optional<int> TileMapSession::cycledEditableTileId(int currentTileId, int direction) const {
    if (m_tileMap == nullptr || direction == 0) {
        return std::nullopt;
    }

    const auto tileSet = m_tileMap->getTileSet();
    if (tileSet == nullptr || tileSet->getSpriteCount() <= 0) {
        return std::nullopt;
    }

    const int spriteCount = tileSet->getSpriteCount();
    const int step = direction > 0 ? 1 : -1;
    if (currentTileId == vde::TileMap::kEmptyTile) {
        return step > 0 ? 0 : spriteCount - 1;
    }

    return (currentTileId + step + spriteCount) % spriteCount;
}

bool TileMapSession::setEditableTileId(const glm::ivec2& tileCoordinate, int tileId) {
    if (!applyEditableTileId(m_activeLayerIndex, tileCoordinate, tileId, true)) {
        return false;
    }

    m_lastEditedLayerIndex = m_activeLayerIndex;
    m_lastPersistenceStatus = m_hasUnsavedChanges
                                  ? "Editable ground layer has unsaved changes."
                                  : "Editable ground layer matches the saved overlay.";
    return true;
}

bool TileMapSession::cycleEditableTile(const glm::ivec2& tileCoordinate, int direction) {
    const auto nextTileId = cycledEditableTileId(editableTileId(tileCoordinate), direction);
    if (!nextTileId.has_value()) {
        return false;
    }

    return setEditableTileId(tileCoordinate, nextTileId.value());
}

bool TileMapSession::undoLastEditableEdit() {
    if (!canUndoEditableEdit()) {
        m_lastPersistenceStatus = "No tile edit is available to undo.";
        return false;
    }

    const TileEditRecord& edit = m_editHistory.at(m_appliedEditCount - 1);
    if (!applyEditableTileId(edit.layerIndex, edit.tileCoordinate, edit.oldTileId, false)) {
        return false;
    }

    m_lastEditedLayerIndex = edit.layerIndex;
    --m_appliedEditCount;
    m_lastPersistenceStatus = m_hasUnsavedChanges
                                  ? "Undid the last tile edit. Overlay has unsaved changes."
                                  : "Undid the last tile edit. Overlay matches the saved state.";
    return true;
}

bool TileMapSession::redoLastEditableEdit() {
    if (!canRedoEditableEdit()) {
        m_lastPersistenceStatus = "No tile edit is available to redo.";
        return false;
    }

    const TileEditRecord& edit = m_editHistory.at(m_appliedEditCount);
    if (!applyEditableTileId(edit.layerIndex, edit.tileCoordinate, edit.newTileId, false)) {
        return false;
    }

    m_lastEditedLayerIndex = edit.layerIndex;
    ++m_appliedEditCount;
    m_lastPersistenceStatus = m_hasUnsavedChanges
                                  ? "Redid the tile edit. Overlay has unsaved changes."
                                  : "Redid the tile edit. Overlay matches the saved state.";
    return true;
}

bool TileMapSession::applyEditableTileId(size_t layerIndex, const glm::ivec2& tileCoordinate,
                                         int tileId, bool recordHistory) {
    if (m_tileMap == nullptr) {
        m_lastPersistenceStatus = "Cannot edit tiles before a map is loaded.";
        return false;
    }
    if (layerIndex >= m_layers.size()) {
        m_lastPersistenceStatus = "Layer index out of range.";
        return false;
    }

    const glm::ivec2 clampedTile = clampTileCoordinate(tileCoordinate);
    const int oldTileId = readLayerTile(layerIndex, clampedTile);
    if (oldTileId == tileId) {
        return false;
    }

    writeLayerTile(layerIndex, clampedTile, tileId);

    if (layerIndex < static_cast<size_t>(m_tileMap->getLayerCount())) {
        const vde::TileCollisionKind oldCollision = m_tileMap->getCollisionKind(oldTileId);
        const vde::TileCollisionKind newCollision = m_tileMap->getCollisionKind(tileId);
        if (oldCollision != vde::TileCollisionKind::None ||
            newCollision != vde::TileCollisionKind::None) {
            rebuildCollisionCache();
        }
    }

    if (recordHistory) {
        m_editHistory.resize(m_appliedEditCount);
        m_editHistory.push_back(TileEditRecord{
            .layerIndex = layerIndex,
            .tileCoordinate = clampedTile,
            .oldTileId = oldTileId,
            .newTileId = tileId,
        });
        m_appliedEditCount = m_editHistory.size();
    }

    refreshDirtyStateForTileEdit(layerIndex, clampedTile, oldTileId, tileId);
    markRuntimeChanged(layerIndex);
    return true;
}

bool TileMapSession::saveEditableLayerOverlay() {
    if (m_tileMap == nullptr) {
        m_lastPersistenceStatus = "Cannot save overlay before a map is loaded.";
        return false;
    }

    try {
        const std::filesystem::path effectivePath =
            m_overlayPath.empty() ? std::filesystem::path(kOverlayFileName) : m_overlayPath;

        std::vector<std::vector<int>> currentTiles;
        currentTiles.reserve(m_layers.size());
        for (size_t i = 0; i < m_layers.size(); ++i) {
            currentTiles.push_back(captureLayerTiles(i));
        }

        const OrderedJson root =
            buildOverlayJson(*m_tileMap, m_layers, currentTiles, m_sourceMapId);
        writeTextFile(effectivePath, root.dump(2));
        m_savedLayers = m_layers;
        m_savedLayerTiles = currentTiles;
        m_hasUnsavedChanges = false;
        m_lastPersistenceStatus = "Saved ground overlay to " + overlayFileName() + ".";
        std::cout << m_lastPersistenceStatus << '\n';
        return true;
    } catch (const std::exception& ex) {
        m_lastPersistenceStatus = std::string("Failed to save overlay: ") + ex.what();
        std::cerr << m_lastPersistenceStatus << '\n';
        return false;
    }
}

bool TileMapSession::reloadEditableLayerOverlay() {
    if (m_tileMap == nullptr) {
        m_lastPersistenceStatus = "Cannot load overlay before a map is loaded.";
        return false;
    }

    const std::filesystem::path effectivePath =
        m_overlayPath.empty() ? std::filesystem::path(kOverlayFileName) : m_overlayPath;
    std::error_code existsError;
    const bool overlayExists = std::filesystem::exists(effectivePath, existsError);
    if (existsError) {
        m_lastPersistenceStatus = "Failed to inspect overlay file: " + effectivePath.string();
        std::cerr << m_lastPersistenceStatus << '\n';
        return false;
    }

    if (!overlayExists) {
        std::vector<LayerDefinition> restoredLayers = m_importedLayers;
        std::vector<std::vector<int>> restoredTiles;
        restoredTiles.reserve(restoredLayers.size());
        for (const auto& layer : restoredLayers) {
            restoredTiles.push_back(layer.tiles);
        }

        auto stagedTileMap = m_tileMap->clone();
        applyLayerStackToTileMap(*stagedTileMap, restoredLayers, restoredTiles);

        m_tileMap = std::move(stagedTileMap);
        m_layers = std::move(restoredLayers);
        m_savedLayers = m_layers;
        m_savedLayerTiles = std::move(restoredTiles);
        rebuildCollisionCache();

        clearEditHistory();
        m_hasUnsavedChanges = false;
        m_activeLayerIndex = 0;
        markRuntimeLayoutChanged();
        m_lastPersistenceStatus = "No saved overlay found; using imported ground layer.";
        std::cout << m_lastPersistenceStatus << '\n';
        return true;
    }

    try {
        const OrderedJson root = OrderedJson::parse(readTextFile(effectivePath));
        std::vector<LayerAndTiles> loadedLayers =
            parseOverlayLayers(root, *m_tileMap, m_sourceMapId);

        if (loadedLayers.empty()) {
            throw std::invalid_argument("Overlay contains no layers");
        }

        // Parse all layers into a new state first; apply atomically if parsing succeeds.
        std::vector<LayerDefinition> newLayers;
        std::vector<std::vector<int>> newSavedTiles;
        newLayers.reserve(loadedLayers.size());
        newSavedTiles.reserve(loadedLayers.size());

        for (size_t i = 0; i < loadedLayers.size(); ++i) {
            LayerDefinition def = std::move(loadedLayers[i].first);
            std::vector<int>& tiles = loadedLayers[i].second;
            def.tiles = tiles;
            newLayers.push_back(std::move(def));
            newSavedTiles.push_back(std::move(tiles));
        }

        auto stagedTileMap = m_tileMap->clone();
        applyLayerStackToTileMap(*stagedTileMap, newLayers, newSavedTiles);

        m_tileMap = std::move(stagedTileMap);
        m_layers = std::move(newLayers);
        m_savedLayers = m_layers;
        m_savedLayerTiles = std::move(newSavedTiles);
        rebuildCollisionCache();
        if (m_activeLayerIndex >= m_layers.size()) {
            m_activeLayerIndex = 0;
        }
        clearEditHistory();
        m_hasUnsavedChanges = false;
        markRuntimeLayoutChanged();
        m_lastPersistenceStatus = "Loaded ground overlay from " + overlayFileName() + ".";
        std::cout << m_lastPersistenceStatus << '\n';
        return true;
    } catch (const std::exception& ex) {
        m_lastPersistenceStatus = std::string("Failed to load overlay: ") + ex.what();
        std::cerr << m_lastPersistenceStatus << '\n';
        return false;
    }
}

std::vector<int> TileMapSession::captureLayerTiles(size_t layerIndex) const {
    if (m_tileMap == nullptr) {
        return {};
    }

    if (layerIndex < m_layers.size()) {
        if (layerIndex == 0 && m_layers[layerIndex].tiles.empty()) {
            return captureTileMapLayerTiles(*m_tileMap, 0);
        }
        return m_layers[layerIndex].tiles;
    }

    return {};
}

void TileMapSession::clearEditHistory() {
    m_editHistory.clear();
    m_appliedEditCount = 0;
    m_lastEditedLayerIndex.reset();
}

void TileMapSession::refreshDirtyStateForTileEdit(size_t layerIndex,
                                                  const glm::ivec2& tileCoordinate, int oldTileId,
                                                  int newTileId) {
    if (layerIndex >= m_savedLayerTiles.size() || m_tileMap == nullptr) {
        refreshDirtyState();
        return;
    }

    const size_t columnCount = static_cast<size_t>(m_tileMap->getColumnCount());
    const size_t tileIndex =
        static_cast<size_t>(tileCoordinate.y) * columnCount + static_cast<size_t>(tileCoordinate.x);
    const auto& savedTiles = m_savedLayerTiles[layerIndex];

    if (tileIndex < savedTiles.size() && newTileId != savedTiles[tileIndex]) {
        // Fast path: this tile is now dirty, so the session is definitely unsaved.
        m_hasUnsavedChanges = true;
        return;
    }

    // The new value matches saved (or index is out of bounds). Need a full scan to determine
    // whether any tile in any layer is still different from saved.
    (void)oldTileId;
    refreshDirtyState();
}

void TileMapSession::refreshDirtyState() {
    if (m_layers.size() != m_savedLayerTiles.size() || m_layers.size() != m_savedLayers.size()) {
        m_hasUnsavedChanges = true;
        return;
    }

    for (size_t i = 0; i < m_layers.size(); ++i) {
        if (!sameLayerMetadata(m_layers[i], m_savedLayers[i])) {
            m_hasUnsavedChanges = true;
            return;
        }
        if (captureLayerTiles(i) != m_savedLayerTiles[i]) {
            m_hasUnsavedChanges = true;
            return;
        }
    }
    m_hasUnsavedChanges = false;
}

void TileMapSession::rebuildCollisionCache() {
    if (m_tileMap == nullptr) {
        m_solidRects.clear();
        m_oneWayRects.clear();
        return;
    }

    m_solidRects.clear();
    m_oneWayRects.clear();
    const size_t layerCount =
        std::min(m_layers.size(), static_cast<size_t>(m_tileMap->getLayerCount()));
    for (size_t layerIndex = 0; layerIndex < layerCount; ++layerIndex) {
        if (!m_layers[layerIndex].collisionEnabled) {
            continue;
        }

        for (const auto& rect : m_tileMap->extractCollisionRects(static_cast<int>(layerIndex))) {
            if (rect.kind == vde::TileCollisionKind::Solid) {
                m_solidRects.push_back(rect);
            } else if (rect.kind == vde::TileCollisionKind::OneWay) {
                m_oneWayRects.push_back(rect);
            }
        }
    }
}

}  // namespace levelbuilder