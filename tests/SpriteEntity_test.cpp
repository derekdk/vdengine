/**
 * @file SpriteEntity_test.cpp
 * @brief Unit tests for SpriteEntity class
 */

#include <gtest/gtest.h>
#include <vde/api/Entity.h>
#include <vde/api/GameTypes.h>

namespace vde {
namespace test {

class SpriteEntityTest : public ::testing::Test {
protected:
    void SetUp() override {
        sprite = std::make_shared<SpriteEntity>();
    }

    std::shared_ptr<SpriteEntity> sprite;
};

// ============================================================================
// Constructor Tests
// ============================================================================

TEST_F(SpriteEntityTest, DefaultConstructor) {
    SpriteEntity entity;
    EXPECT_EQ(entity.getTextureId(), INVALID_RESOURCE_ID);
    EXPECT_EQ(entity.getTexture(), nullptr);
}

TEST_F(SpriteEntityTest, ConstructorWithResourceId) {
    SpriteEntity entity(42);
    EXPECT_EQ(entity.getTextureId(), 42);
    EXPECT_EQ(entity.getTexture(), nullptr);
}

// ============================================================================
// Texture Tests
// ============================================================================

TEST_F(SpriteEntityTest, SetTextureIdWorks) {
    sprite->setTextureId(123);
    EXPECT_EQ(sprite->getTextureId(), 123);
}

TEST_F(SpriteEntityTest, SetTextureIdDoesNotAffectDirectTexture) {
    sprite->setTextureId(123);
    // Direct texture should still be null
    EXPECT_EQ(sprite->getTexture(), nullptr);
}

// Note: Can't easily test setTexture(shared_ptr) without mocking Vulkan

// ============================================================================
// Color Tests
// ============================================================================

TEST_F(SpriteEntityTest, DefaultColorIsWhite) {
    Color color = sprite->getColor();
    EXPECT_FLOAT_EQ(color.r, 1.0f);
    EXPECT_FLOAT_EQ(color.g, 1.0f);
    EXPECT_FLOAT_EQ(color.b, 1.0f);
    EXPECT_FLOAT_EQ(color.a, 1.0f);
}

TEST_F(SpriteEntityTest, SetColorWorks) {
    Color red(1.0f, 0.0f, 0.0f, 1.0f);
    sprite->setColor(red);
    
    Color result = sprite->getColor();
    EXPECT_FLOAT_EQ(result.r, 1.0f);
    EXPECT_FLOAT_EQ(result.g, 0.0f);
    EXPECT_FLOAT_EQ(result.b, 0.0f);
    EXPECT_FLOAT_EQ(result.a, 1.0f);
}

TEST_F(SpriteEntityTest, SetColorWithAlpha) {
    Color semiTransparent(1.0f, 1.0f, 1.0f, 0.5f);
    sprite->setColor(semiTransparent);
    
    Color result = sprite->getColor();
    EXPECT_FLOAT_EQ(result.a, 0.5f);
}

// ============================================================================
// UV Rectangle Tests
// ============================================================================

TEST_F(SpriteEntityTest, DefaultUVRectIsFullTexture) {
    // Default UV rect should cover the entire texture (0,0) to (1,1)
    sprite->setUVRect(0.0f, 0.0f, 1.0f, 1.0f); // Reset to default
    // No direct getter for UV rect components, but setUVRect should work
}

TEST_F(SpriteEntityTest, SetUVRectForSpriteSheet) {
    // Simulate a 4x4 sprite sheet, select sprite at row 1, col 2
    float u = 2.0f / 4.0f;  // 0.5
    float v = 1.0f / 4.0f;  // 0.25
    float w = 1.0f / 4.0f;  // 0.25
    float h = 1.0f / 4.0f;  // 0.25
    
    sprite->setUVRect(u, v, w, h);
    // UV rect is set - no direct getters but it will be used in render()
}

// ============================================================================
// Anchor Tests
// ============================================================================

TEST_F(SpriteEntityTest, DefaultAnchorIsCenter) {
    EXPECT_FLOAT_EQ(sprite->getAnchorX(), 0.5f);
    EXPECT_FLOAT_EQ(sprite->getAnchorY(), 0.5f);
}

TEST_F(SpriteEntityTest, SetAnchorToBottomLeft) {
    sprite->setAnchor(0.0f, 0.0f);
    EXPECT_FLOAT_EQ(sprite->getAnchorX(), 0.0f);
    EXPECT_FLOAT_EQ(sprite->getAnchorY(), 0.0f);
}

TEST_F(SpriteEntityTest, SetAnchorToTopRight) {
    sprite->setAnchor(1.0f, 1.0f);
    EXPECT_FLOAT_EQ(sprite->getAnchorX(), 1.0f);
    EXPECT_FLOAT_EQ(sprite->getAnchorY(), 1.0f);
}

TEST_F(SpriteEntityTest, SetAnchorToCustomPoint) {
    sprite->setAnchor(0.25f, 0.75f);
    EXPECT_FLOAT_EQ(sprite->getAnchorX(), 0.25f);
    EXPECT_FLOAT_EQ(sprite->getAnchorY(), 0.75f);
}

// ============================================================================
// Entity Inheritance Tests
// ============================================================================

TEST_F(SpriteEntityTest, InheritsFromEntity) {
    // SpriteEntity should have Entity base class functionality
    Entity* basePtr = sprite.get();
    EXPECT_NE(basePtr, nullptr);
    
    // Test transform functions from Entity
    sprite->setPosition(10.0f, 20.0f, 0.0f);
    EXPECT_FLOAT_EQ(sprite->getPosition().x, 10.0f);
    EXPECT_FLOAT_EQ(sprite->getPosition().y, 20.0f);
}

TEST_F(SpriteEntityTest, SetPositionForSprite) {
    sprite->setPosition(100.0f, 200.0f, 0.0f);
    Position pos = sprite->getPosition();
    EXPECT_FLOAT_EQ(pos.x, 100.0f);
    EXPECT_FLOAT_EQ(pos.y, 200.0f);
    EXPECT_FLOAT_EQ(pos.z, 0.0f);
}

TEST_F(SpriteEntityTest, SetRotationForSprite) {
    sprite->setRotation(0.0f, 0.0f, 45.0f);  // 45 degree roll for 2D rotation
    Rotation rot = sprite->getRotation();
    EXPECT_FLOAT_EQ(rot.roll, 45.0f);
}

TEST_F(SpriteEntityTest, SetScaleForSprite) {
    sprite->setScale(2.0f, 3.0f, 1.0f);  // Non-uniform 2D scale
    Scale scl = sprite->getScale();
    EXPECT_FLOAT_EQ(scl.x, 2.0f);
    EXPECT_FLOAT_EQ(scl.y, 3.0f);
    EXPECT_FLOAT_EQ(scl.z, 1.0f);
}

TEST_F(SpriteEntityTest, GetModelMatrixWorks) {
    sprite->setPosition(1.0f, 2.0f, 0.0f);
    sprite->setScale(2.0f);
    
    glm::mat4 model = sprite->getModelMatrix();
    
    // The matrix should have translation and scale applied
    // Check translation component (column 3)
    EXPECT_FLOAT_EQ(model[3][0], 1.0f);
    EXPECT_FLOAT_EQ(model[3][1], 2.0f);
    EXPECT_FLOAT_EQ(model[3][2], 0.0f);
}

TEST_F(SpriteEntityTest, VisibilityWorks) {
    EXPECT_TRUE(sprite->isVisible());
    
    sprite->setVisible(false);
    EXPECT_FALSE(sprite->isVisible());
    
    sprite->setVisible(true);
    EXPECT_TRUE(sprite->isVisible());
}

TEST_F(SpriteEntityTest, HasUniqueId) {
    SpriteEntity sprite1;
    SpriteEntity sprite2;
    
    EXPECT_NE(sprite1.getId(), sprite2.getId());
}

TEST_F(SpriteEntityTest, NameWorks) {
    sprite->setName("MySprite");
    EXPECT_EQ(sprite->getName(), "MySprite");
}

} // namespace test
} // namespace vde
