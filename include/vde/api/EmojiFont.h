#pragma once

/**
 * @file EmojiFont.h
 * @brief Color emoji font loader supporting COLR/CPAL vector-layered emoji.
 *
 * Loads a color emoji font file (e.g., Segoe UI Emoji on Windows, Noto Color
 * Emoji on Linux), parses the COLR v0 and CPAL tables, and renders multi-layer
 * color emoji glyphs into a texture atlas via stb_truetype.
 *
 * Usage:
 * @code
 * vde::EmojiFont emoji;
 * if (emoji.loadFromFile(ctx, vde::EmojiFont::findSystemEmojiFont(), 32)) {
 *     // Use with TextRenderer for color emoji in text
 *     auto tex = vde::TextRenderer::createTexture(ctx, u8"Hello 😀", font, &emoji, style);
 * }
 * @endcode
 */

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace vde {

class Texture;
class VulkanContext;

/**
 * @brief Per-emoji glyph metrics and atlas location.
 */
struct EmojiGlyph {
    int width;       ///< Rendered width in pixels
    int height;      ///< Rendered height in pixels
    int bearingX;    ///< Horizontal bearing (offset from cursor)
    int bearingY;    ///< Vertical bearing (offset from baseline)
    float advanceX;  ///< Horizontal advance to next character
    int atlasX;      ///< X position in the atlas texture
    int atlasY;      ///< Y position in the atlas texture
};

/**
 * @brief Loads a color emoji font and builds a color glyph atlas.
 *
 * Supports COLR v0 + CPAL (vector-layered) emoji fonts. Each layer glyph
 * is rendered with stb_truetype and composited with palette colors using
 * premultiplied alpha blending.
 *
 * The atlas is a simple grid of fixed-size cells. All emoji are rendered
 * at the requested pixel size and packed into the atlas texture.
 */
class EmojiFont {
  public:
    EmojiFont() = default;
    ~EmojiFont() = default;

    // Non-copyable (owns atlas texture)
    EmojiFont(const EmojiFont&) = delete;
    EmojiFont& operator=(const EmojiFont&) = delete;

    // Movable
    EmojiFont(EmojiFont&& other) noexcept;
    EmojiFont& operator=(EmojiFont&& other) noexcept;

    /**
     * @brief Load a color emoji font and build the glyph atlas.
     * @param ctx Vulkan context for GPU upload (may be nullptr for CPU-only)
     * @param path Path to the emoji font file (.ttf/.otf)
     * @param sizePixels Desired emoji render size in pixels
     * @return true if the font was loaded and at least one color emoji was found
     * @note On failure, getLastError() contains a human-readable reason.
     */
    bool loadFromFile(VulkanContext* ctx, const std::string& path, int sizePixels);

    /**
     * @brief Check whether the font was loaded successfully.
     */
    bool isLoaded() const { return m_loaded; }

    /**
     * @brief Get the last load failure message.
     */
    const std::string& getLastError() const { return m_lastError; }

    /**
     * @brief Look up a color emoji glyph by Unicode codepoint.
     * @param codepoint Unicode codepoint (e.g., 0x1F600 for 😀)
     * @return Pointer to EmojiGlyph, or nullptr if the codepoint is not available
     */
    const EmojiGlyph* getGlyph(char32_t codepoint) const;

    /**
     * @brief Check whether a color emoji is available for the given codepoint.
     */
    bool hasGlyph(char32_t codepoint) const;

    /**
     * @brief Get the atlas texture containing all rendered emoji.
     */
    std::shared_ptr<Texture> getAtlasTexture() const { return m_atlas; }

    /**
     * @brief Get the atlas width in pixels.
     */
    int atlasWidth() const { return m_atlasWidth; }

    /**
     * @brief Get the atlas height in pixels.
     */
    int atlasHeight() const { return m_atlasHeight; }

    /**
     * @brief Get the emoji cell size in pixels (width and height are equal).
     */
    int emojiSize() const { return m_emojiSize; }

    /**
     * @brief Copy a single emoji's RGBA pixels into the provided buffer.
     * @param codepoint Unicode codepoint to copy
     * @param out Output buffer (must be at least width * height * 4 bytes)
     * @return true if the glyph was found and pixels were copied
     */
    bool copyGlyphPixels(char32_t codepoint, uint8_t* out) const;

    /**
     * @brief Get the list of all available emoji codepoints.
     */
    const std::vector<char32_t>& getAvailableCodepoints() const { return m_availableCodepoints; }

    /**
     * @brief Find the system's default color emoji font.
     * @return Absolute path to the system emoji font, or empty string if not found
     *
     * Platform search order:
     * - Windows: C:\\Windows\\Fonts\\seguiemj.ttf (Segoe UI Emoji)
     * - macOS:   /System/Library/Fonts/Apple Color Emoji.ttc
     * - Linux:   /usr/share/fonts/truetype/noto/NotoColorEmoji.ttf
     */
    static std::string findSystemEmojiFont();

  private:
    bool m_loaded = false;
    int m_emojiSize = 0;
    int m_atlasWidth = 0;
    int m_atlasHeight = 0;
    std::shared_ptr<Texture> m_atlas;
    std::unordered_map<char32_t, EmojiGlyph> m_glyphMap;
    std::vector<char32_t> m_availableCodepoints;
    std::string m_lastError;
};

}  // namespace vde
