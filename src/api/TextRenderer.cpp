/**
 * @file TextRenderer.cpp
 * @brief Rasterizes text strings into RGBA textures using BitmapFont data.
 */

#include <vde/Texture.h>
#include <vde/VulkanContext.h>
#include <vde/api/TextRenderer.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace vde {

std::shared_ptr<Texture> TextRenderer::createTexture(VulkanContext* ctx, const std::string& text,
                                                     const BitmapFont& font,
                                                     const TextStyle& style) {
    const int scale = std::max(1, style.pixelScale);
    const int spacing = std::max(0, style.letterSpacing);
    const int glyphW = font.glyphWidth();
    const int glyphH = font.glyphHeight();

    // Handle empty string — return a minimal 1x1 transparent texture
    if (text.empty()) {
        auto tex = std::make_shared<Texture>();
        const uint8_t transparent[4] = {0, 0, 0, 0};
        tex->loadFromData(transparent, 1, 1);
        if (ctx) {
            tex->uploadToGPU(ctx);
        }
        return tex;
    }

    const int cellW = (glyphW + spacing) * scale;
    const int texW = static_cast<int>(text.size()) * cellW;
    const int texH = glyphH * scale;

    // Convert Color (0-1 floats) to 8-bit components
    const uint8_t r = static_cast<uint8_t>(std::clamp(style.color.r, 0.0f, 1.0f) * 255.0f);
    const uint8_t g = static_cast<uint8_t>(std::clamp(style.color.g, 0.0f, 1.0f) * 255.0f);
    const uint8_t b = static_cast<uint8_t>(std::clamp(style.color.b, 0.0f, 1.0f) * 255.0f);
    const uint8_t a = static_cast<uint8_t>(std::clamp(style.color.a, 0.0f, 1.0f) * 255.0f);

    std::vector<uint8_t> pixels(static_cast<size_t>(texW) * texH * 4, 0);

    for (int ci = 0; ci < static_cast<int>(text.size()); ++ci) {
        const int x0 = ci * cellW;
        for (int row = 0; row < glyphH; ++row) {
            const uint8_t bits = font.glyphRow(text[ci], row);
            for (int col = 0; col < glyphW; ++col) {
                if (!((bits >> (7 - col)) & 1))
                    continue;
                // Stamp a scale x scale block
                for (int py = 0; py < scale; ++py) {
                    for (int px = 0; px < scale; ++px) {
                        const int tx = x0 + col * scale + px;
                        const int ty = row * scale + py;
                        if (tx < texW && ty < texH) {
                            const size_t idx = (static_cast<size_t>(ty) * texW + tx) * 4;
                            pixels[idx + 0] = r;
                            pixels[idx + 1] = g;
                            pixels[idx + 2] = b;
                            pixels[idx + 3] = a;
                        }
                    }
                }
            }
        }
    }

    auto tex = std::make_shared<Texture>();
    tex->loadFromData(pixels.data(), static_cast<uint32_t>(texW), static_cast<uint32_t>(texH));
    if (ctx) {
        tex->uploadToGPU(ctx);
    }
    return tex;
}

// ---- TrueTypeFont overload -------------------------------------------------

std::shared_ptr<Texture> TextRenderer::createTexture(VulkanContext* ctx, const std::string& text,
                                                     const TrueTypeFont& font,
                                                     const TextStyle& style) {
    const int scale = std::max(1, style.pixelScale);
    const int spacing = std::max(0, style.letterSpacing);

    // Handle empty string
    if (text.empty()) {
        auto tex = std::make_shared<Texture>();
        const uint8_t transparent[4] = {0, 0, 0, 0};
        tex->loadFromData(transparent, 1, 1);
        if (ctx) {
            tex->uploadToGPU(ctx);
        }
        return tex;
    }

    // First pass: compute total width and max height
    float cursorX = 0.0f;
    const float fontSize = font.fontSize();
    int minY = 0;
    int maxY = 0;

    for (size_t i = 0; i < text.size(); ++i) {
        const GlyphInfo* g = font.getGlyph(text[i]);
        if (!g)
            continue;

        int top = g->yOffset;
        int bottom = g->yOffset + g->height;
        minY = std::min(minY, top);
        maxY = std::max(maxY, bottom);

        cursorX += g->advanceX + spacing;
    }

    const int texW = static_cast<int>(std::ceil(cursorX)) * scale;
    const int texH = (maxY - minY) * scale;

    if (texW <= 0 || texH <= 0) {
        auto tex = std::make_shared<Texture>();
        const uint8_t transparent[4] = {0, 0, 0, 0};
        tex->loadFromData(transparent, 1, 1);
        if (ctx) {
            tex->uploadToGPU(ctx);
        }
        return tex;
    }

    // Convert Color to 8-bit
    const uint8_t r = static_cast<uint8_t>(std::clamp(style.color.r, 0.0f, 1.0f) * 255.0f);
    const uint8_t g = static_cast<uint8_t>(std::clamp(style.color.g, 0.0f, 1.0f) * 255.0f);
    const uint8_t b = static_cast<uint8_t>(std::clamp(style.color.b, 0.0f, 1.0f) * 255.0f);

    // Allocate RGBA pixel buffer
    std::vector<uint8_t> pixels(static_cast<size_t>(texW) * texH * 4, 0);

    const int atlasW = font.atlasWidth();
    const int atlasH = font.atlasHeight();
    auto atlasTex = font.getAtlasTexture();

    // We need the atlas pixel data. Since the atlas was loaded via loadFromData,
    // we reconstruct glyph coverage from UV rects and the atlas dimensions.
    // The atlas pixel data is white RGB + alpha coverage. We sample from the
    // atlas texture's CPU-side data.
    const uint8_t* atlasPixels = atlasTex ? atlasTex->getPixelData() : nullptr;

    if (!atlasPixels) {
        // Can't sample atlas — return empty texture
        auto tex = std::make_shared<Texture>();
        const uint8_t transparent[4] = {0, 0, 0, 0};
        tex->loadFromData(transparent, 1, 1);
        if (ctx) {
            tex->uploadToGPU(ctx);
        }
        return tex;
    }

    // Second pass: composite glyphs into the output buffer
    float cx = 0.0f;
    const int baselineOffset = -minY;

    for (size_t i = 0; i < text.size(); ++i) {
        const GlyphInfo* gl = font.getGlyph(text[i]);
        if (!gl) {
            cx += fontSize * 0.5f + spacing;  // Approximate advance for unknown chars
            continue;
        }

        // Source rect in atlas (pixel coords)
        const int srcX0 = static_cast<int>(std::round(gl->uvX0 * atlasW));
        const int srcY0 = static_cast<int>(std::round(gl->uvY0 * atlasH));

        // Destination position
        const int dstX0 = static_cast<int>(std::round(cx)) * scale + gl->xOffset * scale;
        const int dstY0 = (baselineOffset + gl->yOffset) * scale;

        for (int gy = 0; gy < gl->height; ++gy) {
            for (int gx = 0; gx < gl->width; ++gx) {
                // Read alpha from atlas (RGBA, 4 bytes per pixel)
                const int sx = srcX0 + gx;
                const int sy = srcY0 + gy;
                if (sx < 0 || sx >= atlasW || sy < 0 || sy >= atlasH)
                    continue;
                const uint8_t alpha = atlasPixels[(static_cast<size_t>(sy) * atlasW + sx) * 4 + 3];
                if (alpha == 0)
                    continue;

                // Stamp a scale × scale block
                for (int py = 0; py < scale; ++py) {
                    for (int px = 0; px < scale; ++px) {
                        const int tx = dstX0 + gx * scale + px;
                        const int ty = dstY0 + gy * scale + py;
                        if (tx < 0 || tx >= texW || ty < 0 || ty >= texH)
                            continue;
                        const size_t idx = (static_cast<size_t>(ty) * texW + tx) * 4;
                        pixels[idx + 0] = r;
                        pixels[idx + 1] = g;
                        pixels[idx + 2] = b;
                        pixels[idx + 3] = alpha;
                    }
                }
            }
        }

        cx += gl->advanceX + spacing;
    }

    auto tex = std::make_shared<Texture>();
    tex->loadFromData(pixels.data(), static_cast<uint32_t>(texW), static_cast<uint32_t>(texH));
    if (ctx) {
        tex->uploadToGPU(ctx);
    }
    return tex;
}

}  // namespace vde
