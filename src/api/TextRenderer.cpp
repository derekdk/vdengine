/**
 * @file TextRenderer.cpp
 * @brief Rasterizes text strings into RGBA textures using BitmapFont or TrueTypeFont data.
 */

#include <vde/Texture.h>
#include <vde/VulkanContext.h>
#include <vde/api/TextRenderer.h>
#include <vde/api/Utf8.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
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
    const float styleAlpha = std::clamp(style.color.a, 0.0f, 1.0f);

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
        std::cerr << "TextRenderer: TrueType atlas CPU pixel data is unavailable; "
                     "returning an empty texture\n";
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
                const uint8_t atlasAlpha =
                    atlasPixels[(static_cast<size_t>(sy) * atlasW + sx) * 4 + 3];
                if (atlasAlpha == 0)
                    continue;
                const uint8_t alpha =
                    static_cast<uint8_t>(static_cast<float>(atlasAlpha) * styleAlpha);

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

// ---- Emoji-aware UTF-8 overload --------------------------------------------

std::shared_ptr<Texture> TextRenderer::createTexture(VulkanContext* ctx,
                                                     const std::string& utf8Text,
                                                     const TrueTypeFont& font,
                                                     const EmojiFont* emoji,
                                                     const TextStyle& style) {
    // If no emoji font, delegate to the existing overload
    if (!emoji || !emoji->isLoaded()) {
        return createTexture(ctx, utf8Text, font, style);
    }

    const int scale = std::max(1, style.pixelScale);
    const int spacing = std::max(0, style.letterSpacing);

    // Handle empty string
    if (utf8Text.empty()) {
        auto tex = std::make_shared<Texture>();
        const uint8_t transparent[4] = {0, 0, 0, 0};
        tex->loadFromData(transparent, 1, 1);
        if (ctx) {
            tex->uploadToGPU(ctx);
        }
        return tex;
    }

    // Decode the UTF-8 string into codepoints
    std::vector<char32_t> codepoints;
    {
        size_t pos = 0;
        while (pos < utf8Text.size()) {
            codepoints.push_back(utf8::decode(utf8Text, pos));
        }
    }

    if (codepoints.empty()) {
        auto tex = std::make_shared<Texture>();
        const uint8_t transparent[4] = {0, 0, 0, 0};
        tex->loadFromData(transparent, 1, 1);
        if (ctx) {
            tex->uploadToGPU(ctx);
        }
        return tex;
    }

    const float fontSize = font.fontSize();
    const int emojiSz = emoji->emojiSize();

    // First pass: compute total width and text height bounds
    float cursorX = 0.0f;
    int minY = 0;
    int maxY = 0;

    for (char32_t cp : codepoints) {
        // Check emoji first
        const EmojiGlyph* eg = emoji->getGlyph(cp);
        if (eg) {
            // Emoji height contribution: treat as centered on the text line
            int emojiTop = -emojiSz / 2;
            int emojiBottom = emojiSz - emojiSz / 2;
            minY = std::min(minY, emojiTop);
            maxY = std::max(maxY, emojiBottom);
            cursorX += eg->advanceX + spacing;
            continue;
        }

        // Regular glyph (ASCII range)
        if (cp >= 0x20 && cp <= 0x7E) {
            const GlyphInfo* g = font.getGlyph(static_cast<char>(cp));
            if (g) {
                minY = std::min(minY, g->yOffset);
                maxY = std::max(maxY, g->yOffset + g->height);
                cursorX += g->advanceX + spacing;
            } else {
                cursorX += fontSize * 0.5f + spacing;
            }
        } else {
            cursorX += fontSize * 0.5f + spacing;
        }
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

    // Style color as 8-bit
    const uint8_t r = static_cast<uint8_t>(std::clamp(style.color.r, 0.0f, 1.0f) * 255.0f);
    const uint8_t g = static_cast<uint8_t>(std::clamp(style.color.g, 0.0f, 1.0f) * 255.0f);
    const uint8_t b = static_cast<uint8_t>(std::clamp(style.color.b, 0.0f, 1.0f) * 255.0f);
    const float styleAlpha = std::clamp(style.color.a, 0.0f, 1.0f);

    std::vector<uint8_t> pixels(static_cast<size_t>(texW) * texH * 4, 0);

    const int ttfAtlasW = font.atlasWidth();
    const int ttfAtlasH = font.atlasHeight();
    auto ttfAtlasTex = font.getAtlasTexture();
    const uint8_t* ttfAtlasPixels = ttfAtlasTex ? ttfAtlasTex->getPixelData() : nullptr;

    const uint8_t* emojiAtlasPixels =
        emoji->getAtlasTexture() ? emoji->getAtlasTexture()->getPixelData() : nullptr;
    const int emojiAtlasW = emoji->atlasWidth();

    const int baselineOffset = -minY;

    // Second pass: composite glyphs
    float cx = 0.0f;

    for (char32_t cp : codepoints) {
        // Check emoji first
        const EmojiGlyph* eg = emoji->getGlyph(cp);
        if (eg && emojiAtlasPixels) {
            // Place emoji centered vertically on the text line
            int dstX0 = static_cast<int>(std::round(cx)) * scale;
            int dstY0 = (baselineOffset - emojiSz / 2) * scale;

            // Copy emoji pixels from the emoji atlas (full color, no style tinting)
            for (int ey = 0; ey < eg->height; ++ey) {
                for (int ex = 0; ex < eg->width; ++ex) {
                    const size_t srcIdx =
                        (static_cast<size_t>(eg->atlasY + ey) * emojiAtlasW + eg->atlasX + ex) * 4;
                    uint8_t eR = emojiAtlasPixels[srcIdx + 0];
                    uint8_t eG = emojiAtlasPixels[srcIdx + 1];
                    uint8_t eB = emojiAtlasPixels[srcIdx + 2];
                    uint8_t eA = emojiAtlasPixels[srcIdx + 3];
                    if (eA == 0)
                        continue;

                    // Stamp scaled block
                    for (int py = 0; py < scale; ++py) {
                        for (int px = 0; px < scale; ++px) {
                            int tx = dstX0 + ex * scale + px;
                            int ty = dstY0 + ey * scale + py;
                            if (tx < 0 || tx >= texW || ty < 0 || ty >= texH)
                                continue;
                            size_t idx = (static_cast<size_t>(ty) * texW + tx) * 4;
                            pixels[idx + 0] = eR;
                            pixels[idx + 1] = eG;
                            pixels[idx + 2] = eB;
                            pixels[idx + 3] = eA;
                        }
                    }
                }
            }

            cx += eg->advanceX + spacing;
            continue;
        }

        // Regular glyph (ASCII range, TrueType atlas)
        if (cp >= 0x20 && cp <= 0x7E && ttfAtlasPixels) {
            const GlyphInfo* gl = font.getGlyph(static_cast<char>(cp));
            if (!gl) {
                cx += fontSize * 0.5f + spacing;
                continue;
            }

            const int srcX0 = static_cast<int>(std::round(gl->uvX0 * ttfAtlasW));
            const int srcY0 = static_cast<int>(std::round(gl->uvY0 * ttfAtlasH));
            const int dstX0 = static_cast<int>(std::round(cx)) * scale + gl->xOffset * scale;
            const int dstY0 = (baselineOffset + gl->yOffset) * scale;

            for (int gy = 0; gy < gl->height; ++gy) {
                for (int gx = 0; gx < gl->width; ++gx) {
                    const int sx = srcX0 + gx;
                    const int sy = srcY0 + gy;
                    if (sx < 0 || sx >= ttfAtlasW || sy < 0 || sy >= ttfAtlasH)
                        continue;
                    const uint8_t atlasAlpha =
                        ttfAtlasPixels[(static_cast<size_t>(sy) * ttfAtlasW + sx) * 4 + 3];
                    if (atlasAlpha == 0)
                        continue;
                    const uint8_t alpha =
                        static_cast<uint8_t>(static_cast<float>(atlasAlpha) * styleAlpha);

                    for (int py = 0; py < scale; ++py) {
                        for (int px = 0; px < scale; ++px) {
                            int tx = dstX0 + gx * scale + px;
                            int ty = dstY0 + gy * scale + py;
                            if (tx < 0 || tx >= texW || ty < 0 || ty >= texH)
                                continue;
                            size_t idx = (static_cast<size_t>(ty) * texW + tx) * 4;
                            pixels[idx + 0] = r;
                            pixels[idx + 1] = g;
                            pixels[idx + 2] = b;
                            pixels[idx + 3] = alpha;
                        }
                    }
                }
            }

            cx += gl->advanceX + spacing;
        } else {
            cx += fontSize * 0.5f + spacing;
        }
    }

    auto tex = std::make_shared<Texture>();
    tex->loadFromData(pixels.data(), static_cast<uint32_t>(texW), static_cast<uint32_t>(texH));
    if (ctx) {
        tex->uploadToGPU(ctx);
    }
    return tex;
}

}  // namespace vde
