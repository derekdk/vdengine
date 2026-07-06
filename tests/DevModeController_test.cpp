/**
 * @file DevModeController_test.cpp
 * @brief Unit tests for levelbuilder::DevModeController.
 */

#include "../games/level_builder/DevModeController.h"
#include <gtest/gtest.h>

namespace levelbuilder::test {

TEST(DevModeControllerTest, EnterDefaultsToMoveModeAndCopiesPosition) {
    DevModeController controller;

    controller.enter({2.5f, 3.5f});

    EXPECT_TRUE(controller.isEnabled());
    EXPECT_EQ(controller.activeSubmode(), DevelopmentSubmode::MoveMode);
    EXPECT_STREQ(controller.activeSubmodeName(), "Move");
    EXPECT_FLOAT_EQ(controller.position().x, 2.5f);
    EXPECT_FLOAT_EQ(controller.position().y, 3.5f);
}

TEST(DevModeControllerTest, UpdateMovesFreelyWhileEnabled) {
    DevModeController controller;
    controller.enter({1.0f, 2.0f});

    controller.updateMoveMode(0.5f, {1.0f, 0.0f});

    EXPECT_GT(controller.position().x, 1.0f);
    EXPECT_FLOAT_EQ(controller.position().y, 2.0f);
}

TEST(DevModeControllerTest, DisabledControllerDoesNotMove) {
    DevModeController controller;
    controller.setPosition({4.0f, -1.0f});

    controller.updateMoveMode(1.0f, {0.0f, 1.0f});

    EXPECT_FLOAT_EQ(controller.position().x, 4.0f);
    EXPECT_FLOAT_EQ(controller.position().y, -1.0f);
}

TEST(DevModeControllerTest, CyclingSubmodesReachesSelectTileMode) {
    DevModeController controller;
    controller.enter({0.0f, 0.0f});

    controller.cycleToNextAvailableSubmode();
    EXPECT_EQ(controller.activeSubmode(), DevelopmentSubmode::SelectTileMode);

    controller.cycleToPreviousAvailableSubmode();
    EXPECT_EQ(controller.activeSubmode(), DevelopmentSubmode::MoveMode);
}

TEST(DevModeControllerTest, SelectTileModeInitialSelectionCanBeSetExplicitly) {
    DevModeController controller;
    controller.enter({0.0f, 0.0f});
    controller.cycleToNextAvailableSubmode();

    controller.setSelectedTile({3, 4});

    EXPECT_TRUE(controller.hasSelection());
    EXPECT_EQ(controller.selectedTile(), glm::ivec2(3, 4));
}

TEST(DevModeControllerTest, SelectTileModeStepsImmediatelyAndClampsToBounds) {
    DevModeController controller;
    controller.enter({0.0f, 0.0f});
    controller.cycleToNextAvailableSubmode();
    controller.setSelectedTile({1, 1});

    EXPECT_TRUE(controller.updateSelectTileMode(0.016f, {1, 0}, {3, 3}));
    EXPECT_EQ(controller.selectedTile(), glm::ivec2(2, 1));

    EXPECT_TRUE(controller.updateSelectTileMode(0.016f, {-1, -1}, {3, 3}));
    EXPECT_EQ(controller.selectedTile(), glm::ivec2(1, 0));

    EXPECT_FALSE(controller.updateSelectTileMode(0.016f, {0, 0}, {3, 3}));
    EXPECT_EQ(controller.selectedTile(), glm::ivec2(1, 0));

    EXPECT_TRUE(controller.updateSelectTileMode(0.016f, {-1, -1}, {3, 3}));
    EXPECT_EQ(controller.selectedTile(), glm::ivec2(0, 0));

    EXPECT_FALSE(controller.updateSelectTileMode(0.016f, {-1, 0}, {3, 3}));
    EXPECT_EQ(controller.selectedTile(), glm::ivec2(0, 0));
}

TEST(DevModeControllerTest, SelectTileModeRepeatsAfterDelayWhenHeld) {
    DevModeController controller;
    controller.enter({0.0f, 0.0f});
    controller.cycleToNextAvailableSubmode();
    controller.setSelectedTile({0, 0});

    EXPECT_TRUE(controller.updateSelectTileMode(0.016f, {1, 0}, {4, 4}));
    EXPECT_EQ(controller.selectedTile(), glm::ivec2(1, 0));

    EXPECT_FALSE(controller.updateSelectTileMode(0.10f, {1, 0}, {4, 4}));
    EXPECT_EQ(controller.selectedTile(), glm::ivec2(1, 0));

    EXPECT_TRUE(controller.updateSelectTileMode(0.20f, {1, 0}, {4, 4}));
    EXPECT_EQ(controller.selectedTile(), glm::ivec2(2, 0));
}

TEST(DevModeControllerTest, ClipboardPersistsAcrossSubmodeChangesUntilExit) {
    DevModeController controller;
    controller.enter({0.0f, 0.0f});

    controller.setClipboardTile(7);

    ASSERT_TRUE(controller.hasClipboardTile());
    const auto initialClipboardTile = controller.clipboardTile();
    ASSERT_TRUE(initialClipboardTile.has_value());
    EXPECT_EQ(*initialClipboardTile, 7);

    controller.cycleToNextAvailableSubmode();
    const auto clipboardAfterNext = controller.clipboardTile();
    ASSERT_TRUE(clipboardAfterNext.has_value());
    EXPECT_EQ(*clipboardAfterNext, 7);

    controller.cycleToPreviousAvailableSubmode();
    const auto clipboardAfterPrevious = controller.clipboardTile();
    ASSERT_TRUE(clipboardAfterPrevious.has_value());
    EXPECT_EQ(*clipboardAfterPrevious, 7);

    controller.exit();

    EXPECT_FALSE(controller.hasClipboardTile());
    EXPECT_FALSE(controller.clipboardTile().has_value());
}

TEST(DevModeControllerTest, EnterClearsClipboardForNewDevelopmentSession) {
    DevModeController controller;
    controller.enter({0.0f, 0.0f});
    controller.setClipboardTile(4);

    controller.exit();
    controller.enter({2.0f, 1.0f});

    EXPECT_FALSE(controller.hasClipboardTile());
    EXPECT_FALSE(controller.clipboardTile().has_value());
}

}  // namespace levelbuilder::test