/**
 * @file CanvasRegistry_test.cpp
 * @brief Unit tests for CanvasRegistry multi-document container.
 */

#include "CanvasRegistry.h"
#include <gtest/gtest.h>

using namespace vde::tools;

// ============================================================================
// Test Fixture
// ============================================================================

class CanvasRegistryTest : public ::testing::Test {
  protected:
    void SetUp() override {}

    CanvasRegistry registry;

    /**
     * @brief Helper: create a canvas with a new 8x8 document.
     */
    Canvas* createCanvas(const std::string& name, uint32_t w = 8, uint32_t h = 8) {
        auto doc = std::make_unique<ImageDocument>();
        doc->createNew(w, h);
        return registry.create(name, std::move(doc));
    }
};

// ============================================================================
// Creation Tests
// ============================================================================

TEST_F(CanvasRegistryTest, CreateReturnsValidCanvas) {
    Canvas* c = createCanvas("test");
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->name, "test");
    EXPECT_NE(c->document, nullptr);
    EXPECT_EQ(c->document->getWidth(), 8u);
}

TEST_F(CanvasRegistryTest, CreateAssignsMonotonicIds) {
    Canvas* c1 = createCanvas("first");
    Canvas* c2 = createCanvas("second");

    ASSERT_NE(c1, nullptr);
    ASSERT_NE(c2, nullptr);
    EXPECT_LT(c1->id, c2->id);
}

TEST_F(CanvasRegistryTest, CreateDuplicateNameReturnsNull) {
    EXPECT_NE(createCanvas("hero"), nullptr);
    EXPECT_EQ(createCanvas("hero"), nullptr);
}

TEST_F(CanvasRegistryTest, CreateIncreasesCount) {
    EXPECT_EQ(registry.count(), 0u);
    createCanvas("a");
    EXPECT_EQ(registry.count(), 1u);
    createCanvas("b");
    EXPECT_EQ(registry.count(), 2u);
}

// ============================================================================
// Lookup Tests
// ============================================================================

TEST_F(CanvasRegistryTest, GetByIdFindsCanvas) {
    Canvas* c = createCanvas("hero");
    ASSERT_NE(c, nullptr);

    Canvas* found = registry.getById(c->id);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->name, "hero");
}

TEST_F(CanvasRegistryTest, GetByIdReturnsNullForMissing) {
    EXPECT_EQ(registry.getById(999), nullptr);
}

TEST_F(CanvasRegistryTest, GetByNameFindsCanvas) {
    createCanvas("player");

    Canvas* found = registry.getByName("player");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->name, "player");
}

TEST_F(CanvasRegistryTest, GetByNameReturnsNullForMissing) {
    EXPECT_EQ(registry.getByName("nope"), nullptr);
}

TEST_F(CanvasRegistryTest, ResolveByNumericId) {
    Canvas* c = createCanvas("hero");
    ASSERT_NE(c, nullptr);

    Canvas* found = registry.resolve(std::to_string(c->id));
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->name, "hero");
}

TEST_F(CanvasRegistryTest, ResolveByName) {
    createCanvas("hero");

    Canvas* found = registry.resolve("hero");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->name, "hero");
}

TEST_F(CanvasRegistryTest, ResolveReturnsNullForMissing) {
    EXPECT_EQ(registry.resolve("missing"), nullptr);
}

// ============================================================================
// Has / HasName Tests
// ============================================================================

TEST_F(CanvasRegistryTest, HasReturnsTrueForExisting) {
    Canvas* c = createCanvas("test");
    ASSERT_NE(c, nullptr);
    EXPECT_TRUE(registry.has(c->id));
}

TEST_F(CanvasRegistryTest, HasReturnsFalseForMissing) {
    EXPECT_FALSE(registry.has(999));
}

TEST_F(CanvasRegistryTest, HasNameReturnsTrueForExisting) {
    createCanvas("test");
    EXPECT_TRUE(registry.hasName("test"));
}

TEST_F(CanvasRegistryTest, HasNameReturnsFalseForMissing) {
    EXPECT_FALSE(registry.hasName("nope"));
}

// ============================================================================
// Removal Tests
// ============================================================================

TEST_F(CanvasRegistryTest, RemoveDeletesCanvas) {
    Canvas* c = createCanvas("hero");
    uint32_t id = c->id;

    EXPECT_TRUE(registry.remove(id));
    EXPECT_EQ(registry.count(), 0u);
    EXPECT_FALSE(registry.has(id));
    EXPECT_FALSE(registry.hasName("hero"));
}

TEST_F(CanvasRegistryTest, RemoveReturnsFalseForMissing) {
    EXPECT_FALSE(registry.remove(999));
}

TEST_F(CanvasRegistryTest, RemoveDoesNotAffectOtherCanvases) {
    Canvas* c1 = createCanvas("a");
    Canvas* c2 = createCanvas("b");
    uint32_t removeId = c1->id;
    uint32_t keepId = c2->id;

    registry.remove(removeId);

    EXPECT_FALSE(registry.has(removeId));
    EXPECT_TRUE(registry.has(keepId));
    EXPECT_EQ(registry.count(), 1u);
}

// ============================================================================
// GetIds Tests
// ============================================================================

TEST_F(CanvasRegistryTest, GetIdsReturnsAllIds) {
    Canvas* c1 = createCanvas("a");
    Canvas* c2 = createCanvas("b");
    Canvas* c3 = createCanvas("c");

    auto ids = registry.getIds();
    EXPECT_EQ(ids.size(), 3u);
    EXPECT_EQ(ids[0], c1->id);
    EXPECT_EQ(ids[1], c2->id);
    EXPECT_EQ(ids[2], c3->id);
}

TEST_F(CanvasRegistryTest, GetIdsEmptyWhenNoCanvases) {
    auto ids = registry.getIds();
    EXPECT_TRUE(ids.empty());
}

// ============================================================================
// Unique Name Generation Tests
// ============================================================================

TEST_F(CanvasRegistryTest, GenerateUniqueNameBasic) {
    std::string name = registry.generateUniqueName();
    EXPECT_EQ(name, "untitled_1");
}

TEST_F(CanvasRegistryTest, GenerateUniqueNameIncrementsOnConflict) {
    createCanvas("untitled_1");

    std::string name = registry.generateUniqueName();
    EXPECT_EQ(name, "untitled_2");
}

TEST_F(CanvasRegistryTest, GenerateUniqueNameCustomBase) {
    std::string name = registry.generateUniqueName("sprite");
    EXPECT_EQ(name, "sprite_1");
}

TEST_F(CanvasRegistryTest, GenerateUniqueNameTracksDeletions) {
    Canvas* c = createCanvas("untitled_1");
    uint32_t id = c->id;
    registry.remove(id);

    // After removing untitled_1, generating should give untitled_1 again
    std::string name = registry.generateUniqueName();
    EXPECT_EQ(name, "untitled_1");
}
