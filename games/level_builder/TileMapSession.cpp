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
constexpr glm::vec2 kDefaultSpawnPoint(4.5f, 7.0f);
constexpr const char* kImportedMapPath = "assets/tiled/tilemap_demo.tmj";
constexpr const char* kOverlayFileName = "level_builder_ground.overlay.json";
constexpr const char* kOverlayFormatId = "vde.level_builder.ground_overlay";
constexpr int kOverlayFormatVersion = 1;
constexpr int kEditableLayerIndex = 0;

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

std::vector<int> parseOverlayTiles(const OrderedJson& root, const vde::TileMap& tileMap,
                                   const std::string& sourceMapId) {
    if (!root.is_object()) {
        throw std::invalid_argument("LevelBuilder overlay root must be an object");
    }

    if (requireString(root, "format", "root") != kOverlayFormatId) {
        throw std::invalid_argument("LevelBuilder overlay format is not supported");
    }

    if (requireInt(root, "version", "root") != kOverlayFormatVersion) {
        throw std::invalid_argument("LevelBuilder overlay version is not supported");
    }

    if (requireString(root, "base_map", "root") != sourceMapId) {
        throw std::invalid_argument("LevelBuilder overlay targets a different source map");
    }

    if (!root.contains("editable_layer") || !root.at("editable_layer").is_object()) {
        throw std::invalid_argument("LevelBuilder overlay missing editable_layer object");
    }

    const OrderedJson& editableLayer = root.at("editable_layer");
    const std::string expectedLayerName = tileMap.getLayerInfo(kEditableLayerIndex).name;
    if (requireString(editableLayer, "name", "editable_layer") != expectedLayerName) {
        throw std::invalid_argument("LevelBuilder overlay targets a different editable layer");
    }

    if (requireInt(editableLayer, "columns", "editable_layer") != tileMap.getColumnCount() ||
        requireInt(editableLayer, "rows", "editable_layer") != tileMap.getRowCount()) {
        throw std::invalid_argument("LevelBuilder overlay dimensions do not match the map");
    }

    if (!editableLayer.contains("tiles") || !editableLayer.at("tiles").is_array()) {
        throw std::invalid_argument("LevelBuilder overlay missing tiles array");
    }

    const OrderedJson& tilesJson = editableLayer.at("tiles");
    const size_t expectedTileCount =
        static_cast<size_t>(tileMap.getColumnCount()) * static_cast<size_t>(tileMap.getRowCount());
    if (tilesJson.size() != expectedTileCount) {
        throw std::invalid_argument("LevelBuilder overlay tile count does not match the map");
    }

    std::vector<int> tiles;
    tiles.reserve(expectedTileCount);
    for (const auto& tileValue : tilesJson) {
        if (!tileValue.is_number_integer()) {
            throw std::invalid_argument("LevelBuilder overlay tiles must be integers");
        }
        tiles.push_back(tileValue.get<int>());
    }

    return tiles;
}

OrderedJson buildOverlayJson(const vde::TileMap& tileMap, const std::vector<int>& tiles,
                             const std::string& sourceMapId) {
    OrderedJson editableLayer{{"name", tileMap.getLayerInfo(kEditableLayerIndex).name},
                              {"columns", tileMap.getColumnCount()},
                              {"rows", tileMap.getRowCount()},
                              {"tiles", tiles}};

    return OrderedJson{{"format", kOverlayFormatId},
                       {"version", kOverlayFormatVersion},
                       {"base_map", sourceMapId},
                       {"editable_layer", std::move(editableLayer)}};
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
    options.layerDepthStep = 0.06f;

    auto imported = vde::TileMapImport::importTiledJsonFile(context, kImportedMapPath, options);
    if (imported.tileMap == nullptr) {
        throw std::runtime_error("LevelBuilder failed to import a tilemap");
    }

    adoptTileMap(imported.tileMap, findSpawnPoint(imported.objects), imported.objects.size(),
                 kImportedMapPath);
    (void)reloadEditableLayerOverlay();
}

void TileMapSession::adoptTileMap(std::shared_ptr<vde::TileMap> tileMap, glm::vec2 spawnPoint,
                                  size_t importedObjectCount, const std::string& sourceMapId) {
    if (tileMap == nullptr) {
        throw std::invalid_argument("TileMapSession requires a valid tilemap");
    }

    m_tileMap = std::move(tileMap);
    m_spawnPoint = spawnPoint;
    m_importedObjectCount = importedObjectCount;
    m_sourceMapId = sourceMapId.empty() ? std::string(kImportedMapPath) : sourceMapId;
    if (m_tileMap->getLayerCount() <= kEditableLayerIndex) {
        throw std::invalid_argument("TileMapSession requires an editable layer at index 0");
    }

    m_importedEditableTiles = captureEditableLayerTiles();
    m_hasUnsavedChanges = false;
    if (m_overlayPath.empty()) {
        m_overlayPath = kOverlayFileName;
    }
    m_lastPersistenceStatus = "Using imported ground layer.";

    rebuildCollisionCache();
}

void TileMapSession::setOverlayPath(std::filesystem::path overlayPath) {
    m_overlayPath =
        overlayPath.empty() ? std::filesystem::path(kOverlayFileName) : std::move(overlayPath);
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
    if (m_tileMap == nullptr) {
        return "Layer 0";
    }

    const std::string& layerName = m_tileMap->getLayerInfo(kEditableLayerIndex).name;
    return layerName.empty() ? "Layer 0" : layerName;
}

std::string TileMapSession::overlayFileName() const {
    const std::filesystem::path effectivePath =
        m_overlayPath.empty() ? std::filesystem::path(kOverlayFileName) : m_overlayPath;
    const std::filesystem::path fileName = effectivePath.filename();
    return fileName.empty() ? effectivePath.string() : fileName.string();
}

int TileMapSession::editableTileId(const glm::ivec2& tileCoordinate) const {
    if (m_tileMap == nullptr) {
        return vde::TileMap::kEmptyTile;
    }

    const glm::ivec2 clampedTile = clampTileCoordinate(tileCoordinate);
    return m_tileMap->getTile(kEditableLayerIndex, clampedTile.x, clampedTile.y);
}

bool TileMapSession::setEditableTileId(const glm::ivec2& tileCoordinate, int tileId) {
    if (m_tileMap == nullptr) {
        m_lastPersistenceStatus = "Cannot edit tiles before a map is loaded.";
        return false;
    }

    const glm::ivec2 clampedTile = clampTileCoordinate(tileCoordinate);
    const int oldTileId = m_tileMap->getTile(kEditableLayerIndex, clampedTile.x, clampedTile.y);
    if (oldTileId == tileId) {
        return false;
    }

    m_tileMap->setTile(kEditableLayerIndex, clampedTile.x, clampedTile.y, tileId);

    const vde::TileCollisionKind oldCollision = m_tileMap->getCollisionKind(oldTileId);
    const vde::TileCollisionKind newCollision = m_tileMap->getCollisionKind(tileId);
    if (oldCollision != vde::TileCollisionKind::None ||
        newCollision != vde::TileCollisionKind::None) {
        rebuildCollisionCache();
    }

    m_hasUnsavedChanges = true;
    m_lastPersistenceStatus = "Editable ground layer has unsaved changes.";

    return true;
}

bool TileMapSession::cycleEditableTile(const glm::ivec2& tileCoordinate, int direction) {
    if (m_tileMap == nullptr || direction == 0) {
        return false;
    }

    const auto tileSet = m_tileMap->getTileSet();
    if (tileSet == nullptr || tileSet->getSpriteCount() <= 0) {
        return false;
    }

    const int spriteCount = tileSet->getSpriteCount();
    const int step = direction > 0 ? 1 : -1;
    const int currentTileId = editableTileId(tileCoordinate);
    const int nextTileId = (currentTileId == vde::TileMap::kEmptyTile)
                               ? (step > 0 ? 0 : spriteCount - 1)
                               : (currentTileId + step + spriteCount) % spriteCount;

    return setEditableTileId(tileCoordinate, nextTileId);
}

bool TileMapSession::saveEditableLayerOverlay() {
    if (m_tileMap == nullptr) {
        m_lastPersistenceStatus = "Cannot save overlay before a map is loaded.";
        return false;
    }

    try {
        const std::filesystem::path effectivePath =
            m_overlayPath.empty() ? std::filesystem::path(kOverlayFileName) : m_overlayPath;
        const OrderedJson root =
            buildOverlayJson(*m_tileMap, captureEditableLayerTiles(), m_sourceMapId);
        writeTextFile(effectivePath, root.dump(2));
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

    if (!applyEditableLayerTiles(m_importedEditableTiles)) {
        m_lastPersistenceStatus = "Failed to restore the imported ground layer.";
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
        m_hasUnsavedChanges = false;
        m_lastPersistenceStatus = "No saved overlay found; using imported ground layer.";
        std::cout << m_lastPersistenceStatus << '\n';
        return true;
    }

    try {
        const OrderedJson root = OrderedJson::parse(readTextFile(effectivePath));
        if (!applyEditableLayerTiles(parseOverlayTiles(root, *m_tileMap, m_sourceMapId))) {
            m_lastPersistenceStatus = "Failed to apply the saved overlay tiles.";
            return false;
        }

        m_hasUnsavedChanges = false;
        m_lastPersistenceStatus = "Loaded ground overlay from " + overlayFileName() + ".";
        std::cout << m_lastPersistenceStatus << '\n';
        return true;
    } catch (const std::exception& ex) {
        m_lastPersistenceStatus = std::string("Failed to load overlay: ") + ex.what();
        std::cerr << m_lastPersistenceStatus << '\n';
        return false;
    }
}

std::vector<int> TileMapSession::captureEditableLayerTiles() const {
    if (m_tileMap == nullptr) {
        return {};
    }

    const int columns = m_tileMap->getColumnCount();
    const int rows = m_tileMap->getRowCount();
    std::vector<int> tiles;
    tiles.reserve(static_cast<size_t>(columns) * static_cast<size_t>(rows));
    for (int row = 0; row < rows; ++row) {
        for (int column = 0; column < columns; ++column) {
            tiles.push_back(m_tileMap->getTile(kEditableLayerIndex, column, row));
        }
    }

    return tiles;
}

bool TileMapSession::applyEditableLayerTiles(const std::vector<int>& tiles) {
    if (m_tileMap == nullptr) {
        return false;
    }

    const size_t expectedTileCount = static_cast<size_t>(m_tileMap->getColumnCount()) *
                                     static_cast<size_t>(m_tileMap->getRowCount());
    if (tiles.size() != expectedTileCount) {
        return false;
    }

    m_tileMap->loadLayerFromArray(kEditableLayerIndex, tiles);
    rebuildCollisionCache();
    return true;
}

void TileMapSession::rebuildCollisionCache() {
    if (m_tileMap == nullptr) {
        m_solidRects.clear();
        m_oneWayRects.clear();
        return;
    }

    const std::vector<vde::TileCollisionRect> collisions = m_tileMap->extractCollisionRects();
    m_solidRects.clear();
    m_oneWayRects.clear();
    for (const auto& rect : collisions) {
        if (rect.kind == vde::TileCollisionKind::Solid) {
            m_solidRects.push_back(rect);
        } else if (rect.kind == vde::TileCollisionKind::OneWay) {
            m_oneWayRects.push_back(rect);
        }
    }
}

}  // namespace levelbuilder