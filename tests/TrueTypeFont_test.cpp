/**
 * @file TrueTypeFont_test.cpp
 * @brief Unit tests for TrueTypeFont class.
 */

#include <vde/Texture.h>
#include <vde/api/TextRenderer.h>
#include <vde/api/TrueTypeFont.h>

#include <gtest/gtest.h>

namespace vde {
namespace test {

namespace {
// Helper: check if a value is a power of two
bool isPowerOfTwo(int v) {
    return v > 0 && (v & (v - 1)) == 0;
}
}  // namespace

class TrueTypeFontTest : public ::testing::Test {};

TEST_F(TrueTypeFontTest, MissingFileFallsBackGracefully) {
    TrueTypeFont font;
    bool result = font.loadFromFile(nullptr, "nonexistent_path/missing.ttf", 32.0f);
    EXPECT_FALSE(result);
    EXPECT_FALSE(font.isLoaded());
    EXPECT_EQ(font.getAtlasTexture(), nullptr);
}

TEST_F(TrueTypeFontTest, EmptyPathFails) {
    TrueTypeFont font;
    bool result = font.loadFromFile(nullptr, "", 32.0f);
    EXPECT_FALSE(result);
    EXPECT_FALSE(font.isLoaded());
}

TEST_F(TrueTypeFontTest, FallbackFontReturnsSmallBitmapFont) {
    const auto& fb = TrueTypeFont::fallbackFont();
    EXPECT_EQ(fb.glyphWidth(), 5);
    EXPECT_EQ(fb.glyphHeight(), 7);
}

TEST_F(TrueTypeFontTest, LoadBundledFont) {
    TrueTypeFont font;
    bool result = font.loadFromFile(nullptr, VDE_ASSETS_DIR "/fonts/VDE_default.ttf", 32.0f);
    ASSERT_TRUE(result) << "Failed to load bundled VDE_default.ttf";
    EXPECT_TRUE(font.isLoaded());
}

TEST_F(TrueTypeFontTest, AtlasDimensionsArePowersOfTwo) {
    TrueTypeFont font;
    ASSERT_TRUE(font.loadFromFile(nullptr, VDE_ASSETS_DIR "/fonts/VDE_default.ttf", 32.0f));
    EXPECT_TRUE(isPowerOfTwo(font.atlasWidth())) << "atlas width=" << font.atlasWidth();
    EXPECT_TRUE(isPowerOfTwo(font.atlasHeight())) << "atlas height=" << font.atlasHeight();
}

TEST_F(TrueTypeFontTest, AtlasTextureIsCreated) {
    TrueTypeFont font;
    ASSERT_TRUE(font.loadFromFile(nullptr, VDE_ASSETS_DIR "/fonts/VDE_default.ttf", 32.0f));
    auto atlas = font.getAtlasTexture();
    ASSERT_NE(atlas, nullptr);
    EXPECT_TRUE(atlas->isLoaded());
    EXPECT_FALSE(atlas->isOnGPU());  // No VulkanContext provided
}

TEST_F(TrueTypeFontTest, GlyphLookupForPrintableChars) {
    TrueTypeFont font;
    ASSERT_TRUE(font.loadFromFile(nullptr, VDE_ASSETS_DIR "/fonts/VDE_default.ttf", 32.0f));

    // All printable ASCII chars should have valid glyph info
    for (char c = 0x20; c <= 0x7E; ++c) {
        const GlyphInfo* g = font.getGlyph(c);
        ASSERT_NE(g, nullptr) << "missing glyph for char " << static_cast<int>(c);
        EXPECT_GE(g->advanceX, 0.0f) << "negative advance for char " << static_cast<int>(c);
    }
}

TEST_F(TrueTypeFontTest, GlyphLookupOutOfRangeReturnsNull) {
    TrueTypeFont font;
    ASSERT_TRUE(font.loadFromFile(nullptr, VDE_ASSETS_DIR "/fonts/VDE_default.ttf", 32.0f));
    EXPECT_EQ(font.getGlyph('\x01'), nullptr);
    EXPECT_EQ(font.getGlyph('\x7F'), nullptr);
}

TEST_F(TrueTypeFontTest, FontSizeIsStoredCorrectly) {
    TrueTypeFont font;
    ASSERT_TRUE(font.loadFromFile(nullptr, VDE_ASSETS_DIR "/fonts/VDE_default.ttf", 48.0f));
    EXPECT_FLOAT_EQ(font.fontSize(), 48.0f);
}

TEST_F(TrueTypeFontTest, LineHeightIsPositive) {
    TrueTypeFont font;
    ASSERT_TRUE(font.loadFromFile(nullptr, VDE_ASSETS_DIR "/fonts/VDE_default.ttf", 32.0f));
    EXPECT_GT(font.lineHeight(), 0.0f);
}

TEST_F(TrueTypeFontTest, MoveConstructor) {
    TrueTypeFont font;
    ASSERT_TRUE(font.loadFromFile(nullptr, VDE_ASSETS_DIR "/fonts/VDE_default.ttf", 32.0f));

    TrueTypeFont moved(std::move(font));
    EXPECT_TRUE(moved.isLoaded());
    EXPECT_FALSE(font.isLoaded());  // NOLINT — testing moved-from state
}

TEST_F(TrueTypeFontTest, CreateTextureWithTrueTypeFont) {
    TrueTypeFont font;
    ASSERT_TRUE(font.loadFromFile(nullptr, VDE_ASSETS_DIR "/fonts/VDE_default.ttf", 32.0f));

    TextStyle style;
    style.pixelScale = 1;
    auto tex = TextRenderer::createTexture(nullptr, "Hello", font, style);
    ASSERT_NE(tex, nullptr);
    EXPECT_GT(tex->getWidth(), 0u);
    EXPECT_GT(tex->getHeight(), 0u);
    EXPECT_TRUE(tex->isLoaded());
}

TEST_F(TrueTypeFontTest, EmptyStringWithTrueTypeFontReturns1x1) {
    TrueTypeFont font;
    ASSERT_TRUE(font.loadFromFile(nullptr, VDE_ASSETS_DIR "/fonts/VDE_default.ttf", 32.0f));

    auto tex = TextRenderer::createTexture(nullptr, "", font);
    ASSERT_NE(tex, nullptr);
    EXPECT_EQ(tex->getWidth(), 1u);
    EXPECT_EQ(tex->getHeight(), 1u);
}

}  // namespace test
}  // namespace vde
