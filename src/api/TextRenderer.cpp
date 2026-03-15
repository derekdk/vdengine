/**
 * @file TextRenderer.cpp
 * @brief Rasterizes text strings into RGBA textures using BitmapFont data.
 */

#include <vde/Texture.h>
#include <vde/VulkanContext.h>
#include <vde/api/TextRenderer.h>

#include <algorithm>
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

}  // namespace vde
