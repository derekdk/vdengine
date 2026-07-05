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

    controller.update(0.5f, {1.0f, 0.0f});

    EXPECT_GT(controller.position().x, 1.0f);
    EXPECT_FLOAT_EQ(controller.position().y, 2.0f);
}

TEST(DevModeControllerTest, DisabledControllerDoesNotMove) {
    DevModeController controller;
    controller.setPosition({4.0f, -1.0f});

    controller.update(1.0f, {0.0f, 1.0f});

    EXPECT_FLOAT_EQ(controller.position().x, 4.0f);
    EXPECT_FLOAT_EQ(controller.position().y, -1.0f);
}

TEST(DevModeControllerTest, CyclingSubmodesKeepsMoveModeWhileOnlyMoveIsAvailable) {
    DevModeController controller;
    controller.enter({0.0f, 0.0f});

    controller.cycleToNextAvailableSubmode();
    EXPECT_EQ(controller.activeSubmode(), DevelopmentSubmode::MoveMode);

    controller.cycleToPreviousAvailableSubmode();
    EXPECT_EQ(controller.activeSubmode(), DevelopmentSubmode::MoveMode);
}

}  // namespace levelbuilder::test