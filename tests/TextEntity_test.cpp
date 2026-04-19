/**
 * @file TextEntity_test.cpp
 * @brief Unit tests for TextEntity class.
 */

#include <vde/Texture.h>
#include <vde/api/BitmapFont.h>
#include <vde/api/TextEntity.h>
#include <vde/api/TextRenderer.h>
#include <vde/api/TrueTypeFont.h>

#include <gtest/gtest.h>

namespace vde {
namespace test {

// ============================================================================
// TextEntity property tests (no Scene/VulkanContext required)
// ============================================================================

class TextEntityTest : public ::testing::Test {
  protected:
    TextEntity entity;
};

TEST_F(TextEntityTest, DefaultTextIsEmpty) {
    EXPECT_TRUE(entity.getText().empty());
}

TEST_F(TextEntityTest, SetTextUpdatesText) {
    entity.setText("HELLO");
    EXPECT_EQ(entity.getText(), "HELLO");
}

TEST_F(TextEntityTest, SetTextSameValueDoesNotTriggerChange) {
    entity.setText("HELLO");
    // Call update to clear dirty flag (without a scene, rebuildTexture
    // will use nullptr context — TextRenderer handles that gracefully)
    entity.update(0.0f);

    // Set same text again — internally the dirty flag should stay false.
    // We verify indirectly: after update the texture should remain the same
    // object (no rebuild occurred).
    auto texBefore = entity.getTexture();
    entity.setText("HELLO");
    entity.update(0.0f);
    auto texAfter = entity.getTexture();
    EXPECT_EQ(texBefore, texAfter);
}

TEST_F(TextEntityTest, SetStyleUpdatesStyle) {
    TextStyle style;
    style.pixelScale = 3;
    style.letterSpacing = 2;
    style.color = Color::red();
    entity.setStyle(style);

    EXPECT_EQ(entity.getStyle().pixelScale, 3);
    EXPECT_EQ(entity.getStyle().letterSpacing, 2);
}

TEST_F(TextEntityTest, SetFontClearsTypeTrueFont) {
    // Start with TrueType font
    TrueTypeFont ttf;
    entity.setTrueTypeFont(&ttf);

    // Switch to bitmap font
    entity.setFont(BitmapFont::large());

    // After update, should use BitmapFont (no crash even though TTF isn't loaded)
    entity.setText("TEST");
    entity.update(0.0f);
    EXPECT_NE(entity.getTexture(), nullptr);
}

TEST_F(TextEntityTest, UpdateRebuildsTextureWhenDirty) {
    entity.setText("A");
    entity.update(0.0f);

    auto texA = entity.getTexture();
    ASSERT_NE(texA, nullptr);

    entity.setText("AB");
    entity.update(0.0f);

    auto texAB = entity.getTexture();
    ASSERT_NE(texAB, nullptr);
    // Texture should be different since text changed
    EXPECT_NE(texA, texAB);
}

TEST_F(TextEntityTest, MultipleSetCallsSingleRebuild) {
    entity.setText("FIRST");
    entity.update(0.0f);

    // Make multiple changes before update
    entity.setText("SECOND");
    entity.setStyle({.color = Color::green(), .pixelScale = 2, .letterSpacing = 1});

    // Single update should produce a texture matching the final state
    entity.update(0.0f);
    auto tex = entity.getTexture();
    ASSERT_NE(tex, nullptr);
    EXPECT_EQ(entity.getText(), "SECOND");
}

TEST_F(TextEntityTest, TextureDimensionsUpdateAfterStyleChange) {
    entity.setText("HI");
    entity.setStyle({.color = Color::white(), .pixelScale = 1, .letterSpacing = 1});
    entity.update(0.0f);
    auto tex1 = entity.getTexture();
    ASSERT_NE(tex1, nullptr);
    uint32_t w1 = tex1->getWidth();
    uint32_t h1 = tex1->getHeight();

    // Double the pixel scale — texture should be larger
    entity.setStyle({.color = Color::white(), .pixelScale = 2, .letterSpacing = 1});
    entity.update(0.0f);
    auto tex2 = entity.getTexture();
    ASSERT_NE(tex2, nullptr);
    EXPECT_GT(tex2->getWidth(), w1);
    EXPECT_GT(tex2->getHeight(), h1);
}

TEST_F(TextEntityTest, EmptyTextProducesValidTexture) {
    entity.setText("");
    entity.update(0.0f);
    auto tex = entity.getTexture();
    ASSERT_NE(tex, nullptr);
    EXPECT_EQ(tex->getWidth(), 1u);
    EXPECT_EQ(tex->getHeight(), 1u);
}

TEST_F(TextEntityTest, InheritsFromSpriteEntity) {
    // Verify TextEntity can be used as a SpriteEntity
    SpriteEntity* base = &entity;
    EXPECT_NE(base, nullptr);

    // Transform operations work
    entity.setPosition(1.0f, 2.0f, 3.0f);
    EXPECT_FLOAT_EQ(entity.getPosition().x, 1.0f);
    EXPECT_FLOAT_EQ(entity.getPosition().y, 2.0f);
    EXPECT_FLOAT_EQ(entity.getPosition().z, 3.0f);

    entity.setAnchor(0.0f, 1.0f);
    EXPECT_FLOAT_EQ(entity.getAnchorX(), 0.0f);
    EXPECT_FLOAT_EQ(entity.getAnchorY(), 1.0f);
}

// ============================================================================
// Auto-sizing tests
// ============================================================================

TEST_F(TextEntityTest, DefaultWorldHeightIsZero) {
    EXPECT_FLOAT_EQ(entity.getWorldHeight(), 0.0f);
    EXPECT_FLOAT_EQ(entity.getMaxWidth(), 0.0f);
}

TEST_F(TextEntityTest, SetWorldHeightStoresValue) {
    entity.setWorldHeight(0.5f);
    EXPECT_FLOAT_EQ(entity.getWorldHeight(), 0.5f);
}

TEST_F(TextEntityTest, SetMaxWidthStoresValue) {
    entity.setMaxWidth(3.0f);
    EXPECT_FLOAT_EQ(entity.getMaxWidth(), 3.0f);
}

TEST_F(TextEntityTest, AutoSizingAppliesAfterUpdate) {
    entity.setText("HELLO");
    entity.setFont(BitmapFont::small());
    entity.setStyle({.color = Color::white(), .pixelScale = 1, .letterSpacing = 1});
    entity.setWorldHeight(0.5f);
    entity.update(0.0f);

    auto tex = entity.getTexture();
    ASSERT_NE(tex, nullptr);
    ASSERT_GE(tex->getWidth(), 2u);

    // Scale should match the auto-sized dimensions
    float expectedAspect =
        static_cast<float>(tex->getWidth()) / static_cast<float>(tex->getHeight());
    EXPECT_FLOAT_EQ(entity.getScale().y, 0.5f);
    EXPECT_NEAR(entity.getScale().x, 0.5f * expectedAspect, 0.001f);
}

TEST_F(TextEntityTest, AutoSizingReappliesOnTextChange) {
    entity.setText("A");
    entity.setFont(BitmapFont::small());
    entity.setStyle({.color = Color::white(), .pixelScale = 1, .letterSpacing = 1});
    entity.setWorldHeight(0.5f);
    entity.update(0.0f);

    float widthA = entity.getScale().x;

    entity.setText("ABCDEF");
    entity.update(0.0f);

    // Longer text should produce a wider scale
    EXPECT_GT(entity.getScale().x, widthA);
    EXPECT_FLOAT_EQ(entity.getScale().y, 0.5f);
}

TEST_F(TextEntityTest, AutoSizingDisabledByDefault) {
    entity.setText("HELLO");
    entity.update(0.0f);

    // Without setWorldHeight, scale should remain the default 1,1,1
    EXPECT_FLOAT_EQ(entity.getScale().x, 1.0f);
    EXPECT_FLOAT_EQ(entity.getScale().y, 1.0f);
}

TEST_F(TextEntityTest, AutoSizingWithMaxWidth) {
    entity.setText("THIS IS A LONG STRING THAT SHOULD BE CLAMPED");
    entity.setFont(BitmapFont::small());
    entity.setStyle({.color = Color::white(), .pixelScale = 1, .letterSpacing = 1});
    entity.setWorldHeight(0.5f);
    entity.setMaxWidth(2.0f);
    entity.update(0.0f);

    // Width should not exceed maxWidth
    EXPECT_LE(entity.getScale().x, 2.0f + 0.001f);
    // Height should be reduced proportionally
    EXPECT_LE(entity.getScale().y, 0.5f + 0.001f);
}

TEST_F(TextEntityTest, ManualSizeToFitWorksOnTextEntity) {
    entity.setText("HI");
    entity.setFont(BitmapFont::small());
    entity.setStyle({.color = Color::white(), .pixelScale = 1, .letterSpacing = 1});
    entity.update(0.0f);

    // Manual sizeToFit (inherited from SpriteEntity)
    entity.sizeToFit(0.35f);

    auto tex = entity.getTexture();
    ASSERT_NE(tex, nullptr);
    float expectedAspect =
        static_cast<float>(tex->getWidth()) / static_cast<float>(tex->getHeight());
    EXPECT_FLOAT_EQ(entity.getScale().y, 0.35f);
    EXPECT_NEAR(entity.getScale().x, 0.35f * expectedAspect, 0.001f);
}

}  // namespace test
}  // namespace vde
