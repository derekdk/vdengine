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

}  // namespace levelbuilder::test