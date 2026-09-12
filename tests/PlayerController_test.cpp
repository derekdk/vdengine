/**
 * @file PlayerController_test.cpp
 * @brief Unit tests for levelbuilder::PlayerController jump timing behavior.
 */

#include <vde/api/Scene.h>
#include <vde/api/TileMap.h>

#include "../games/level_builder/PlayerController.h"
#include "../games/level_builder/TileMapSession.h"
#include <gtest/gtest.h>

namespace levelbuilder::test {
namespace {

constexpr float kStandingSpawnY = 1.711f;

std::shared_ptr<vde::TileMap> makeFloorMap() {
    auto tileMap = std::make_shared<vde::TileMap>(1.0f, 1.0f, 6, 4);
    tileMap->setLayerName(0, "ground");
    tileMap->setCollisionKind(1, vde::TileCollisionKind::Solid);
    tileMap->fillRegion(0, 0, 5, 0, 1);
    return tileMap;
}

TileMapSession makeFloorSession() {
    TileMapSession session;
    session.adoptTileMap(makeFloorMap(), {2.5f, kStandingSpawnY}, 0u, "test-map");
    return session;
}

}  // namespace

TEST(PlayerControllerTest, JumpPressedOnFirstFrameAfterResetIsBuffered) {
    vde::Scene scene;
    PlayerController controller;
    controller.createEntities(scene);

    TileMapSession session = makeFloorSession();
    controller.reset(session, nullptr);

    const float groundedY = controller.getPosition().y;
    controller.update(1.0f / 60.0f, 0.0f, true, session);
    const float bufferedJumpY = controller.getPosition().y;

    EXPECT_NEAR(bufferedJumpY, groundedY, 0.0015f);

    controller.update(1.0f / 60.0f, 0.0f, false, session);
    EXPECT_GT(controller.getPosition().y, bufferedJumpY);
}

TEST(PlayerControllerTest, JumpPressedOnLandingFrameIsBuffered) {
    vde::Scene scene;
    PlayerController controller;
    controller.createEntities(scene);

    TileMapSession session = makeFloorSession();
    controller.reset(session, nullptr);
    controller.setPosition({2.5f, 3.0f});
    controller.stopMotion();

    controller.update(0.25f, 0.0f, true, session);
    const float landedY = controller.getPosition().y;

    controller.update(1.0f / 60.0f, 0.0f, false, session);
    EXPECT_GT(controller.getPosition().y, landedY);
}

}  // namespace levelbuilder::test