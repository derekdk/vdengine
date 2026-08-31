/**
 * @file SceneOrdering_test.cpp
 * @brief Unit tests for Scene entity ordering.
 */

#include <vde/api/Entity.h>
#include <vde/api/Scene.h>
#include <vde/api/TextEntity.h>

#include <gtest/gtest.h>

namespace vde::test {

TEST(SceneOrderingTest, MoveEntityToBackPreservesEntityLookup) {
    Scene scene;
    auto first = scene.addEntity<MeshEntity>();
    auto second = scene.addEntity<SpriteEntity>();
    auto third = scene.addEntity<TextEntity>();

    EXPECT_TRUE(scene.moveEntityToBack(third->getId()));
    ASSERT_EQ(scene.getEntities().size(), 3u);
    EXPECT_EQ(scene.getEntities().at(0), third);
    EXPECT_EQ(scene.getEntities().at(1), first);
    EXPECT_EQ(scene.getEntities().at(2), second);
    EXPECT_EQ(scene.getEntity(first->getId()), first.get());
    EXPECT_EQ(scene.getEntity(second->getId()), second.get());
    EXPECT_EQ(scene.getEntity(third->getId()), third.get());
    EXPECT_FALSE(scene.moveEntityToBack(99999));
}

}  // namespace vde::test