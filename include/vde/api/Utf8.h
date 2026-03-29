#pragma once

/**
 * @file Utf8.h
 * @brief Header-only UTF-8 encoding/decoding and emoji classification utilities.
 *
 * Provides functions for iterating UTF-8 strings by codepoint, encoding
 * codepoints back to UTF-8, and classifying emoji codepoint ranges.
 */

#include <cstdint>
#include <string>

namespace vde {
namespace utf8 {

/// Unicode replacement character returned on invalid sequences.
constexpr char32_t kReplacementChar = 0xFFFD;

/**
 * @brief Decode one UTF-8 codepoint starting at s[pos].
 * @param s UTF-8 encoded string
 * @param pos Current byte position; advanced past consumed bytes on return
 * @return Decoded codepoint, or kReplacementChar on invalid sequences
 */
inline char32_t decode(const std::string& s, size_t& pos) {
    if (pos >= s.size())
        return kReplacementChar;

    uint8_t b0 = static_cast<uint8_t>(s[pos]);

    // 1-byte (ASCII)
    if (b0 < 0x80) {
        pos += 1;
        return static_cast<char32_t>(b0);
    }

    // 2-byte: 110xxxxx 10xxxxxx
    if ((b0 & 0xE0) == 0xC0) {
        if (pos + 1 >= s.size()) {
            pos = s.size();
            return kReplacementChar;
        }
        uint8_t b1 = static_cast<uint8_t>(s[pos + 1]);
        if ((b1 & 0xC0) != 0x80) {
            pos += 1;
            return kReplacementChar;
        }
        char32_t cp = (static_cast<char32_t>(b0 & 0x1F) << 6) |
                      static_cast<char32_t>(b1 & 0x3F);
        pos += 2;
        return cp < 0x80 ? kReplacementChar : cp;  // reject overlong
    }

    // 3-byte: 1110xxxx 10xxxxxx 10xxxxxx
    if ((b0 & 0xF0) == 0xE0) {
        if (pos + 2 >= s.size()) {
            pos = s.size();
            return kReplacementChar;
        }
        uint8_t b1 = static_cast<uint8_t>(s[pos + 1]);
        uint8_t b2 = static_cast<uint8_t>(s[pos + 2]);
        if ((b1 & 0xC0) != 0x80 || (b2 & 0xC0) != 0x80) {
            pos += 1;
            return kReplacementChar;
        }
        char32_t cp = (static_cast<char32_t>(b0 & 0x0F) << 12) |
                      (static_cast<char32_t>(b1 & 0x3F) << 6) |
                      static_cast<char32_t>(b2 & 0x3F);
        pos += 3;
        return cp < 0x800 ? kReplacementChar : cp;  // reject overlong
    }

    // 4-byte: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
    if ((b0 & 0xF8) == 0xF0) {
        if (pos + 3 >= s.size()) {
            pos = s.size();
            return kReplacementChar;
        }
        uint8_t b1 = static_cast<uint8_t>(s[pos + 1]);
        uint8_t b2 = static_cast<uint8_t>(s[pos + 2]);
        uint8_t b3 = static_cast<uint8_t>(s[pos + 3]);
        if ((b1 & 0xC0) != 0x80 || (b2 & 0xC0) != 0x80 || (b3 & 0xC0) != 0x80) {
            pos += 1;
            return kReplacementChar;
        }
        char32_t cp = (static_cast<char32_t>(b0 & 0x07) << 18) |
                      (static_cast<char32_t>(b1 & 0x3F) << 12) |
                      (static_cast<char32_t>(b2 & 0x3F) << 6) |
                      static_cast<char32_t>(b3 & 0x3F);
        pos += 4;
        if (cp < 0x10000 || cp > 0x10FFFF)
            return kReplacementChar;
        return cp;
    }

    // Invalid leading byte
    pos += 1;
    return kReplacementChar;
}

/**
 * @brief Encode a Unicode codepoint as UTF-8 bytes appended to out.
 * @param cp Unicode codepoint
 * @param out String to append encoded bytes to
 */
inline void encode(char32_t cp, std::string& out) {
    if (cp < 0x80) {
        out += static_cast<char>(cp);
    } else if (cp < 0x800) {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp <= 0x10FFFF) {
        out += static_cast<char>(0xF0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
}

/**
 * @brief Check if a codepoint falls in a standard emoji range.
 * @param cp Unicode codepoint
 * @return true if the codepoint is in a commonly recognized emoji block
 */
inline bool isEmoji(char32_t cp) {
    // Miscellaneous Symbols
    if (cp >= 0x2600 && cp <= 0x26FF)
        return true;
    // Dingbats
    if (cp >= 0x2700 && cp <= 0x27BF)
        return true;
    // Emoticons
    if (cp >= 0x1F600 && cp <= 0x1F64F)
        return true;
    // Miscellaneous Symbols and Pictographs
    if (cp >= 0x1F300 && cp <= 0x1F5FF)
        return true;
    // Transport and Map Symbols
    if (cp >= 0x1F680 && cp <= 0x1F6FF)
        return true;
    // Supplemental Symbols and Pictographs
    if (cp >= 0x1F900 && cp <= 0x1F9FF)
        return true;
    // Symbols and Pictographs Extended-A
    if (cp >= 0x1FA00 && cp <= 0x1FA6F)
        return true;
    // Symbols and Pictographs Extended-B
    if (cp >= 0x1FA70 && cp <= 0x1FAFF)
        return true;
    // Regional indicator symbols
    if (cp >= 0x1F1E0 && cp <= 0x1F1FF)
        return true;
    // Mahjong Tiles and Domino Tiles
    if (cp >= 0x1F000 && cp <= 0x1F0FF)
        return true;
    // Variation selectors and ZWJ used in emoji sequences
    if (cp == 0x200D || cp == 0xFE0F || cp == 0x20E3)
        return true;
    // Common BMP emoji symbols
    if (cp == 0x2764 || cp == 0x2763)
        return true;  // hearts
    if (cp == 0x231A || cp == 0x231B)
        return true;
    if (cp >= 0x23E9 && cp <= 0x23F3)
        return true;
    if (cp == 0x23F8 || cp == 0x23F9 || cp == 0x23FA)
        return true;
    if (cp >= 0x25AA && cp <= 0x25AB)
        return true;
    if (cp == 0x25B6 || cp == 0x25C0)
        return true;
    if (cp >= 0x25FB && cp <= 0x25FE)
        return true;
    if (cp >= 0x2648 && cp <= 0x2653)
        return true;  // zodiac
    if (cp >= 0x2B05 && cp <= 0x2B55)
        return true;  // arrows and geometric shapes (⬅⬆⬇⬛⬜⭐⭕)
    if (cp == 0x203C || cp == 0x2049)
        return true;
    if (cp == 0x2122 || cp == 0x2139)
        return true;
    if (cp >= 0x2194 && cp <= 0x2199)
        return true;
    if (cp == 0x21A9 || cp == 0x21AA)
        return true;
    return false;
}

/**
 * @brief Iterate over all codepoints in a UTF-8 string.
 * @param s UTF-8 encoded string
 * @param fn Callable invoked as fn(char32_t) for each decoded codepoint
 */
template <typename Fn>
inline void forEach(const std::string& s, Fn fn) {
    size_t pos = 0;
    while (pos < s.size()) {
        fn(decode(s, pos));
    }
}

/**
 * @brief Count the number of codepoints in a UTF-8 string.
 * @param s UTF-8 encoded string
 * @return Number of codepoints
 */
inline size_t length(const std::string& s) {
    size_t count = 0;
    size_t pos = 0;
    while (pos < s.size()) {
        decode(s, pos);
        ++count;
    }
    return count;
}

}  // namespace utf8
}  // namespace vde
