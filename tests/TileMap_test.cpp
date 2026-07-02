/**
 * @file TileMap_test.cpp
 * @brief Unit tests for TileMap, TileMapImport, and RepeatingBackground.
 */

#include <vde/Texture.h>
#include <vde/api/TileMap.h>
#include <vde/api/TileMapImport.h>

#include <memory>
#include <stdexcept>
#include <variant>
#include <vector>

#include <gtest/gtest.h>

namespace vde::test {

namespace {

std::shared_ptr<Texture> makeTestTexture(uint32_t width, uint32_t height) {
    auto texture = std::make_shared<Texture>();
    std::vector<uint8_t> pixels(static_cast<size_t>(width) * height * 4, 255);
    texture->loadFromData(pixels.data(), width, height);
    return texture;
}

std::shared_ptr<SpriteSheet> makeTestTileSet(int columns, int rows) {
    return SpriteSheet::createGrid(
        makeTestTexture(static_cast<uint32_t>(columns * 16), static_cast<uint32_t>(rows * 16)),
        columns, rows);
}

}  // namespace

TEST(TileMapTest, ConstructorCreatesBaseLayer) {
    TileMap map(1.0f, 2.0f, 8, 6);

    EXPECT_EQ(map.getLayerCount(), 1);
    EXPECT_FLOAT_EQ(map.getTileWidth(), 1.0f);
    EXPECT_FLOAT_EQ(map.getTileHeight(), 2.0f);
    EXPECT_EQ(map.getColumnCount(), 8);
    EXPECT_EQ(map.getRowCount(), 6);
    EXPECT_TRUE(map.getLayerInfo(0).name == "base");
}

TEST(TileMapTest, InvalidConstructorThrows) {
    EXPECT_THROW(TileMap(0.0f, 1.0f, 4, 4), std::invalid_argument);
    EXPECT_THROW(TileMap(1.0f, 1.0f, 0, 4), std::invalid_argument);
}

TEST(TileMapTest, BaseLayerSetAndGetTile) {
    TileMap map(1.0f, 1.0f, 4, 4);
    map.setTile(2, 1, 3);

    EXPECT_EQ(map.getTile(2, 1), 3);
    EXPECT_EQ(map.getTile(0, 0), TileMap::kEmptyTile);
}

TEST(TileMapTest, AdditionalLayerStoresIndependentTiles) {
    TileMap map(1.0f, 1.0f, 4, 4);
    const int foreground = map.addLayer("foreground");

    map.setTile(0, 0, 1);
    map.setTile(foreground, 0, 0, 2);

    EXPECT_EQ(map.getTile(0, 0), 1);
    EXPECT_EQ(map.getTile(foreground, 0, 0), 2);
}

TEST(TileMapTest, FillRegionUsesInclusiveBounds) {
    TileMap map(1.0f, 1.0f, 5, 5);
    map.fillRegion(1, 1, 3, 2, 4);

    EXPECT_EQ(map.getTile(1, 1), 4);
    EXPECT_EQ(map.getTile(3, 2), 4);
    EXPECT_EQ(map.getTile(0, 0), TileMap::kEmptyTile);
}

TEST(TileMapTest, LoadFromArrayValidatesSize) {
    TileMap map(1.0f, 1.0f, 3, 2);
    std::vector<int> tiles = {0, 1, 2, 3, 4};

    EXPECT_THROW(map.loadFromArray(tiles), std::invalid_argument);
}

TEST(TileMapTest, SpriteSheetValidationRejectsOutOfRangeTileIds) {
    TileMap map(1.0f, 1.0f, 2, 2);
    map.setTile(0, 0, 3);

    EXPECT_THROW(map.setTileSet(makeTestTileSet(1, 1)), std::out_of_range);
}

TEST(TileMapTest, ComputeVisibleBoundsClampsToMap) {
    TileMap map(2.0f, 1.0f, 10, 6);
    map.setPosition(4.0f, 3.0f, 0.0f);

    Rect2D rect{.left = 5.0f, .right = 11.9f, .bottom = 4.1f, .top = 7.0f};
    TileVisibilityBounds bounds = map.computeVisibleBounds(rect);

    EXPECT_EQ(bounds.minColumn, 0);
    EXPECT_EQ(bounds.maxColumn, 3);
    EXPECT_EQ(bounds.minRow, 1);
    EXPECT_EQ(bounds.maxRow, 3);
}

TEST(TileMapTest, ComputeVisibleBoundsReturnsEmptyWhenOutsideMap) {
    TileMap map(1.0f, 1.0f, 4, 4);
    map.setPosition(10.0f, 10.0f, 0.0f);

    TileVisibilityBounds bounds =
        map.computeVisibleBounds(Rect2D{.left = 0.0f, .right = 4.0f, .bottom = 0.0f, .top = 4.0f});

    EXPECT_TRUE(bounds.empty());
}

TEST(TileMapTest, ExtractCollisionRectsMergesSolidBlocks) {
    TileMap map(1.0f, 1.0f, 5, 4);
    map.setCollisionKind(1, TileCollisionKind::Solid);
    map.fillRegion(1, 0, 3, 1, 1);

    std::vector<TileCollisionRect> rects = map.extractCollisionRects();

    ASSERT_EQ(rects.size(), 1u);
    const auto& rect = rects.front();
    EXPECT_EQ(rect.kind, TileCollisionKind::Solid);
    EXPECT_FLOAT_EQ(rect.center.x, 2.5f);
    EXPECT_FLOAT_EQ(rect.center.y, 1.0f);
    EXPECT_FLOAT_EQ(rect.halfExtents.x, 1.5f);
    EXPECT_FLOAT_EQ(rect.halfExtents.y, 1.0f);
}

TEST(TileMapTest, ExtractCollisionRectsKeepsOneWayRowsSeparate) {
    TileMap map(1.0f, 1.0f, 6, 4);
    map.setCollisionKind(2, TileCollisionKind::OneWay);
    map.fillRegion(1, 1, 3, 1, 2);
    map.fillRegion(1, 2, 3, 2, 2);

    std::vector<TileCollisionRect> rects = map.extractCollisionRects();

    ASSERT_EQ(rects.size(), 2u);
    const auto& lowerRect = rects.front();
    const auto& upperRect = rects.at(1);
    EXPECT_EQ(lowerRect.kind, TileCollisionKind::OneWay);
    EXPECT_EQ(upperRect.kind, TileCollisionKind::OneWay);
    EXPECT_FLOAT_EQ(lowerRect.halfExtents.x, 1.5f);
    EXPECT_FLOAT_EQ(lowerRect.halfExtents.y, 0.5f);
    EXPECT_NE(lowerRect.center.y, upperRect.center.y);
}

TEST(TileMapTest, ExtractCollisionRectsFiltersByLayer) {
    TileMap map(1.0f, 1.0f, 4, 4);
    int topLayer = map.addLayer("top");
    map.setCollisionKind(1, TileCollisionKind::Solid);
    map.setTile(0, 0, 1);
    map.setTile(topLayer, 1, 1, 1);

    std::vector<TileCollisionRect> baseRects = map.extractCollisionRects(0);
    std::vector<TileCollisionRect> topRects = map.extractCollisionRects(topLayer);

    ASSERT_EQ(baseRects.size(), 1u);
    ASSERT_EQ(topRects.size(), 1u);
    EXPECT_EQ(baseRects.front().layerIndex, 0);
    EXPECT_EQ(topRects.front().layerIndex, topLayer);
}

TEST(TileMapImportTest, ImportTiledJsonBuildsTileMapAndFlipsRows) {
    auto texture = makeTestTileSet(2, 2)->getTexture();

    const std::string jsonText = R"json(
{
    "type": "map",
    "orientation": "orthogonal",
    "renderorder": "right-down",
    "width": 3,
    "height": 2,
    "tilewidth": 16,
    "tileheight": 16,
    "layers": [
        {
            "type": "tilelayer",
            "name": "ground",
            "visible": true,
            "data": [1, 2, 0, 3, 4, 0]
        },
        {
            "type": "tilelayer",
            "name": "decor",
            "visible": false,
            "data": [0, 0, 0, 0, 1, 0]
        }
    ],
    "tilesets": [
        {
            "firstgid": 1,
            "name": "terrain",
            "tilewidth": 16,
            "tileheight": 16,
            "tilecount": 4,
            "columns": 2,
            "image": "terrain.png",
            "imagewidth": 32,
            "imageheight": 32,
            "tiles": [
                {
                    "id": 0,
                    "properties": [
                        {"name": "collision", "type": "string", "value": "solid"}
                    ]
                },
                {
                    "id": 3,
                    "properties": [
                        {"name": "collision", "type": "string", "value": "oneway"}
                    ]
                }
            ]
        }
    ]
}
)json";

    TileMapImportOptions options;
    options.layerDepthStep = 0.07f;
    auto imported = TileMapImport::importTiledJson(texture, jsonText, options);

    ASSERT_NE(imported.tileMap, nullptr);
    EXPECT_EQ(imported.tileMap->getColumnCount(), 3);
    EXPECT_EQ(imported.tileMap->getRowCount(), 2);
    EXPECT_EQ(imported.tileMap->getLayerCount(), 2);
    EXPECT_EQ(imported.tileMap->getLayerInfo(0).name, "ground");
    EXPECT_EQ(imported.tileMap->getLayerInfo(1).name, "decor");
    EXPECT_FALSE(imported.tileMap->isLayerVisible(1));
    EXPECT_FLOAT_EQ(imported.tileMap->getLayerDepth(1), 0.07f);

    EXPECT_EQ(imported.tileMap->getTile(0, 0), 2);
    EXPECT_EQ(imported.tileMap->getTile(1, 0), 3);
    EXPECT_EQ(imported.tileMap->getTile(0, 1), 0);
    EXPECT_EQ(imported.tileMap->getTile(1, 1), 1);
    EXPECT_EQ(imported.tileMap->getTile(2, 1), TileMap::kEmptyTile);

    ASSERT_NE(imported.tileMap->getTileSet(), nullptr);
    EXPECT_EQ(imported.tileMap->getTileSet()->getSpriteCount(), 4);
    EXPECT_EQ(imported.tileMap->getCollisionKind(0), TileCollisionKind::Solid);
    EXPECT_EQ(imported.tileMap->getCollisionKind(3), TileCollisionKind::OneWay);
}

TEST(TileMapImportTest, ImportTiledJsonFileLoadsCheckedInSample) {
    const std::string mapPath = std::string(VDE_ASSETS_DIR) + "/tiled/tilemap_demo.tmj";

    auto imported = TileMapImport::importTiledJsonFile(nullptr, mapPath);

    ASSERT_NE(imported.tileMap, nullptr);
    EXPECT_EQ(imported.tileMap->getColumnCount(), 128);
    EXPECT_EQ(imported.tileMap->getRowCount(), 24);
    EXPECT_EQ(imported.tileMap->getLayerCount(), 2);
    EXPECT_EQ(imported.tileMap->getLayerInfo(0).name, "ground");
    EXPECT_EQ(imported.tileMap->getLayerInfo(1).name, "accents");
    EXPECT_EQ(imported.tileMap->getCollisionKind(0), TileCollisionKind::Solid);
    EXPECT_EQ(imported.tileMap->getCollisionKind(3), TileCollisionKind::OneWay);

    ASSERT_EQ(imported.objects.size(), 1u);
    EXPECT_EQ(imported.objects.front().name, "spawn");
    EXPECT_TRUE(imported.objects.front().point);
    EXPECT_NEAR(imported.objects.front().position.x, 4.5f, 0.0001f);
    EXPECT_NEAR(imported.objects.front().position.y, 7.0f, 0.0001f);
}

TEST(TileMapImportTest, ImportTiledJsonExtractsPointAndRectangleObjects) {
    auto texture = makeTestTileSet(2, 1)->getTexture();

    const std::string jsonText = R"json(
{
    "type": "map",
    "orientation": "orthogonal",
    "renderorder": "right-down",
    "width": 2,
    "height": 2,
    "tilewidth": 16,
    "tileheight": 16,
    "layers": [
        {
            "type": "tilelayer",
            "name": "ground",
            "data": [1, 0, 0, 0]
        },
        {
            "type": "objectgroup",
            "name": "markers",
            "objects": [
                {
                    "id": 7,
                    "name": "spawn",
                    "type": "spawn",
                    "point": true,
                    "x": 24,
                    "y": 8,
                    "properties": [
                        {"name": "facing", "type": "string", "value": "right"}
                    ]
                },
                {
                    "id": 8,
                    "name": "gate",
                    "x": 16,
                    "y": 16,
                    "width": 16,
                    "height": 16,
                    "properties": [
                        {"name": "collision", "type": "string", "value": "solid"},
                        {"name": "enabled", "type": "bool", "value": true},
                        {"name": "priority", "type": "int", "value": 3},
                        {"name": "weight", "type": "float", "value": 1.5}
                    ]
                }
            ]
        }
    ],
    "tilesets": [
        {
            "firstgid": 1,
            "name": "terrain",
            "tilewidth": 16,
            "tileheight": 16,
            "tilecount": 2,
            "columns": 2,
            "image": "terrain.png",
            "imagewidth": 32,
            "imageheight": 16
        }
    ]
}
)json";

    auto imported = TileMapImport::importTiledJson(texture, jsonText);

    ASSERT_EQ(imported.objects.size(), 2u);

    const auto& spawn = imported.objects.at(0);
    EXPECT_TRUE(spawn.point);
    EXPECT_EQ(spawn.name, "spawn");
    EXPECT_EQ(spawn.layerName, "markers");
    EXPECT_NEAR(spawn.position.x, 1.5f, 0.0001f);
    EXPECT_NEAR(spawn.position.y, 1.5f, 0.0001f);
    ASSERT_TRUE(spawn.properties.contains("facing"));
    EXPECT_EQ(std::get<std::string>(spawn.properties.at("facing")), "right");

    const auto& gate = imported.objects.at(1);
    EXPECT_FALSE(gate.point);
    EXPECT_NEAR(gate.position.x, 1.0f, 0.0001f);
    EXPECT_NEAR(gate.position.y, 0.0f, 0.0001f);
    EXPECT_NEAR(gate.size.x, 1.0f, 0.0001f);
    EXPECT_NEAR(gate.size.y, 1.0f, 0.0001f);
    EXPECT_EQ(gate.collisionKind, TileCollisionKind::Solid);
    ASSERT_TRUE(gate.properties.contains("enabled"));
    ASSERT_TRUE(gate.properties.contains("priority"));
    ASSERT_TRUE(gate.properties.contains("weight"));
    EXPECT_TRUE(std::get<bool>(gate.properties.at("enabled")));
    EXPECT_EQ(std::get<int>(gate.properties.at("priority")), 3);
    EXPECT_FLOAT_EQ(std::get<float>(gate.properties.at("weight")), 1.5f);
}

TEST(TileMapImportTest, ImportTiledJsonRejectsRectangleObjectsOutsideBounds) {
    auto texture = makeTestTileSet(2, 1)->getTexture();

    const std::string jsonText = R"json(
{
    "type": "map",
    "orientation": "orthogonal",
    "renderorder": "right-down",
    "width": 2,
    "height": 2,
    "tilewidth": 16,
    "tileheight": 16,
    "layers": [
        {
            "type": "tilelayer",
            "name": "ground",
            "data": [1, 0, 0, 0]
        },
        {
            "type": "objectgroup",
            "name": "markers",
            "objects": [
                {
                    "id": 8,
                    "name": "gate",
                    "x": 16,
                    "y": 16,
                    "width": 17,
                    "height": 17
                }
            ]
        }
    ],
    "tilesets": [
        {
            "firstgid": 1,
            "name": "terrain",
            "tilewidth": 16,
            "tileheight": 16,
            "tilecount": 2,
            "columns": 2,
            "image": "terrain.png",
            "imagewidth": 32,
            "imageheight": 16
        }
    ]
}
)json";

    try {
        (void)TileMapImport::importTiledJson(texture, jsonText);
        FAIL() << "Expected import to reject rectangle objects outside map bounds";
    } catch (const std::invalid_argument& ex) {
        EXPECT_NE(std::string(ex.what()).find("map bounds"), std::string::npos);
    }
}

TEST(TileMapImportTest, ImportTiledJsonRejectsMapsWithoutTileLayers) {
    auto texture = makeTestTileSet(1, 1)->getTexture();

    const std::string jsonText = R"json(
{
    "type": "map",
    "orientation": "orthogonal",
    "renderorder": "right-down",
    "width": 1,
    "height": 1,
    "tilewidth": 16,
    "tileheight": 16,
    "layers": [
        {
            "type": "objectgroup",
            "name": "markers",
            "objects": [
                {
                    "id": 1,
                    "point": true,
                    "x": 0,
                    "y": 0
                }
            ]
        }
    ],
    "tilesets": [
        {
            "firstgid": 1,
            "name": "terrain",
            "tilewidth": 16,
            "tileheight": 16,
            "tilecount": 1,
            "columns": 1,
            "image": "terrain.png",
            "imagewidth": 16,
            "imageheight": 16
        }
    ]
}
)json";

    try {
        (void)TileMapImport::importTiledJson(texture, jsonText);
        FAIL() << "Expected import to reject maps without tile layers";
    } catch (const std::invalid_argument& ex) {
        EXPECT_NE(std::string(ex.what()).find("at least one tile layer"), std::string::npos);
    }
}

TEST(TileMapImportTest, ImportTiledJsonRejectsUnsupportedInfiniteMaps) {
    auto texture = makeTestTileSet(1, 1)->getTexture();

    const std::string jsonText = R"json(
{
    "type": "map",
    "orientation": "orthogonal",
    "renderorder": "right-down",
    "infinite": true,
    "width": 1,
    "height": 1,
    "tilewidth": 16,
    "tileheight": 16,
    "layers": [
        {"type": "tilelayer", "name": "ground", "data": [1]}
    ],
    "tilesets": [
        {
            "firstgid": 1,
            "name": "terrain",
            "tilewidth": 16,
            "tileheight": 16,
            "tilecount": 1,
            "columns": 1,
            "image": "terrain.png",
            "imagewidth": 16,
            "imageheight": 16
        }
    ]
}
)json";

    try {
        (void)TileMapImport::importTiledJson(texture, jsonText);
        FAIL() << "Expected import to reject infinite maps";
    } catch (const std::invalid_argument& ex) {
        EXPECT_NE(std::string(ex.what()).find("finite orthogonal"), std::string::npos);
    }
}

TEST(RepeatingBackgroundTest, InvalidArgumentsThrow) {
    auto texture = makeTestTexture(16, 16);

    EXPECT_THROW(RepeatingBackground(nullptr, 1.0f, 1, 1), std::invalid_argument);
    EXPECT_THROW(RepeatingBackground(texture, 0.0f, 1, 1), std::invalid_argument);
    EXPECT_THROW(RepeatingBackground(texture, 1.0f, 0, 1), std::invalid_argument);
}

TEST(RepeatingBackgroundTest, ScrollOffsetAdvancesWithVelocity) {
    auto texture = makeTestTexture(16, 16);
    RepeatingBackground background(texture, 2.0f, 3, 2);
    background.setScrollVelocity(4.0f, -2.0f);

    background.update(0.5f);
    glm::vec2 offset = background.getScrollOffset();

    EXPECT_FLOAT_EQ(offset.x, 2.0f);
    EXPECT_FLOAT_EQ(offset.y, -1.0f);
}

TEST(RepeatingBackgroundTest, ScrollOffsetWrapsToPatternSize) {
    auto texture = makeTestTexture(16, 16);
    RepeatingBackground background(texture, 2.0f, 3, 2);
    background.setScrollOffset(7.5f, 5.0f);

    glm::vec2 offset = background.getScrollOffset();
    EXPECT_FLOAT_EQ(offset.x, 1.5f);
    EXPECT_FLOAT_EQ(offset.y, 1.0f);
}

}  // namespace vde::test