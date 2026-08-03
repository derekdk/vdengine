/**
 * @file TileMapSession_test.cpp
 * @brief Unit tests for levelbuilder::TileMapSession persistence helpers.
 */

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <system_error>

#include "../games/level_builder/TileMapSession.h"
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

namespace levelbuilder::test {
namespace {

std::shared_ptr<vde::TileMap> makeEditableMap() {
    auto tileMap = std::make_shared<vde::TileMap>(1.0f, 1.0f, 3, 2);
    tileMap->setLayerName(0, "ground");

    tileMap->setTile(0, 0, 1);
    tileMap->setTile(1, 0, 2);
    tileMap->setTile(2, 0, 3);
    tileMap->setTile(0, 1, 4);
    tileMap->setTile(1, 1, 5);
    tileMap->setTile(2, 1, 6);

    return tileMap;
}

std::shared_ptr<vde::TileMap> makeImportedMultiLayerMap() {
    auto tileMap = std::make_shared<vde::TileMap>(1.0f, 1.0f, 3, 2);
    tileMap->setLayerName(0, "ground");

    tileMap->setTile(0, 0, 1);
    tileMap->setTile(1, 0, 2);
    tileMap->setTile(2, 0, 3);
    tileMap->setTile(0, 1, 4);
    tileMap->setTile(1, 1, 5);
    tileMap->setTile(2, 1, 6);

    const int accentsLayer = tileMap->addLayer("accents");
    tileMap->setLayerDepth(accentsLayer, 0.35f);
    tileMap->setLayerVisible(accentsLayer, true);
    tileMap->setTile(accentsLayer, 1, 0, 8);
    tileMap->setTile(accentsLayer, 2, 1, 9);
    tileMap->setPosition(2.0f, 3.0f, -0.4f);

    return tileMap;
}

std::shared_ptr<vde::TileMap> makeCollisionMultiLayerMap() {
    auto tileMap = std::make_shared<vde::TileMap>(1.0f, 1.0f, 3, 2);
    tileMap->setLayerName(0, "ground");
    tileMap->setCollisionKind(1, vde::TileCollisionKind::Solid);
    tileMap->setTile(0, 0, 0, 1);

    const int decorationsLayer = tileMap->addLayer("decorations");
    tileMap->setCollisionKind(2, vde::TileCollisionKind::Solid);
    tileMap->setTile(decorationsLayer, 2, 1, 2);
    return tileMap;
}

std::filesystem::path makeTempOverlayPath() {
    const auto uniqueStamp =
        std::filesystem::file_time_type::clock::now().time_since_epoch().count();
    return std::filesystem::temp_directory_path() /
           ("vde_level_builder_overlay_" + std::to_string(uniqueStamp) + ".json");
}

}  // namespace

TEST(TileMapSessionTest, SaveAndReloadOverlayRestoresEditableLayerState) {
    TileMapSession session;
    const std::filesystem::path overlayPath = makeTempOverlayPath();
    session.setOverlayPath(overlayPath);
    session.adoptTileMap(makeEditableMap(), {2.0f, 3.0f}, 1u, "test-map");

    EXPECT_FALSE(session.hasUnsavedChanges());
    ASSERT_TRUE(session.setEditableTileId({1, 0}, 9));
    EXPECT_TRUE(session.hasUnsavedChanges());

    ASSERT_TRUE(session.saveEditableLayerOverlay());
    EXPECT_FALSE(session.hasUnsavedChanges());
    EXPECT_EQ(session.editableTileId({1, 0}), 9);

    ASSERT_TRUE(session.setEditableTileId({1, 0}, 2));
    ASSERT_TRUE(session.setEditableTileId({2, 1}, 1));
    EXPECT_TRUE(session.hasUnsavedChanges());

    ASSERT_TRUE(session.reloadEditableLayerOverlay());
    EXPECT_EQ(session.editableTileId({1, 0}), 9);
    EXPECT_EQ(session.editableTileId({2, 1}), 6);
    EXPECT_FALSE(session.hasUnsavedChanges());

    std::error_code error;
    std::filesystem::remove(overlayPath, error);
}

TEST(TileMapSessionTest, ReloadWithoutOverlayFallsBackToImportedLayer) {
    TileMapSession session;
    const std::filesystem::path overlayPath = makeTempOverlayPath();
    session.setOverlayPath(overlayPath);
    session.adoptTileMap(makeEditableMap(), {0.0f, 0.0f}, 0u, "test-map");

    ASSERT_TRUE(session.setEditableTileId({0, 0}, 8));
    ASSERT_TRUE(session.setEditableTileId({2, 1}, 2));

    ASSERT_TRUE(session.reloadEditableLayerOverlay());
    EXPECT_EQ(session.editableTileId({0, 0}), 1);
    EXPECT_EQ(session.editableTileId({2, 1}), 6);
    EXPECT_FALSE(session.hasUnsavedChanges());
    EXPECT_NE(session.lastPersistenceStatus().find("No saved overlay found"), std::string::npos);

    std::error_code error;
    std::filesystem::remove(overlayPath, error);
}

TEST(TileMapSessionTest, ReloadFailurePreservesEditableLayerStateAndUndoHistory) {
    TileMapSession session;
    const std::filesystem::path overlayPath = makeTempOverlayPath();
    session.setOverlayPath(overlayPath);
    session.adoptTileMap(makeEditableMap(), {0.0f, 0.0f}, 0u, "test-map");

    ASSERT_TRUE(session.setEditableTileId({0, 0}, 8));
    ASSERT_EQ(session.undoDepth(), 1u);
    ASSERT_TRUE(session.hasUnsavedChanges());

    std::ofstream output(overlayPath, std::ios::binary | std::ios::trunc);
    ASSERT_TRUE(output.is_open());
    output << "{";
    output.close();

    EXPECT_FALSE(session.reloadEditableLayerOverlay());
    EXPECT_EQ(session.editableTileId({0, 0}), 8);
    EXPECT_TRUE(session.hasUnsavedChanges());
    EXPECT_TRUE(session.canUndoEditableEdit());
    EXPECT_EQ(session.undoDepth(), 1u);

    std::error_code error;
    std::filesystem::remove(overlayPath, error);
}

TEST(TileMapSessionTest, UndoAndRedoRestoreEditableLayerState) {
    TileMapSession session;
    session.adoptTileMap(makeEditableMap(), {1.0f, 1.0f}, 0u, "test-map");

    ASSERT_TRUE(session.setEditableTileId({1, 0}, 9));
    ASSERT_TRUE(session.setEditableTileId({2, 1}, 1));
    EXPECT_TRUE(session.canUndoEditableEdit());
    EXPECT_FALSE(session.canRedoEditableEdit());
    EXPECT_EQ(session.undoDepth(), 2u);
    EXPECT_EQ(session.redoDepth(), 0u);

    ASSERT_TRUE(session.undoLastEditableEdit());
    EXPECT_EQ(session.editableTileId({1, 0}), 9);
    EXPECT_EQ(session.editableTileId({2, 1}), 6);
    EXPECT_EQ(session.undoDepth(), 1u);
    EXPECT_EQ(session.redoDepth(), 1u);

    ASSERT_TRUE(session.undoLastEditableEdit());
    EXPECT_EQ(session.editableTileId({1, 0}), 2);
    EXPECT_EQ(session.undoDepth(), 0u);
    EXPECT_EQ(session.redoDepth(), 2u);

    ASSERT_TRUE(session.redoLastEditableEdit());
    EXPECT_EQ(session.editableTileId({1, 0}), 9);
    EXPECT_EQ(session.undoDepth(), 1u);
    EXPECT_EQ(session.redoDepth(), 1u);
}

TEST(TileMapSessionTest, UndoToSavedStateClearsDirtyFlagAndBranchEditDropsRedoHistory) {
    TileMapSession session;
    const std::filesystem::path overlayPath = makeTempOverlayPath();
    session.setOverlayPath(overlayPath);
    session.adoptTileMap(makeEditableMap(), {1.0f, 1.0f}, 0u, "test-map");

    ASSERT_TRUE(session.setEditableTileId({0, 0}, 8));
    ASSERT_TRUE(session.saveEditableLayerOverlay());
    EXPECT_FALSE(session.hasUnsavedChanges());

    ASSERT_TRUE(session.setEditableTileId({1, 1}, 7));
    EXPECT_TRUE(session.hasUnsavedChanges());
    EXPECT_FALSE(session.canRedoEditableEdit());

    ASSERT_TRUE(session.undoLastEditableEdit());
    EXPECT_FALSE(session.hasUnsavedChanges());
    EXPECT_TRUE(session.canRedoEditableEdit());

    ASSERT_TRUE(session.setEditableTileId({2, 0}, 0));
    EXPECT_TRUE(session.hasUnsavedChanges());
    EXPECT_FALSE(session.canRedoEditableEdit());
    EXPECT_EQ(session.redoDepth(), 0u);

    std::error_code error;
    std::filesystem::remove(overlayPath, error);
}

// --- Phase 1: Multi-layer session model tests ---

TEST(TileMapSessionTest, AddLayerCreatesNewLayerWithCorrectDefaults) {
    TileMapSession session;
    session.adoptTileMap(makeEditableMap(), {0.0f, 0.0f}, 0u, "test-map");

    EXPECT_EQ(session.layerCount(), 1u);
    EXPECT_EQ(session.activeLayerIndex(), 0u);

    const size_t newIndex = session.addLayer("sky");
    EXPECT_EQ(newIndex, 1u);
    EXPECT_EQ(session.layerCount(), 2u);

    const LayerDefinition* def = session.layerDefinition(1);
    ASSERT_NE(def, nullptr);
    EXPECT_EQ(def->name, "sky");
    EXPECT_EQ(def->id, "layer_1");
    EXPECT_FALSE(def->collisionEnabled);
    EXPECT_EQ(def->followFactorX, 1.0f);
    EXPECT_EQ(def->followFactorY, 1.0f);
    EXPECT_EQ(def->scrollVelocityX, 0.0f);
    EXPECT_EQ(def->scrollVelocityY, 0.0f);

    // New layer tiles are all empty.
    EXPECT_TRUE(def->tiles.size() == 3u * 2u);
    for (int t : def->tiles) {
        EXPECT_EQ(t, vde::TileMap::kEmptyTile);
    }

    // Adding a layer marks the session as having unsaved changes.
    EXPECT_TRUE(session.hasUnsavedChanges());
}

TEST(TileMapSessionTest, UnsavedLayerCreationStaysDirtyAfterUndoingLayerEdits) {
    TileMapSession session;
    session.adoptTileMap(makeEditableMap(), {0.0f, 0.0f}, 0u, "test-map");

    const size_t layer1 = session.addLayer("detail");
    ASSERT_TRUE(session.setActiveLayerIndex(layer1));
    EXPECT_TRUE(session.hasUnsavedChanges());

    ASSERT_TRUE(session.setEditableTileId({1, 0}, 7));
    EXPECT_TRUE(session.hasUnsavedChanges());

    ASSERT_TRUE(session.undoLastEditableEdit());
    EXPECT_EQ(session.editableTileId({1, 0}), vde::TileMap::kEmptyTile);
    EXPECT_TRUE(session.hasUnsavedChanges());
}

TEST(TileMapSessionTest, SetActiveLayerIndexSwitchesEditTarget) {
    TileMapSession session;
    session.adoptTileMap(makeEditableMap(), {0.0f, 0.0f}, 0u, "test-map");

    const size_t layer1 = session.addLayer("detail");
    EXPECT_TRUE(session.setActiveLayerIndex(layer1));
    EXPECT_EQ(session.activeLayerIndex(), 1u);

    // Edits on the active layer go to that layer's tile storage.
    ASSERT_TRUE(session.setEditableTileId({0, 0}, 7));
    EXPECT_EQ(session.editableTileId({0, 0}), 7);

    // Layer 0 tile at (0,0) should be unchanged.
    ASSERT_TRUE(session.setActiveLayerIndex(0));
    EXPECT_EQ(session.editableTileId({0, 0}), 1);  // original imported value
}

TEST(TileMapSessionTest, AdoptTileMapCapturesImportedTileLayersIntoSessionModel) {
    TileMapSession session;
    session.adoptTileMap(makeImportedMultiLayerMap(), {0.0f, 0.0f}, 0u, "test-map");

    ASSERT_EQ(session.layerCount(), 2u);

    const LayerDefinition* ground = session.layerDefinition(0);
    const LayerDefinition* accents = session.layerDefinition(1);
    ASSERT_NE(ground, nullptr);
    ASSERT_NE(accents, nullptr);

    EXPECT_EQ(ground->name, "ground");
    EXPECT_EQ(accents->name, "accents");
    EXPECT_FLOAT_EQ(ground->depthZ, 0.0f);
    EXPECT_FLOAT_EQ(accents->depthZ, 0.35f);
    EXPECT_TRUE(ground->visible);
    EXPECT_TRUE(accents->visible);
    EXPECT_EQ(accents->tiles[1], 8);
    EXPECT_EQ(accents->tiles[5], 9);
}

TEST(TileMapSessionTest, MultiLayerSaveAndReloadPreservesAllLayers) {
    TileMapSession session;
    const std::filesystem::path overlayPath = makeTempOverlayPath();
    session.setOverlayPath(overlayPath);
    session.adoptTileMap(makeEditableMap(), {0.0f, 0.0f}, 0u, "test-map");

    // Edit layer 0.
    ASSERT_TRUE(session.setEditableTileId({0, 0}, 9));

    // Add a second layer and edit it.
    const size_t layer1 = session.addLayer("detail");
    ASSERT_TRUE(session.setActiveLayerIndex(layer1));
    ASSERT_TRUE(session.setEditableTileId({1, 1}, 5));
    ASSERT_TRUE(session.setEditableTileId({2, 0}, 3));

    ASSERT_TRUE(session.saveEditableLayerOverlay());
    EXPECT_FALSE(session.hasUnsavedChanges());
    EXPECT_EQ(session.layerCount(), 2u);

    // Mutate both layers, then reload.
    ASSERT_TRUE(session.setEditableTileId({2, 0}, 99));
    ASSERT_TRUE(session.setActiveLayerIndex(0));
    ASSERT_TRUE(session.setEditableTileId({0, 0}, 1));
    EXPECT_TRUE(session.hasUnsavedChanges());

    ASSERT_TRUE(session.reloadEditableLayerOverlay());
    EXPECT_FALSE(session.hasUnsavedChanges());
    EXPECT_EQ(session.layerCount(), 2u);

    // Layer 0 should be restored.
    EXPECT_EQ(session.editableTileId({0, 0}), 9);

    // Layer 1 should be restored.
    ASSERT_TRUE(session.setActiveLayerIndex(1));
    EXPECT_EQ(session.editableTileId({1, 1}), 5);
    EXPECT_EQ(session.editableTileId({2, 0}), 3);

    std::error_code error;
    std::filesystem::remove(overlayPath, error);
}

TEST(TileMapSessionTest, SavedOverlayUsesVersionTwoLayersArraySchema) {
    TileMapSession session;
    const std::filesystem::path overlayPath = makeTempOverlayPath();
    session.setOverlayPath(overlayPath);
    session.adoptTileMap(makeEditableMap(), {0.0f, 0.0f}, 0u, "test-map");

    const size_t layer1 = session.addLayer("parallax");
    ASSERT_TRUE(session.setEditableTileId({0, 0}, 9));
    ASSERT_TRUE(session.setActiveLayerIndex(layer1));
    ASSERT_TRUE(session.setEditableTileId({2, 1}, 4));

    ASSERT_TRUE(session.saveEditableLayerOverlay());

    std::ifstream input(overlayPath, std::ios::binary);
    ASSERT_TRUE(input.is_open());
    nlohmann::ordered_json root = nlohmann::ordered_json::parse(input);

    EXPECT_EQ(root.at("version").get<int>(), 2);
    ASSERT_TRUE(root.contains("layers"));
    ASSERT_TRUE(root.at("layers").is_array());
    ASSERT_EQ(root.at("layers").size(), 2u);
    EXPECT_EQ(root.at("layers").at(0).at("name").get<std::string>(), "ground");
    EXPECT_EQ(root.at("layers").at(1).at("name").get<std::string>(), "parallax");
    EXPECT_EQ(root.at("layers").at(1).at("collision_enabled").get<bool>(), false);
    EXPECT_EQ(root.at("layers").at(1).at("tiles").at(5).get<int>(), 4);

    std::error_code error;
    std::filesystem::remove(overlayPath, error);
}

TEST(TileMapSessionTest, RuntimeTileMapsUseOneEntityPerSessionLayer) {
    TileMapSession session;
    session.adoptTileMap(makeImportedMultiLayerMap(), {0.0f, 0.0f}, 0u, "test-map");

    auto groundRuntime = session.createRuntimeTileMap(0);
    auto accentsRuntime = session.createRuntimeTileMap(1);

    ASSERT_NE(groundRuntime, nullptr);
    ASSERT_NE(accentsRuntime, nullptr);
    EXPECT_EQ(groundRuntime->getLayerCount(), 1);
    EXPECT_EQ(accentsRuntime->getLayerCount(), 1);
    EXPECT_FLOAT_EQ(groundRuntime->getPosition().z, -0.4f);
    EXPECT_FLOAT_EQ(accentsRuntime->getPosition().z, -0.05f);
    EXPECT_EQ(groundRuntime->getTile(1, 0), 2);
    EXPECT_EQ(accentsRuntime->getTile(1, 0), 8);
    EXPECT_EQ(accentsRuntime->getTile(2, 1), 9);
}

TEST(TileMapSessionTest, SyncRuntimeTileMapRefreshesOnlyEditedLayer) {
    TileMapSession session;
    session.adoptTileMap(makeImportedMultiLayerMap(), {0.0f, 0.0f}, 0u, "test-map");

    auto groundRuntime = session.createRuntimeTileMap(0);
    auto accentsRuntime = session.createRuntimeTileMap(1);
    ASSERT_NE(groundRuntime, nullptr);
    ASSERT_NE(accentsRuntime, nullptr);

    ASSERT_TRUE(session.setActiveLayerIndex(1));
    ASSERT_TRUE(session.setEditableTileId({2, 1}, 4));
    ASSERT_TRUE(session.syncRuntimeTileMap(1, *accentsRuntime));

    EXPECT_EQ(accentsRuntime->getTile(2, 1), 4);
    EXPECT_EQ(groundRuntime->getTile(2, 1), 6);
}

TEST(TileMapSessionTest, LayerMetadataChangesSetDirtyAndPersistAcrossSaveLoad) {
    TileMapSession session;
    const std::filesystem::path overlayPath = makeTempOverlayPath();
    session.setOverlayPath(overlayPath);
    session.adoptTileMap(makeImportedMultiLayerMap(), {0.0f, 0.0f}, 0u, "test-map");

    ASSERT_TRUE(session.setLayerVisibility(1, false));
    ASSERT_TRUE(session.adjustLayerDepthZ(1, 0.20f));
    EXPECT_TRUE(session.hasUnsavedChanges());

    ASSERT_TRUE(session.saveEditableLayerOverlay());
    EXPECT_FALSE(session.hasUnsavedChanges());

    ASSERT_TRUE(session.setLayerVisibility(1, true));
    ASSERT_TRUE(session.setLayerDepthZ(1, 0.10f));
    EXPECT_TRUE(session.hasUnsavedChanges());

    ASSERT_TRUE(session.reloadEditableLayerOverlay());
    const LayerDefinition* accents = session.layerDefinition(1);
    ASSERT_NE(accents, nullptr);
    EXPECT_FALSE(accents->visible);
    EXPECT_FLOAT_EQ(accents->depthZ, 0.55f);
    EXPECT_FALSE(session.hasUnsavedChanges());

    std::error_code error;
    std::filesystem::remove(overlayPath, error);
}

TEST(TileMapSessionTest, SyncRuntimeTileMapRefreshesDepthAndVisibilityChanges) {
    TileMapSession session;
    session.adoptTileMap(makeImportedMultiLayerMap(), {0.0f, 0.0f}, 0u, "test-map");

    auto accentsRuntime = session.createRuntimeTileMap(1);
    ASSERT_NE(accentsRuntime, nullptr);

    ASSERT_TRUE(session.setLayerVisibility(1, false));
    ASSERT_TRUE(session.adjustLayerDepthZ(1, 0.25f));
    ASSERT_TRUE(session.syncRuntimeTileMap(1, *accentsRuntime));

    EXPECT_FALSE(accentsRuntime->isLayerVisible(0));
    EXPECT_FLOAT_EQ(accentsRuntime->getPosition().z, 0.20f);
}

TEST(TileMapSessionTest, ScrollPresetChangesPersistAndRestoreSavedMetadata) {
    TileMapSession session;
    const std::filesystem::path overlayPath = makeTempOverlayPath();
    session.setOverlayPath(overlayPath);
    session.adoptTileMap(makeImportedMultiLayerMap(), {0.0f, 0.0f}, 0u, "test-map");

    ASSERT_TRUE(session.setLayerScrollPreset(1, LayerScrollPreset::DriftingDecorative));
    EXPECT_TRUE(session.hasUnsavedChanges());
    const LayerDefinition* accents = session.layerDefinition(1);
    ASSERT_NE(accents, nullptr);
    EXPECT_FLOAT_EQ(accents->followFactorX, 0.25f);
    EXPECT_FLOAT_EQ(accents->followFactorY, 0.50f);
    EXPECT_FLOAT_EQ(accents->scrollVelocityX, 0.35f);

    ASSERT_TRUE(session.saveEditableLayerOverlay());
    ASSERT_TRUE(session.setLayerScrollPreset(1, LayerScrollPreset::Gameplay));
    ASSERT_TRUE(session.reloadEditableLayerOverlay());

    accents = session.layerDefinition(1);
    ASSERT_NE(accents, nullptr);
    EXPECT_EQ(session.layerScrollPreset(1), LayerScrollPreset::DriftingDecorative);
    EXPECT_FLOAT_EQ(accents->scrollVelocityX, 0.35f);
    EXPECT_FALSE(session.hasUnsavedChanges());

    std::error_code error;
    std::filesystem::remove(overlayPath, error);
}

TEST(TileMapSessionTest, RuntimeLayerPositionCombinesCameraFollowSavedBaseAndRuntimeOffsets) {
    TileMapSession session;
    session.adoptTileMap(makeImportedMultiLayerMap(), {0.0f, 0.0f}, 0u, "test-map");
    ASSERT_TRUE(session.setLayerScrollPreset(1, LayerScrollPreset::StrongParallax));

    const auto position = session.runtimeLayerPosition(1, {10.0f, 4.0f}, {1.0f, -2.0f});

    ASSERT_TRUE(position.has_value());
    EXPECT_FLOAT_EQ(position->x, 9.0f);
    EXPECT_FLOAT_EQ(position->y, 2.4f);
    EXPECT_FLOAT_EQ(position->z, -0.05f);
}

TEST(TileMapSessionTest, OldV1OverlayLoadsWithPreservedImportedLayers) {
    TileMapSession session;
    const std::filesystem::path overlayPath = makeTempOverlayPath();
    session.setOverlayPath(overlayPath);
    session.adoptTileMap(makeImportedMultiLayerMap(), {0.0f, 0.0f}, 0u, "test-map");

    // Write a v1-format overlay manually.
    const std::string v1Json = R"({
  "format": "vde.level_builder.ground_overlay",
  "version": 1,
  "base_map": "test-map",
  "editable_layer": {
    "name": "ground",
    "columns": 3,
    "rows": 2,
    "tiles": [10, 11, 12, 13, 14, 15]
  }
})";
    {
        std::ofstream out(overlayPath, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(out.is_open());
        out << v1Json;
    }

    ASSERT_TRUE(session.reloadEditableLayerOverlay());
    EXPECT_EQ(session.layerCount(), 2u);
    EXPECT_EQ(session.activeLayerIndex(), 0u);
    EXPECT_FALSE(session.hasUnsavedChanges());

    // Tiles from the v1 overlay should be applied to layer 0.
    EXPECT_EQ(session.editableTileId({0, 0}), 10);
    EXPECT_EQ(session.editableTileId({1, 0}), 11);
    EXPECT_EQ(session.editableTileId({2, 1}), 15);

    ASSERT_TRUE(session.setActiveLayerIndex(1));
    EXPECT_EQ(session.editableTileId({1, 0}), 8);
    EXPECT_EQ(session.editableTileId({2, 1}), 9);

    std::error_code error;
    std::filesystem::remove(overlayPath, error);
}

TEST(TileMapSessionTest, TileEditRecordIncludesLayerIdentityForUndoRedo) {
    TileMapSession session;
    session.adoptTileMap(makeEditableMap(), {0.0f, 0.0f}, 0u, "test-map");

    const size_t layer1 = session.addLayer("detail");

    // Edit on layer 0.
    ASSERT_TRUE(session.setEditableTileId({0, 0}, 8));

    // Switch to layer 1 and edit.
    ASSERT_TRUE(session.setActiveLayerIndex(layer1));
    ASSERT_TRUE(session.setEditableTileId({1, 0}, 7));

    EXPECT_EQ(session.undoDepth(), 2u);

    // Undo the layer-1 edit.
    ASSERT_TRUE(session.undoLastEditableEdit());
    EXPECT_EQ(session.lastEditedLayerIndex(), layer1);
    ASSERT_TRUE(session.setActiveLayerIndex(layer1));
    EXPECT_EQ(session.editableTileId({1, 0}), vde::TileMap::kEmptyTile);

    // Undo the layer-0 edit.
    ASSERT_TRUE(session.undoLastEditableEdit());
    EXPECT_EQ(session.lastEditedLayerIndex(), 0u);
    ASSERT_TRUE(session.setActiveLayerIndex(0));
    EXPECT_EQ(session.editableTileId({0, 0}), 1);  // original value

    // Redo both.
    ASSERT_TRUE(session.redoLastEditableEdit());
    EXPECT_EQ(session.lastEditedLayerIndex(), 0u);
    EXPECT_EQ(session.editableTileId({0, 0}), 8);

    ASSERT_TRUE(session.redoLastEditableEdit());
    EXPECT_EQ(session.lastEditedLayerIndex(), layer1);
    ASSERT_TRUE(session.setActiveLayerIndex(layer1));
    EXPECT_EQ(session.editableTileId({1, 0}), 7);
}

TEST(TileMapSessionTest, CollisionCacheExcludesDisabledLayers) {
    TileMapSession session;
    const std::filesystem::path overlayPath = makeTempOverlayPath();
    session.setOverlayPath(overlayPath);
    session.adoptTileMap(makeCollisionMultiLayerMap(), {0.0f, 0.0f}, 0u, "test-map");

    ASSERT_EQ(session.solidRects().size(), 2u);
    ASSERT_TRUE(session.saveEditableLayerOverlay());

    nlohmann::ordered_json root;
    {
        std::ifstream input(overlayPath, std::ios::binary);
        ASSERT_TRUE(input.is_open());
        root = nlohmann::ordered_json::parse(input);
    }
    root.at("layers").at(1).at("collision_enabled") = false;
    {
        std::ofstream output(overlayPath, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(output.is_open());
        output << root.dump(2);
    }

    ASSERT_TRUE(session.reloadEditableLayerOverlay());
    EXPECT_EQ(session.solidRects().size(), 1u);

    std::error_code error;
    std::filesystem::remove(overlayPath, error);
}

TEST(TileMapSessionTest, LayerDefinitionDefaultsAfterAdoptTileMap) {
    TileMapSession session;
    session.adoptTileMap(makeEditableMap(), {0.0f, 0.0f}, 0u, "test-map");

    EXPECT_EQ(session.layerCount(), 1u);
    const LayerDefinition* def = session.layerDefinition(0);
    ASSERT_NE(def, nullptr);
    EXPECT_EQ(def->id, "layer_0");
    EXPECT_EQ(def->name, "ground");
    EXPECT_TRUE(def->collisionEnabled);
    EXPECT_TRUE(def->visible);
    EXPECT_EQ(def->depthZ, 0.0f);
    EXPECT_EQ(def->followFactorX, 1.0f);
    EXPECT_EQ(def->followFactorY, 1.0f);
    EXPECT_EQ(def->scrollVelocityX, 0.0f);
    EXPECT_EQ(def->scrollVelocityY, 0.0f);
    ASSERT_EQ(def->tiles.size(), 6u);
    EXPECT_EQ(def->tiles[0], 1);
    EXPECT_EQ(def->tiles[1], 2);
    EXPECT_EQ(def->tiles[2], 3);
    EXPECT_EQ(def->tiles[3], 4);
    EXPECT_EQ(def->tiles[4], 5);
    EXPECT_EQ(def->tiles[5], 6);

    EXPECT_EQ(session.layerDefinition(1), nullptr);
}

}  // namespace levelbuilder::test