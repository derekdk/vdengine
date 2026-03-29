/**
 * @file EmojiFont_test.cpp
 * @brief Unit tests for EmojiFont color emoji loader.
 */

#include <vde/Texture.h>
#include <vde/api/EmojiFont.h>
#include <vde/api/TextRenderer.h>
#include <vde/api/TrueTypeFont.h>

#include <gtest/gtest.h>

#include <cstring>
#include <filesystem>

namespace vde {
namespace test {

class EmojiFontTest : public ::testing::Test {};

// ---- findSystemEmojiFont ---------------------------------------------------

TEST_F(EmojiFontTest, FindSystemEmojiFont) {
    std::string path = EmojiFont::findSystemEmojiFont();
    // The path may be empty on CI or headless systems — just verify it either
    // points to an existing file or is empty.
    if (!path.empty()) {
        EXPECT_TRUE(std::filesystem::exists(path))
            << "findSystemEmojiFont returned non-existent path: " << path;
    }
}

// ---- Error handling --------------------------------------------------------

TEST_F(EmojiFontTest, LoadNonexistentFileFails) {
    EmojiFont emoji;
    bool result = emoji.loadFromFile(nullptr, "nonexistent_path/missing.ttf", 32);
    EXPECT_FALSE(result);
    EXPECT_FALSE(emoji.isLoaded());
    EXPECT_FALSE(emoji.getLastError().empty());
}

TEST_F(EmojiFontTest, LoadEmptyPathFails) {
    EmojiFont emoji;
    bool result = emoji.loadFromFile(nullptr, "", 32);
    EXPECT_FALSE(result);
    EXPECT_FALSE(emoji.isLoaded());
}

TEST_F(EmojiFontTest, LoadZeroSizeFails) {
    std::string path = EmojiFont::findSystemEmojiFont();
    if (path.empty()) {
        GTEST_SKIP() << "No system emoji font available";
    }
    EmojiFont emoji;
    bool result = emoji.loadFromFile(nullptr, path, 0);
    EXPECT_FALSE(result);
    EXPECT_FALSE(emoji.isLoaded());
}

TEST_F(EmojiFontTest, DefaultStateIsUnloaded) {
    EmojiFont emoji;
    EXPECT_FALSE(emoji.isLoaded());
    EXPECT_EQ(emoji.atlasWidth(), 0);
    EXPECT_EQ(emoji.atlasHeight(), 0);
    EXPECT_EQ(emoji.emojiSize(), 0);
    EXPECT_TRUE(emoji.getAvailableCodepoints().empty());
    EXPECT_EQ(emoji.getAtlasTexture(), nullptr);
    EXPECT_FALSE(emoji.hasGlyph(0x1F600));
    EXPECT_EQ(emoji.getGlyph(0x1F600), nullptr);
}

// ---- System font tests (skipped if no emoji font is available) -------------

class EmojiFontSystemTest : public ::testing::Test {
  protected:
    void SetUp() override {
        m_path = EmojiFont::findSystemEmojiFont();
        if (m_path.empty()) {
            GTEST_SKIP() << "No system emoji font available";
        }
    }

    std::string m_path;
};

TEST_F(EmojiFontSystemTest, LoadSucceeds) {
    EmojiFont emoji;
    bool result = emoji.loadFromFile(nullptr, m_path, 32);
    ASSERT_TRUE(result) << "Failed to load: " << emoji.getLastError();
    EXPECT_TRUE(emoji.isLoaded());
}

TEST_F(EmojiFontSystemTest, AtlasDimensionsArePositive) {
    EmojiFont emoji;
    ASSERT_TRUE(emoji.loadFromFile(nullptr, m_path, 32));
    EXPECT_GT(emoji.atlasWidth(), 0);
    EXPECT_GT(emoji.atlasHeight(), 0);
    EXPECT_EQ(emoji.emojiSize(), 32);
}

TEST_F(EmojiFontSystemTest, HasCommonEmoji) {
    EmojiFont emoji;
    ASSERT_TRUE(emoji.loadFromFile(nullptr, m_path, 32));
    // Most color emoji fonts include grinning face
    EXPECT_TRUE(emoji.hasGlyph(0x1F600));  // 😀
}

TEST_F(EmojiFontSystemTest, GlyphLookupReturnsValidMetrics) {
    EmojiFont emoji;
    ASSERT_TRUE(emoji.loadFromFile(nullptr, m_path, 32));

    const EmojiGlyph* glyph = emoji.getGlyph(0x1F600);
    if (glyph) {
        EXPECT_GT(glyph->width, 0);
        EXPECT_GT(glyph->height, 0);
        EXPECT_GT(glyph->advanceX, 0.0f);
        EXPECT_GE(glyph->atlasX, 0);
        EXPECT_GE(glyph->atlasY, 0);
    }
}

TEST_F(EmojiFontSystemTest, AvailableCodepointsNonEmpty) {
    EmojiFont emoji;
    ASSERT_TRUE(emoji.loadFromFile(nullptr, m_path, 32));
    EXPECT_FALSE(emoji.getAvailableCodepoints().empty());
}

TEST_F(EmojiFontSystemTest, CopyGlyphPixels) {
    EmojiFont emoji;
    ASSERT_TRUE(emoji.loadFromFile(nullptr, m_path, 32));

    if (!emoji.hasGlyph(0x1F600)) {
        GTEST_SKIP() << "Grinning face not in font";
    }

    const EmojiGlyph* glyph = emoji.getGlyph(0x1F600);
    ASSERT_NE(glyph, nullptr);

    std::vector<uint8_t> pixels(glyph->width * glyph->height * 4, 0);
    ASSERT_TRUE(emoji.copyGlyphPixels(0x1F600, pixels.data()));

    // Verify at least some pixels are non-transparent (the emoji should have content)
    bool anyVisible = false;
    for (size_t i = 3; i < pixels.size(); i += 4) {
        if (pixels[i] > 0) {
            anyVisible = true;
            break;
        }
    }
    EXPECT_TRUE(anyVisible) << "All pixels in the grinning face emoji are transparent";
}

TEST_F(EmojiFontSystemTest, CopyGlyphPixelsForMissingCodepointFails) {
    EmojiFont emoji;
    ASSERT_TRUE(emoji.loadFromFile(nullptr, m_path, 32));

    uint8_t dummy[4];
    EXPECT_FALSE(emoji.copyGlyphPixels(0xFFFFFF, dummy));
}

TEST_F(EmojiFontSystemTest, AtlasTextureIsCreated) {
    EmojiFont emoji;
    ASSERT_TRUE(emoji.loadFromFile(nullptr, m_path, 32));

    auto atlas = emoji.getAtlasTexture();
    ASSERT_NE(atlas, nullptr);
    EXPECT_TRUE(atlas->isLoaded());
    EXPECT_FALSE(atlas->isOnGPU());  // No VulkanContext provided
    EXPECT_GT(atlas->getWidth(), 0u);
    EXPECT_GT(atlas->getHeight(), 0u);
}

TEST_F(EmojiFontSystemTest, MoveConstructor) {
    EmojiFont emoji;
    ASSERT_TRUE(emoji.loadFromFile(nullptr, m_path, 32));
    int expectedWidth = emoji.atlasWidth();

    EmojiFont moved(std::move(emoji));
    EXPECT_TRUE(moved.isLoaded());
    EXPECT_EQ(moved.atlasWidth(), expectedWidth);
    EXPECT_FALSE(emoji.isLoaded());  // NOLINT — testing moved-from state
}

TEST_F(EmojiFontSystemTest, MoveAssignment) {
    EmojiFont emoji;
    ASSERT_TRUE(emoji.loadFromFile(nullptr, m_path, 32));
    int expectedWidth = emoji.atlasWidth();

    EmojiFont target;
    target = std::move(emoji);
    EXPECT_TRUE(target.isLoaded());
    EXPECT_EQ(target.atlasWidth(), expectedWidth);
    EXPECT_FALSE(emoji.isLoaded());  // NOLINT — testing moved-from state
}

TEST_F(EmojiFontSystemTest, DifferentSizesProduceDifferentAtlases) {
    EmojiFont small;
    ASSERT_TRUE(small.loadFromFile(nullptr, m_path, 16));

    EmojiFont large;
    ASSERT_TRUE(large.loadFromFile(nullptr, m_path, 64));

    EXPECT_EQ(small.emojiSize(), 16);
    EXPECT_EQ(large.emojiSize(), 64);
}

// ---- TextRenderer emoji overload -------------------------------------------

class TextRendererEmojiTest : public ::testing::Test {
  protected:
    void SetUp() override {
        m_emojiPath = EmojiFont::findSystemEmojiFont();
        if (m_emojiPath.empty()) {
            GTEST_SKIP() << "No system emoji font available";
        }
        ASSERT_TRUE(m_emoji.loadFromFile(nullptr, m_emojiPath, 32));
        ASSERT_TRUE(m_font.loadFromFile(nullptr, VDE_ASSETS_DIR "/fonts/VDE_default.ttf", 32.0f));
    }

    std::string m_emojiPath;
    EmojiFont m_emoji;
    TrueTypeFont m_font;
};

TEST_F(TextRendererEmojiTest, AsciiOnlyDelegatesToBaseOverload) {
    TextStyle style;
    style.pixelScale = 1;
    auto tex = TextRenderer::createTexture(nullptr, "Hello", m_font, &m_emoji, style);
    ASSERT_NE(tex, nullptr);
    EXPECT_GT(tex->getWidth(), 0u);
    EXPECT_GT(tex->getHeight(), 0u);
}

TEST_F(TextRendererEmojiTest, EmojiOnlyText) {
    TextStyle style;
    style.pixelScale = 1;
    // Two emoji: 😀🚀
    auto tex = TextRenderer::createTexture(nullptr, "\xF0\x9F\x98\x80\xF0\x9F\x9A\x80",
                                           m_font, &m_emoji, style);
    ASSERT_NE(tex, nullptr);
    EXPECT_GT(tex->getWidth(), 0u);
    EXPECT_GT(tex->getHeight(), 0u);
}

TEST_F(TextRendererEmojiTest, MixedTextAndEmoji) {
    TextStyle style;
    style.pixelScale = 1;
    auto tex = TextRenderer::createTexture(nullptr, "Hi \xF0\x9F\x98\x80 world",
                                           m_font, &m_emoji, style);
    ASSERT_NE(tex, nullptr);
    EXPECT_GT(tex->getWidth(), 0u);
    EXPECT_GT(tex->getHeight(), 0u);
}

TEST_F(TextRendererEmojiTest, NullEmojiFontFallsBack) {
    TextStyle style;
    style.pixelScale = 1;
    // nullptr emoji font should still work (emoji codepoints will be skipped)
    auto tex = TextRenderer::createTexture(nullptr, "Hello \xF0\x9F\x98\x80", m_font,
                                           nullptr, style);
    ASSERT_NE(tex, nullptr);
    EXPECT_GT(tex->getWidth(), 0u);
}

}  // namespace test
}  // namespace vde
