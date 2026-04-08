/**
 * @file BitmapFont_test.cpp
 * @brief Unit tests for BitmapFont class (Phase 6 — Audit Remediation)
 *
 * Tests glyph metrics, row bitmask retrieval, and edge cases for the
 * built-in small (5x7) and large (8x13) bitmap fonts.
 */

#include <vde/api/BitmapFont.h>

#include <gtest/gtest.h>

namespace vde {
namespace test {

// ============================================================================
// Small font metrics
// ============================================================================

TEST(BitmapFontSmall, GlyphWidthIs5) {
    const BitmapFont& font = BitmapFont::small();
    EXPECT_EQ(font.glyphWidth(), 5);
}

TEST(BitmapFontSmall, GlyphHeightIs7) {
    const BitmapFont& font = BitmapFont::small();
    EXPECT_EQ(font.glyphHeight(), 7);
}

// ============================================================================
// Large font metrics
// ============================================================================

TEST(BitmapFontLarge, GlyphWidthIs8) {
    const BitmapFont& font = BitmapFont::large();
    EXPECT_EQ(font.glyphWidth(), 8);
}

TEST(BitmapFontLarge, GlyphHeightIs13) {
    const BitmapFont& font = BitmapFont::large();
    EXPECT_EQ(font.glyphHeight(), 13);
}

// ============================================================================
// Singleton identity
// ============================================================================

TEST(BitmapFontSmall, SingletonReturnsSameInstance) {
    const BitmapFont& a = BitmapFont::small();
    const BitmapFont& b = BitmapFont::small();
    EXPECT_EQ(&a, &b);
}

TEST(BitmapFontLarge, SingletonReturnsSameInstance) {
    const BitmapFont& a = BitmapFont::large();
    const BitmapFont& b = BitmapFont::large();
    EXPECT_EQ(&a, &b);
}

// ============================================================================
// Known glyph data — 'A' has non-zero rows
// ============================================================================

TEST(BitmapFontSmall, GlyphRowAHasData) {
    const BitmapFont& font = BitmapFont::small();
    bool anyNonZero = false;
    for (int row = 0; row < font.glyphHeight(); ++row) {
        if (font.glyphRow('A', row) != 0) {
            anyNonZero = true;
            break;
        }
    }
    EXPECT_TRUE(anyNonZero);
}

TEST(BitmapFontLarge, GlyphRowAHasData) {
    const BitmapFont& font = BitmapFont::large();
    bool anyNonZero = false;
    for (int row = 0; row < font.glyphHeight(); ++row) {
        if (font.glyphRow('A', row) != 0) {
            anyNonZero = true;
            break;
        }
    }
    EXPECT_TRUE(anyNonZero);
}

// ============================================================================
// Space glyph is blank
// ============================================================================

TEST(BitmapFontSmall, SpaceGlyphIsBlank) {
    const BitmapFont& font = BitmapFont::small();
    for (int row = 0; row < font.glyphHeight(); ++row) {
        EXPECT_EQ(font.glyphRow(' ', row), 0u);
    }
}

TEST(BitmapFontLarge, SpaceGlyphIsBlank) {
    const BitmapFont& font = BitmapFont::large();
    for (int row = 0; row < font.glyphHeight(); ++row) {
        EXPECT_EQ(font.glyphRow(' ', row), 0u);
    }
}

// ============================================================================
// Out-of-range characters return blank
// ============================================================================

TEST(BitmapFontSmall, OutOfRangeCharReturnsZero) {
    const BitmapFont& font = BitmapFont::small();
    // Characters below 0x20 and above 0x7E should map to blank
    for (int row = 0; row < font.glyphHeight(); ++row) {
        EXPECT_EQ(font.glyphRow('\0', row), 0u);
        EXPECT_EQ(font.glyphRow('\n', row), 0u);
        EXPECT_EQ(font.glyphRow(0x7F, row), 0u);
    }
}

TEST(BitmapFontLarge, OutOfRangeCharReturnsZero) {
    const BitmapFont& font = BitmapFont::large();
    for (int row = 0; row < font.glyphHeight(); ++row) {
        EXPECT_EQ(font.glyphRow('\0', row), 0u);
        EXPECT_EQ(font.glyphRow('\n', row), 0u);
        EXPECT_EQ(font.glyphRow(0x7F, row), 0u);
    }
}

// ============================================================================
// Out-of-range row index returns zero
// ============================================================================

TEST(BitmapFontSmall, OutOfRangeRowReturnsZero) {
    const BitmapFont& font = BitmapFont::small();
    EXPECT_EQ(font.glyphRow('A', -1), 0u);
    EXPECT_EQ(font.glyphRow('A', font.glyphHeight()), 0u);
    EXPECT_EQ(font.glyphRow('A', 999), 0u);
}

TEST(BitmapFontLarge, OutOfRangeRowReturnsZero) {
    const BitmapFont& font = BitmapFont::large();
    EXPECT_EQ(font.glyphRow('A', -1), 0u);
    EXPECT_EQ(font.glyphRow('A', font.glyphHeight()), 0u);
    EXPECT_EQ(font.glyphRow('A', 999), 0u);
}

// ============================================================================
// All printable ASCII glyphs are accessible without crash
// ============================================================================

TEST(BitmapFontSmall, AllPrintableASCIIAccessible) {
    const BitmapFont& font = BitmapFont::small();
    for (char c = 0x20; c <= 0x7E; ++c) {
        for (int row = 0; row < font.glyphHeight(); ++row) {
            // Just exercise the path — result is valid uint8_t
            [[maybe_unused]] uint8_t val = font.glyphRow(c, row);
        }
    }
}

TEST(BitmapFontLarge, AllPrintableASCIIAccessible) {
    const BitmapFont& font = BitmapFont::large();
    for (char c = 0x20; c <= 0x7E; ++c) {
        for (int row = 0; row < font.glyphHeight(); ++row) {
            [[maybe_unused]] uint8_t val = font.glyphRow(c, row);
        }
    }
}

// ============================================================================
// Different glyphs produce different bitmasks
// ============================================================================

TEST(BitmapFontSmall, DifferentGlyphsProduceDifferentData) {
    const BitmapFont& font = BitmapFont::small();
    // 'A' and 'B' should not share the exact same row data
    bool anyDiff = false;
    for (int row = 0; row < font.glyphHeight(); ++row) {
        if (font.glyphRow('A', row) != font.glyphRow('B', row)) {
            anyDiff = true;
            break;
        }
    }
    EXPECT_TRUE(anyDiff);
}

TEST(BitmapFontLarge, DifferentGlyphsProduceDifferentData) {
    const BitmapFont& font = BitmapFont::large();
    bool anyDiff = false;
    for (int row = 0; row < font.glyphHeight(); ++row) {
        if (font.glyphRow('A', row) != font.glyphRow('B', row)) {
            anyDiff = true;
            break;
        }
    }
    EXPECT_TRUE(anyDiff);
}

}  // namespace test
}  // namespace vde
