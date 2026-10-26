/**
 * @file EmojiFont_test.cpp
 * @brief Unit tests for EmojiFont color emoji loader.
 */

#include <vde/Texture.h>
#include <vde/api/EmojiFont.h>
#include <vde/api/TextRenderer.h>
#include <vde/api/TrueTypeFont.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <string>
#include <system_error>
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

#ifdef __APPLE__
// ImageIO decodes sbix emoji bitmaps with private codec dylibs (libPng.dylib,
// libJPEG.dylib, ...). DYLD_LIBRARY_PATH is searched by leaf name before their
// install paths and APFS is case-insensitive by default, so a directory holding
// Homebrew's libpng.dylib/libjpeg.dylib/libgif.dylib/libtiff.dylib replaces
// ImageIO's copies inside CoreText rendering.
static std::vector<std::string> findShadowedImageIOCodecs() {
    std::vector<std::string> shadowed;
    const char* searchPath = std::getenv("DYLD_LIBRARY_PATH");
    if (!searchPath) {
        return shadowed;
    }
    const std::string paths = searchPath;
    size_t start = 0;
    while (start <= paths.size()) {
        size_t end = paths.find(':', start);
        if (end == std::string::npos) {
            end = paths.size();
        }
        const std::filesystem::path dir = paths.substr(start, end - start);
        if (!dir.empty()) {
            for (const char* name :
                 {"libPng.dylib", "libJPEG.dylib", "libGIF.dylib", "libTIFF.dylib"}) {
                std::error_code error;
                if (std::filesystem::exists(dir / name, error)) {
                    shadowed.push_back((dir / name).string());
                }
            }
        }
        start = end + 1;
    }
    return shadowed;
}

static std::string joinPaths(const std::vector<std::string>& paths) {
    std::string joined;
    for (const auto& path : paths) {
        joined += (joined.empty() ? "" : ", ") + path;
    }
    return joined;
}

TEST_F(EmojiFontTest, AppleImageIOCodecsAreNotShadowedByDyldLibraryPath) {
    const auto shadowed = findShadowedImageIOCodecs();
    EXPECT_TRUE(shadowed.empty())
        << "DYLD_LIBRARY_PATH replaces ImageIO codec libraries used to draw Apple emoji: "
        << joinPaths(shadowed) << ". Use DYLD_FALLBACK_LIBRARY_PATH instead.";
}

TEST_F(EmojiFontTest, AppleBitmapFontRendersColoredGlyphsWithoutGPU) {
    const auto shadowed = findShadowedImageIOCodecs();
    ASSERT_TRUE(shadowed.empty()) << "ImageIO codecs shadowed by DYLD_LIBRARY_PATH: "
                                  << joinPaths(shadowed);
    const auto path = EmojiFont::findSystemEmojiFont();
    ASSERT_FALSE(path.empty());
    EmojiFont emoji;
    ASSERT_TRUE(emoji.loadFromFile(nullptr, path, 32)) << emoji.getLastError();
    ASSERT_TRUE(emoji.hasGlyph(0x1F600));
    ASSERT_TRUE(emoji.hasGlyph(0x1F680));
    const auto* glyph = emoji.getGlyph(0x1F600);
    ASSERT_NE(glyph, nullptr);
    EXPECT_EQ(glyph->width, 32);
    EXPECT_EQ(glyph->height, 32);
    EXPECT_EQ(emoji.emojiSize(), 32);
    std::vector<uint8_t> pixels(static_cast<size_t>(glyph->width) * glyph->height * 4);
    ASSERT_TRUE(emoji.copyGlyphPixels(0x1F600, pixels.data()));
    bool hasColor = false;
    bool hasTransparency = false;
    for (size_t i = 0; i < pixels.size(); i += 4) {
        hasTransparency = hasTransparency || pixels[i + 3] == 0;
        hasColor = hasColor || (pixels[i + 3] > 0 &&
                                (pixels[i] != pixels[i + 1] || pixels[i + 1] != pixels[i + 2]));
    }
    EXPECT_TRUE(hasColor);
    EXPECT_TRUE(hasTransparency);
    ASSERT_NE(emoji.getAtlasTexture(), nullptr);
    EXPECT_FALSE(emoji.getAtlasTexture()->isOnGPU());

    EXPECT_FALSE(emoji.loadFromFile(nullptr, path, 16385));
    EXPECT_FALSE(emoji.isLoaded());
    EXPECT_TRUE(emoji.getAvailableCodepoints().empty());
    EXPECT_EQ(emoji.getAtlasTexture(), nullptr);
    EXPECT_FALSE(emoji.getLastError().empty());
}
#endif

// ---- Synthetic font / collection dispatch tests ------------------------------

namespace {

void putU16(std::vector<uint8_t>& out, size_t at, uint32_t value) {
    out[at] = static_cast<uint8_t>(value >> 8);
    out[at + 1] = static_cast<uint8_t>(value);
}

void putU32(std::vector<uint8_t>& out, size_t at, uint32_t value) {
    putU16(out, at, value >> 16);
    putU16(out, at + 2, value & 0xFFFF);
}

std::vector<uint8_t> bytesU16(std::initializer_list<uint32_t> values) {
    std::vector<uint8_t> out(values.size() * 2);
    size_t at = 0;
    for (uint32_t value : values) {
        putU16(out, at, value);
        at += 2;
    }
    return out;
}

struct SyntheticTable {
    uint32_t tag;
    std::vector<uint8_t> data;
};

/// Build an sfnt face whose table offsets are relative to a file in which the
/// face starts at @p faceOffset (as required inside a TrueType collection).
std::vector<uint8_t> buildFace(const std::vector<SyntheticTable>& tables, uint32_t faceOffset) {
    const size_t headerSize = 12 + tables.size() * 16;
    std::vector<uint8_t> face(headerSize, 0);
    putU32(face, 0, 0x00010000);
    putU16(face, 4, static_cast<uint32_t>(tables.size()));
    for (size_t i = 0; i < tables.size(); ++i) {
        const size_t record = 12 + i * 16;
        putU32(face, record, tables[i].tag);
        putU32(face, record + 8, static_cast<uint32_t>(faceOffset + face.size()));
        putU32(face, record + 12, static_cast<uint32_t>(tables[i].data.size()));
        face.insert(face.end(), tables[i].data.begin(), tables[i].data.end());
        face.resize((face.size() + 3) & ~size_t{3}, 0);
    }
    return face;
}

/// One-glyph font mapping U+1F600 to a square drawn with a red COLR v0 layer.
std::vector<SyntheticTable> colorEmojiTables(bool includeColor, bool includeOutlines) {
    std::vector<uint8_t> head(54, 0);
    putU32(head, 0, 0x00010000);
    putU32(head, 12, 0x5F0F3CF5);
    putU16(head, 18, 1000);  // unitsPerEm; indexToLocFormat (offset 50) = short
    std::vector<uint8_t> hhea(36, 0);
    putU32(hhea, 0, 0x00010000);
    putU16(hhea, 4, 800);              // ascender
    putU16(hhea, 6, 0x10000 - 200);    // descender
    putU16(hhea, 34, 2);               // numberOfHMetrics
    std::vector<uint8_t> cmap(40, 0);  // format 12, platform 3 encoding 10
    putU16(cmap, 2, 1);
    putU16(cmap, 4, 3);
    putU16(cmap, 6, 10);
    putU32(cmap, 8, 12);
    putU16(cmap, 12, 12);
    putU32(cmap, 16, 28);
    putU32(cmap, 24, 1);
    putU32(cmap, 28, 0x1F600);
    putU32(cmap, 32, 0x1F600);
    putU32(cmap, 36, 1);
    // Glyph 1: square (100,0)-(900,800), one contour of four on-curve points.
    const auto glyf = bytesU16(
        {1, 100, 0, 900, 800, 3, 0, 0x0101, 0x0101, 100, 800, 0, 0x10000 - 800, 0, 0, 800, 0});

    std::vector<SyntheticTable> tables;
    if (includeColor) {
        tables.push_back({0x434F4C52, bytesU16({0, 1, 0, 14, 0, 20, 1, 1, 0, 1, 1, 0})});  // COLR
        tables.push_back({0x4350414C, bytesU16({0, 1, 1, 1, 0, 14, 0, 0, 0xFFFF})});       // CPAL
    }
    tables.push_back({0x636D6170, cmap});  // cmap
    if (includeOutlines) {
        tables.push_back({0x676C7966, glyf});  // glyf
    }
    tables.push_back({0x68656164, head});                            // head
    tables.push_back({0x68686561, hhea});                            // hhea
    tables.push_back({0x686D7478, bytesU16({1000, 0, 1000, 100})});  // hmtx
    if (includeOutlines) {
        tables.push_back({0x6C6F6361, bytesU16({0, 0, 17})});  // loca
    }
    tables.push_back({0x6D617870, bytesU16({0, 0x5000, 2})});  // maxp
    return tables;
}

std::vector<uint8_t> buildCollection(const std::vector<SyntheticTable>& tables) {
    std::vector<uint8_t> collection(16, 0);
    putU32(collection, 0, 0x74746366);  // 'ttcf'
    putU32(collection, 4, 0x00010000);
    putU32(collection, 8, 1);
    putU32(collection, 12, 16);
    const auto face = buildFace(tables, 16);
    collection.insert(collection.end(), face.begin(), face.end());
    return collection;
}

class TempFontFile {
  public:
    explicit TempFontFile(const std::vector<uint8_t>& bytes) {
        static int counter = 0;
        m_path = std::filesystem::temp_directory_path() /
                 ("vde_emoji_test_" +
                  std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
                  "_" + std::to_string(counter++) + ".ttf");
        std::ofstream out(m_path, std::ios::binary);
        out.write(reinterpret_cast<const char*>(bytes.data()),
                  static_cast<std::streamsize>(bytes.size()));
    }
    ~TempFontFile() {
        std::error_code error;
        std::filesystem::remove(m_path, error);
    }
    TempFontFile(const TempFontFile&) = delete;
    TempFontFile& operator=(const TempFontFile&) = delete;
    [[nodiscard]] std::string path() const { return m_path.string(); }

  private:
    std::filesystem::path m_path;
};

void expectRedSyntheticEmoji(const EmojiFont& emoji) {
    ASSERT_TRUE(emoji.isLoaded());
    ASSERT_TRUE(emoji.hasGlyph(0x1F600));
    EXPECT_EQ(emoji.getAvailableCodepoints().size(), 1u);
    ASSERT_NE(emoji.getAtlasTexture(), nullptr);
    EXPECT_FALSE(emoji.getAtlasTexture()->isOnGPU());
    std::vector<uint8_t> pixels(16 * 16 * 4);
    ASSERT_TRUE(emoji.copyGlyphPixels(0x1F600, pixels.data()));
    bool hasRed = false;
    bool hasTransparency = false;
    for (size_t i = 0; i < pixels.size(); i += 4) {
        hasTransparency = hasTransparency || pixels[i + 3] == 0;
        hasRed = hasRed || (pixels[i + 3] > 200 && pixels[i] > 200 && pixels[i + 1] < 50 &&
                            pixels[i + 2] < 50);
    }
    EXPECT_TRUE(hasRed);
    EXPECT_TRUE(hasTransparency);
}

}  // namespace

TEST_F(EmojiFontTest, SyntheticColrFontLoads) {
    TempFontFile file(buildFace(colorEmojiTables(true, true), 0));
    EmojiFont emoji;
    ASSERT_TRUE(emoji.loadFromFile(nullptr, file.path(), 16)) << emoji.getLastError();
    expectRedSyntheticEmoji(emoji);
}

TEST_F(EmojiFontTest, ColrCollectionUsesFirstFaceTables) {
    // A 'ttcf' header is only a container: COLR/CPAL collections must still use
    // the COLR backend on every platform, with tables read from the selected face.
    TempFontFile file(buildCollection(colorEmojiTables(true, true)));
    EmojiFont emoji;
    ASSERT_TRUE(emoji.loadFromFile(nullptr, file.path(), 16)) << emoji.getLastError();
    expectRedSyntheticEmoji(emoji);
}

TEST_F(EmojiFontTest, CollectionWithoutColorTablesFails) {
    TempFontFile file(buildCollection(colorEmojiTables(false, true)));
    EmojiFont emoji;
    EXPECT_FALSE(emoji.loadFromFile(nullptr, file.path(), 16));
    EXPECT_FALSE(emoji.isLoaded());
    EXPECT_NE(emoji.getLastError().find("COLR/CPAL"), std::string::npos) << emoji.getLastError();
}

TEST_F(EmojiFontTest, ColorFaceWithoutOutlineTablesFails) {
    TempFontFile file(buildCollection(colorEmojiTables(true, false)));
    EmojiFont emoji;
    EXPECT_FALSE(emoji.loadFromFile(nullptr, file.path(), 16));
    EXPECT_FALSE(emoji.isLoaded());
    EXPECT_NE(emoji.getLastError().find("Failed to initialize"), std::string::npos)
        << emoji.getLastError();
}

TEST_F(EmojiFontTest, MalformedFontHeadersFail) {
    auto truncatedCollection = buildCollection(colorEmojiTables(true, true));
    truncatedCollection.resize(14);

    auto facePastEnd = buildCollection(colorEmojiTables(true, true));
    putU32(facePastEnd, 12, 0xFFFFFF00);

    auto directoryPastEnd = buildCollection(colorEmojiTables(true, true));
    putU16(directoryPastEnd, 16 + 4, 0xFFFF);

    auto unknownVersion = buildFace(colorEmojiTables(true, true), 0);
    putU32(unknownVersion, 0, 0xDEADBEEF);

    const std::vector<uint8_t> tooShort = {'t', 't', 'c', 'f'};

    for (const auto& bytes :
         {truncatedCollection, facePastEnd, directoryPastEnd, unknownVersion, tooShort}) {
        TempFontFile file(bytes);
        EmojiFont emoji;
        EXPECT_FALSE(emoji.loadFromFile(nullptr, file.path(), 16));
        EXPECT_FALSE(emoji.isLoaded());
        EXPECT_FALSE(emoji.getLastError().empty());
    }
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

    std::vector<uint8_t> pixels(static_cast<size_t>(glyph->width) * glyph->height * 4, 0);
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
    if (!f.is_open()) {
        return 0xFFFF;
    }
    auto pos = f.tellg();
    if (pos < 0 || pos > static_cast<std::ifstream::pos_type>(256 * 1024 * 1024)) {
        return 0xFFFF;
    }
    auto sz = static_cast<size_t>(pos);
    if (sz < 12) {
        return 0xFFFF;
    }
    f.seekg(0);
    std::vector<uint8_t> data(sz);
    if (!f.read(reinterpret_cast<char*>(data.data()), sz)) {
        return 0xFFFF;
    }

    // OpenType table directory: numTables at offset 4
    uint16_t numTables = (static_cast<uint16_t>(data[4]) << 8) | data[5];
    if (sz < static_cast<size_t>(12) + static_cast<size_t>(numTables) * 16u) {
        return 0xFFFF;
    }

    constexpr uint32_t kTagCOLR = 0x434F4C52u;
    for (uint16_t i = 0; i < numTables; ++i) {
        const uint8_t* e = data.data() + 12 + static_cast<ptrdiff_t>(i) * 16;
        uint32_t tag = (static_cast<uint32_t>(e[0]) << 24) | (static_cast<uint32_t>(e[1]) << 16) |
                       (static_cast<uint32_t>(e[2]) << 8) | e[3];
        if (tag == kTagCOLR) {
            uint32_t off = (static_cast<uint32_t>(e[8]) << 24) |
                           (static_cast<uint32_t>(e[9]) << 16) |
                           (static_cast<uint32_t>(e[10]) << 8) | e[11];
            if (off + 2 > sz) {
                return 0xFFFF;
            }
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
    EXPECT_TRUE(std::ranges::is_sorted(codepoints))
        << "Available codepoints are not in ascending order";
}

TEST_F(EmojiFontColrInvariantsTest, NoDuplicateCodepoints) {
    const auto& codepoints = m_emoji.getAvailableCodepoints();
    ASSERT_FALSE(codepoints.empty());
    // The list is already sorted, so adjacent duplicates would be immediately obvious
    auto it = std::ranges::adjacent_find(codepoints);
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
