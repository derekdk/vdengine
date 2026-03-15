/**
 * @file TrueTypeFont.cpp
 * @brief TrueType font loading and glyph atlas generation via stb_truetype.
 */

#include <vde/Texture.h>
#include <vde/VulkanContext.h>
#include <vde/api/TrueTypeFont.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>

#include <stb_truetype.h>

namespace vde {

// ---- Move operations -------------------------------------------------------

TrueTypeFont::TrueTypeFont(TrueTypeFont&& other) noexcept
    : m_loaded(other.m_loaded), m_fontSize(other.m_fontSize), m_lineHeight(other.m_lineHeight),
      m_atlasWidth(other.m_atlasWidth), m_atlasHeight(other.m_atlasHeight),
      m_atlas(std::move(other.m_atlas)), m_glyphs(std::move(other.m_glyphs)) {
    other.m_loaded = false;
}

TrueTypeFont& TrueTypeFont::operator=(TrueTypeFont&& other) noexcept {
    if (this != &other) {
        m_loaded = other.m_loaded;
        m_fontSize = other.m_fontSize;
        m_lineHeight = other.m_lineHeight;
        m_atlasWidth = other.m_atlasWidth;
        m_atlasHeight = other.m_atlasHeight;
        m_atlas = std::move(other.m_atlas);
        m_glyphs = std::move(other.m_glyphs);
        other.m_loaded = false;
    }
    return *this;
}

// ---- Loading ---------------------------------------------------------------

bool TrueTypeFont::loadFromFile(VulkanContext* ctx, const std::string& path, float sizePixels) {
    m_loaded = false;
    m_atlas.reset();
    m_glyphs.clear();
    m_atlasWidth = 0;
    m_atlasHeight = 0;
    m_fontSize = 0.0f;
    m_lineHeight = 0.0f;

    if (sizePixels <= 0.0f) {
        return false;
    }

    // Read the font file into memory
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return false;
    }
    const auto fileSize = file.tellg();
    if (fileSize <= 0) {
        return false;
    }
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> fontData(static_cast<size_t>(fileSize));
    if (!file.read(reinterpret_cast<char*>(fontData.data()), fileSize)) {
        return false;
    }
    file.close();

    // Initialize stb_truetype
    stbtt_fontinfo fontInfo;
    if (!stbtt_InitFont(&fontInfo, fontData.data(),
                        stbtt_GetFontOffsetForIndex(fontData.data(), 0))) {
        return false;
    }

    m_fontSize = sizePixels;
    const float scale = stbtt_ScaleForPixelHeight(&fontInfo, sizePixels);

    // Get font vertical metrics
    int ascent = 0;
    int descent = 0;
    int lineGap = 0;
    stbtt_GetFontVMetrics(&fontInfo, &ascent, &descent, &lineGap);
    m_lineHeight = (ascent - descent) * scale;

    // ASCII printable range: 0x20 (space) through 0x7E (~)
    constexpr int kFirstChar = 0x20;
    constexpr int kCharCount = 95;  // 0x7E - 0x20 + 1

    // Bake glyphs into a single-channel bitmap, then try 512×512 first
    int atlasW = 512;
    int atlasH = 512;

    std::vector<stbtt_bakedchar> charData(kCharCount);
    std::vector<uint8_t> bitmap(static_cast<size_t>(atlasW) * atlasH);

    int bakeResult = stbtt_BakeFontBitmap(fontData.data(), 0, sizePixels, bitmap.data(), atlasW,
                                          atlasH, kFirstChar, kCharCount, charData.data());

    // If bakeResult is negative, not all glyphs fit — try 1024×1024
    if (bakeResult < 0) {
        atlasW = 1024;
        atlasH = 1024;
        bitmap.resize(static_cast<size_t>(atlasW) * atlasH);
        charData.resize(kCharCount);
        bakeResult = stbtt_BakeFontBitmap(fontData.data(), 0, sizePixels, bitmap.data(), atlasW,
                                          atlasH, kFirstChar, kCharCount, charData.data());
        if (bakeResult < 0) {
            std::cerr << "TrueTypeFont: glyphs do not fit in 1024x1024 atlas; "
                         "try a smaller font size (recommended 8-100px)\n";
            return false;
        }
    }

    m_atlasWidth = atlasW;
    m_atlasHeight = atlasH;

    // Convert single-channel bitmap to RGBA
    std::vector<uint8_t> rgba(static_cast<size_t>(atlasW) * atlasH * 4);
    for (int i = 0; i < atlasW * atlasH; ++i) {
        rgba[i * 4 + 0] = 255;        // R
        rgba[i * 4 + 1] = 255;        // G
        rgba[i * 4 + 2] = 255;        // B
        rgba[i * 4 + 3] = bitmap[i];  // A = coverage
    }

    // Build glyph info table
    m_glyphs.resize(kCharCount);
    for (int i = 0; i < kCharCount; ++i) {
        const auto& bc = charData[i];
        auto& g = m_glyphs[i];
        g.uvX0 = static_cast<float>(bc.x0) / atlasW;
        g.uvY0 = static_cast<float>(bc.y0) / atlasH;
        g.uvX1 = static_cast<float>(bc.x1) / atlasW;
        g.uvY1 = static_cast<float>(bc.y1) / atlasH;
        g.width = bc.x1 - bc.x0;
        g.height = bc.y1 - bc.y0;
        g.xOffset = static_cast<int>(std::round(bc.xoff));
        g.yOffset = static_cast<int>(std::round(bc.yoff));
        g.advanceX = bc.xadvance;
    }

    // Upload atlas texture
    m_atlas = std::make_shared<Texture>();
    m_atlas->loadFromData(rgba.data(), static_cast<uint32_t>(atlasW),
                          static_cast<uint32_t>(atlasH));
    if (ctx) {
        m_atlas->uploadToGPU(ctx);
    }

    m_loaded = true;
    return true;
}

// ---- Glyph lookup ----------------------------------------------------------

const GlyphInfo* TrueTypeFont::getGlyph(char c) const {
    if (!m_loaded || m_glyphs.empty()) {
        return nullptr;
    }
    if (c < 0x20 || c > 0x7E) {
        return nullptr;
    }
    return &m_glyphs[static_cast<size_t>(c - 0x20)];
}

}  // namespace vde
