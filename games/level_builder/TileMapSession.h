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

struct LayerDefinition {
    std::string id;
    std::string name;
    std::vector<int> tiles;
    float depthZ = 0.0f;
    bool visible = true;
    bool collisionEnabled = true;
    float followFactorX = 1.0f;
    float followFactorY = 1.0f;
    float scrollVelocityX = 0.0f;
    float scrollVelocityY = 0.0f;
    float scrollOffsetX = 0.0f;
    float scrollOffsetY = 0.0f;
};

enum class LayerScrollPreset {
    Gameplay,
    MildParallax,
    StrongParallax,
    DriftingDecorative,
};

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
    [[nodiscard]] size_t layerCount() const { return m_layers.size(); }
    [[nodiscard]] size_t activeLayerIndex() const { return m_activeLayerIndex; }
    [[nodiscard]] const LayerDefinition* layerDefinition(size_t index) const;
    [[nodiscard]] std::optional<LayerScrollPreset> layerScrollPreset(size_t index) const;
    [[nodiscard]] static const char* layerScrollPresetName(LayerScrollPreset preset);
    [[nodiscard]] std::optional<glm::vec3>
    runtimeLayerPosition(size_t index, const glm::vec2& cameraPosition,
                         const glm::vec2& runtimeScrollOffset = glm::vec2(0.0f)) const;
    [[nodiscard]] std::shared_ptr<vde::TileMap> createRuntimeTileMap(size_t layerIndex) const;
    bool syncRuntimeTileMap(size_t layerIndex, vde::TileMap& runtimeTileMap) const;
    bool setActiveLayerIndex(size_t index);
    bool setLayerVisibility(size_t index, bool visible);
    bool toggleLayerVisibility(size_t index);
    bool setLayerDepthZ(size_t index, float depthZ);
    bool adjustLayerDepthZ(size_t index, float deltaZ);
    bool setLayerScrollPreset(size_t index, LayerScrollPreset preset);
    bool cycleLayerScrollPreset(size_t index, int direction);
    size_t addLayer(const std::string& name = "");
    bool setEditableTileId(const glm::ivec2& tileCoordinate, int tileId);
    bool cycleEditableTile(const glm::ivec2& tileCoordinate, int direction);
    bool undoLastEditableEdit();
    bool redoLastEditableEdit();
    [[nodiscard]] std::optional<size_t> lastEditedLayerIndex() const {
        return m_lastEditedLayerIndex;
    }

  private:
    struct TileEditRecord {
        size_t layerIndex = 0;
        glm::ivec2 tileCoordinate{0, 0};
        int oldTileId = vde::TileMap::kEmptyTile;
        int newTileId = vde::TileMap::kEmptyTile;
    };

    bool applyEditableTileId(size_t layerIndex, const glm::ivec2& tileCoordinate, int tileId,
                             bool recordHistory);
    [[nodiscard]] std::vector<int> captureLayerTiles(size_t layerIndex) const;
    bool applyLayerTiles(size_t layerIndex, const std::vector<int>& tiles);
    void clearEditHistory();
    void refreshDirtyStateForTileEdit(size_t layerIndex, const glm::ivec2& tileCoordinate,
                                      int oldTileId, int newTileId);
    void refreshDirtyState();
    void rebuildCollisionCache();
    [[nodiscard]] int readLayerTile(size_t layerIndex, const glm::ivec2& tileCoord) const;
    void writeLayerTile(size_t layerIndex, const glm::ivec2& tileCoord, int tileId);

    std::shared_ptr<vde::TileMap> m_tileMap;
    std::vector<vde::TileCollisionRect> m_solidRects;
    std::vector<vde::TileCollisionRect> m_oneWayRects;
    std::vector<LayerDefinition> m_importedLayers;
    std::vector<LayerDefinition> m_savedLayers;
    std::vector<std::vector<int>> m_savedLayerTiles;
    std::vector<TileEditRecord> m_editHistory;
    size_t m_appliedEditCount = 0;
    size_t m_importedObjectCount = 0;
    glm::vec2 m_spawnPoint{0.0f};
    std::string m_sourceMapId;
    std::filesystem::path m_overlayPath;
    bool m_hasUnsavedChanges = false;
    std::string m_lastPersistenceStatus;
    std::vector<LayerDefinition> m_layers;
    size_t m_activeLayerIndex = 0;
    std::optional<size_t> m_lastEditedLayerIndex;
};

}  // namespace levelbuilder