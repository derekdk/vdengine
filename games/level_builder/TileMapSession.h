#pragma once

#include <vde/api/GameAPI.h>

#include <glm/vec2.hpp>

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace vde {
class VulkanContext;
}

namespace levelbuilder {

class TileMapSession {
  public:
    void load(vde::VulkanContext* context);
    void adoptTileMap(std::shared_ptr<vde::TileMap> tileMap, glm::vec2 spawnPoint,
                      size_t importedObjectCount = 0, const std::string& sourceMapId = {});
    void setOverlayPath(std::filesystem::path overlayPath);
    bool saveEditableLayerOverlay();
    bool reloadEditableLayerOverlay();

    [[nodiscard]] std::shared_ptr<vde::TileMap> tileMap() const { return m_tileMap; }
    [[nodiscard]] const std::vector<vde::TileCollisionRect>& solidRects() const {
        return m_solidRects;
    }
    [[nodiscard]] const std::vector<vde::TileCollisionRect>& oneWayRects() const {
        return m_oneWayRects;
    }
    [[nodiscard]] glm::vec2 spawnPoint() const { return m_spawnPoint; }
    [[nodiscard]] size_t importedObjectCount() const { return m_importedObjectCount; }
    [[nodiscard]] glm::ivec2 maxTileCoordinate() const;
    [[nodiscard]] glm::ivec2 clampTileCoordinate(const glm::ivec2& tileCoordinate) const;
    [[nodiscard]] glm::ivec2 worldToTileClamped(const glm::vec2& worldPosition) const;
    [[nodiscard]] glm::ivec2 nearestTileToWorld(const glm::vec2& worldPosition) const;
    [[nodiscard]] glm::vec2 tileCenterWorld(const glm::ivec2& tileCoordinate) const;
    [[nodiscard]] std::string editableLayerName() const;
    [[nodiscard]] int editableTileId(const glm::ivec2& tileCoordinate) const;
    [[nodiscard]] std::optional<int> cycledEditableTileId(int currentTileId, int direction) const;
    [[nodiscard]] const std::filesystem::path& overlayPath() const { return m_overlayPath; }
    [[nodiscard]] std::string overlayFileName() const;
    [[nodiscard]] bool hasUnsavedChanges() const { return m_hasUnsavedChanges; }
    [[nodiscard]] const std::string& lastPersistenceStatus() const {
        return m_lastPersistenceStatus;
    }
    [[nodiscard]] bool canUndoEditableEdit() const { return m_appliedEditCount > 0; }
    [[nodiscard]] bool canRedoEditableEdit() const {
        return m_appliedEditCount < m_editHistory.size();
    }
    [[nodiscard]] size_t undoDepth() const { return m_appliedEditCount; }
    [[nodiscard]] size_t redoDepth() const { return m_editHistory.size() - m_appliedEditCount; }
    bool setEditableTileId(const glm::ivec2& tileCoordinate, int tileId);
    bool cycleEditableTile(const glm::ivec2& tileCoordinate, int direction);
    bool undoLastEditableEdit();
    bool redoLastEditableEdit();

  private:
    struct TileEditRecord {
        glm::ivec2 tileCoordinate{0, 0};
        int oldTileId = vde::TileMap::kEmptyTile;
        int newTileId = vde::TileMap::kEmptyTile;
    };

    bool applyEditableTileId(const glm::ivec2& tileCoordinate, int tileId, bool recordHistory);
    [[nodiscard]] std::vector<int> captureEditableLayerTiles() const;
    bool applyEditableLayerTiles(const std::vector<int>& tiles);
    void clearEditHistory();
    void refreshDirtyState();
    void rebuildCollisionCache();

    std::shared_ptr<vde::TileMap> m_tileMap;
    std::vector<vde::TileCollisionRect> m_solidRects;
    std::vector<vde::TileCollisionRect> m_oneWayRects;
    std::vector<int> m_importedEditableTiles;
    std::vector<int> m_savedEditableTiles;
    std::vector<TileEditRecord> m_editHistory;
    size_t m_appliedEditCount = 0;
    size_t m_importedObjectCount = 0;
    glm::vec2 m_spawnPoint{0.0f};
    std::string m_sourceMapId;
    std::filesystem::path m_overlayPath;
    bool m_hasUnsavedChanges = false;
    std::string m_lastPersistenceStatus;
};

}  // namespace levelbuilder