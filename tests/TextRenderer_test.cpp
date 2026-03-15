/**
 * @file TextRenderer_test.cpp
 * @brief Unit tests for BitmapFont and TextRenderer classes.
 */

#include <vde/Texture.h>
#include <vde/api/BitmapFont.h>
#include <vde/api/TextRenderer.h>

#include <gtest/gtest.h>

namespace vde {
namespace test {

// ============================================================================
// BitmapFont tests
// ============================================================================

class BitmapFontTest : public ::testing::Test {};

TEST_F(BitmapFontTest, SmallFontDimensions) {
    const auto& font = BitmapFont::small();
    EXPECT_EQ(font.glyphWidth(), 5);
    EXPECT_EQ(font.glyphHeight(), 7);
}

TEST_F(BitmapFontTest, LargeFontDimensions) {
    const auto& font = BitmapFont::large();
    EXPECT_EQ(font.glyphWidth(), 8);
    EXPECT_EQ(font.glyphHeight(), 13);
}

TEST_F(BitmapFontTest, SmallFontSingleton) {
    const auto& a = BitmapFont::small();
    const auto& b = BitmapFont::small();
    EXPECT_EQ(&a, &b);
}

TEST_F(BitmapFontTest, LargeFontSingleton) {
    const auto& a = BitmapFont::large();
    const auto& b = BitmapFont::large();
    EXPECT_EQ(&a, &b);
}

TEST_F(BitmapFontTest, SpaceGlyphIsBlank) {
    const auto& font = BitmapFont::small();
    for (int row = 0; row < font.glyphHeight(); ++row) {
        EXPECT_EQ(font.glyphRow(' ', row), 0);
    }
}

TEST_F(BitmapFontTest, LetterAHasNonZeroRows) {
    const auto& font = BitmapFont::small();
    bool anyNonZero = false;
    for (int row = 0; row < font.glyphHeight(); ++row) {
        if (font.glyphRow('A', row) != 0)
            anyNonZero = true;
    }
    EXPECT_TRUE(anyNonZero);
}

TEST_F(BitmapFontTest, OutOfRangeRowReturnsZero) {
    const auto& font = BitmapFont::small();
    EXPECT_EQ(font.glyphRow('A', -1), 0);
    EXPECT_EQ(font.glyphRow('A', 7), 0);
    EXPECT_EQ(font.glyphRow('A', 100), 0);
}

TEST_F(BitmapFontTest, UnsupportedCharReturnsZero) {
    const auto& font = BitmapFont::small();
    // Characters outside 0x20-0x7E
    EXPECT_EQ(font.glyphRow('\x01', 0), 0);
    EXPECT_EQ(font.glyphRow('\x7F', 0), 0);
}

TEST_F(BitmapFontTest, DigitsHaveNonZeroRows) {
    const auto& font = BitmapFont::small();
    for (char c = '0'; c <= '9'; ++c) {
        bool anyNonZero = false;
        for (int row = 0; row < font.glyphHeight(); ++row) {
            if (font.glyphRow(c, row) != 0)
                anyNonZero = true;
        }
        EXPECT_TRUE(anyNonZero) << "digit '" << c << "' should have non-zero rows";
    }
}

TEST_F(BitmapFontTest, LargeFontLetterAHasNonZeroRows) {
    const auto& font = BitmapFont::large();
    bool anyNonZero = false;
    for (int row = 0; row < font.glyphHeight(); ++row) {
        if (font.glyphRow('A', row) != 0)
            anyNonZero = true;
    }
    EXPECT_TRUE(anyNonZero);
}

// ============================================================================
// TextRenderer tests (CPU-side only, no VulkanContext)
// ============================================================================

class TextRendererTest : public ::testing::Test {};

TEST_F(TextRendererTest, EmptyStringReturnsValidTexture) {
    auto tex = TextRenderer::createTexture(nullptr, "", BitmapFont::small());
    ASSERT_NE(tex, nullptr);
    EXPECT_EQ(tex->getWidth(), 1u);
    EXPECT_EQ(tex->getHeight(), 1u);
}

TEST_F(TextRendererTest, SingleCharSmallFontDimensions) {
    const auto& font = BitmapFont::small();
    TextStyle style;
    style.pixelScale = 1;
    style.letterSpacing = 1;

    auto tex = TextRenderer::createTexture(nullptr, "A", font, style);
    ASSERT_NE(tex, nullptr);

    // Width = 1 char * (5 + 1) * 1 = 6
    // Height = 7 * 1 = 7
    EXPECT_EQ(tex->getWidth(), 6u);
    EXPECT_EQ(tex->getHeight(), 7u);
}

TEST_F(TextRendererTest, MultiCharSmallFontDimensions) {
    const auto& font = BitmapFont::small();
    TextStyle style;
    style.pixelScale = 1;
    style.letterSpacing = 1;

    auto tex = TextRenderer::createTexture(nullptr, "HELLO", font, style);
    ASSERT_NE(tex, nullptr);

    // Width = 5 chars * (5 + 1) * 1 = 30
    // Height = 7 * 1 = 7
    EXPECT_EQ(tex->getWidth(), 30u);
    EXPECT_EQ(tex->getHeight(), 7u);
}

TEST_F(TextRendererTest, PixelScaleAffectsDimensions) {
    const auto& font = BitmapFont::small();
    TextStyle style;
    style.pixelScale = 3;
    style.letterSpacing = 1;

    auto tex = TextRenderer::createTexture(nullptr, "AB", font, style);
    ASSERT_NE(tex, nullptr);

    // Width = 2 chars * (5 + 1) * 3 = 36
    // Height = 7 * 3 = 21
    EXPECT_EQ(tex->getWidth(), 36u);
    EXPECT_EQ(tex->getHeight(), 21u);
}

TEST_F(TextRendererTest, LargeFontDimensions) {
    const auto& font = BitmapFont::large();
    TextStyle style;
    style.pixelScale = 1;
    style.letterSpacing = 1;

    auto tex = TextRenderer::createTexture(nullptr, "XY", font, style);
    ASSERT_NE(tex, nullptr);

    // Width = 2 chars * (8 + 1) * 1 = 18
    // Height = 13 * 1 = 13
    EXPECT_EQ(tex->getWidth(), 18u);
    EXPECT_EQ(tex->getHeight(), 13u);
}

TEST_F(TextRendererTest, ZeroLetterSpacing) {
    const auto& font = BitmapFont::small();
    TextStyle style;
    style.pixelScale = 1;
    style.letterSpacing = 0;

    auto tex = TextRenderer::createTexture(nullptr, "AB", font, style);
    ASSERT_NE(tex, nullptr);

    // Width = 2 chars * (5 + 0) * 1 = 10
    EXPECT_EQ(tex->getWidth(), 10u);
    EXPECT_EQ(tex->getHeight(), 7u);
}

TEST_F(TextRendererTest, NegativePixelScaleClampedToOne) {
    const auto& font = BitmapFont::small();
    TextStyle style;
    style.pixelScale = -5;
    style.letterSpacing = 1;

    auto tex = TextRenderer::createTexture(nullptr, "A", font, style);
    ASSERT_NE(tex, nullptr);
    // Clamped to scale=1: width = (5+1)*1 = 6, height = 7
    EXPECT_EQ(tex->getWidth(), 6u);
    EXPECT_EQ(tex->getHeight(), 7u);
}

TEST_F(TextRendererTest, TextureIsLoadedButNotOnGPU) {
    auto tex = TextRenderer::createTexture(nullptr, "TEST", BitmapFont::small());
    ASSERT_NE(tex, nullptr);
    EXPECT_TRUE(tex->isLoaded());
    EXPECT_FALSE(tex->isOnGPU());
}

}  // namespace test
}  // namespace vde
