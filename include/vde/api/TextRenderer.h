#pragma once

/**
 * @file TextRenderer.h
 * @brief Utility for rendering text strings into Texture objects.
 *
 * Uses a BitmapFont or TrueTypeFont to rasterize a string into an RGBA
 * texture that can be applied to a SpriteEntity.
 */

#include <vde/api/BitmapFont.h>
#include <vde/api/GameTypes.h>
#include <vde/api/TrueTypeFont.h>

#include <memory>
#include <string>

namespace vde {

class EmojiFont;
class Texture;
class VulkanContext;

/**
 * @brief Style parameters for text rendering.
 */
struct TextStyle {
    Color color;            ///< Text foreground color (default: white)
    int pixelScale = 1;     ///< Scale factor applied to each font pixel (>= 1)
    int letterSpacing = 1;  ///< Extra columns between glyphs in font pixels

    bool operator==(const TextStyle&) const = default;
};

/**
 * @brief Static utility that rasterizes text strings into Texture objects.
 *
 * @example
 * @code
 * auto tex = vde::TextRenderer::createTexture(
 *     ctx, "HELLO", vde::BitmapFont::small(),
 *     { .color = vde::Color::yellow(), .pixelScale = 3 });
 * sprite->setTexture(tex);
 * @endcode
 */
class TextRenderer {
  public:
    /**
     * @brief Render a text string into an RGBA texture.
     * @param ctx Vulkan context used to upload the texture to the GPU
     * @param text The string to render (ASCII printable characters)
     * @param font Bitmap font to use for glyph lookup
     * @param style Rendering style (color, scale, spacing)
     * @return Shared pointer to the created texture (always non-null; empty
     *         strings return a 1x1 transparent texture)
     */
    static std::shared_ptr<Texture> createTexture(VulkanContext* ctx, const std::string& text,
                                                  const BitmapFont& font,
                                                  const TextStyle& style = {});

    /**
     * @brief Render a text string into an RGBA texture using a TrueType font.
     * @param ctx Vulkan context used to upload the texture to the GPU
     * @param text The string to render (ASCII printable characters)
     * @param font TrueType font with a pre-built glyph atlas
     * @param style Rendering style (color, scale, spacing)
     * @return Shared pointer to the created texture (always non-null; empty
     *         strings return a 1x1 transparent texture)
     */
    static std::shared_ptr<Texture> createTexture(VulkanContext* ctx, const std::string& text,
                                                  const TrueTypeFont& font,
                                                  const TextStyle& style = {});

    /**
     * @brief Render a UTF-8 text string with inline color emoji support.
     * @param ctx Vulkan context used to upload the texture to the GPU
     * @param utf8Text The string to render (UTF-8 encoded, may contain emoji)
     * @param font TrueType font for regular text glyphs
     * @param emoji Color emoji font for emoji codepoints (may be nullptr)
     * @param style Rendering style (color affects text only; emoji keep original colors)
     * @return Shared pointer to the created texture (always non-null; empty
     *         strings return a 1x1 transparent texture)
     */
    static std::shared_ptr<Texture> createTexture(VulkanContext* ctx, const std::string& utf8Text,
                                                  const TrueTypeFont& font, const EmojiFont* emoji,
                                                  const TextStyle& style = {});
};

}  // namespace vde
