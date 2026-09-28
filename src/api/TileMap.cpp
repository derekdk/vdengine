/**
 * @file TileMap.cpp
 * @brief Implementation of tile-based rendering helpers.
 */

#include <vde/Texture.h>
#include <vde/api/Mesh.h>
#include <vde/api/Scene.h>
#include <vde/api/TileMap.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace vde {

namespace {

constexpr float kCullEpsilon = 1e-4f;

struct RowSpan {
    int row = 0;
    int startColumn = 0;
    int endColumn = 0;
};

bool sameBounds(const TileVisibilityBounds& a, const TileVisibilityBounds& b) {
    return a.minColumn == b.minColumn && a.maxColumn == b.maxColumn && a.minRow == b.minRow &&
           a.maxRow == b.maxRow;
}

bool sameRect(const Rect2D& a, const Rect2D& b) {
    return a.left == b.left && a.right == b.right && a.bottom == b.bottom && a.top == b.top;
}

glm::vec2 makeCenter(float x0, float y0, float width, float height) {
    return {x0 + width * 0.5f, y0 + height * 0.5f};
}

}  // namespace

TileMap::TileMap(float tileWidth, float tileHeight, int columns, int rows)
    : m_tileWidth(tileWidth), m_tileHeight(tileHeight), m_columns(columns), m_rows(rows) {
    if (tileWidth <= 0.0f || tileHeight <= 0.0f) {
        throw std::invalid_argument("TileMap tile dimensions must be positive");
    }
    if (columns <= 0 || rows <= 0) {
        throw std::invalid_argument("TileMap grid dimensions must be positive");
    }

    addLayer("base");
}

std::shared_ptr<TileMap> TileMap::clone() const {
    auto copy = std::make_shared<TileMap>(m_tileWidth, m_tileHeight, m_columns, m_rows);
    copy->m_cullingEnabled = m_cullingEnabled;
    copy->m_tileSet = m_tileSet;
    copy->m_layers = m_layers;
    copy->m_collisionKinds = m_collisionKinds;
    copy->m_lastVisibleBounds = TileVisibilityBounds{};
    copy->m_meshDirty = true;
    copy->setTransform(getTransform());
    copy->setVisible(isVisible());
    copy->setTexture(m_tileSet ? m_tileSet->getTexture() : std::shared_ptr<Texture>{});
    return copy;
}

int TileMap::addLayer(const std::string& name) {
    LayerData layer;
    layer.info.name = name;
    layer.info.depth = static_cast<float>(m_layers.size()) * 0.01f;
    layer.tiles.assign(static_cast<size_t>(m_columns) * static_cast<size_t>(m_rows), kEmptyTile);
    m_layers.push_back(std::move(layer));
    markDirty();
    return static_cast<int>(m_layers.size() - 1);
}

const TileMap::LayerInfo& TileMap::getLayerInfo(int layerIndex) const {
    validateLayerIndex(layerIndex);
    return m_layers.at(static_cast<size_t>(layerIndex)).info;
}

void TileMap::setLayerName(int layerIndex, const std::string& name) {
    validateLayerIndex(layerIndex);
    auto& info = m_layers.at(static_cast<size_t>(layerIndex)).info;
    if (info.name == name) {
        return;
    }

    info.name = name;
}

void TileMap::setLayerVisible(int layerIndex, bool visible) {
    validateLayerIndex(layerIndex);
    auto& info = m_layers.at(static_cast<size_t>(layerIndex)).info;
    if (info.visible == visible) {
        return;
    }
    info.visible = visible;
    markDirty();
}

bool TileMap::isLayerVisible(int layerIndex) const {
    validateLayerIndex(layerIndex);
    return m_layers.at(static_cast<size_t>(layerIndex)).info.visible;
}

void TileMap::setLayerDepth(int layerIndex, float depth) {
    validateLayerIndex(layerIndex);
    auto& info = m_layers.at(static_cast<size_t>(layerIndex)).info;
    if (info.depth == depth) {
        return;
    }
    info.depth = depth;
    markDirty();
}

float TileMap::getLayerDepth(int layerIndex) const {
    validateLayerIndex(layerIndex);
    return m_layers.at(static_cast<size_t>(layerIndex)).info.depth;
}

void TileMap::setTileSet(std::shared_ptr<SpriteSheet> tileSet) {
    m_tileSet = std::move(tileSet);
    setTexture(m_tileSet ? m_tileSet->getTexture() : std::shared_ptr<Texture>{});

    if (m_tileSet) {
        const int spriteCount = m_tileSet->getSpriteCount();
        for (const auto& layer : m_layers) {
            for (int tileId : layer.tiles) {
                if (tileId >= spriteCount) {
                    throw std::out_of_range("TileMap tile ID exceeds SpriteSheet sprite count");
                }
            }
        }
    }

    markDirty();
}

void TileMap::setTile(int column, int row, int tileId) {
    setTile(0, column, row, tileId);
}

void TileMap::setTile(int layerIndex, int column, int row, int tileId) {
    validateLayerIndex(layerIndex);
    validateTileCoordinate(column, row);
    validateTileId(tileId);

    auto& tiles = m_layers.at(static_cast<size_t>(layerIndex)).tiles;
    tiles.at(getTileOffset(column, row)) = tileId;
    markDirty();
}

int TileMap::getTile(int column, int row) const {
    return getTile(0, column, row);
}

int TileMap::getTile(int layerIndex, int column, int row) const {
    validateLayerIndex(layerIndex);
    validateTileCoordinate(column, row);
    const auto& tiles = m_layers.at(static_cast<size_t>(layerIndex)).tiles;
    return tiles.at(getTileOffset(column, row));
}

void TileMap::fillRegion(int startColumn, int startRow, int endColumn, int endRow, int tileId) {
    fillRegion(0, startColumn, startRow, endColumn, endRow, tileId);
}

void TileMap::fillRegion(int layerIndex, int startColumn, int startRow, int endColumn, int endRow,
                         int tileId) {
    validateLayerIndex(layerIndex);
    validateTileId(tileId);

    const int minColumn = std::min(startColumn, endColumn);
    const int maxColumn = std::max(startColumn, endColumn);
    const int minRow = std::min(startRow, endRow);
    const int maxRow = std::max(startRow, endRow);
    validateTileCoordinate(minColumn, minRow);
    validateTileCoordinate(maxColumn, maxRow);

    auto& tiles = m_layers.at(static_cast<size_t>(layerIndex)).tiles;
    for (int row = minRow; row <= maxRow; ++row) {
        for (int column = minColumn; column <= maxColumn; ++column) {
            tiles.at(getTileOffset(column, row)) = tileId;
        }
    }

    markDirty();
}

void TileMap::loadFromArray(const std::vector<int>& tiles) {
    loadLayerFromArray(0, tiles);
}

void TileMap::loadLayerFromArray(int layerIndex, const std::vector<int>& tiles) {
    validateLayerIndex(layerIndex);
    const size_t expectedSize = static_cast<size_t>(m_columns) * static_cast<size_t>(m_rows);
    if (tiles.size() != expectedSize) {
        throw std::invalid_argument("TileMap array size does not match grid dimensions");
    }

    for (int tileId : tiles) {
        validateTileId(tileId);
    }

    m_layers.at(static_cast<size_t>(layerIndex)).tiles = tiles;
    markDirty();
}

void TileMap::setCulling(bool enabled) {
    if (m_cullingEnabled == enabled) {
        return;
    }
    m_cullingEnabled = enabled;
    markDirty();
}

TileVisibilityBounds TileMap::computeVisibleBounds(const Rect2D& rect) const {
    const auto position = getPosition();
    const float localLeft = rect.left - position.x;
    const float localRight = rect.right - position.x;
    const float localBottom = rect.bottom - position.y;
    const float localTop = rect.top - position.y;

    const float mapWidth = static_cast<float>(m_columns) * m_tileWidth;
    const float mapHeight = static_cast<float>(m_rows) * m_tileHeight;
    if (localRight <= 0.0f || localTop <= 0.0f || localLeft >= mapWidth ||
        localBottom >= mapHeight) {
        return TileVisibilityBounds{};
    }

    int minColumn = static_cast<int>(std::floor(localLeft / m_tileWidth));
    int maxColumn = static_cast<int>(std::floor((localRight - kCullEpsilon) / m_tileWidth));
    int minRow = static_cast<int>(std::floor(localBottom / m_tileHeight));
    int maxRow = static_cast<int>(std::floor((localTop - kCullEpsilon) / m_tileHeight));

    minColumn = std::clamp(minColumn, 0, m_columns - 1);
    maxColumn = std::clamp(maxColumn, 0, m_columns - 1);
    minRow = std::clamp(minRow, 0, m_rows - 1);
    maxRow = std::clamp(maxRow, 0, m_rows - 1);

    TileVisibilityBounds bounds;
    bounds.minColumn = minColumn;
    bounds.maxColumn = maxColumn;
    bounds.minRow = minRow;
    bounds.maxRow = maxRow;
    return bounds;
}

TileVisibilityBounds TileMap::computeVisibleBoundsFromCamera() const {
    if (!m_cullingEnabled || !m_scene) {
        return TileVisibilityBounds{
            .minColumn = 0, .maxColumn = m_columns - 1, .minRow = 0, .maxRow = m_rows - 1};
    }

    const auto* camera2D = dynamic_cast<const Camera2D*>(m_scene->getCamera());
    if (!camera2D) {
        return TileVisibilityBounds{
            .minColumn = 0, .maxColumn = m_columns - 1, .minRow = 0, .maxRow = m_rows - 1};
    }

    return computeVisibleBounds(camera2D->getVisibleRect());
}

void TileMap::setCollisionKind(int tileId, TileCollisionKind kind) {
    if (tileId < 0) {
        throw std::invalid_argument("TileMap collision IDs must be non-negative");
    }
    if (kind == TileCollisionKind::None) {
        m_collisionKinds.erase(tileId);
        return;
    }
    m_collisionKinds[tileId] = kind;
}

TileCollisionKind TileMap::getCollisionKind(int tileId) const {
    if (tileId < 0) {
        return TileCollisionKind::None;
    }
    const auto it = m_collisionKinds.find(tileId);
    if (it == m_collisionKinds.end()) {
        return TileCollisionKind::None;
    }
    return it->second;
}

std::vector<TileCollisionRect> TileMap::extractCollisionRects(int layerIndex) const {
    if (layerIndex < -1) {
        throw std::out_of_range("TileMap layer index is out of range");
    }
    if (layerIndex >= getLayerCount()) {
        throw std::out_of_range("TileMap layer index is out of range");
    }

    std::vector<TileCollisionRect> rects;
    const auto position = getPosition();
    const int firstLayer = (layerIndex < 0) ? 0 : layerIndex;
    const int lastLayer = (layerIndex < 0) ? (getLayerCount() - 1) : layerIndex;

    for (int currentLayer = firstLayer; currentLayer <= lastLayer; ++currentLayer) {
        const auto& tiles = m_layers.at(static_cast<size_t>(currentLayer)).tiles;
        std::vector<RowSpan> solidSpans;

        for (int row = 0; row < m_rows; ++row) {
            int column = 0;
            while (column < m_columns) {
                const int tileId = tiles.at(getTileOffset(column, row));
                const TileCollisionKind kind = getCollisionKind(tileId);
                if (kind == TileCollisionKind::None) {
                    ++column;
                    continue;
                }

                const int startColumn = column;
                while (column + 1 < m_columns &&
                       getCollisionKind(tiles.at(getTileOffset(column + 1, row))) == kind) {
                    ++column;
                }
                const int endColumn = column;

                if (kind == TileCollisionKind::Solid) {
                    solidSpans.push_back(
                        RowSpan{.row = row, .startColumn = startColumn, .endColumn = endColumn});
                } else {
                    const float width =
                        static_cast<float>(endColumn - startColumn + 1) * m_tileWidth;
                    const float height = m_tileHeight;
                    const float x0 = position.x + static_cast<float>(startColumn) * m_tileWidth;
                    const float y0 = position.y + static_cast<float>(row) * m_tileHeight;
                    rects.push_back(
                        TileCollisionRect{.center = makeCenter(x0, y0, width, height),
                                          .halfExtents = glm::vec2(width * 0.5f, height * 0.5f),
                                          .kind = TileCollisionKind::OneWay,
                                          .layerIndex = currentLayer});
                }

                ++column;
            }
        }

        for (size_t index = 0; index < solidSpans.size(); ++index) {
            const RowSpan seed = solidSpans.at(index);
            int endRow = seed.row;
            while (index + 1 < solidSpans.size() && solidSpans.at(index + 1).row == endRow + 1 &&
                   solidSpans.at(index + 1).startColumn == seed.startColumn &&
                   solidSpans.at(index + 1).endColumn == seed.endColumn) {
                ++index;
                endRow = solidSpans.at(index).row;
            }

            const float width =
                static_cast<float>(seed.endColumn - seed.startColumn + 1) * m_tileWidth;
            const float height = static_cast<float>(endRow - seed.row + 1) * m_tileHeight;
            const float x0 = position.x + static_cast<float>(seed.startColumn) * m_tileWidth;
            const float y0 = position.y + static_cast<float>(seed.row) * m_tileHeight;
            rects.push_back(TileCollisionRect{.center = makeCenter(x0, y0, width, height),
                                              .halfExtents = glm::vec2(width * 0.5f, height * 0.5f),
                                              .kind = TileCollisionKind::Solid,
                                              .layerIndex = currentLayer});
        }
    }

    return rects;
}

void TileMap::render() {
    const TileVisibilityBounds bounds = computeVisibleBoundsFromCamera();
    if (m_meshDirty || !sameBounds(bounds, m_lastVisibleBounds)) {
        rebuildMesh(bounds);
        m_lastVisibleBounds = bounds;
        m_meshDirty = false;
    }

    MeshEntity::render();
}

void TileMap::markDirty() {
    m_meshDirty = true;
}

void TileMap::validateLayerIndex(int layerIndex) const {
    if (layerIndex < 0 || layerIndex >= getLayerCount()) {
        throw std::out_of_range("TileMap layer index is out of range");
    }
}

void TileMap::validateTileCoordinate(int column, int row) const {
    if (column < 0 || column >= m_columns || row < 0 || row >= m_rows) {
        throw std::out_of_range("TileMap tile coordinate is out of range");
    }
}

void TileMap::validateTileId(int tileId) const {
    if (tileId < kEmptyTile) {
        throw std::invalid_argument("TileMap tile ID is invalid");
    }
    if (tileId >= 0 && m_tileSet && tileId >= m_tileSet->getSpriteCount()) {
        throw std::out_of_range("TileMap tile ID exceeds SpriteSheet sprite count");
    }
}

size_t TileMap::getTileOffset(int column, int row) const {
    return static_cast<size_t>(row) * static_cast<size_t>(m_columns) + static_cast<size_t>(column);
}

void TileMap::rebuildMesh(const TileVisibilityBounds& bounds) {
    auto mesh = std::make_shared<Mesh>();

    if (!m_tileSet || bounds.empty()) {
        mesh->setData({}, {});
        if (m_scene && m_mesh) {
            m_scene->retireResource(m_mesh);
        }
        setMesh(std::move(mesh));
        return;
    }

    size_t visibleTileBudget = 0;
    for (const auto& layer : m_layers) {
        if (layer.info.visible) {
            visibleTileBudget += static_cast<size_t>(bounds.maxColumn - bounds.minColumn + 1) *
                                 static_cast<size_t>(bounds.maxRow - bounds.minRow + 1);
        }
    }

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    vertices.reserve(visibleTileBudget * 4);
    indices.reserve(visibleTileBudget * 6);

    for (const auto& layer : m_layers) {
        if (!layer.info.visible) {
            continue;
        }

        for (int row = bounds.minRow; row <= bounds.maxRow; ++row) {
            for (int column = bounds.minColumn; column <= bounds.maxColumn; ++column) {
                const int tileId = layer.tiles.at(getTileOffset(column, row));
                if (tileId == kEmptyTile) {
                    continue;
                }
                if (tileId < 0 || tileId >= m_tileSet->getSpriteCount()) {
                    continue;
                }

                const auto uv = m_tileSet->getUVRect(tileId);
                const float x0 = static_cast<float>(column) * m_tileWidth;
                const float y0 = static_cast<float>(row) * m_tileHeight;
                const float x1 = x0 + m_tileWidth;
                const float y1 = y0 + m_tileHeight;
                const float z = layer.info.depth;

                const auto baseIndex = static_cast<uint32_t>(vertices.size());
                vertices.push_back(Vertex{.position = {x0, y0, z},
                                          .color = {1.0f, 1.0f, 1.0f},
                                          .texCoord = {uv.u, uv.v + uv.height}});
                vertices.push_back(Vertex{.position = {x1, y0, z},
                                          .color = {1.0f, 1.0f, 1.0f},
                                          .texCoord = {uv.u + uv.width, uv.v + uv.height}});
                vertices.push_back(Vertex{.position = {x1, y1, z},
                                          .color = {1.0f, 1.0f, 1.0f},
                                          .texCoord = {uv.u + uv.width, uv.v}});
                vertices.push_back(Vertex{.position = {x0, y1, z},
                                          .color = {1.0f, 1.0f, 1.0f},
                                          .texCoord = {uv.u, uv.v}});

                indices.push_back(baseIndex + 0);
                indices.push_back(baseIndex + 1);
                indices.push_back(baseIndex + 2);
                indices.push_back(baseIndex + 2);
                indices.push_back(baseIndex + 3);
                indices.push_back(baseIndex + 0);
            }
        }
    }

    mesh->setData(vertices, indices);
    if (m_scene && m_mesh) {
        m_scene->retireResource(m_mesh);
    }
    setMesh(std::move(mesh));
}

RepeatingBackground::RepeatingBackground(std::shared_ptr<Texture> texture, float tileSize,
                                         int tilesX, int tilesY)
    : RepeatingBackground(std::move(texture), tileSize, tileSize, tilesX, tilesY) {}

RepeatingBackground::RepeatingBackground(std::shared_ptr<Texture> texture, float tileWidth,
                                         float tileHeight, int tilesX, int tilesY)
    : m_tileWidth(tileWidth), m_tileHeight(tileHeight), m_tilesX(tilesX), m_tilesY(tilesY) {
    if (!texture) {
        throw std::invalid_argument("RepeatingBackground requires a texture");
    }
    if (tileWidth <= 0.0f || tileHeight <= 0.0f) {
        throw std::invalid_argument("RepeatingBackground tile dimensions must be positive");
    }
    if (tilesX <= 0 || tilesY <= 0) {
        throw std::invalid_argument("RepeatingBackground pattern dimensions must be positive");
    }

    setTexture(std::move(texture));
}

void RepeatingBackground::setParallaxFactor(float factorX, float factorY) {
    const glm::vec2 updated(factorX, factorY);
    if (m_parallaxFactor == updated) {
        return;
    }
    m_parallaxFactor = updated;
    markDirty();
}

void RepeatingBackground::setScrollVelocity(float velocityX, float velocityY) {
    const glm::vec2 updated(velocityX, velocityY);
    if (m_scrollVelocity == updated) {
        return;
    }
    m_scrollVelocity = updated;
}

void RepeatingBackground::setScrollOffset(float offsetX, float offsetY) {
    m_scrollOffset = glm::vec2(offsetX, offsetY);
    wrapScrollOffset();
    markDirty();
}

void RepeatingBackground::update(float deltaTime) {
    if (deltaTime <= 0.0f) {
        return;
    }
    if (m_scrollVelocity == glm::vec2(0.0f, 0.0f)) {
        return;
    }

    m_scrollOffset += m_scrollVelocity * deltaTime;
    wrapScrollOffset();
    markDirty();
}

void RepeatingBackground::render() {
    if (!m_scene) {
        MeshEntity::render();
        return;
    }

    const auto* camera2D = dynamic_cast<const Camera2D*>(m_scene->getCamera());
    if (!camera2D) {
        MeshEntity::render();
        return;
    }

    const Rect2D visibleRect = camera2D->getVisibleRect();
    const glm::vec2 cameraPosition = camera2D->getPosition();
    if (m_meshDirty || !m_hasLastRenderState || !sameRect(visibleRect, m_lastVisibleRect) ||
        cameraPosition != m_lastCameraPosition) {
        rebuildMesh(visibleRect, cameraPosition);
        m_lastVisibleRect = visibleRect;
        m_lastCameraPosition = cameraPosition;
        m_hasLastRenderState = true;
        m_meshDirty = false;
    }

    MeshEntity::render();
}

void RepeatingBackground::markDirty() {
    m_meshDirty = true;
}

void RepeatingBackground::rebuildMesh(const Rect2D& visibleRect, const glm::vec2& cameraPosition) {
    auto mesh = std::make_shared<Mesh>();

    const float patternWidth = static_cast<float>(m_tilesX) * m_tileWidth;
    const float patternHeight = static_cast<float>(m_tilesY) * m_tileHeight;
    const auto position = getPosition();

    const float shiftX =
        position.x + m_scrollOffset.x + cameraPosition.x * (1.0f - m_parallaxFactor.x);
    const float shiftY =
        position.y + m_scrollOffset.y + cameraPosition.y * (1.0f - m_parallaxFactor.y);

    const float startX =
        std::floor((visibleRect.left - shiftX) / m_tileWidth) * m_tileWidth + shiftX - m_tileWidth;
    const float endX = visibleRect.right + m_tileWidth;
    const float startY = std::floor((visibleRect.bottom - shiftY) / m_tileHeight) * m_tileHeight +
                         shiftY - m_tileHeight;
    const float endY = visibleRect.top + m_tileHeight;

    const int tileCountX = std::max(1, static_cast<int>(std::ceil((endX - startX) / m_tileWidth)));
    const int tileCountY = std::max(1, static_cast<int>(std::ceil((endY - startY) / m_tileHeight)));

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    vertices.reserve(static_cast<size_t>(tileCountX) * static_cast<size_t>(tileCountY) * 4);
    indices.reserve(static_cast<size_t>(tileCountX) * static_cast<size_t>(tileCountY) * 6);

    for (int y = 0; y < tileCountY; ++y) {
        for (int x = 0; x < tileCountX; ++x) {
            const float x0 = startX + static_cast<float>(x) * m_tileWidth;
            const float y0 = startY + static_cast<float>(y) * m_tileHeight;
            const float x1 = x0 + m_tileWidth;
            const float y1 = y0 + m_tileHeight;
            const auto baseIndex = static_cast<uint32_t>(vertices.size());

            vertices.push_back(Vertex{
                .position = {x0, y0, 0.0f}, .color = {1.0f, 1.0f, 1.0f}, .texCoord = {0.0f, 1.0f}});
            vertices.push_back(Vertex{
                .position = {x1, y0, 0.0f}, .color = {1.0f, 1.0f, 1.0f}, .texCoord = {1.0f, 1.0f}});
            vertices.push_back(Vertex{
                .position = {x1, y1, 0.0f}, .color = {1.0f, 1.0f, 1.0f}, .texCoord = {1.0f, 0.0f}});
            vertices.push_back(Vertex{
                .position = {x0, y1, 0.0f}, .color = {1.0f, 1.0f, 1.0f}, .texCoord = {0.0f, 0.0f}});

            indices.push_back(baseIndex + 0);
            indices.push_back(baseIndex + 1);
            indices.push_back(baseIndex + 2);
            indices.push_back(baseIndex + 2);
            indices.push_back(baseIndex + 3);
            indices.push_back(baseIndex + 0);
        }
    }

    mesh->setData(vertices, indices);
    if (m_scene && m_mesh) {
        m_scene->retireResource(m_mesh);
    }
    setMesh(std::move(mesh));

    if (patternWidth <= 0.0f || patternHeight <= 0.0f) {
        return;
    }
}

void RepeatingBackground::wrapScrollOffset() {
    const float patternWidth = static_cast<float>(m_tilesX) * m_tileWidth;
    const float patternHeight = static_cast<float>(m_tilesY) * m_tileHeight;

    if (patternWidth > 0.0f) {
        m_scrollOffset.x = std::fmod(m_scrollOffset.x, patternWidth);
    }
    if (patternHeight > 0.0f) {
        m_scrollOffset.y = std::fmod(m_scrollOffset.y, patternHeight);
    }
}

}  // namespace vde