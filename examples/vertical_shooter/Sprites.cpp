/**
 * @file Sprites.cpp
 * @brief Procedural texture generation for player, enemies, projectiles, and stars.
 */

#include "Sprites.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace shooter {

// ============================================================================
// Pixel helpers
// ============================================================================

static void setPixel(std::vector<uint8_t>& px, int size, int x, int y, uint8_t r, uint8_t g,
                     uint8_t b, uint8_t a = 255) {
    if (x < 0 || x >= size || y < 0 || y >= size)
        return;
    int idx = (y * size + x) * 4;
    px[idx + 0] = r;
    px[idx + 1] = g;
    px[idx + 2] = b;
    px[idx + 3] = a;
}

static void fillCircle(std::vector<uint8_t>& px, int size, float cx, float cy, float radius,
                       uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
    int minX = std::max(0, static_cast<int>(cx - radius));
    int maxX = std::min(size - 1, static_cast<int>(cx + radius));
    int minY = std::max(0, static_cast<int>(cy - radius));
    int maxY = std::min(size - 1, static_cast<int>(cy + radius));
    float r2 = radius * radius;
    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            float dx = x + 0.5f - cx;
            float dy = y + 0.5f - cy;
            if (dx * dx + dy * dy <= r2)
                setPixel(px, size, x, y, r, g, b, a);
        }
    }
}

static bool pointInTriangle(float px, float py, float x0, float y0, float x1, float y1, float x2,
                            float y2) {
    float d1 = (px - x1) * (y0 - y1) - (x0 - x1) * (py - y1);
    float d2 = (px - x2) * (y1 - y2) - (x1 - x2) * (py - y2);
    float d3 = (px - x0) * (y2 - y0) - (x2 - x0) * (py - y0);
    bool hasNeg = (d1 < 0) || (d2 < 0) || (d3 < 0);
    bool hasPos = (d1 > 0) || (d2 > 0) || (d3 > 0);
    return !(hasNeg && hasPos);
}

static std::shared_ptr<vde::Texture> uploadTexture(vde::VulkanContext* ctx,
                                                   std::vector<uint8_t>& pixels, int size) {
    auto tex = std::make_shared<vde::Texture>();
    tex->loadFromData(pixels.data(), static_cast<uint32_t>(size), static_cast<uint32_t>(size));
    tex->uploadToGPU(ctx);
    return tex;
}

static std::shared_ptr<vde::Texture> uploadTexture(vde::VulkanContext* ctx,
                                                   std::vector<uint8_t>& pixels, int w, int h) {
    auto tex = std::make_shared<vde::Texture>();
    tex->loadFromData(pixels.data(), static_cast<uint32_t>(w), static_cast<uint32_t>(h));
    tex->uploadToGPU(ctx);
    return tex;
}

// ============================================================================
// Bitmap text font (5x7 pixels, row-major, bit7=leftmost column)
// ============================================================================

// clang-format off
static constexpr uint8_t kFont5x7[26][7] = {
    {0x70, 0x88, 0x88, 0xF8, 0x88, 0x88, 0x88}, // A
    {0xF0, 0x88, 0x88, 0xF0, 0x88, 0x88, 0xF0}, // B
    {0x70, 0x80, 0x80, 0x80, 0x80, 0x80, 0x70}, // C
    {0xF0, 0x88, 0x88, 0x88, 0x88, 0x88, 0xF0}, // D
    {0xF8, 0x80, 0x80, 0xF0, 0x80, 0x80, 0xF8}, // E
    {0xF8, 0x80, 0x80, 0xF0, 0x80, 0x80, 0x80}, // F
    {0x70, 0x80, 0x80, 0x98, 0x88, 0x88, 0x70}, // G
    {0x88, 0x88, 0x88, 0xF8, 0x88, 0x88, 0x88}, // H
    {0x70, 0x20, 0x20, 0x20, 0x20, 0x20, 0x70}, // I
    {0x18, 0x08, 0x08, 0x08, 0x08, 0x88, 0x70}, // J
    {0x88, 0x90, 0xA0, 0xC0, 0xA0, 0x90, 0x88}, // K
    {0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0xF8}, // L
    {0x88, 0xD8, 0xA8, 0x88, 0x88, 0x88, 0x88}, // M
    {0x88, 0xC8, 0xA8, 0x98, 0x88, 0x88, 0x88}, // N
    {0x70, 0x88, 0x88, 0x88, 0x88, 0x88, 0x70}, // O
    {0xF0, 0x88, 0x88, 0xF0, 0x80, 0x80, 0x80}, // P
    {0x70, 0x88, 0x88, 0x88, 0xA8, 0x98, 0x78}, // Q
    {0xF0, 0x88, 0x88, 0xF0, 0xA0, 0x90, 0x88}, // R
    {0x70, 0x80, 0x80, 0x70, 0x08, 0x08, 0x70}, // S
    {0xF8, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20}, // T
    {0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x70}, // U
    {0x88, 0x88, 0x88, 0x88, 0x88, 0x50, 0x20}, // V
    {0x88, 0x88, 0x88, 0xA8, 0xD8, 0x88, 0x88}, // W
    {0x88, 0x88, 0x50, 0x20, 0x50, 0x88, 0x88}, // X
    {0x88, 0x88, 0x50, 0x20, 0x20, 0x20, 0x20}, // Y
    {0xF8, 0x08, 0x10, 0x20, 0x40, 0x80, 0xF8}, // Z
};
// clang-format on

// Returns the 5-bit row mask for a character (uppercase output, supports A-Z, space, '/', '-')
static uint8_t fontRow(char c, int row) {
    c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
    if (c >= 'A' && c <= 'Z')
        return kFont5x7[c - 'A'][row];
    if (c == '/') {
        // Diagonal slash centered in 7 rows
        constexpr uint8_t kSlash[7] = {0x00, 0x08, 0x10, 0x20, 0x40, 0x80, 0x00};
        return kSlash[row];
    }
    if (c == '-') {
        return (row == 3) ? 0xF8 : 0x00;
    }
    return 0;  // space and unknowns
}

std::shared_ptr<vde::Texture> createTextTexture(vde::VulkanContext* ctx, const std::string& text,
                                                int pixelScale, uint8_t r, uint8_t g, uint8_t b) {
    constexpr int GLYPH_W = 5;
    constexpr int GLYPH_H = 7;
    constexpr int GAP = 1;    // one-pixel column gap between characters
    constexpr int PAD_Y = 1;  // one-pixel top/bottom padding

    const int cellW = (GLYPH_W + GAP) * pixelScale;
    const int texW = static_cast<int>(text.size()) * cellW;
    const int texH = (GLYPH_H + 2 * PAD_Y) * pixelScale;

    std::vector<uint8_t> px(texW * texH * 4, 0);

    for (int ci = 0; ci < static_cast<int>(text.size()); ++ci) {
        const int x0 = ci * cellW;
        for (int row = 0; row < GLYPH_H; ++row) {
            const uint8_t bits = fontRow(text[ci], row);
            for (int col = 0; col < GLYPH_W; ++col) {
                if (!((bits >> (7 - col)) & 1))
                    continue;
                // Stamp a pixelScale × pixelScale block
                for (int py = 0; py < pixelScale; ++py) {
                    for (int px2 = 0; px2 < pixelScale; ++px2) {
                        const int tx = x0 + col * pixelScale + px2;
                        const int ty = (row + PAD_Y) * pixelScale + py;
                        if (tx < texW && ty < texH) {
                            const int idx = (ty * texW + tx) * 4;
                            px[idx + 0] = r;
                            px[idx + 1] = g;
                            px[idx + 2] = b;
                            px[idx + 3] = 255;
                        }
                    }
                }
            }
        }
    }

    return uploadTexture(ctx, px, texW, texH);
}

// ============================================================================
// Player wedge ship (32x32)
// ============================================================================

std::shared_ptr<vde::Texture> createPlayerTexture(vde::VulkanContext* ctx) {
    constexpr int S = 32;
    std::vector<uint8_t> px(S * S * 4, 0);

    // Wedge triangle: tip at (16, 2), base corners at (4, 28) and (28, 28)
    for (int y = 0; y < S; ++y) {
        for (int x = 0; x < S; ++x) {
            float fx = x + 0.5f, fy = y + 0.5f;
            if (pointInTriangle(fx, fy, 16.0f, 2.0f, 4.0f, 28.0f, 28.0f, 28.0f)) {
                // Gradient: brighter toward nose
                float t = 1.0f - (fy - 2.0f) / 26.0f;
                uint8_t r = static_cast<uint8_t>(30 + 60 * t);
                uint8_t g = static_cast<uint8_t>(180 + 75 * t);
                uint8_t b = static_cast<uint8_t>(220 + 35 * t);
                setPixel(px, S, x, y, r, g, b);
            }
        }
    }

    // Cockpit highlight
    fillCircle(px, S, 16.0f, 12.0f, 3.0f, 200, 240, 255);

    // Engine glow at the bottom
    fillCircle(px, S, 12.0f, 27.0f, 2.5f, 255, 160, 50);
    fillCircle(px, S, 20.0f, 27.0f, 2.5f, 255, 160, 50);

    return uploadTexture(ctx, px, S);
}

// ============================================================================
// Bullet textures
// ============================================================================

std::shared_ptr<vde::Texture> createBulletTexture(vde::VulkanContext* ctx, WeaponType weapon) {
    constexpr int S = 8;
    std::vector<uint8_t> px(S * S * 4, 0);

    switch (weapon) {
    case WeaponType::Basic:
        // Elongated white bolt
        for (int y = 1; y < 7; ++y)
            for (int x = 3; x < 5; ++x)
                setPixel(px, S, x, y, 220, 240, 255);
        // Bright core
        setPixel(px, S, 3, 2, 255, 255, 255);
        setPixel(px, S, 4, 2, 255, 255, 255);
        break;

    case WeaponType::Spread:
        // Small diamond
        fillCircle(px, S, 4.0f, 4.0f, 2.5f, 180, 255, 180);
        setPixel(px, S, 4, 2, 255, 255, 200);
        break;

    case WeaponType::Rapid:
        // Thin bright line
        for (int y = 0; y < 8; ++y)
            setPixel(px, S, 4, y, 255, 200, 100);
        setPixel(px, S, 3, 3, 255, 220, 150);
        setPixel(px, S, 5, 3, 255, 220, 150);
        break;

    default:
        break;
    }
    return uploadTexture(ctx, px, S);
}

// ============================================================================
// Enemy textures (24x24)
// ============================================================================

std::shared_ptr<vde::Texture> createEnemyTexture(vde::VulkanContext* ctx, EnemyType type) {
    constexpr int S = 24;
    std::vector<uint8_t> px(S * S * 4, 0);

    switch (type) {
    case EnemyType::Turret:
        // Circular body with a barrel pointing down
        fillCircle(px, S, 12.0f, 10.0f, 8.0f, 200, 50, 50);
        for (int y = 10; y < 22; ++y)
            for (int x = 10; x < 14; ++x)
                setPixel(px, S, x, y, 160, 40, 40);
        fillCircle(px, S, 12.0f, 10.0f, 3.0f, 255, 100, 100);
        break;

    case EnemyType::Drone:
        // Diamond shape, green
        for (int y = 0; y < S; ++y) {
            for (int x = 0; x < S; ++x) {
                float dx = std::abs(x + 0.5f - 12.0f);
                float dy = std::abs(y + 0.5f - 12.0f);
                if (dx / 10.0f + dy / 10.0f <= 1.0f) {
                    uint8_t g = static_cast<uint8_t>(150 + 80 * (1.0f - dy / 10.0f));
                    setPixel(px, S, x, y, 30, g, 60);
                }
            }
        }
        fillCircle(px, S, 12.0f, 12.0f, 2.0f, 200, 255, 200);
        break;

    case EnemyType::Chaser:
        // Downward-pointing triangle, yellow/orange
        for (int y = 0; y < S; ++y) {
            for (int x = 0; x < S; ++x) {
                float fx = x + 0.5f, fy = y + 0.5f;
                if (pointInTriangle(fx, fy, 12.0f, 22.0f, 2.0f, 4.0f, 22.0f, 4.0f)) {
                    setPixel(px, S, x, y, 255, 180, 30);
                }
            }
        }
        fillCircle(px, S, 12.0f, 10.0f, 3.0f, 255, 220, 100);
        break;

    case EnemyType::Tank:
        // Large rectangle with armor plating
        for (int y = 3; y < 21; ++y) {
            for (int x = 2; x < 22; ++x) {
                uint8_t shade = static_cast<uint8_t>(160 + ((x + y) % 4) * 15);
                setPixel(px, S, x, y, shade, static_cast<uint8_t>(shade / 2), 30);
            }
        }
        // Turret on top
        fillCircle(px, S, 12.0f, 10.0f, 4.0f, 200, 100, 20);
        fillCircle(px, S, 12.0f, 10.0f, 2.0f, 255, 140, 40);
        break;
    }

    return uploadTexture(ctx, px, S);
}

// ============================================================================
// Enemy bullet (8x8)
// ============================================================================

std::shared_ptr<vde::Texture> createEnemyBulletTexture(vde::VulkanContext* ctx) {
    constexpr int S = 8;
    std::vector<uint8_t> px(S * S * 4, 0);
    fillCircle(px, S, 4.0f, 4.0f, 3.0f, 255, 80, 80);
    fillCircle(px, S, 4.0f, 4.0f, 1.5f, 255, 180, 180);
    return uploadTexture(ctx, px, S);
}

// ============================================================================
// Star (8x8 soft glow)
// ============================================================================

std::shared_ptr<vde::Texture> createStarTexture(vde::VulkanContext* ctx) {
    constexpr int S = 8;
    std::vector<uint8_t> px(S * S * 4, 0);
    for (int y = 0; y < S; ++y) {
        for (int x = 0; x < S; ++x) {
            float dx = x + 0.5f - 4.0f;
            float dy = y + 0.5f - 4.0f;
            float d = std::sqrt(dx * dx + dy * dy);
            if (d < 3.5f) {
                float brightness = 1.0f - d / 3.5f;
                auto b = static_cast<uint8_t>(255 * brightness);
                setPixel(px, S, x, y, b, b, b, b);
            }
        }
    }
    return uploadTexture(ctx, px, S);
}

// ============================================================================
// Explosion (16x16 radial burst)
// ============================================================================

std::shared_ptr<vde::Texture> createExplosionTexture(vde::VulkanContext* ctx) {
    constexpr int S = 16;
    std::vector<uint8_t> px(S * S * 4, 0);
    for (int y = 0; y < S; ++y) {
        for (int x = 0; x < S; ++x) {
            float dx = x + 0.5f - 8.0f;
            float dy = y + 0.5f - 8.0f;
            float d = std::sqrt(dx * dx + dy * dy);
            if (d < 7.5f) {
                float t = 1.0f - d / 7.5f;
                uint8_t r = static_cast<uint8_t>(255 * std::min(1.0f, t * 2.0f));
                uint8_t g = static_cast<uint8_t>(200 * t * t);
                uint8_t b = static_cast<uint8_t>(50 * t * t * t);
                uint8_t a = static_cast<uint8_t>(255 * t);
                setPixel(px, S, x, y, r, g, b, a);
            }
        }
    }
    return uploadTexture(ctx, px, S);
}

}  // namespace shooter
