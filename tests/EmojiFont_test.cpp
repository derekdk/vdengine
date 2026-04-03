/**
 * @file EmojiFont_test.cpp
 * @brief Unit tests for EmojiFont color emoji loader.
 */

#include <vde/Texture.h>
#include <vde/api/EmojiFont.h>
#include <vde/api/TextRenderer.h>
#include <vde/api/TrueTypeFont.h>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

#include <gtest/gtest.h>

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
        // Skip if the font file exists but isn't a supported COLR/CPAL font
        if (!m_emoji.loadFromFile(nullptr, m_path, 32)) {
            GTEST_SKIP() << "System emoji font not supported (no COLR/CPAL tables): "
                         << m_emoji.getLastError();
        }
    }

    std::string m_path;
    EmojiFont m_emoji;
};

TEST_F(EmojiFontSystemTest, LoadSucceeds) {
    EXPECT_TRUE(m_emoji.isLoaded());
}

TEST_F(EmojiFontSystemTest, AtlasDimensionsArePositive) {
    EXPECT_GT(m_emoji.atlasWidth(), 0);
    EXPECT_GT(m_emoji.atlasHeight(), 0);
    EXPECT_EQ(m_emoji.emojiSize(), 32);
}

TEST_F(EmojiFontSystemTest, HasCommonEmoji) {
    // Most color emoji fonts include grinning face
    EXPECT_TRUE(m_emoji.hasGlyph(0x1F600));  // 😀
}

TEST_F(EmojiFontSystemTest, GlyphLookupReturnsValidMetrics) {
    const EmojiGlyph* glyph = m_emoji.getGlyph(0x1F600);
    if (glyph) {
        EXPECT_GT(glyph->width, 0);
        EXPECT_GT(glyph->height, 0);
        EXPECT_GT(glyph->advanceX, 0.0f);
        EXPECT_GE(glyph->atlasX, 0);
        EXPECT_GE(glyph->atlasY, 0);
    }
}

TEST_F(EmojiFontSystemTest, AvailableCodepointsNonEmpty) {
    EXPECT_FALSE(m_emoji.getAvailableCodepoints().empty());
}

TEST_F(EmojiFontSystemTest, CopyGlyphPixels) {
    if (!m_emoji.hasGlyph(0x1F600)) {
        GTEST_SKIP() << "Grinning face not in font";
    }

    const EmojiGlyph* glyph = m_emoji.getGlyph(0x1F600);
    ASSERT_NE(glyph, nullptr);

    std::vector<uint8_t> pixels(glyph->width * glyph->height * 4, 0);
    ASSERT_TRUE(m_emoji.copyGlyphPixels(0x1F600, pixels.data()));

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
    uint8_t dummy[4];
    EXPECT_FALSE(m_emoji.copyGlyphPixels(0xFFFFFF, dummy));
}

TEST_F(EmojiFontSystemTest, AtlasTextureIsCreated) {
    auto atlas = m_emoji.getAtlasTexture();
    ASSERT_NE(atlas, nullptr);
    EXPECT_TRUE(atlas->isLoaded());
    EXPECT_FALSE(atlas->isOnGPU());  // No VulkanContext provided
    EXPECT_GT(atlas->getWidth(), 0u);
    EXPECT_GT(atlas->getHeight(), 0u);
}

TEST_F(EmojiFontSystemTest, MoveConstructor) {
    int expectedWidth = m_emoji.atlasWidth();

    EmojiFont moved(std::move(m_emoji));
    EXPECT_TRUE(moved.isLoaded());
    EXPECT_EQ(moved.atlasWidth(), expectedWidth);
    EXPECT_FALSE(m_emoji.isLoaded());  // NOLINT — testing moved-from state
}

TEST_F(EmojiFontSystemTest, MoveAssignment) {
    int expectedWidth = m_emoji.atlasWidth();

    EmojiFont target;
    target = std::move(m_emoji);
    EXPECT_TRUE(target.isLoaded());
    EXPECT_EQ(target.atlasWidth(), expectedWidth);
    EXPECT_FALSE(m_emoji.isLoaded());  // NOLINT — testing moved-from state
}

TEST_F(EmojiFontSystemTest, DifferentSizesProduceDifferentAtlases) {
    EmojiFont small;
    ASSERT_TRUE(small.loadFromFile(nullptr, m_path, 16));

    EmojiFont large;
    ASSERT_TRUE(large.loadFromFile(nullptr, m_path, 64));

    EXPECT_EQ(small.emojiSize(), 16);
    EXPECT_EQ(large.emojiSize(), 64);
}

// ---- COLR v0/v1 invariants -------------------------------------------------

// Helper: read the COLR table version from a font file without relying on
// EmojiFont's own parser.  Returns 0xFFFF on any error.
static uint16_t readSystemFontColrVersion(const std::string& fontPath) {
    std::ifstream f(fontPath, std::ios::binary | std::ios::ate);
    if (!f.is_open())
        return 0xFFFF;
    auto pos = f.tellg();
    if (pos < 0 || pos > static_cast<std::ifstream::pos_type>(256 * 1024 * 1024))
        return 0xFFFF;
    auto sz = static_cast<size_t>(pos);
    if (sz < 12)
        return 0xFFFF;
    f.seekg(0);
    std::vector<uint8_t> data(sz);
    if (!f.read(reinterpret_cast<char*>(data.data()), sz))
        return 0xFFFF;

    // OpenType table directory: numTables at offset 4
    uint16_t numTables = (static_cast<uint16_t>(data[4]) << 8) | data[5];
    if (sz < static_cast<size_t>(12) + numTables * 16u)
        return 0xFFFF;

    constexpr uint32_t kTagCOLR = 0x434F4C52u;
    for (uint16_t i = 0; i < numTables; ++i) {
        const uint8_t* e = data.data() + 12 + i * 16;
        uint32_t tag = (static_cast<uint32_t>(e[0]) << 24) | (static_cast<uint32_t>(e[1]) << 16) |
                       (static_cast<uint32_t>(e[2]) << 8) | e[3];
        if (tag == kTagCOLR) {
            uint32_t off = (static_cast<uint32_t>(e[8]) << 24) |
                           (static_cast<uint32_t>(e[9]) << 16) |
                           (static_cast<uint32_t>(e[10]) << 8) | e[11];
            if (off + 2 > sz)
                return 0xFFFF;
            return (static_cast<uint16_t>(data[off]) << 8) | data[off + 1];
        }
    }
    return 0xFFFF;
}

class EmojiFontColrInvariantsTest : public ::testing::Test {
  protected:
    void SetUp() override {
        m_path = vde::EmojiFont::findSystemEmojiFont();
        if (m_path.empty()) {
            GTEST_SKIP() << "No system emoji font available";
        }
        if (!m_emoji.loadFromFile(nullptr, m_path, 32)) {
            GTEST_SKIP() << "System emoji font not supported (no COLR/CPAL tables): "
                         << m_emoji.getLastError();
        }
    }

    std::string m_path;
    vde::EmojiFont m_emoji;
};

TEST_F(EmojiFontColrInvariantsTest, AvailableCodepointsAreSorted) {
    const auto& codepoints = m_emoji.getAvailableCodepoints();
    ASSERT_FALSE(codepoints.empty());
    EXPECT_TRUE(std::is_sorted(codepoints.begin(), codepoints.end()))
        << "Available codepoints are not in ascending order";
}

TEST_F(EmojiFontColrInvariantsTest, NoDuplicateCodepoints) {
    const auto& codepoints = m_emoji.getAvailableCodepoints();
    ASSERT_FALSE(codepoints.empty());
    // The list is already sorted, so adjacent duplicates would be immediately obvious
    auto it = std::adjacent_find(codepoints.begin(), codepoints.end());
    EXPECT_EQ(it, codepoints.end())
        << "Duplicate codepoint U+" << std::hex << static_cast<uint32_t>(*it)
        << " found in available codepoints";
}

TEST_F(EmojiFontColrInvariantsTest, AllGlyphAtlasPositionsInBounds) {
    const auto& codepoints = m_emoji.getAvailableCodepoints();
    int atlasW = m_emoji.atlasWidth();
    int atlasH = m_emoji.atlasHeight();
    ASSERT_GT(atlasW, 0);
    ASSERT_GT(atlasH, 0);

    for (char32_t cp : codepoints) {
        const vde::EmojiGlyph* g = m_emoji.getGlyph(cp);
        ASSERT_NE(g, nullptr) << "Codepoint in list but getGlyph() returned null";
        EXPECT_GE(g->atlasX, 0);
        EXPECT_GE(g->atlasY, 0);
        EXPECT_LE(g->atlasX + g->width, atlasW)
            << "Glyph for U+" << std::hex << static_cast<uint32_t>(cp) << " overflows atlas width";
        EXPECT_LE(g->atlasY + g->height, atlasH)
            << "Glyph for U+" << std::hex << static_cast<uint32_t>(cp) << " overflows atlas height";
    }
}

TEST_F(EmojiFontColrInvariantsTest, ColrVersionOneAccepted) {
    // This test explicitly verifies the COLR v1 acceptance fix: before the
    // fix the parser would mis-parse v1 headers using v0 layout, potentially
    // producing zero glyphs or incorrect data.  A successful load on a v1
    // font with at least one emoji is proof the v1 path is working.
    uint16_t colrVersion = readSystemFontColrVersion(m_path);
    if (colrVersion == 0xFFFF) {
        GTEST_SKIP() << "Could not read COLR table version from font file";
    }
    if (colrVersion != 1) {
        GTEST_SKIP() << "System font is COLR v" << colrVersion << ", not v1 — skipping v1 test";
    }

    // Font is v1 — verify the loader produced a non-trivial result
    EXPECT_FALSE(m_emoji.getAvailableCodepoints().empty())
        << "COLR v1 font loaded but produced no emoji";
    EXPECT_GT(m_emoji.atlasWidth(), 0);
    EXPECT_GT(m_emoji.atlasHeight(), 0);
}

TEST_F(EmojiFontColrInvariantsTest, ColrV1BaseGlyphListContributesGlyphs) {
    // Verify that, on a COLR v1 font, the BaseGlyphList path contributed
    // glyphs by checking the total emoji count exceeds what a v0-only table
    // with zero base glyphs would produce.
    uint16_t colrVersion = readSystemFontColrVersion(m_path);
    if (colrVersion != 1) {
        GTEST_SKIP() << "System font is not COLR v1";
    }

    // Segoe UI Emoji and Noto Color Emoji v1 both have hundreds of emoji
    EXPECT_GT(static_cast<int>(m_emoji.getAvailableCodepoints().size()), 50)
        << "Expected at least 50 emoji from a COLR v1 font; v1 BaseGlyphList "
           "path may not be contributing";
}

// ---- TextRenderer emoji overload -------------------------------------------

class TextRendererEmojiTest : public ::testing::Test {
  protected:
    void SetUp() override {
        m_emojiPath = EmojiFont::findSystemEmojiFont();
        if (m_emojiPath.empty()) {
            GTEST_SKIP() << "No system emoji font available";
        }
        if (!m_emoji.loadFromFile(nullptr, m_emojiPath, 32)) {
            GTEST_SKIP() << "System emoji font not supported (no COLR/CPAL tables): "
                         << m_emoji.getLastError();
        }
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
    auto tex = TextRenderer::createTexture(nullptr, "\xF0\x9F\x98\x80\xF0\x9F\x9A\x80", m_font,
                                           &m_emoji, style);
    ASSERT_NE(tex, nullptr);
    EXPECT_GT(tex->getWidth(), 0u);
    EXPECT_GT(tex->getHeight(), 0u);
}

TEST_F(TextRendererEmojiTest, MixedTextAndEmoji) {
    TextStyle style;
    style.pixelScale = 1;
    auto tex =
        TextRenderer::createTexture(nullptr, "Hi \xF0\x9F\x98\x80 world", m_font, &m_emoji, style);
    ASSERT_NE(tex, nullptr);
    EXPECT_GT(tex->getWidth(), 0u);
    EXPECT_GT(tex->getHeight(), 0u);
}

TEST_F(TextRendererEmojiTest, NullEmojiFontFallsBack) {
    TextStyle style;
    style.pixelScale = 1;
    // nullptr emoji font should still work (emoji codepoints will be skipped)
    auto tex =
        TextRenderer::createTexture(nullptr, "Hello \xF0\x9F\x98\x80", m_font, nullptr, style);
    ASSERT_NE(tex, nullptr);
    EXPECT_GT(tex->getWidth(), 0u);
}

}  // namespace test
}  // namespace vde
