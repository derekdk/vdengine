#pragma once

/**
 * @file TrueTypeFont.h
 * @brief TrueType font loading and glyph atlas generation.
 *
 * Loads a .ttf font file via stb_truetype and builds a glyph-atlas Texture
 * for rendering anti-aliased text. Returns false from loadFromFile() when
 * the font file cannot be loaded; callers should check isLoaded() and fall
 * back to BitmapFont::small() when needed.
 */

#include <vde/api/BitmapFont.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace vde {

class Texture;
class VulkanContext;

/**
 * @brief Per-glyph metrics stored after atlas packing.
 */
struct GlyphInfo {
    float uvX0;      ///< Left edge UV coordinate in atlas (0–1)
    float uvY0;      ///< Top edge UV coordinate in atlas (0–1)
    float uvX1;      ///< Right edge UV coordinate in atlas (0–1)
    float uvY1;      ///< Bottom edge UV coordinate in atlas (0–1)
    int width;       ///< Glyph bitmap width in pixels
    int height;      ///< Glyph bitmap height in pixels
    int xOffset;     ///< Horizontal offset from cursor when rendering
    int yOffset;     ///< Vertical offset from baseline when rendering
    float advanceX;  ///< Horizontal advance to next glyph in pixels
};

/**
 * @brief Loads a TrueType font and builds a glyph-atlas Texture.
 *
 * Usage:
 * @code
 * vde::TrueTypeFont font;
 * if (font.loadFromFile(ctx, "assets/fonts/VDE_default.ttf", 32.0f)) {
 *     auto tex = vde::TextRenderer::createTexture(ctx, "Hello", font, style);
 * }
 * @endcode
 *
 * If loading fails, `isLoaded()` returns false and callers should fall back
 * to `BitmapFont::small()`.
 */
class TrueTypeFont {
  public:
    TrueTypeFont() = default;
    ~TrueTypeFont() = default;

    // Non-copyable (owns atlas texture)
    TrueTypeFont(const TrueTypeFont&) = delete;
    TrueTypeFont& operator=(const TrueTypeFont&) = delete;

    // Movable
    TrueTypeFont(TrueTypeFont&& other) noexcept;
    TrueTypeFont& operator=(TrueTypeFont&& other) noexcept;

    /**
     * @brief Load a TrueType font file and build a glyph atlas.
     * @param ctx Vulkan context for GPU upload (may be nullptr for CPU-only)
     * @param path Path to the .ttf font file
     * @param sizePixels Desired font size in pixels
     * @return true if the font was loaded and atlas built successfully
     */
    bool loadFromFile(VulkanContext* ctx, const std::string& path, float sizePixels);

    /**
     * @brief Check whether the font was loaded successfully.
     * @return true if loadFromFile() succeeded
     */
    bool isLoaded() const { return m_loaded; }

    /**
     * @brief Get the glyph atlas texture.
     * @return Shared pointer to the atlas texture (nullptr if not loaded)
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
     * @brief Get glyph info for an ASCII character.
     * @param c Character to look up (ASCII 0x20–0x7E)
     * @return Pointer to GlyphInfo, or nullptr for unsupported characters
     */
    const GlyphInfo* getGlyph(char c) const;

    /**
     * @brief Get the font size in pixels that was used to build the atlas.
     */
    float fontSize() const { return m_fontSize; }

    /**
     * @brief Get the line height (ascent - descent) in pixels.
     */
    float lineHeight() const { return m_lineHeight; }

    /**
     * @brief Convenience fallback font to use when TTF loading fails.
     * @return Reference to BitmapFont::small()
     */
    static const BitmapFont& fallbackFont() { return BitmapFont::small(); }

  private:
    bool m_loaded = false;
    float m_fontSize = 0.0f;
    float m_lineHeight = 0.0f;
    int m_atlasWidth = 0;
    int m_atlasHeight = 0;
    std::shared_ptr<Texture> m_atlas;
    std::vector<GlyphInfo> m_glyphs;  ///< Indexed by (char - 0x20), 95 entries
};

}  // namespace vde
