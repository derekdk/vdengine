#pragma once

/**
 * @file TileMap.h
 * @brief Tile-based rendering and background helpers for VDE.
 *
 * Provides a layered TileMap that batches visible tiles from a SpriteSheet
 * into a mesh for efficient rendering, along with collision extraction for
 * solid and one-way tiles. Also provides a RepeatingBackground helper for
 * infinite scrolling textures with parallax.
 */

#include <vde/api/Entity.h>
#include <vde/api/GameCamera.h>
#include <vde/api/SpriteSheet.h>

#include <glm/vec2.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace vde {

/**
 * @brief Collision meaning assigned to a tile ID.
 */
enum class TileCollisionKind : uint8_t {
    None = 0,
    Solid,
    OneWay,
};

/**
 * @brief Extracted collision rectangle for one or more tiles.
 */
struct TileCollisionRect {
    glm::vec2 center{0.0f, 0.0f};
    glm::vec2 halfExtents{0.0f, 0.0f};
    TileCollisionKind kind = TileCollisionKind::None;
    int layerIndex = 0;
};

/**
 * @brief Inclusive tile bounds describing the currently visible range.
 */
struct TileVisibilityBounds {
    int minColumn = 0;
    int maxColumn = -1;
    int minRow = 0;
    int maxRow = -1;

    /**
     * @brief Check whether the bounds select any tiles.
     * @return true when the bounds are empty.
     */
    bool empty() const { return maxColumn < minColumn || maxRow < minRow; }
};

/**
 * @brief Mesh-backed layered tilemap with SpriteSheet UV binding and culling.
 *
 * TileMap stores a fixed-size tile grid and one or more layers. Rendering
 * rebuilds a mesh for only the tiles visible to an active Camera2D, keeping
 * large maps practical without creating one entity per tile.
 *
 * Tile coordinates use a bottom-left origin in local tilemap space:
 * column 0, row 0 corresponds to the tile spanning
 * [0, tileWidth] × [0, tileHeight].
 */
class TileMap : public MeshEntity {
  public:
    /**
     * @brief Metadata for a tile layer.
     */
    struct LayerInfo {
        std::string name;
        bool visible = true;
        float depth = 0.0f;
    };

    static constexpr int kEmptyTile = -1;

    /**
     * @brief Construct a fixed-size tilemap.
     * @param tileWidth Width of one tile in world units.
     * @param tileHeight Height of one tile in world units.
     * @param columns Number of columns in the grid.
     * @param rows Number of rows in the grid.
     * @throws std::invalid_argument if any dimension is non-positive.
     */
    TileMap(float tileWidth, float tileHeight, int columns, int rows);

    /**
     * @brief Add a new tile layer.
     * @param name Optional layer name.
     * @return Zero-based layer index.
     */
    int addLayer(const std::string& name = {});

    /**
     * @brief Get the number of layers.
     */
    int getLayerCount() const { return static_cast<int>(m_layers.size()); }

    /**
     * @brief Get layer metadata.
     * @param layerIndex Zero-based layer index.
     */
    const LayerInfo& getLayerInfo(int layerIndex) const;

    /**
     * @brief Rename a layer.
     * @param layerIndex Zero-based layer index.
     * @param name New layer name.
     */
    void setLayerName(int layerIndex, const std::string& name);

    /**
     * @brief Set whether a layer should render.
     * @param layerIndex Zero-based layer index.
     * @param visible True to render the layer.
     */
    void setLayerVisible(int layerIndex, bool visible);

    /**
     * @brief Check whether a layer is visible.
     * @param layerIndex Zero-based layer index.
     * @return true if the layer will render.
     */
    bool isLayerVisible(int layerIndex) const;

    /**
     * @brief Set a layer's Z depth.
     * @param layerIndex Zero-based layer index.
     * @param depth Local-space Z value used for rendering order.
     */
    void setLayerDepth(int layerIndex, float depth);

    /**
     * @brief Get a layer's Z depth.
     * @param layerIndex Zero-based layer index.
     * @return The layer depth value.
     */
    float getLayerDepth(int layerIndex) const;

    /**
     * @brief Set the SpriteSheet used for tile IDs.
     * @param tileSet SpriteSheet whose indices map to tile IDs.
     * @throws std::out_of_range if existing tile IDs exceed the sheet.
     */
    void setTileSet(std::shared_ptr<SpriteSheet> tileSet);

    /**
     * @brief Get the bound SpriteSheet.
     */
    std::shared_ptr<SpriteSheet> getTileSet() const { return m_tileSet; }

    /**
     * @brief Create a CPU-side copy of this tilemap without attached scene or GPU mesh state.
     */
    std::shared_ptr<TileMap> clone() const;

    /**
     * @brief Get the tile width in world units.
     */
    float getTileWidth() const { return m_tileWidth; }

    /**
     * @brief Get the tile height in world units.
     */
    float getTileHeight() const { return m_tileHeight; }

    /**
     * @brief Get the number of columns.
     */
    int getColumnCount() const { return m_columns; }

    /**
     * @brief Get the number of rows.
     */
    int getRowCount() const { return m_rows; }

    /**
     * @brief Set a tile on the base layer.
     * @param column Zero-based column.
     * @param row Zero-based row.
     * @param tileId SpriteSheet index, or kEmptyTile to clear.
     */
    void setTile(int column, int row, int tileId);

    /**
     * @brief Set a tile on a specific layer.
     * @param layerIndex Zero-based layer index.
     * @param column Zero-based column.
     * @param row Zero-based row.
     * @param tileId SpriteSheet index, or kEmptyTile to clear.
     */
    void setTile(int layerIndex, int column, int row, int tileId);

    /**
     * @brief Get a tile from the base layer.
     * @param column Zero-based column.
     * @param row Zero-based row.
     * @return Tile ID or kEmptyTile.
     */
    int getTile(int column, int row) const;

    /**
     * @brief Get a tile from a specific layer.
     * @param layerIndex Zero-based layer index.
     * @param column Zero-based column.
     * @param row Zero-based row.
     * @return Tile ID or kEmptyTile.
     */
    int getTile(int layerIndex, int column, int row) const;

    /**
     * @brief Fill an inclusive region on the base layer.
     * @param startColumn Inclusive start column.
     * @param startRow Inclusive start row.
     * @param endColumn Inclusive end column.
     * @param endRow Inclusive end row.
     * @param tileId SpriteSheet index, or kEmptyTile.
     */
    void fillRegion(int startColumn, int startRow, int endColumn, int endRow, int tileId);

    /**
     * @brief Fill an inclusive region on a specific layer.
     * @param layerIndex Zero-based layer index.
     * @param startColumn Inclusive start column.
     * @param startRow Inclusive start row.
     * @param endColumn Inclusive end column.
     * @param endRow Inclusive end row.
     * @param tileId SpriteSheet index, or kEmptyTile.
     */
    void fillRegion(int layerIndex, int startColumn, int startRow, int endColumn, int endRow,
                    int tileId);

    /**
     * @brief Load base-layer tiles from a row-major array.
     * @param tiles Tile IDs with size columns * rows.
     */
    void loadFromArray(const std::vector<int>& tiles);

    /**
     * @brief Load layer tiles from a row-major array.
     * @param layerIndex Zero-based layer index.
     * @param tiles Tile IDs with size columns * rows.
     */
    void loadLayerFromArray(int layerIndex, const std::vector<int>& tiles);

    /**
     * @brief Enable or disable camera-visible culling.
     * @param enabled True to rebuild only visible tiles.
     */
    void setCulling(bool enabled);

    /**
     * @brief Check whether culling is enabled.
     */
    bool isCullingEnabled() const { return m_cullingEnabled; }

    /**
     * @brief Get the most recent bounds used to build the render mesh.
     */
    TileVisibilityBounds getLastVisibleBounds() const { return m_lastVisibleBounds; }

    /**
     * @brief Compute visible tile bounds for a world-space rectangle.
     * @param rect World-space visible rectangle.
     * @return Inclusive tile bounds clamped to the map.
     */
    TileVisibilityBounds computeVisibleBounds(const Rect2D& rect) const;

    /**
     * @brief Compute visible tile bounds using the attached Camera2D.
     * @return Inclusive tile bounds, or the full map if no Camera2D is active.
     */
    TileVisibilityBounds computeVisibleBoundsFromCamera() const;

    /**
     * @brief Assign collision meaning to a tile ID.
     * @param tileId SpriteSheet tile ID.
     * @param kind Collision meaning for that tile.
     */
    void setCollisionKind(int tileId, TileCollisionKind kind);

    /**
     * @brief Query the collision meaning for a tile ID.
     * @param tileId SpriteSheet tile ID.
     * @return The configured collision kind, or None if unassigned.
     */
    TileCollisionKind getCollisionKind(int tileId) const;

    /**
     * @brief Extract merged world-space collision rectangles.
     *
     * Solid tiles are merged both horizontally and vertically where possible.
     * One-way tiles are merged into horizontal spans per row so consumers can
     * treat them as top-only platforms.
     *
     * @param layerIndex Zero-based layer index, or -1 for all layers.
     * @return Extracted collision rectangles.
     */
    std::vector<TileCollisionRect> extractCollisionRects(int layerIndex = -1) const;

    /**
     * @brief Rebuild visible mesh as needed and render the tilemap.
     */
    void render() override;

  private:
    struct LayerData {
        LayerInfo info;
        std::vector<int> tiles;
    };

    float m_tileWidth = 1.0f;
    float m_tileHeight = 1.0f;
    int m_columns = 0;
    int m_rows = 0;
    bool m_cullingEnabled = true;
    std::shared_ptr<SpriteSheet> m_tileSet;
    std::vector<LayerData> m_layers;
    std::unordered_map<int, TileCollisionKind> m_collisionKinds;
    TileVisibilityBounds m_lastVisibleBounds;
    bool m_meshDirty = true;

    void markDirty();
    void validateLayerIndex(int layerIndex) const;
    void validateTileCoordinate(int column, int row) const;
    void validateTileId(int tileId) const;
    size_t getTileOffset(int column, int row) const;
    void rebuildMesh(const TileVisibilityBounds& bounds);
};

/**
 * @brief Infinite repeating texture layer with parallax.
 *
 * RepeatingBackground uses a single texture tile and rebuilds enough quads to
 * cover the active Camera2D view. The texture grid can scroll independently
 * and follow the camera at a reduced or amplified parallax factor.
 */
class RepeatingBackground : public MeshEntity {
  public:
    /**
     * @brief Construct a repeating background with square tiles.
     * @param texture Texture to repeat.
     * @param tileSize Tile width and height in world units.
     * @param tilesX Logical pattern width in tiles.
     * @param tilesY Logical pattern height in tiles.
     */
    RepeatingBackground(std::shared_ptr<Texture> texture, float tileSize, int tilesX, int tilesY);

    /**
     * @brief Construct a repeating background.
     * @param texture Texture to repeat.
     * @param tileWidth Tile width in world units.
     * @param tileHeight Tile height in world units.
     * @param tilesX Logical pattern width in tiles.
     * @param tilesY Logical pattern height in tiles.
     */
    RepeatingBackground(std::shared_ptr<Texture> texture, float tileWidth, float tileHeight,
                        int tilesX, int tilesY);

    /**
     * @brief Set the camera-follow factor.
     * @param factorX Horizontal parallax factor.
     * @param factorY Vertical parallax factor.
     */
    void setParallaxFactor(float factorX, float factorY);

    /**
     * @brief Get the camera-follow factor.
     */
    glm::vec2 getParallaxFactor() const { return m_parallaxFactor; }

    /**
     * @brief Set autonomous scroll velocity in world units per second.
     * @param velocityX Horizontal scroll velocity.
     * @param velocityY Vertical scroll velocity.
     */
    void setScrollVelocity(float velocityX, float velocityY);

    /**
     * @brief Get the autonomous scroll velocity.
     */
    glm::vec2 getScrollVelocity() const { return m_scrollVelocity; }

    /**
     * @brief Set an explicit scroll offset.
     * @param offsetX Horizontal offset in world units.
     * @param offsetY Vertical offset in world units.
     */
    void setScrollOffset(float offsetX, float offsetY);

    /**
     * @brief Get the current scroll offset.
     */
    glm::vec2 getScrollOffset() const { return m_scrollOffset; }

    /**
     * @brief Advance autonomous scrolling.
     * @param deltaTime Time since last update in seconds.
     */
    void update(float deltaTime) override;

    /**
     * @brief Rebuild visible mesh as needed and render the background.
     */
    void render() override;

  private:
    float m_tileWidth = 1.0f;
    float m_tileHeight = 1.0f;
    int m_tilesX = 1;
    int m_tilesY = 1;
    glm::vec2 m_parallaxFactor{1.0f, 1.0f};
    glm::vec2 m_scrollVelocity{0.0f, 0.0f};
    glm::vec2 m_scrollOffset{0.0f, 0.0f};
    glm::vec2 m_lastCameraPosition{0.0f, 0.0f};
    Rect2D m_lastVisibleRect;
    bool m_hasLastRenderState = false;
    bool m_meshDirty = true;

    void markDirty();
    void rebuildMesh(const Rect2D& visibleRect, const glm::vec2& cameraPosition);
    void wrapScrollOffset();
};

}  // namespace vde