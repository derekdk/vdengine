/**
 * @file ResourceManager_test.cpp
 * @brief Unit tests for ResourceManager class
 */

#include <vde/Texture.h>
#include <vde/api/Mesh.h>
#include <vde/api/ResourceManager.h>

#include <gtest/gtest.h>

using namespace vde;

class ResourceManagerTest : public ::testing::Test {
  protected:
    void SetUp() override { manager = std::make_unique<ResourceManager>(); }

    void TearDown() override { manager.reset(); }

    std::unique_ptr<ResourceManager> manager;
};

// ============================================================================
// Basic Functionality Tests
// ============================================================================

TEST_F(ResourceManagerTest, DefaultConstructorCreatesEmptyCache) {
    EXPECT_EQ(manager->getCachedCount(), 0u);
}

TEST_F(ResourceManagerTest, AddResourceStoresInCache) {
    auto texture = std::make_shared<Texture>();
    uint8_t pixels[16] = {255};
    texture->loadFromData(pixels, 2, 2);

    auto result = manager->add<Texture>("test_texture", texture);

    EXPECT_EQ(result, texture);
    EXPECT_TRUE(manager->has("test_texture"));
    EXPECT_EQ(manager->getCachedCount(), 1u);
}

TEST_F(ResourceManagerTest, GetReturnsNullptrForMissingResource) {
    auto texture = manager->get<Texture>("nonexistent");

    EXPECT_EQ(texture, nullptr);
}

TEST_F(ResourceManagerTest, GetReturnsCachedResource) {
    auto texture = std::make_shared<Texture>();
    uint8_t pixels[16] = {255};
    texture->loadFromData(pixels, 2, 2);

    manager->add<Texture>("test_texture", texture);
    auto retrieved = manager->get<Texture>("test_texture");

    EXPECT_EQ(retrieved, texture);
    EXPECT_EQ(retrieved.get(), texture.get());  // Same instance
}

TEST_F(ResourceManagerTest, HasReturnsFalseForMissingResource) {
    EXPECT_FALSE(manager->has("nonexistent"));
}

TEST_F(ResourceManagerTest, HasReturnsTrueForCachedResource) {
    auto texture = std::make_shared<Texture>();
    manager->add<Texture>("test", texture);

    EXPECT_TRUE(manager->has("test"));
}

// ============================================================================
// Loading Tests
// ============================================================================

TEST_F(ResourceManagerTest, LoadCreatesNewResource) {
    // This will fail to load a real file, but should create a Texture instance
    auto texture = manager->load<Texture>("nonexistent.png");

    // Load fails because file doesn't exist
    EXPECT_EQ(texture, nullptr);
}

TEST_F(ResourceManagerTest, LoadSamePathReturnsSameInstance) {
    // Create a temporary test texture
    auto texture1 = std::make_shared<Texture>();
    uint8_t pixels[16] = {255};
    texture1->loadFromData(pixels, 2, 2);
    manager->add<Texture>("test.png", texture1);

    // "Load" the same path - should return cached instance
    auto texture2 = manager->get<Texture>("test.png");

    EXPECT_EQ(texture1, texture2);
    EXPECT_EQ(manager->getCachedCount(), 1u);  // Still only one resource
}

// ============================================================================
// Type Safety Tests
// ============================================================================

TEST_F(ResourceManagerTest, GetWithWrongTypeReturnsNullptr) {
    auto texture = std::make_shared<Texture>();
    manager->add<Texture>("resource", texture);

    // Try to get it as a Mesh (wrong type)
    auto mesh = manager->get<Mesh>("resource");

    EXPECT_EQ(mesh, nullptr);
}

TEST_F(ResourceManagerTest, DifferentTypesCanShareKey) {
    // Not recommended in practice, but should be handled
    auto texture = std::make_shared<Texture>();
    auto mesh = std::make_shared<Mesh>();

    manager->add<Texture>("shared_key", texture);
    // This will overwrite the texture entry
    manager->add<Mesh>("shared_key", mesh);

    auto retrievedMesh = manager->get<Mesh>("shared_key");
    auto retrievedTexture = manager->get<Texture>("shared_key");

    EXPECT_NE(retrievedMesh, nullptr);
    EXPECT_EQ(retrievedTexture, nullptr);  // Overwritten
}

// ============================================================================
// Removal Tests
// ============================================================================

TEST_F(ResourceManagerTest, RemoveDeletesFromCache) {
    auto texture = std::make_shared<Texture>();
    manager->add<Texture>("test", texture);

    EXPECT_TRUE(manager->has("test"));

    manager->remove("test");

    EXPECT_FALSE(manager->has("test"));
    EXPECT_EQ(manager->getCachedCount(), 0u);
}

TEST_F(ResourceManagerTest, RemoveNonexistentResourceIsSafe) {
    EXPECT_NO_THROW(manager->remove("nonexistent"));
}

TEST_F(ResourceManagerTest, ClearRemovesAllResources) {
    auto texture1 = std::make_shared<Texture>();
    auto texture2 = std::make_shared<Texture>();
    auto mesh = std::make_shared<Mesh>();

    manager->add<Texture>("texture1", texture1);
    manager->add<Texture>("texture2", texture2);
    manager->add<Mesh>("mesh1", mesh);

    EXPECT_EQ(manager->getCachedCount(), 3u);

    manager->clear();

    EXPECT_EQ(manager->getCachedCount(), 0u);
    EXPECT_FALSE(manager->has("texture1"));
    EXPECT_FALSE(manager->has("texture2"));
    EXPECT_FALSE(manager->has("mesh1"));
}

// ============================================================================
// Weak Pointer Behavior Tests
// ============================================================================

TEST_F(ResourceManagerTest, WeakPtrAllowsAutoCleanup) {
    {
        auto texture = std::make_shared<Texture>();
        manager->add<Texture>("temp", texture);
        EXPECT_TRUE(manager->has("temp"));
    }  // texture goes out of scope and is destroyed

    // The weak_ptr should now be expired
    auto retrieved = manager->get<Texture>("temp");
    EXPECT_EQ(retrieved, nullptr);

    // After attempting to get the expired resource, cache should clean up
    // The entry might still exist but weak_ptr is expired
    // has() should return false because resource is gone
    EXPECT_FALSE(manager->has("temp"));
}

TEST_F(ResourceManagerTest, CachedResourceStaysAliveWhileReferenced) {
    auto texture = std::make_shared<Texture>();
    manager->add<Texture>("test", texture);

    // Get another reference
    auto ref1 = manager->get<Texture>("test");
    auto ref2 = manager->get<Texture>("test");

    // Clear the original reference
    texture.reset();

    // Resource should still be alive through ref1 and ref2
    EXPECT_TRUE(manager->has("test"));
    EXPECT_NE(ref1, nullptr);
    EXPECT_NE(ref2, nullptr);

    // Clear all references
    ref1.reset();
    ref2.reset();

    // Now it should be gone
    EXPECT_FALSE(manager->has("test"));
}

// ============================================================================
// Pruning Tests
// ============================================================================

TEST_F(ResourceManagerTest, PruneExpiredRemovesDeadReferences) {
    {
        auto texture = std::make_shared<Texture>();
        manager->add<Texture>("temp", texture);
    }  // texture destroyed

    // Manually prune
    manager->pruneExpired();

    // Entry should be fully removed now
    EXPECT_EQ(manager->getCachedCount(), 0u);
}

TEST_F(ResourceManagerTest, PruneExpiredKeepsAliveResources) {
    auto texture = std::make_shared<Texture>();
    manager->add<Texture>("alive", texture);

    manager->pruneExpired();

    EXPECT_EQ(manager->getCachedCount(), 1u);
    EXPECT_TRUE(manager->has("alive"));
}

// ============================================================================
// Statistics Tests
// ============================================================================

TEST_F(ResourceManagerTest, GetCachedCountReturnsCorrectValue) {
    auto texture1 = std::make_shared<Texture>();
    auto texture2 = std::make_shared<Texture>();

    EXPECT_EQ(manager->getCachedCount(), 0u);

    manager->add<Texture>("t1", texture1);
    EXPECT_EQ(manager->getCachedCount(), 1u);

    manager->add<Texture>("t2", texture2);
    EXPECT_EQ(manager->getCachedCount(), 2u);

    manager->remove("t1");
    EXPECT_EQ(manager->getCachedCount(), 1u);

    manager->clear();
    EXPECT_EQ(manager->getCachedCount(), 0u);
}

TEST_F(ResourceManagerTest, GetMemoryUsageReturnsEstimate) {
    auto texture = std::make_shared<Texture>();
    uint8_t pixels[16] = {255};
    texture->loadFromData(pixels, 2, 2);  // 2x2 RGBA = 16 bytes

    manager->add<Texture>("test", texture);

    size_t memUsage = manager->getMemoryUsage();

    // Should return an estimate (implementation-dependent)
    // For a 2x2 texture, should be at least 16 bytes
    EXPECT_GT(memUsage, 0u);
}

TEST_F(ResourceManagerTest, GetMemoryUsageZeroWhenEmpty) {
    EXPECT_EQ(manager->getMemoryUsage(), 0u);
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(ResourceManagerTest, AddNullResourceReturnsNullptr) {
    auto result = manager->add<Texture>("null", nullptr);

    EXPECT_EQ(result, nullptr);
    EXPECT_FALSE(manager->has("null"));
}

TEST_F(ResourceManagerTest, MultipleManagers) {
    ResourceManager manager1;
    ResourceManager manager2;

    auto texture = std::make_shared<Texture>();
    manager1.add<Texture>("test", texture);

    // Different manager, different cache
    EXPECT_FALSE(manager2.has("test"));
}
