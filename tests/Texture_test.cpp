/**
 * @file Texture_test.cpp
 * @brief Unit tests for Texture class
 */

#include <vde/Texture.h>

#include <gtest/gtest.h>

using namespace vde;

class TextureTest : public ::testing::Test {
  protected:
    void SetUp() override {
        // Tests run without Vulkan context
    }

    void TearDown() override {
        // Cleanup
    }
};

// ============================================================================
// Resource Interface Tests
// ============================================================================

TEST_F(TextureTest, InheritsFromResource) {
    Texture texture;
    Resource* resource = &texture;  // Should compile if inheritance is correct
    EXPECT_NE(resource, nullptr);
}

TEST_F(TextureTest, GetTypeNameReturnsTexture) {
    Texture texture;
    EXPECT_STREQ(texture.getTypeName(), "Texture");
}

TEST_F(TextureTest, DefaultConstructorNotLoaded) {
    Texture texture;
    EXPECT_FALSE(texture.isLoaded());
}

// ============================================================================
// Data Loading Tests (CPU-side only, no Vulkan)
// ============================================================================

TEST_F(TextureTest, LoadFromDataStoresData) {
    Texture texture;

    // Create 2x2 RGBA test data
    uint8_t pixels[] = {255, 0,   0,   255,   // Red
                        0,   255, 0,   255,   // Green
                        0,   0,   255, 255,   // Blue
                        255, 255, 0,   255};  // Yellow

    bool result = texture.loadFromData(pixels, 2, 2);

    EXPECT_TRUE(result);
    EXPECT_TRUE(texture.isLoaded());
    EXPECT_EQ(texture.getWidth(), 2u);
    EXPECT_EQ(texture.getHeight(), 2u);
}

TEST_F(TextureTest, LoadFromDataWithNullPixelsFails) {
    Texture texture;

    bool result = texture.loadFromData(nullptr, 256, 256);

    EXPECT_FALSE(result);
    EXPECT_FALSE(texture.isLoaded());
}

TEST_F(TextureTest, LoadFromDataWithZeroDimensionsFails) {
    Texture texture;
    uint8_t pixels[4] = {255, 255, 255, 255};

    bool result1 = texture.loadFromData(pixels, 0, 256);
    EXPECT_FALSE(result1);

    bool result2 = texture.loadFromData(pixels, 256, 0);
    EXPECT_FALSE(result2);
}

TEST_F(TextureTest, NotOnGPUAfterCPULoad) {
    Texture texture;
    uint8_t pixels[16] = {255};

    texture.loadFromData(pixels, 2, 2);

    EXPECT_TRUE(texture.isLoaded());
    EXPECT_FALSE(texture.isOnGPU());
    EXPECT_FALSE(texture.isValid());  // isValid requires GPU upload
}

// ============================================================================
// File Loading Tests
// ============================================================================

TEST_F(TextureTest, LoadFromFileWithInvalidPathFails) {
    Texture texture;

    bool result = texture.loadFromFile("nonexistent_file_that_does_not_exist.png");

    EXPECT_FALSE(result);
    EXPECT_FALSE(texture.isLoaded());
}

TEST_F(TextureTest, LoadFromFileSetsPath) {
    Texture texture;

    // This will fail to load, but should still set the path if we implement that
    std::string testPath = "test_texture.png";
    texture.loadFromFile(testPath);

    // Path is set even if loading fails
    EXPECT_EQ(texture.getPath(), testPath);
}

// ============================================================================
// Move Semantics Tests
// ============================================================================

TEST_F(TextureTest, MoveConstructorTransfersData) {
    Texture texture1;
    uint8_t pixels[16] = {255};
    texture1.loadFromData(pixels, 2, 2);

    Texture texture2(std::move(texture1));

    EXPECT_TRUE(texture2.isLoaded());
    EXPECT_EQ(texture2.getWidth(), 2u);
    EXPECT_EQ(texture2.getHeight(), 2u);

    // Original should be in valid but undefined state
    EXPECT_EQ(texture1.getWidth(), 0u);
    EXPECT_EQ(texture1.getHeight(), 0u);
}

TEST_F(TextureTest, MoveAssignmentTransfersData) {
    Texture texture1;
    uint8_t pixels[16] = {255};
    texture1.loadFromData(pixels, 2, 2);

    Texture texture2;
    texture2 = std::move(texture1);

    EXPECT_TRUE(texture2.isLoaded());
    EXPECT_EQ(texture2.getWidth(), 2u);
    EXPECT_EQ(texture2.getHeight(), 2u);

    EXPECT_EQ(texture1.getWidth(), 0u);
    EXPECT_EQ(texture1.getHeight(), 0u);
}

// ============================================================================
// Cleanup Tests
// ============================================================================

TEST_F(TextureTest, CleanupClearsData) {
    Texture texture;
    uint8_t pixels[16] = {255};
    texture.loadFromData(pixels, 2, 2);

    texture.cleanup();

    EXPECT_EQ(texture.getWidth(), 0u);
    EXPECT_EQ(texture.getHeight(), 0u);
    EXPECT_FALSE(texture.isOnGPU());
}

TEST_F(TextureTest, CleanupMultipleTimesIsSafe) {
    Texture texture;
    uint8_t pixels[16] = {255};
    texture.loadFromData(pixels, 2, 2);

    // Should be safe to call multiple times
    EXPECT_NO_THROW(texture.cleanup());
    EXPECT_NO_THROW(texture.cleanup());
    EXPECT_NO_THROW(texture.cleanup());
}

// ============================================================================
// Resource ID Tests
// ============================================================================

TEST_F(TextureTest, DefaultResourceIdIsInvalid) {
    Texture texture;
    EXPECT_EQ(texture.getId(), INVALID_RESOURCE_ID);
}

// Note: GPU upload tests would require a VulkanContext and are beyond unit test scope
// Integration tests with VulkanContext should verify uploadToGPU() functionality
