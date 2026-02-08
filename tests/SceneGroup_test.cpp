/**
 * @file SceneGroup_test.cpp
 * @brief Unit tests for SceneGroup (Phase 2)
 *
 * Tests SceneGroup construction, factory helpers, and scene ordering.
 */

#include <vde/api/SceneGroup.h>

#include <gtest/gtest.h>

namespace vde::test {

// ============================================================================
// Construction Tests
// ============================================================================

TEST(SceneGroupTest, DefaultConstructedIsEmpty) {
    SceneGroup group;
    EXPECT_TRUE(group.empty());
    EXPECT_EQ(group.size(), 0);
    EXPECT_TRUE(group.name.empty());
}

TEST(SceneGroupTest, CreateWithInitializerList) {
    auto group = SceneGroup::create("gameplay", {"world", "hud", "minimap"});

    EXPECT_EQ(group.name, "gameplay");
    EXPECT_EQ(group.size(), 3);
    EXPECT_FALSE(group.empty());
    EXPECT_EQ(group.sceneNames[0], "world");
    EXPECT_EQ(group.sceneNames[1], "hud");
    EXPECT_EQ(group.sceneNames[2], "minimap");
}

TEST(SceneGroupTest, CreateWithVector) {
    std::vector<std::string> scenes = {"main", "overlay"};
    auto group = SceneGroup::create("ui", scenes);

    EXPECT_EQ(group.name, "ui");
    EXPECT_EQ(group.size(), 2);
    EXPECT_EQ(group.sceneNames[0], "main");
    EXPECT_EQ(group.sceneNames[1], "overlay");
}

TEST(SceneGroupTest, CreateSingleScene) {
    auto group = SceneGroup::create("solo", {"menu"});

    EXPECT_EQ(group.size(), 1);
    EXPECT_EQ(group.sceneNames[0], "menu");
}

TEST(SceneGroupTest, CreateEmptyGroup) {
    auto group = SceneGroup::create("empty", {});

    EXPECT_TRUE(group.empty());
    EXPECT_EQ(group.size(), 0);
    EXPECT_EQ(group.name, "empty");
}

// ============================================================================
// Ordering Tests
// ============================================================================

TEST(SceneGroupTest, PreservesInsertionOrder) {
    auto group = SceneGroup::create("test", {"c", "a", "b", "d"});

    ASSERT_EQ(group.size(), 4);
    EXPECT_EQ(group.sceneNames[0], "c");
    EXPECT_EQ(group.sceneNames[1], "a");
    EXPECT_EQ(group.sceneNames[2], "b");
    EXPECT_EQ(group.sceneNames[3], "d");
}

TEST(SceneGroupTest, FirstSceneIsPrimary) {
    auto group = SceneGroup::create("test", {"primary", "secondary", "overlay"});

    // By convention, first scene is the primary/rendered scene
    EXPECT_EQ(group.sceneNames.front(), "primary");
}

// ============================================================================
// Aggregate Value Semantics
// ============================================================================

TEST(SceneGroupTest, CopySemantics) {
    auto group1 = SceneGroup::create("original", {"a", "b"});
    SceneGroup group2 = group1;

    EXPECT_EQ(group2.name, "original");
    EXPECT_EQ(group2.size(), 2);

    // Modifying copy doesn't affect original
    group2.sceneNames.push_back("c");
    EXPECT_EQ(group1.size(), 2);
    EXPECT_EQ(group2.size(), 3);
}

TEST(SceneGroupTest, MoveSemantics) {
    auto group1 = SceneGroup::create("original", {"a", "b"});
    SceneGroup group2 = std::move(group1);

    EXPECT_EQ(group2.name, "original");
    EXPECT_EQ(group2.size(), 2);
}

}  // namespace vde::test
