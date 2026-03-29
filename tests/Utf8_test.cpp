/**
 * @file Utf8_test.cpp
 * @brief Unit tests for UTF-8 encoding/decoding and emoji classification utilities.
 */

#include <vde/api/Utf8.h>

#include <gtest/gtest.h>

namespace vde {
namespace test {

class Utf8DecodeTest : public ::testing::Test {};

// ---- Single-byte (ASCII) ---------------------------------------------------

TEST_F(Utf8DecodeTest, AsciiCharacters) {
    std::string s = "A";
    size_t pos = 0;
    EXPECT_EQ(utf8::decode(s, pos), U'A');
    EXPECT_EQ(pos, 1u);
}

TEST_F(Utf8DecodeTest, AsciiNullByte) {
    std::string s(1, '\0');
    size_t pos = 0;
    EXPECT_EQ(utf8::decode(s, pos), U'\0');
    EXPECT_EQ(pos, 1u);
}

TEST_F(Utf8DecodeTest, AsciiFullString) {
    std::string s = "Hello";
    size_t pos = 0;
    EXPECT_EQ(utf8::decode(s, pos), U'H');
    EXPECT_EQ(utf8::decode(s, pos), U'e');
    EXPECT_EQ(utf8::decode(s, pos), U'l');
    EXPECT_EQ(utf8::decode(s, pos), U'l');
    EXPECT_EQ(utf8::decode(s, pos), U'o');
    EXPECT_EQ(pos, 5u);
}

// ---- Two-byte sequences ----------------------------------------------------

TEST_F(Utf8DecodeTest, TwoByteCharacter) {
    // U+00E9 (é) = 0xC3 0xA9
    std::string s = "\xC3\xA9";
    size_t pos = 0;
    EXPECT_EQ(utf8::decode(s, pos), U'\u00E9');
    EXPECT_EQ(pos, 2u);
}

TEST_F(Utf8DecodeTest, TwoByteMinimal) {
    // U+0080 = 0xC2 0x80 (smallest valid 2-byte)
    std::string s = "\xC2\x80";
    size_t pos = 0;
    EXPECT_EQ(utf8::decode(s, pos), U'\u0080');
    EXPECT_EQ(pos, 2u);
}

// ---- Three-byte sequences --------------------------------------------------

TEST_F(Utf8DecodeTest, ThreeByteCharacter) {
    // U+2764 (❤) = 0xE2 0x9D 0xA4
    std::string s = "\xE2\x9D\xA4";
    size_t pos = 0;
    EXPECT_EQ(utf8::decode(s, pos), U'\u2764');
    EXPECT_EQ(pos, 3u);
}

TEST_F(Utf8DecodeTest, ThreeByteEuroSign) {
    // U+20AC (€) = 0xE2 0x82 0xAC
    std::string s = "\xE2\x82\xAC";
    size_t pos = 0;
    EXPECT_EQ(utf8::decode(s, pos), U'\u20AC');
    EXPECT_EQ(pos, 3u);
}

// ---- Four-byte sequences (emoji) -------------------------------------------

TEST_F(Utf8DecodeTest, FourByteEmoji) {
    // U+1F600 (😀) = 0xF0 0x9F 0x98 0x80
    std::string s = "\xF0\x9F\x98\x80";
    size_t pos = 0;
    EXPECT_EQ(utf8::decode(s, pos), U'\U0001F600');
    EXPECT_EQ(pos, 4u);
}

TEST_F(Utf8DecodeTest, FourByteRocket) {
    // U+1F680 (🚀) = 0xF0 0x9F 0x9A 0x80
    std::string s = "\xF0\x9F\x9A\x80";
    size_t pos = 0;
    EXPECT_EQ(utf8::decode(s, pos), U'\U0001F680');
    EXPECT_EQ(pos, 4u);
}

TEST_F(Utf8DecodeTest, MixedAsciiAndEmoji) {
    // "Hi😀!" = H i 😀 !
    std::string s = "Hi\xF0\x9F\x98\x80!";
    size_t pos = 0;
    EXPECT_EQ(utf8::decode(s, pos), U'H');
    EXPECT_EQ(utf8::decode(s, pos), U'i');
    EXPECT_EQ(utf8::decode(s, pos), U'\U0001F600');
    EXPECT_EQ(utf8::decode(s, pos), U'!');
    EXPECT_EQ(pos, s.size());
}

// ---- Invalid sequences -----------------------------------------------------

TEST_F(Utf8DecodeTest, InvalidLeadingByte) {
    std::string s = "\xFF";
    size_t pos = 0;
    EXPECT_EQ(utf8::decode(s, pos), utf8::kReplacementChar);
    EXPECT_EQ(pos, 1u);
}

TEST_F(Utf8DecodeTest, TruncatedTwoByte) {
    std::string s = "\xC3";  // missing continuation byte
    size_t pos = 0;
    EXPECT_EQ(utf8::decode(s, pos), utf8::kReplacementChar);
    EXPECT_EQ(pos, s.size());
}

TEST_F(Utf8DecodeTest, TruncatedFourByte) {
    std::string s = "\xF0\x9F\x98";  // missing last byte
    size_t pos = 0;
    EXPECT_EQ(utf8::decode(s, pos), utf8::kReplacementChar);
    EXPECT_EQ(pos, s.size());
}

TEST_F(Utf8DecodeTest, OverlongTwoByte) {
    // Overlong encoding of U+0001 as 0xC0 0x81 → should reject
    std::string s = "\xC0\x81";
    size_t pos = 0;
    EXPECT_EQ(utf8::decode(s, pos), utf8::kReplacementChar);
}

TEST_F(Utf8DecodeTest, EndOfString) {
    std::string s = "";
    size_t pos = 0;
    EXPECT_EQ(utf8::decode(s, pos), utf8::kReplacementChar);
}

TEST_F(Utf8DecodeTest, PositionBeyondEnd) {
    std::string s = "A";
    size_t pos = 5;
    EXPECT_EQ(utf8::decode(s, pos), utf8::kReplacementChar);
}

// ============================================================================
// UTF-8 Encode tests
// ============================================================================

class Utf8EncodeTest : public ::testing::Test {};

TEST_F(Utf8EncodeTest, AsciiEncode) {
    std::string out;
    utf8::encode(U'A', out);
    EXPECT_EQ(out, "A");
}

TEST_F(Utf8EncodeTest, TwoByteEncode) {
    std::string out;
    utf8::encode(U'\u00E9', out);  // é
    EXPECT_EQ(out, "\xC3\xA9");
}

TEST_F(Utf8EncodeTest, ThreeByteEncode) {
    std::string out;
    utf8::encode(U'\u2764', out);  // ❤
    EXPECT_EQ(out, "\xE2\x9D\xA4");
}

TEST_F(Utf8EncodeTest, FourByteEncode) {
    std::string out;
    utf8::encode(U'\U0001F600', out);  // 😀
    EXPECT_EQ(out, "\xF0\x9F\x98\x80");
}

TEST_F(Utf8EncodeTest, RoundTrip) {
    // Encode then decode should return the original codepoint
    for (char32_t cp : {U'A', U'\u00E9', U'\u2764', U'\U0001F600', U'\U0001F680'}) {
        std::string encoded;
        utf8::encode(cp, encoded);

        size_t pos = 0;
        char32_t decoded = utf8::decode(encoded, pos);
        EXPECT_EQ(decoded, cp) << "Round-trip failed for U+" << std::hex
                               << static_cast<uint32_t>(cp);
        EXPECT_EQ(pos, encoded.size());
    }
}

// ============================================================================
// Emoji classification tests
// ============================================================================

class Utf8EmojiTest : public ::testing::Test {};

TEST_F(Utf8EmojiTest, CommonEmoji) {
    EXPECT_TRUE(utf8::isEmoji(U'\U0001F600'));  // 😀
    EXPECT_TRUE(utf8::isEmoji(U'\U0001F680'));  // 🚀
    EXPECT_TRUE(utf8::isEmoji(U'\U0001F525'));  // 🔥
    EXPECT_TRUE(utf8::isEmoji(U'\U0001F4A7'));  // 💧
    EXPECT_TRUE(utf8::isEmoji(U'\U0001F3AE'));  // 🎮
}

TEST_F(Utf8EmojiTest, BmpEmoji) {
    EXPECT_TRUE(utf8::isEmoji(0x2764));  // ❤
    EXPECT_TRUE(utf8::isEmoji(0x2B50));  // ⭐
    EXPECT_TRUE(utf8::isEmoji(0x2600));  // ☀
}

TEST_F(Utf8EmojiTest, NonEmojiCodepoints) {
    EXPECT_FALSE(utf8::isEmoji(U'A'));
    EXPECT_FALSE(utf8::isEmoji(U'0'));
    EXPECT_FALSE(utf8::isEmoji(U' '));
    EXPECT_FALSE(utf8::isEmoji(U'\u00E9'));  // é (accented letter)
}

TEST_F(Utf8EmojiTest, ZwjAndVariationSelector) {
    EXPECT_TRUE(utf8::isEmoji(0x200D));  // ZWJ
    EXPECT_TRUE(utf8::isEmoji(0xFE0F));  // Variation selector-16
}

// ============================================================================
// forEach and length tests
// ============================================================================

class Utf8UtilTest : public ::testing::Test {};

TEST_F(Utf8UtilTest, LengthAscii) {
    EXPECT_EQ(utf8::length("Hello"), 5u);
}

TEST_F(Utf8UtilTest, LengthMixed) {
    // "Hi😀" = 2 ASCII + 1 emoji = 3 codepoints
    EXPECT_EQ(utf8::length("Hi\xF0\x9F\x98\x80"), 3u);
}

TEST_F(Utf8UtilTest, LengthEmpty) {
    EXPECT_EQ(utf8::length(""), 0u);
}

TEST_F(Utf8UtilTest, ForEachCollectsAll) {
    std::string s = "A\xE2\x9D\xA4\xF0\x9F\x98\x80";  // A❤😀
    std::vector<char32_t> codepoints;
    utf8::forEach(s, [&](char32_t cp) { codepoints.push_back(cp); });

    ASSERT_EQ(codepoints.size(), 3u);
    EXPECT_EQ(codepoints[0], U'A');
    EXPECT_EQ(codepoints[1], U'\u2764');
    EXPECT_EQ(codepoints[2], U'\U0001F600');
}

}  // namespace test
}  // namespace vde
