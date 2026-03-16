#pragma once

/**
 * @file BitmapFont.h
 * @brief Embedded pixel font for bitmap text rendering.
 *
 * Provides zero-dependency bitmap fonts baked into the engine binary.
 * No external font files are required.
 */

#include <cstdint>

namespace vde {

/**
 * @brief A bitmap font that stores glyph bitmasks for ASCII characters.
 *
 * Each glyph is stored as an array of row bitmasks where the most significant
 * bits represent the leftmost columns. Ships two built-in sizes:
 * - `BitmapFont::small()` — 5x7 pixel glyphs
 * - `BitmapFont::large()` — 8x13 pixel glyphs
 *
 * Supports ASCII printable characters 0x20–0x7E. Characters outside this range
 * are rendered as blank (space).
 *
 * @example
 * @code
 * const auto& font = vde::BitmapFont::small();
 * int w = font.glyphWidth();   // 5
 * int h = font.glyphHeight();  // 7
 * uint8_t row = font.glyphRow('A', 0); // top row of 'A'
 * @endcode
 */
class BitmapFont {
  public:
    /**
     * @brief Get the 5x7 built-in bitmap font.
     * @return Reference to the singleton small font
     */
    static const BitmapFont& small();

    /**
     * @brief Get the 8x13 built-in bitmap font.
     * @return Reference to the singleton large font
     */
    static const BitmapFont& large();

    /**
     * @brief Get a bitmask row for a glyph.
     * @param c The character to look up (ASCII 0x20–0x7E)
     * @param row The row index (0 = top row)
     * @return Bitmask with the MSB representing the leftmost column.
     *         Returns 0 for unsupported characters or out-of-range rows.
     */
    uint8_t glyphRow(char c, int row) const;

    /**
     * @brief Get the width of each glyph in pixels.
     * @return Glyph width
     */
    int glyphWidth() const { return m_glyphWidth; }

    /**
     * @brief Get the height of each glyph in pixels.
     * @return Glyph height
     */
    int glyphHeight() const { return m_glyphHeight; }

  private:
    BitmapFont(int glyphWidth, int glyphHeight, const uint8_t* data, int glyphCount,
               char firstChar);

    int m_glyphWidth;
    int m_glyphHeight;
    const uint8_t* m_data;  ///< Packed row data: glyphCount * glyphHeight bytes
    int m_glyphCount;
    char m_firstChar;  ///< First character code in the table (e.g. 0x20)
};

}  // namespace vde
