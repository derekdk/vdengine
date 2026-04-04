/**
 * @file EmojiFont.cpp
 * @brief Color emoji font loader and atlas builder using COLR/CPAL tables.
 *
 * Implements full COLR v0 parsing plus a best-effort subset of COLR v1.
 */

#include <vde/Texture.h>
#include <vde/VulkanContext.h>
#include <vde/api/EmojiFont.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

#include <stb_truetype.h>

namespace vde {

// ============================================================================
// Internal helpers
// ============================================================================

namespace {

// ---- Big-endian reading ----------------------------------------------------

inline uint16_t readU16BE(const uint8_t* p) {
    return (static_cast<uint16_t>(p[0]) << 8) | static_cast<uint16_t>(p[1]);
}

inline uint32_t readU32BE(const uint8_t* p) {
    return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) |
           (static_cast<uint32_t>(p[2]) << 8) | static_cast<uint32_t>(p[3]);
}

inline uint32_t readU24BE(const uint8_t* p) {
    return (static_cast<uint32_t>(p[0]) << 16) | (static_cast<uint32_t>(p[1]) << 8) |
           static_cast<uint32_t>(p[2]);
}

// ---- OpenType table lookup -------------------------------------------------

constexpr uint32_t kTagCOLR = 0x434F4C52;  // 'COLR'
constexpr uint32_t kTagCPAL = 0x4350414C;  // 'CPAL'

struct TableEntry {
    uint32_t offset = 0;
    uint32_t length = 0;
};

/// Find a table by its 4-byte tag in the OpenType table directory.
TableEntry findTable(const uint8_t* data, size_t dataSize, uint32_t tag) {
    if (dataSize < 12)
        return {};
    uint16_t numTables = readU16BE(data + 4);
    if (dataSize < static_cast<size_t>(12) + numTables * 16)
        return {};

    for (uint16_t i = 0; i < numTables; ++i) {
        const uint8_t* entry = data + 12 + i * 16;
        if (readU32BE(entry) == tag) {
            uint32_t offset = readU32BE(entry + 8);
            uint32_t length = readU32BE(entry + 12);
            if (static_cast<size_t>(offset) + length <= dataSize) {
                return {offset, length};
            }
        }
    }
    return {};
}

// ---- COLR v0 structures ---------------------------------------------------

struct ColrBaseGlyph {
    uint16_t glyphID;
    uint16_t firstLayerIndex;
    uint16_t numLayers;
};

struct ColrLayerRecord {
    uint16_t glyphID;
    uint16_t paletteIndex;
};

struct PaletteColor {
    uint8_t r, g, b, a;
};

// ---- COLR v1 paint tree support --------------------------------------------

constexpr uint8_t kPaintColrLayers = 1;
constexpr uint8_t kPaintSolid = 2;
constexpr uint8_t kPaintVarSolid = 3;
constexpr uint8_t kPaintGlyph = 10;
constexpr uint8_t kPaintColrGlyph = 11;
constexpr uint8_t kPaintComposite = 32;
constexpr int kMaxPaintDepth = 64;

/// Context for walking COLR v1 paint trees.
struct ColrV1Ctx {
    const uint8_t* data;
    uint32_t length;
    uint32_t layerListOff;
    uint32_t layerListNum;
    uint32_t baseGlyphListOff;
    uint32_t baseGlyphListNum;
};

/// Try to resolve a paint at @p off to a CPAL palette index.
/// Returns 0xFFFF (foreground-color fallback) for unsupported paint types.
uint16_t resolvePaintColor(const ColrV1Ctx& ctx, uint32_t off, int depth) {
    if (depth > kMaxPaintDepth || off + 1 > ctx.length)
        return 0xFFFF;

    uint8_t fmt = ctx.data[off];
    switch (fmt) {
    case kPaintSolid:
    case kPaintVarSolid:
        if (off + 3 > ctx.length)
            return 0xFFFF;
        return readU16BE(ctx.data + off + 1);
    default:
        // Transform formats (12-31) wrap a child paint at Offset24 +1
        if (fmt >= 12 && fmt <= 31) {
            if (off + 4 > ctx.length)
                return 0xFFFF;
            return resolvePaintColor(ctx, off + readU24BE(ctx.data + off + 1), depth + 1);
        }
        return 0xFFFF;
    }
}

/// Flatten a v1 paint tree at @p off into ColrLayerRecord entries.
bool flattenPaint(const ColrV1Ctx& ctx, uint32_t off, std::vector<ColrLayerRecord>& out,
                  int depth) {
    if (depth > kMaxPaintDepth || off + 1 > ctx.length)
        return false;

    const size_t prevSize = out.size();
    uint8_t fmt = ctx.data[off];

    switch (fmt) {
    case kPaintColrLayers: {
        if (off + 6 > ctx.length)
            return false;
        uint8_t numLayers = ctx.data[off + 1];
        uint32_t firstIdx = readU32BE(ctx.data + off + 2);
        if (ctx.layerListOff == 0)
            return false;
        for (uint8_t i = 0; i < numLayers; ++i) {
            uint32_t idx = firstIdx + i;
            if (idx >= ctx.layerListNum)
                break;
            // Use uint64_t to prevent overflow on malformed data
            uint64_t entryAddr = static_cast<uint64_t>(ctx.layerListOff) + 4 + idx * 4;
            if (entryAddr + 4 > ctx.length)
                break;
            uint32_t paintOff = readU32BE(ctx.data + entryAddr);
            flattenPaint(ctx, ctx.layerListOff + paintOff, out, depth + 1);
        }
        return out.size() > prevSize;
    }

    case kPaintGlyph: {
        if (off + 6 > ctx.length)
            return false;
        uint32_t childOff = readU24BE(ctx.data + off + 1);
        uint16_t glyphID = readU16BE(ctx.data + off + 4);
        uint16_t paletteIdx = resolvePaintColor(ctx, off + childOff, depth + 1);
        out.push_back({glyphID, paletteIdx});
        return true;
    }

    case kPaintColrGlyph: {
        if (off + 3 > ctx.length)
            return false;
        uint16_t glyphID = readU16BE(ctx.data + off + 1);
        if (ctx.baseGlyphListOff == 0 || ctx.baseGlyphListOff + 4 > ctx.length)
            return false;
        // Binary search in sorted BaseGlyphList
        uint32_t lo = 0, hi = ctx.baseGlyphListNum;
        while (lo < hi) {
            uint32_t mid = lo + (hi - lo) / 2;
            uint32_t recOff = ctx.baseGlyphListOff + 4 + mid * 6;
            if (recOff + 6 > ctx.length)
                return false;
            uint16_t g = readU16BE(ctx.data + recOff);
            if (g < glyphID)
                lo = mid + 1;
            else if (g > glyphID)
                hi = mid;
            else {
                uint32_t paintOff = readU32BE(ctx.data + recOff + 2);
                return flattenPaint(ctx, ctx.baseGlyphListOff + paintOff, out, depth + 1);
            }
        }
        return false;
    }

    case kPaintComposite: {
        if (off + 8 > ctx.length)
            return false;
        uint32_t sourceOff = readU24BE(ctx.data + off + 1);
        // Skip compositeMode byte at off+4; backdrop offset is at off+5
        uint32_t backdropOff = readU24BE(ctx.data + off + 5);
        flattenPaint(ctx, off + backdropOff, out, depth + 1);
        flattenPaint(ctx, off + sourceOff, out, depth + 1);
        return out.size() > prevSize;
    }

    default: {
        // Transform formats (12-31) wrap a child paint at Offset24 +1
        if (fmt >= 12 && fmt <= 31) {
            if (off + 4 > ctx.length)
                return false;
            return flattenPaint(ctx, off + readU24BE(ctx.data + off + 1), out, depth + 1);
        }
        return false;
    }
    }
}

/// Parse COLR v1 BaseGlyphList and add entries to existing v0 arrays.
void parseCOLRv1(const ColrV1Ctx& ctx, std::vector<ColrBaseGlyph>& baseGlyphs,
                 std::vector<ColrLayerRecord>& layers) {
    if (ctx.baseGlyphListOff == 0 || ctx.baseGlyphListOff + 4 > ctx.length)
        return;
    if (ctx.baseGlyphListOff + 4 + static_cast<uint64_t>(ctx.baseGlyphListNum) * 6 > ctx.length)
        return;

    // Snapshot v0 glyph IDs for duplicate detection (already sorted by spec)
    std::vector<uint16_t> v0IDs;
    v0IDs.reserve(baseGlyphs.size());
    for (const auto& bg : baseGlyphs)
        v0IDs.push_back(bg.glyphID);

    bool anyAdded = false;
    for (uint32_t i = 0; i < ctx.baseGlyphListNum; ++i) {
        uint32_t recOff = ctx.baseGlyphListOff + 4 + i * 6;
        uint16_t glyphID = readU16BE(ctx.data + recOff);
        uint32_t paintOff = readU32BE(ctx.data + recOff + 2);

        if (std::binary_search(v0IDs.begin(), v0IDs.end(), glyphID))
            continue;
        if (layers.size() > 65000)
            break;

        std::vector<ColrLayerRecord> glyphLayers;
        if (!flattenPaint(ctx, ctx.baseGlyphListOff + paintOff, glyphLayers, 0))
            continue;
        if (glyphLayers.empty() || layers.size() + glyphLayers.size() > 65535)
            continue;

        auto firstIdx = static_cast<uint16_t>(layers.size());
        auto numLayers = static_cast<uint16_t>(glyphLayers.size());
        layers.insert(layers.end(), glyphLayers.begin(), glyphLayers.end());
        baseGlyphs.push_back({glyphID, firstIdx, numLayers});
        anyAdded = true;
    }

    if (anyAdded) {
        std::sort(
            baseGlyphs.begin(), baseGlyphs.end(),
            [](const ColrBaseGlyph& a, const ColrBaseGlyph& b) { return a.glyphID < b.glyphID; });
    }
}

// ---- COLR/CPAL parsing -----------------------------------------------------

/// Parse COLR table (v0 and v1).
bool parseCOLR(const uint8_t* data, uint32_t length, std::vector<ColrBaseGlyph>& baseGlyphs,
               std::vector<ColrLayerRecord>& layers) {
    if (length < 14)
        return false;
    uint16_t version = readU16BE(data);
    if (version > 1)
        return false;

    // ---- v0 fields (present in both v0 and v1 headers) ----
    uint16_t numBaseGlyphs = readU16BE(data + 2);
    uint32_t bgOffset = readU32BE(data + 4);
    uint32_t layerOffset = readU32BE(data + 8);
    uint16_t numLayers = readU16BE(data + 12);

    if (numBaseGlyphs > 0) {
        if (bgOffset + static_cast<uint32_t>(numBaseGlyphs) * 6 > length)
            return false;
        if (layerOffset + static_cast<uint32_t>(numLayers) * 4 > length)
            return false;

        baseGlyphs.resize(numBaseGlyphs);
        for (uint16_t i = 0; i < numBaseGlyphs; ++i) {
            const uint8_t* p = data + bgOffset + i * 6;
            baseGlyphs[i].glyphID = readU16BE(p);
            baseGlyphs[i].firstLayerIndex = readU16BE(p + 2);
            baseGlyphs[i].numLayers = readU16BE(p + 4);
        }

        layers.resize(numLayers);
        for (uint16_t i = 0; i < numLayers; ++i) {
            const uint8_t* p = data + layerOffset + i * 4;
            layers[i].glyphID = readU16BE(p);
            layers[i].paletteIndex = readU16BE(p + 2);
        }
    }

    // ---- v1 extensions ----
    if (version == 1) {
        if (length < 22)
            return false;

        uint32_t baseGlyphListOff = readU32BE(data + 14);
        uint32_t layerListOff = readU32BE(data + 18);

        if (baseGlyphListOff != 0 && baseGlyphListOff + 4 <= length) {
            uint32_t layerListNum = 0;
            if (layerListOff != 0 && layerListOff + 4 <= length)
                layerListNum = readU32BE(data + layerListOff);

            ColrV1Ctx ctx{data,         length,           layerListOff,
                          layerListNum, baseGlyphListOff, readU32BE(data + baseGlyphListOff)};
            parseCOLRv1(ctx, baseGlyphs, layers);
        }
    }

    return !baseGlyphs.empty();
}

/// Parse CPAL table (color palette).
bool parseCPAL(const uint8_t* data, uint32_t length, std::vector<PaletteColor>& colors) {
    if (length < 14)
        return false;
    // uint16_t version = readU16BE(data);
    // uint16_t numEntries = readU16BE(data + 2);
    // uint16_t numPalettes = readU16BE(data + 4);
    uint16_t numColorRecords = readU16BE(data + 6);
    uint32_t colorOffset = readU32BE(data + 8);

    if (colorOffset + static_cast<uint32_t>(numColorRecords) * 4 > length)
        return false;

    colors.resize(numColorRecords);
    const uint8_t* records = data + colorOffset;
    for (uint16_t i = 0; i < numColorRecords; ++i) {
        // CPAL stores BGRA order
        colors[i].b = records[i * 4 + 0];
        colors[i].g = records[i * 4 + 1];
        colors[i].r = records[i * 4 + 2];
        colors[i].a = records[i * 4 + 3];
    }

    return true;
}

/// Binary search for a base glyph in the sorted COLR base glyph array.
const ColrBaseGlyph* findBaseGlyph(const std::vector<ColrBaseGlyph>& baseGlyphs, uint16_t glyphID) {
    auto it =
        std::lower_bound(baseGlyphs.begin(), baseGlyphs.end(), glyphID,
                         [](const ColrBaseGlyph& bg, uint16_t id) { return bg.glyphID < id; });
    if (it != baseGlyphs.end() && it->glyphID == glyphID) {
        return &(*it);
    }
    return nullptr;
}

// ---- Emoji codepoint ranges to scan ----------------------------------------

struct CodepointRange {
    char32_t first;
    char32_t last;
};

// Comprehensive emoji codepoint ranges
constexpr CodepointRange kEmojiRanges[] = {
    {0x203C, 0x203C},    // ‼
    {0x2049, 0x2049},    // ⁉
    {0x2122, 0x2122},    // ™
    {0x2139, 0x2139},    // ℹ
    {0x2194, 0x2199},    // ↔-↙
    {0x21A9, 0x21AA},    // ↩↪
    {0x231A, 0x231B},    // ⌚⌛
    {0x2328, 0x2328},    // ⌨
    {0x23CF, 0x23CF},    // ⏏
    {0x23E9, 0x23F3},    // ⏩-⏳
    {0x23F8, 0x23FA},    // ⏸-⏺
    {0x24C2, 0x24C2},    // Ⓜ
    {0x25AA, 0x25AB},    // ▪▫
    {0x25B6, 0x25B6},    // ▶
    {0x25C0, 0x25C0},    // ◀
    {0x25FB, 0x25FE},    // ◻-◾
    {0x2600, 0x27BF},    // Misc Symbols + Dingbats
    {0x2934, 0x2935},    // ⤴⤵
    {0x2B05, 0x2B07},    // ⬅-⬇
    {0x2B1B, 0x2B1C},    // ⬛⬜
    {0x2B50, 0x2B50},    // ⭐
    {0x2B55, 0x2B55},    // ⭕
    {0x3030, 0x3030},    // 〰
    {0x303D, 0x303D},    // 〽
    {0x3297, 0x3297},    // ㊗
    {0x3299, 0x3299},    // ㊙
    {0x1F000, 0x1F0FF},  // Mahjong + Domino tiles
    {0x1F1E0, 0x1F1FF},  // Regional indicators
    {0x1F200, 0x1F2FF},  // Enclosed ideographic supplement
    {0x1F300, 0x1F5FF},  // Misc Symbols and Pictographs
    {0x1F600, 0x1F64F},  // Emoticons
    {0x1F680, 0x1F6FF},  // Transport and Map
    {0x1F700, 0x1F77F},  // Alchemical Symbols
    {0x1F780, 0x1F7FF},  // Geometric Shapes Extended
    {0x1F800, 0x1F8FF},  // Supplemental Arrows-C
    {0x1F900, 0x1F9FF},  // Supplemental Symbols
    {0x1FA00, 0x1FA6F},  // Chess Symbols + Extended-A
    {0x1FA70, 0x1FAFF},  // Symbols Extended-B
};

// ---- Color glyph rendering -------------------------------------------------

/// Render a single color emoji glyph by compositing COLR layers.
bool renderColorGlyph(const stbtt_fontinfo& info, float scale,
                      const std::vector<ColrBaseGlyph>& baseGlyphs,
                      const std::vector<ColrLayerRecord>& layerRecords,
                      const std::vector<PaletteColor>& palette, uint16_t glyphID, int cellSize,
                      uint8_t* output) {
    const ColrBaseGlyph* bg = findBaseGlyph(baseGlyphs, glyphID);
    if (!bg || bg->numLayers == 0)
        return false;

    // Initialize output to fully transparent
    std::memset(output, 0, static_cast<size_t>(cellSize) * cellSize * 4);

    // Find composite bounding box across all layers
    int compX0 = 0, compY0 = 0, compX1 = 0, compY1 = 0;
    bool firstBox = true;

    for (uint16_t li = 0; li < bg->numLayers; ++li) {
        uint16_t layerIdx = bg->firstLayerIndex + li;
        if (layerIdx >= layerRecords.size())
            continue;

        int x0, y0, x1, y1;
        stbtt_GetGlyphBitmapBox(&info, layerRecords[layerIdx].glyphID, scale, scale, &x0, &y0, &x1,
                                &y1);
        if (x1 <= x0 || y1 <= y0)
            continue;

        if (firstBox) {
            compX0 = x0;
            compY0 = y0;
            compX1 = x1;
            compY1 = y1;
            firstBox = false;
        } else {
            compX0 = std::min(compX0, x0);
            compY0 = std::min(compY0, y0);
            compX1 = std::max(compX1, x1);
            compY1 = std::max(compY1, y1);
        }
    }

    if (firstBox)
        return false;  // no renderable layers

    int compW = compX1 - compX0;
    int compH = compY1 - compY0;

    // Center the composite in the output cell
    int offsetX = (cellSize - compW) / 2;
    int offsetY = (cellSize - compH) / 2;

    // Premultiplied alpha accumulator (float for precision)
    std::vector<float> accum(static_cast<size_t>(cellSize) * cellSize * 4, 0.0f);

    // Render each layer bottom-to-top (COLR v0 order: first layer is bottom)
    for (uint16_t li = 0; li < bg->numLayers; ++li) {
        uint16_t layerIdx = bg->firstLayerIndex + li;
        if (layerIdx >= layerRecords.size())
            continue;

        const auto& layer = layerRecords[layerIdx];

        // Get layer glyph bounding box
        int lx0, ly0, lx1, ly1;
        stbtt_GetGlyphBitmapBox(&info, layer.glyphID, scale, scale, &lx0, &ly0, &lx1, &ly1);
        int lw = lx1 - lx0;
        int lh = ly1 - ly0;
        if (lw <= 0 || lh <= 0)
            continue;

        // Render the layer glyph to a monochrome coverage bitmap
        std::vector<uint8_t> bitmap(static_cast<size_t>(lw) * lh, 0);
        stbtt_MakeGlyphBitmap(&info, bitmap.data(), lw, lh, lw, scale, scale, layer.glyphID);

        // Get palette color (special index 0xFFFF means use foreground color → white)
        PaletteColor color = {255, 255, 255, 255};
        if (layer.paletteIndex != 0xFFFF && layer.paletteIndex < palette.size()) {
            color = palette[layer.paletteIndex];
        }

        // Composite layer into accumulator
        int baseX = (lx0 - compX0) + offsetX;
        int baseY = (ly0 - compY0) + offsetY;

        for (int y = 0; y < lh; ++y) {
            for (int x = 0; x < lw; ++x) {
                int dx = baseX + x;
                int dy = baseY + y;
                if (dx < 0 || dx >= cellSize || dy < 0 || dy >= cellSize)
                    continue;

                float coverage = bitmap[static_cast<size_t>(y) * lw + x] / 255.0f;
                if (coverage < 0.001f)
                    continue;

                float srcA = coverage * (color.a / 255.0f);
                float srcR = (color.r / 255.0f) * srcA;
                float srcG = (color.g / 255.0f) * srcA;
                float srcB = (color.b / 255.0f) * srcA;

                size_t idx = (static_cast<size_t>(dy) * cellSize + dx) * 4;
                float invSrcA = 1.0f - srcA;
                accum[idx + 0] = srcR + accum[idx + 0] * invSrcA;
                accum[idx + 1] = srcG + accum[idx + 1] * invSrcA;
                accum[idx + 2] = srcB + accum[idx + 2] * invSrcA;
                accum[idx + 3] = srcA + accum[idx + 3] * invSrcA;
            }
        }
    }

    // Convert premultiplied alpha → straight alpha for the output RGBA buffer
    for (int i = 0; i < cellSize * cellSize; ++i) {
        float a = accum[static_cast<size_t>(i) * 4 + 3];
        if (a > 0.001f) {
            output[i * 4 + 0] =
                static_cast<uint8_t>(std::clamp(accum[i * 4 + 0] / a * 255.0f, 0.0f, 255.0f));
            output[i * 4 + 1] =
                static_cast<uint8_t>(std::clamp(accum[i * 4 + 1] / a * 255.0f, 0.0f, 255.0f));
            output[i * 4 + 2] =
                static_cast<uint8_t>(std::clamp(accum[i * 4 + 2] / a * 255.0f, 0.0f, 255.0f));
            output[i * 4 + 3] = static_cast<uint8_t>(std::clamp(a * 255.0f, 0.0f, 255.0f));
        }
    }

    return true;
}

}  // namespace

// ============================================================================
// Move operations
// ============================================================================

EmojiFont::EmojiFont(EmojiFont&& other) noexcept
    : m_loaded(other.m_loaded), m_emojiSize(other.m_emojiSize), m_atlasWidth(other.m_atlasWidth),
      m_atlasHeight(other.m_atlasHeight), m_atlas(std::move(other.m_atlas)),
      m_glyphMap(std::move(other.m_glyphMap)),
      m_availableCodepoints(std::move(other.m_availableCodepoints)),
      m_lastError(std::move(other.m_lastError)) {
    other.m_loaded = false;
    other.m_lastError.clear();
}

EmojiFont& EmojiFont::operator=(EmojiFont&& other) noexcept {
    if (this != &other) {
        m_loaded = other.m_loaded;
        m_emojiSize = other.m_emojiSize;
        m_atlasWidth = other.m_atlasWidth;
        m_atlasHeight = other.m_atlasHeight;
        m_atlas = std::move(other.m_atlas);
        m_glyphMap = std::move(other.m_glyphMap);
        m_availableCodepoints = std::move(other.m_availableCodepoints);
        m_lastError = std::move(other.m_lastError);
        other.m_loaded = false;
        other.m_lastError.clear();
    }
    return *this;
}

// ============================================================================
// Loading
// ============================================================================

bool EmojiFont::loadFromFile(VulkanContext* ctx, const std::string& path, int sizePixels) {
    m_loaded = false;
    m_atlas.reset();
    m_glyphMap.clear();
    m_availableCodepoints.clear();
    m_atlasWidth = 0;
    m_atlasHeight = 0;
    m_emojiSize = 0;
    m_lastError.clear();

    if (sizePixels <= 0) {
        m_lastError = "Emoji size must be greater than zero";
        return false;
    }

    // Read the font file
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        m_lastError = "Failed to open emoji font file: " + path;
        return false;
    }
    const auto fileSize = file.tellg();
    if (fileSize <= 0) {
        m_lastError = "Emoji font file is empty: " + path;
        return false;
    }
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> fontData(static_cast<size_t>(fileSize));
    if (!file.read(reinterpret_cast<char*>(fontData.data()), fileSize)) {
        m_lastError = "Failed to read emoji font file: " + path;
        return false;
    }
    file.close();

    // Initialize stb_truetype
    stbtt_fontinfo fontInfo{};
    if (!stbtt_InitFont(&fontInfo, fontData.data(),
                        stbtt_GetFontOffsetForIndex(fontData.data(), 0))) {
        m_lastError = "Failed to initialize emoji font data: " + path;
        return false;
    }

    // Find COLR and CPAL tables
    TableEntry colrEntry = findTable(fontData.data(), fontData.size(), kTagCOLR);
    TableEntry cpalEntry = findTable(fontData.data(), fontData.size(), kTagCPAL);

    if (colrEntry.length == 0 || cpalEntry.length == 0) {
        m_lastError = "Font does not contain COLR/CPAL color emoji tables: " + path;
        return false;
    }

    // Parse COLR and CPAL
    std::vector<ColrBaseGlyph> baseGlyphs;
    std::vector<ColrLayerRecord> layerRecords;
    std::vector<PaletteColor> palette;

    if (!parseCOLR(fontData.data() + colrEntry.offset, colrEntry.length, baseGlyphs,
                   layerRecords)) {
        m_lastError = "Failed to parse COLR table in: " + path;
        return false;
    }

    if (!parseCPAL(fontData.data() + cpalEntry.offset, cpalEntry.length, palette)) {
        m_lastError = "Failed to parse CPAL table in: " + path;
        return false;
    }

    if (baseGlyphs.empty()) {
        m_lastError = "COLR table has no base glyph records";
        return false;
    }

    m_emojiSize = sizePixels;
    float scale = stbtt_ScaleForPixelHeight(&fontInfo, static_cast<float>(sizePixels));

    // Scan emoji ranges to find codepoints that have COLR entries
    struct PendingEmoji {
        char32_t codepoint;
        uint16_t glyphID;
    };
    std::vector<PendingEmoji> pendingEmoji;

    for (const auto& range : kEmojiRanges) {
        for (char32_t cp = range.first; cp <= range.last; ++cp) {
            int glyphID = stbtt_FindGlyphIndex(&fontInfo, static_cast<int>(cp));
            if (glyphID == 0)
                continue;  // no glyph for this codepoint

            if (findBaseGlyph(baseGlyphs, static_cast<uint16_t>(glyphID))) {
                pendingEmoji.push_back({cp, static_cast<uint16_t>(glyphID)});
            }
        }
    }

    if (pendingEmoji.empty()) {
        m_lastError = "No color emoji found in font: " + path;
        return false;
    }

    // Calculate grid atlas dimensions
    int numEmoji = static_cast<int>(pendingEmoji.size());
    int gridCols = static_cast<int>(std::ceil(std::sqrt(numEmoji)));
    int gridRows = (numEmoji + gridCols - 1) / gridCols;
    int atlasW = gridCols * sizePixels;
    int atlasH = gridRows * sizePixels;

    // Round up to power of two for GPU compatibility
    auto nextPow2 = [](int v) -> int {
        v--;
        v |= v >> 1;
        v |= v >> 2;
        v |= v >> 4;
        v |= v >> 8;
        v |= v >> 16;
        return v + 1;
    };
    atlasW = nextPow2(atlasW);
    atlasH = nextPow2(atlasH);

    // Guard against unreasonably large atlases (max 16384x16384 = 1 GB RGBA)
    constexpr int kMaxAtlasDim = 16384;
    if (atlasW > kMaxAtlasDim || atlasH > kMaxAtlasDim) {
        m_lastError = "Emoji atlas dimensions (" + std::to_string(atlasW) + "x" +
                      std::to_string(atlasH) + ") exceed maximum of " +
                      std::to_string(kMaxAtlasDim);
        return false;
    }

    // Allocate atlas pixel buffer (RGBA)
    std::vector<uint8_t> atlasPixels(static_cast<size_t>(atlasW) * atlasH * 4, 0);

    // Render each emoji into its grid cell
    std::vector<uint8_t> cellBuffer(static_cast<size_t>(sizePixels) * sizePixels * 4);
    int col = 0, row = 0;

    for (const auto& pe : pendingEmoji) {
        int cellX = col * sizePixels;
        int cellY = row * sizePixels;

        if (renderColorGlyph(fontInfo, scale, baseGlyphs, layerRecords, palette, pe.glyphID,
                             sizePixels, cellBuffer.data())) {
            // Copy cell buffer into atlas
            for (int y = 0; y < sizePixels; ++y) {
                const uint8_t* src = cellBuffer.data() + static_cast<size_t>(y) * sizePixels * 4;
                uint8_t* dst =
                    atlasPixels.data() + (static_cast<size_t>(cellY + y) * atlasW + cellX) * 4;
                std::memcpy(dst, src, static_cast<size_t>(sizePixels) * 4);
            }

            // Get glyph metrics
            int advanceWidth = 0, lsb = 0;
            stbtt_GetGlyphHMetrics(&fontInfo, pe.glyphID, &advanceWidth, &lsb);

            EmojiGlyph glyph{};
            glyph.width = sizePixels;
            glyph.height = sizePixels;
            glyph.bearingX = static_cast<int>(std::round(lsb * scale));
            glyph.bearingY = 0;
            glyph.advanceX = advanceWidth * scale;
            glyph.atlasX = cellX;
            glyph.atlasY = cellY;

            m_glyphMap[pe.codepoint] = glyph;
            m_availableCodepoints.push_back(pe.codepoint);
        }

        ++col;
        if (col >= gridCols) {
            col = 0;
            ++row;
        }
    }

    if (m_glyphMap.empty()) {
        m_lastError = "Failed to render any color emoji from: " + path;
        return false;
    }

    // Sort available codepoints for deterministic iteration
    std::sort(m_availableCodepoints.begin(), m_availableCodepoints.end());

    // Upload atlas texture
    m_atlasWidth = atlasW;
    m_atlasHeight = atlasH;
    m_atlas = std::make_shared<Texture>();
    m_atlas->loadFromData(atlasPixels.data(), static_cast<uint32_t>(atlasW),
                          static_cast<uint32_t>(atlasH));
    if (ctx) {
        m_atlas->uploadToGPU(ctx);
    }

    m_loaded = true;
    std::cout << "EmojiFont: Loaded " << m_glyphMap.size() << " color emoji at " << sizePixels
              << "px from " << path << " (atlas " << atlasW << "x" << atlasH << ")\n";
    return true;
}

// ============================================================================
// Glyph lookup
// ============================================================================

const EmojiGlyph* EmojiFont::getGlyph(char32_t codepoint) const {
    auto it = m_glyphMap.find(codepoint);
    return (it != m_glyphMap.end()) ? &it->second : nullptr;
}

bool EmojiFont::hasGlyph(char32_t codepoint) const {
    return m_glyphMap.count(codepoint) > 0;
}

bool EmojiFont::copyGlyphPixels(char32_t codepoint, uint8_t* out) const {
    if (!m_loaded || !m_atlas || !out)
        return false;

    const EmojiGlyph* glyph = getGlyph(codepoint);
    if (!glyph)
        return false;

    const uint8_t* atlasPixels = m_atlas->getPixelData();
    if (!atlasPixels)
        return false;

    // Copy the glyph's region from the atlas into the output buffer
    for (int y = 0; y < glyph->height; ++y) {
        const uint8_t* src =
            atlasPixels +
            (static_cast<size_t>(glyph->atlasY + y) * m_atlasWidth + glyph->atlasX) * 4;
        uint8_t* dst = out + static_cast<size_t>(y) * glyph->width * 4;
        std::memcpy(dst, src, static_cast<size_t>(glyph->width) * 4);
    }

    return true;
}

// ============================================================================
// System font discovery
// ============================================================================

std::string EmojiFont::findSystemEmojiFont() {
#ifdef _WIN32
    // Windows: Segoe UI Emoji
    const char* paths[] = {
        "C:\\Windows\\Fonts\\seguiemj.ttf",
    };
#elif defined(__APPLE__)
    // macOS: Apple Color Emoji
    const char* paths[] = {
        "/System/Library/Fonts/Apple Color Emoji.ttc",
        "/System/Library/Fonts/AppleColorEmoji.ttc",
    };
#else
    // Linux: Noto Color Emoji (COLR variant)
    const char* paths[] = {
        "/usr/share/fonts/truetype/noto/NotoColorEmoji.ttf",
        "/usr/share/fonts/noto-color-emoji/NotoColorEmoji.ttf",
        "/usr/share/fonts/google-noto-color-emoji-fonts/NotoColorEmoji.ttf",
        "/usr/local/share/fonts/NotoColorEmoji.ttf",
    };
#endif

    for (const char* p : paths) {
        if (std::filesystem::exists(p)) {
            return p;
        }
    }
    return {};
}

}  // namespace vde
