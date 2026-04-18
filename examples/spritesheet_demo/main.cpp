/**
 * @file main.cpp
 * @brief SpriteSheet demo demonstrating SpriteSheet atlas management and sprite flipping.
 *
 * This example demonstrates:
 * - Grid-based spritesheet with uniform cells
 * - Named sprite regions from a manual atlas
 * - Horizontal and vertical sprite flipping
 * - Integration of SpriteSheet with SpriteEntity
 */

#include <vde/Texture.h>
#include <vde/api/GameAPI.h>
#include <vde/api/SpriteSheet.h>

#include <cmath>
#include <iostream>

#include "../ExampleBase.h"

// ============================================================================
// Input handler
// ============================================================================

class SheetInputHandler : public vde::examples::BaseExampleInputHandler {
  public:
    void onKeyPress(int key) override {
        BaseExampleInputHandler::onKeyPress(key);
        if (key == vde::KEY_LEFT)
            m_left = true;
        if (key == vde::KEY_RIGHT)
            m_right = true;
        if (key == vde::KEY_SPACE)
            m_spacePressed = true;
    }

    void onKeyRelease(int key) override {
        if (key == vde::KEY_LEFT)
            m_left = false;
        if (key == vde::KEY_RIGHT)
            m_right = false;
    }

    bool isLeft() const { return m_left; }
    bool isRight() const { return m_right; }

    bool consumeSpace() {
        bool v = m_spacePressed;
        m_spacePressed = false;
        return v;
    }

  private:
    bool m_left = false;
    bool m_right = false;
    bool m_spacePressed = false;
};

// ============================================================================
// Pixel-art sprite generators (procedural, asymmetric designs)
// ============================================================================

struct RGBA {
    uint8_t r, g, b, a;
};

/// Write a single pixel into an RGBA buffer of known width.
static void putPixel(std::vector<uint8_t>& buf, uint32_t stride, uint32_t x, uint32_t y, RGBA c) {
    size_t off = (static_cast<size_t>(y) * stride + x) * 4;
    buf[off + 0] = c.r;
    buf[off + 1] = c.g;
    buf[off + 2] = c.b;
    buf[off + 3] = c.a;
}

/// Fill a rectangular region in an RGBA buffer.
static void fillRect(std::vector<uint8_t>& buf, uint32_t stride, uint32_t x0, uint32_t y0,
                     uint32_t w, uint32_t h, RGBA c) {
    for (uint32_t y = y0; y < y0 + h; ++y)
        for (uint32_t x = x0; x < x0 + w; ++x)
            putPixel(buf, stride, x, y, c);
}

/// Draw a right-pointing arrow character (16×16) into a cell of an atlas.
/// Clearly asymmetric: body on left, pointed tip on right, eye on left side.
static void drawCharacterRight(std::vector<uint8_t>& buf, uint32_t stride, uint32_t ox, uint32_t oy,
                               RGBA body, RGBA eye, RGBA bg) {
    // Fill cell background
    fillRect(buf, stride, ox, oy, 16, 16, bg);

    // Body: a 10×10 block offset left (columns 1-10, rows 3-12)
    fillRect(buf, stride, ox + 1, oy + 3, 10, 10, body);

    // Pointed nose on right side (triangle-ish)
    //   row 5-10: extend rightward progressively
    for (int r = 0; r < 6; ++r) {
        int extra = (r < 3) ? r + 1 : (5 - r) + 1;  // diamond taper
        for (int e = 0; e < extra; ++e)
            putPixel(buf, stride, ox + 11 + static_cast<uint32_t>(e),
                     oy + 5 + static_cast<uint32_t>(r), body);
    }

    // Eye: 2×2 on the left side of the body
    fillRect(buf, stride, ox + 3, oy + 5, 2, 2, eye);

    // Feet: two small bumps at bottom-left
    fillRect(buf, stride, ox + 2, oy + 13, 2, 2, body);
    fillRect(buf, stride, ox + 6, oy + 13, 2, 2, body);

    // Tail: small appendage on left edge
    fillRect(buf, stride, ox + 0, oy + 7, 1, 3, body);
}

/// Draw a heart icon (16×16) — asymmetric because the left lobe is taller.
static void drawHeart(std::vector<uint8_t>& buf, uint32_t stride, uint32_t ox, uint32_t oy, RGBA fg,
                      RGBA highlight, RGBA bg) {
    fillRect(buf, stride, ox, oy, 16, 16, bg);
    // clang-format off
    // 16×16 heart bitmap (1 = fg, 2 = highlight, 0 = bg)
    static const uint8_t heart[16][16] = {
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,1,1,1,0,0,0,0,1,1,1,0,0,0,0},
        {0,1,2,2,1,1,0,0,1,1,1,1,1,0,0,0},
        {1,2,2,1,1,1,1,0,1,1,1,1,1,1,0,0},
        {1,2,1,1,1,1,1,1,1,1,1,1,1,1,0,0},
        {1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0},
        {1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0},
        {0,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0},
        {0,0,1,1,1,1,1,1,1,1,1,0,0,0,0,0},
        {0,0,0,1,1,1,1,1,1,1,0,0,0,0,0,0},
        {0,0,0,0,1,1,1,1,1,0,0,0,0,0,0,0},
        {0,0,0,0,0,1,1,1,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    };
    // clang-format on
    for (int r = 0; r < 16; ++r)
        for (int c = 0; c < 16; ++c) {
            if (heart[r][c] == 1)
                putPixel(buf, stride, ox + c, oy + r, fg);
            else if (heart[r][c] == 2)
                putPixel(buf, stride, ox + c, oy + r, highlight);
        }
}

/// Draw a lightning bolt icon (16×16) — inherently asymmetric.
static void drawLightning(std::vector<uint8_t>& buf, uint32_t stride, uint32_t ox, uint32_t oy,
                          RGBA fg, RGBA glow, RGBA bg) {
    fillRect(buf, stride, ox, oy, 16, 16, bg);
    // clang-format off
    static const uint8_t bolt[16][16] = {
        {0,0,0,0,0,0,1,1,1,0,0,0,0,0,0,0},
        {0,0,0,0,0,1,1,1,0,0,0,0,0,0,0,0},
        {0,0,0,0,1,1,1,0,0,0,0,0,0,0,0,0},
        {0,0,0,1,1,1,0,0,0,0,0,0,0,0,0,0},
        {0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,0},
        {0,1,2,1,1,1,1,1,1,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,1,1,1,0,0,0,0,0,0,0},
        {0,0,0,0,0,1,2,1,0,0,0,0,0,0,0,0},
        {0,0,0,0,1,2,1,0,0,0,0,0,0,0,0,0},
        {0,0,0,1,2,1,0,0,0,0,0,0,0,0,0,0},
        {0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    };
    // clang-format on
    for (int r = 0; r < 16; ++r)
        for (int c = 0; c < 16; ++c) {
            if (bolt[r][c] == 1)
                putPixel(buf, stride, ox + c, oy + r, fg);
            else if (bolt[r][c] == 2)
                putPixel(buf, stride, ox + c, oy + r, glow);
        }
}

/// Draw a shield icon (16×16) — asymmetric emblem on front.
static void drawShield(std::vector<uint8_t>& buf, uint32_t stride, uint32_t ox, uint32_t oy,
                       RGBA outline, RGBA fill, RGBA emblem, RGBA bg) {
    fillRect(buf, stride, ox, oy, 16, 16, bg);
    // clang-format off
    // 0=bg, 1=outline, 2=fill, 3=emblem
    static const uint8_t shield[16][16] = {
        {0,0,0,1,1,1,1,1,1,1,1,1,0,0,0,0},
        {0,0,1,2,2,2,2,2,2,2,2,2,1,0,0,0},
        {0,1,2,2,2,2,2,2,2,2,2,2,2,1,0,0},
        {0,1,2,2,2,3,2,2,2,2,2,2,2,1,0,0},
        {0,1,2,2,3,3,3,2,2,2,2,2,2,1,0,0},
        {0,1,2,3,3,3,3,3,2,2,2,2,2,1,0,0},
        {0,1,2,2,3,3,3,2,2,2,2,2,2,1,0,0},
        {0,1,2,2,2,3,2,2,2,2,2,2,2,1,0,0},
        {0,1,2,2,2,2,2,2,2,2,2,2,2,1,0,0},
        {0,0,1,2,2,2,2,2,2,2,2,2,1,0,0,0},
        {0,0,1,2,2,2,2,2,2,2,2,2,1,0,0,0},
        {0,0,0,1,2,2,2,2,2,2,2,1,0,0,0,0},
        {0,0,0,0,1,2,2,2,2,2,1,0,0,0,0,0},
        {0,0,0,0,0,1,2,2,2,1,0,0,0,0,0,0},
        {0,0,0,0,0,0,1,1,1,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    };
    // clang-format on
    for (int r = 0; r < 16; ++r)
        for (int c = 0; c < 16; ++c) {
            uint8_t v = shield[r][c];
            if (v == 1)
                putPixel(buf, stride, ox + c, oy + r, outline);
            else if (v == 2)
                putPixel(buf, stride, ox + c, oy + r, fill);
            else if (v == 3)
                putPixel(buf, stride, ox + c, oy + r, emblem);
        }
}

/// Draw a 1px outline around a rectangular region in an RGBA buffer.
static void drawRegionOutline(std::vector<uint8_t>& buf, uint32_t stride, uint32_t x0, uint32_t y0,
                              uint32_t w, uint32_t h, RGBA color) {
    for (uint32_t x = x0; x < x0 + w; ++x) {
        putPixel(buf, stride, x, y0, color);
        putPixel(buf, stride, x, y0 + h - 1, color);
    }
    for (uint32_t y = y0 + 1; y < y0 + h - 1; ++y) {
        putPixel(buf, stride, x0, y, color);
        putPixel(buf, stride, x0 + w - 1, y, color);
    }
}

/// Draw a 32×32 large creature — same asymmetric style as the 16×16 version but bigger,
/// with more detail: belly patch, pupil, horn, three feet.
static void drawBigCreature(std::vector<uint8_t>& buf, uint32_t stride, uint32_t ox, uint32_t oy,
                            RGBA body, RGBA belly, RGBA eye, RGBA bg) {
    fillRect(buf, stride, ox, oy, 32, 32, bg);
    // Body: 20×18 block
    fillRect(buf, stride, ox + 2, oy + 6, 20, 18, body);
    // Belly: lighter patch on lower-left of body
    fillRect(buf, stride, ox + 4, oy + 14, 8, 8, belly);
    // Nose: triangle extending right from body
    for (int r = 0; r < 10; ++r) {
        int extra = (r < 5) ? r + 1 : (9 - r) + 1;
        for (int e = 0; e < extra; ++e)
            putPixel(buf, stride, ox + 22 + static_cast<uint32_t>(e),
                     oy + 11 + static_cast<uint32_t>(r), body);
    }
    // Eye: 4×3 on left side
    fillRect(buf, stride, ox + 5, oy + 9, 4, 3, eye);
    // Pupil
    putPixel(buf, stride, ox + 6, oy + 10, {0, 0, 0, 255});
    putPixel(buf, stride, ox + 7, oy + 10, {0, 0, 0, 255});
    // Three feet at bottom
    fillRect(buf, stride, ox + 4, oy + 24, 3, 4, body);
    fillRect(buf, stride, ox + 10, oy + 24, 3, 4, body);
    fillRect(buf, stride, ox + 16, oy + 24, 3, 4, body);
    // Tail on left edge
    fillRect(buf, stride, ox + 0, oy + 14, 2, 5, body);
    // Horn/spike on top
    fillRect(buf, stride, ox + 14, oy + 3, 2, 3, body);
    putPixel(buf, stride, ox + 14, oy + 2, body);
}

/// Draw a 32×12 horizontal sword — hilt on left, blade extending right.
static void drawSword(std::vector<uint8_t>& buf, uint32_t stride, uint32_t ox, uint32_t oy,
                      RGBA blade, RGBA hilt, RGBA highlight, RGBA bg) {
    fillRect(buf, stride, ox, oy, 32, 12, bg);
    // Hilt grip
    fillRect(buf, stride, ox + 1, oy + 3, 5, 6, hilt);
    // Cross-guard
    fillRect(buf, stride, ox + 6, oy + 1, 2, 10, hilt);
    // Blade
    fillRect(buf, stride, ox + 8, oy + 4, 21, 4, blade);
    // Blade tip
    fillRect(buf, stride, ox + 29, oy + 5, 2, 2, blade);
    putPixel(buf, stride, ox + 31, oy + 5, blade);
    // Highlight along blade top edge
    for (uint32_t x = 9; x < 29; ++x)
        putPixel(buf, stride, ox + x, oy + 4, highlight);
}

/// Draw a 48×16 banner — pole on left, cloth body with wavy right edge and emblem.
static void drawBanner(std::vector<uint8_t>& buf, uint32_t stride, uint32_t ox, uint32_t oy,
                       RGBA cloth, RGBA trim, RGBA emblem, RGBA bg) {
    fillRect(buf, stride, ox, oy, 48, 16, bg);
    // Pole
    fillRect(buf, stride, ox, oy, 2, 16, trim);
    // Cloth body
    fillRect(buf, stride, ox + 2, oy + 2, 44, 12, cloth);
    // Trim top/bottom edges
    fillRect(buf, stride, ox + 2, oy + 2, 44, 1, trim);
    fillRect(buf, stride, ox + 2, oy + 13, 44, 1, trim);
    // Wavy right edge: triangular notch
    for (int r = 0; r < 6; ++r) {
        for (int c = 0; c <= r; ++c) {
            putPixel(buf, stride, ox + 45 - static_cast<uint32_t>(c),
                     oy + 7 + static_cast<uint32_t>(r), bg);
            putPixel(buf, stride, ox + 45 - static_cast<uint32_t>(c),
                     oy + 8 - static_cast<uint32_t>(r), bg);
        }
    }
    // Diamond emblem in center
    for (int r = -3; r <= 3; ++r) {
        int hw = 3 - std::abs(r);
        for (int c = -hw; c <= hw; ++c)
            putPixel(buf, stride, ox + 22 + static_cast<uint32_t>(c),
                     oy + 8 + static_cast<uint32_t>(r), emblem);
    }
}

/// Draw an 8×8 coin — circular shape with highlight.
static void drawCoin(std::vector<uint8_t>& buf, uint32_t stride, uint32_t ox, uint32_t oy,
                     RGBA outer, RGBA inner, RGBA bg) {
    fillRect(buf, stride, ox, oy, 8, 8, bg);
    // clang-format off
    static const uint8_t coin[8][8] = {
        {0,0,1,1,1,1,0,0},
        {0,1,2,2,2,1,1,0},
        {1,2,2,2,2,2,1,1},
        {1,2,2,2,2,2,1,1},
        {1,1,2,2,2,2,1,1},
        {1,1,2,2,2,2,1,1},
        {0,1,1,1,1,1,1,0},
        {0,0,1,1,1,1,0,0},
    };
    // clang-format on
    for (int r = 0; r < 8; ++r)
        for (int c = 0; c < 8; ++c) {
            if (coin[r][c] == 1)
                putPixel(buf, stride, ox + c, oy + r, outer);
            else if (coin[r][c] == 2)
                putPixel(buf, stride, ox + c, oy + r, inner);
        }
}

/// Draw an 8×8 gem — diamond shape with facets.
static void drawGem(std::vector<uint8_t>& buf, uint32_t stride, uint32_t ox, uint32_t oy,
                    RGBA outer, RGBA inner, RGBA bg) {
    fillRect(buf, stride, ox, oy, 8, 8, bg);
    // clang-format off
    static const uint8_t gem[8][8] = {
        {0,0,0,1,1,0,0,0},
        {0,0,1,2,2,1,0,0},
        {0,1,2,2,2,2,1,0},
        {1,2,2,2,2,2,2,1},
        {1,1,2,2,2,2,1,1},
        {0,1,1,2,2,1,1,0},
        {0,0,1,1,1,1,0,0},
        {0,0,0,1,1,0,0,0},
    };
    // clang-format on
    for (int r = 0; r < 8; ++r)
        for (int c = 0; c < 8; ++c) {
            if (gem[r][c] == 1)
                putPixel(buf, stride, ox + c, oy + r, outer);
            else if (gem[r][c] == 2)
                putPixel(buf, stride, ox + c, oy + r, inner);
        }
}

/// Draw a 16×32 tall tower — pointed roof, stone body, windows, door.
static void drawTower(std::vector<uint8_t>& buf, uint32_t stride, uint32_t ox, uint32_t oy,
                      RGBA stone, RGBA roof, RGBA window, RGBA bg) {
    fillRect(buf, stride, ox, oy, 16, 32, bg);
    // Pointed roof
    for (int r = 0; r < 8; ++r) {
        for (int c = 7 - r; c <= 8 + r && c < 16; ++c)
            if (c >= 0)
                putPixel(buf, stride, ox + static_cast<uint32_t>(c), oy + static_cast<uint32_t>(r),
                         roof);
    }
    // Stone body
    fillRect(buf, stride, ox + 2, oy + 8, 12, 22, stone);
    // Battlements
    fillRect(buf, stride, ox + 2, oy + 7, 2, 2, stone);
    fillRect(buf, stride, ox + 6, oy + 7, 2, 2, stone);
    fillRect(buf, stride, ox + 10, oy + 7, 2, 2, stone);
    // Windows (two rows)
    fillRect(buf, stride, ox + 4, oy + 11, 3, 3, window);
    fillRect(buf, stride, ox + 9, oy + 11, 3, 3, window);
    fillRect(buf, stride, ox + 4, oy + 17, 3, 3, window);
    fillRect(buf, stride, ox + 9, oy + 17, 3, 3, window);
    // Door at bottom center
    fillRect(buf, stride, ox + 6, oy + 24, 4, 6, window);
}

// ============================================================================
// Scene
// ============================================================================

class SheetScene : public vde::examples::BaseExampleScene {
  public:
    SheetScene() : BaseExampleScene() {}

    void onEnter() override {
        printExampleHeader();

        auto* camera = new vde::Camera2D(18.0f, 11.0f);
        camera->setPosition(0.0f, 0.0f);
        camera->setZoom(1.0f);
        setCamera(camera);

        setBackgroundColor(vde::Color::fromHex(0x1a1a2e));

        // =================================================================
        // Build ONE atlas texture with sprites of DIFFERENT sizes packed in.
        //
        //   Atlas: 96 × 64 pixels.  Layout:
        //
        //   (0,0)   32×32  big_creature   (32,0)  16×16  heart
        //   (48,0)  16×16  bolt           (32,16) 16×16  player
        //   (48,16) 16×16  shield         (64,0)  32×12  sword
        //   (64,12) 8×8    coin           (72,12) 8×8    gem
        //   (0,32)  16×32  tower          (16,32) 48×16  banner
        //
        //   Sizes used: 32×32, 16×16, 32×12, 8×8, 16×32, 48×16
        // =================================================================
        constexpr uint32_t kTexW = 96;
        constexpr uint32_t kTexH = 64;
        constexpr RGBA kBg{0, 0, 0, 0};  // Transparent — checkerboard shows through
        constexpr RGBA kEye{255, 255, 255, 255};

        std::vector<uint8_t> atlasPixels(kTexW * kTexH * 4, 0);
        fillRect(atlasPixels, kTexW, 0, 0, kTexW, kTexH, kBg);

        // --- Draw each sprite into the atlas ---

        // big_creature 32×32 at (0,0)
        drawBigCreature(atlasPixels, kTexW, 0, 0, {230, 57, 70, 255}, {255, 180, 160, 255}, kEye,
                        kBg);

        // heart 16×16 at (32,0)
        drawHeart(atlasPixels, kTexW, 32, 0, {220, 40, 60, 255}, {255, 150, 170, 255}, kBg);

        // bolt 16×16 at (48,0)
        drawLightning(atlasPixels, kTexW, 48, 0, {255, 220, 50, 255}, {255, 255, 200, 255}, kBg);

        // player 16×16 at (32,16)
        drawCharacterRight(atlasPixels, kTexW, 32, 16, {50, 180, 80, 255}, kEye, kBg);

        // shield 16×16 at (48,16)
        drawShield(atlasPixels, kTexW, 48, 16, {80, 80, 100, 255}, {60, 120, 200, 255},
                   {255, 215, 0, 255}, kBg);

        // sword 32×12 at (64,0)
        drawSword(atlasPixels, kTexW, 64, 0, {180, 200, 220, 255}, {120, 80, 40, 255},
                  {240, 240, 255, 255}, kBg);

        // coin 8×8 at (64,12)
        drawCoin(atlasPixels, kTexW, 64, 12, {180, 140, 20, 255}, {255, 220, 50, 255}, kBg);

        // gem 8×8 at (72,12)
        drawGem(atlasPixels, kTexW, 72, 12, {40, 100, 200, 255}, {100, 180, 255, 255}, kBg);

        // tower 16×32 at (0,32)
        drawTower(atlasPixels, kTexW, 0, 32, {140, 130, 110, 255}, {160, 50, 50, 255},
                  {60, 50, 40, 255}, kBg);

        // banner 48×16 at (16,32)
        drawBanner(atlasPixels, kTexW, 16, 32, {180, 30, 30, 255}, {140, 110, 20, 255},
                   {255, 220, 50, 255}, kBg);

        // --- Draw colored outlines around each region ---
        constexpr RGBA kOutline{120, 120, 180, 255};
        drawRegionOutline(atlasPixels, kTexW, 0, 0, 32, 32, kOutline);    // big_creature
        drawRegionOutline(atlasPixels, kTexW, 32, 0, 16, 16, kOutline);   // heart
        drawRegionOutline(atlasPixels, kTexW, 48, 0, 16, 16, kOutline);   // bolt
        drawRegionOutline(atlasPixels, kTexW, 32, 16, 16, 16, kOutline);  // player
        drawRegionOutline(atlasPixels, kTexW, 48, 16, 16, 16, kOutline);  // shield
        drawRegionOutline(atlasPixels, kTexW, 64, 0, 32, 12, kOutline);   // sword
        drawRegionOutline(atlasPixels, kTexW, 64, 12, 8, 8, kOutline);    // coin
        drawRegionOutline(atlasPixels, kTexW, 72, 12, 8, 8, kOutline);    // gem
        drawRegionOutline(atlasPixels, kTexW, 0, 32, 16, 32, kOutline);   // tower
        drawRegionOutline(atlasPixels, kTexW, 16, 32, 48, 16, kOutline);  // banner

        // Upload the atlas
        auto atlasTex = std::make_shared<vde::Texture>();
        atlasTex->loadFromData(atlasPixels.data(), kTexW, kTexH);
        if (auto* ctx = getGame()->getVulkanContext()) {
            atlasTex->uploadToGPU(ctx);
        }

        // =================================================================
        // Checkerboard texture — placed behind sprites to prove the alpha
        // mask is correct: transparent atlas pixels let the pattern show
        // through while opaque sprite pixels occlude it.
        // =================================================================
        {
            constexpr uint32_t kChkW = 32;
            constexpr uint32_t kChkH = 32;
            constexpr uint32_t kChkCell = 4;  // 8×8 grid of checks
            constexpr RGBA kChkLight{200, 200, 200, 255};
            constexpr RGBA kChkDark{100, 100, 100, 255};

            std::vector<uint8_t> chkPixels(kChkW * kChkH * 4);
            for (uint32_t y = 0; y < kChkH; ++y)
                for (uint32_t x = 0; x < kChkW; ++x) {
                    bool light = ((x / kChkCell) + (y / kChkCell)) % 2 == 0;
                    putPixel(chkPixels, kChkW, x, y, light ? kChkLight : kChkDark);
                }

            auto chkTex = std::make_shared<vde::Texture>();
            chkTex->loadFromData(chkPixels.data(), kChkW, kChkH);
            if (auto* ctx = getGame()->getVulkanContext()) {
                chkTex->uploadToGPU(ctx);
            }

            // Behind right-side extracted sprites
            auto chkRight = addEntity<vde::SpriteEntity>();
            chkRight->setTexture(chkTex);
            chkRight->setPosition(3.5f, 1.5f, -0.1f);
            chkRight->setScale(10.0f, 8.0f, 1.0f);
            chkRight->setAnchor(0.5f, 0.5f);

            // Behind bottom interactive player strip
            auto chkBottom = addEntity<vde::SpriteEntity>();
            chkBottom->setTexture(chkTex);
            chkBottom->setPosition(0.0f, -3.8f, -0.1f);
            chkBottom->setScale(16.0f, 2.5f, 1.0f);
            chkBottom->setAnchor(0.5f, 0.5f);
        }

        // =================================================================
        // LEFT: Display the full atlas as one big sprite.
        // Atlas is 96×64 px; display at 6.0 wide => 4.0 tall.
        // =================================================================
        {
            constexpr float kAtlasDisplayW = 6.0f;
            constexpr float kAtlasDisplayH =
                kAtlasDisplayW * (static_cast<float>(kTexH) / static_cast<float>(kTexW));

            auto fullAtlas = addEntity<vde::SpriteEntity>();
            fullAtlas->setTexture(atlasTex);
            fullAtlas->setPosition(-5.0f, 0.8f, 0.0f);
            fullAtlas->setScale(kAtlasDisplayW, kAtlasDisplayH, 1.0f);
            fullAtlas->setAnchor(0.5f, 0.5f);
        }

        // =================================================================
        // RIGHT: Extract each sprite by name using addSprite() with pixel
        // coordinates.  Display at proportional world-unit sizes.
        //
        //   Scale: 16 px = 0.9 world units (base unit).
        // =================================================================
        constexpr float kPxToWorld = 0.9f / 16.0f;

        m_sheet = vde::SpriteSheet::create(atlasTex);
        m_sheet->addSprite("big_creature", 0, 0, 32, 32);
        m_sheet->addSprite("heart", 32, 0, 16, 16);
        m_sheet->addSprite("bolt", 48, 0, 16, 16);
        m_sheet->addSprite("player", 32, 16, 16, 16);
        m_sheet->addSprite("shield", 48, 16, 16, 16);
        m_sheet->addSprite("sword", 64, 0, 32, 12);
        m_sheet->addSprite("coin", 64, 12, 8, 8);
        m_sheet->addSprite("gem", 72, 12, 8, 8);
        m_sheet->addSprite("tower", 0, 32, 16, 32);
        m_sheet->addSprite("banner", 16, 32, 48, 16);
        addResource<vde::SpriteSheet>(m_sheet);

        // Helper to create a sprite from a named region
        auto makeSprite = [&](const char* name, float x, float y) {
            auto uv = m_sheet->getUVRect(name);
            auto sprite = addEntity<vde::SpriteEntity>();
            sprite->setTexture(atlasTex);
            sprite->setUVRect(uv.u, uv.v, uv.width, uv.height);
            // Scale proportional to pixel size
            float pw = uv.width * static_cast<float>(kTexW) * kPxToWorld;
            float ph = uv.height * static_cast<float>(kTexH) * kPxToWorld;
            sprite->setScale(pw, ph, 1.0f);
            sprite->setPosition(x, y, 0.0f);
            sprite->setAnchor(0.5f, 0.5f);
            return sprite;
        };

        // Row 1 (y ≈ 3.2): Large sprites
        makeSprite("big_creature", 1.0f, 3.2f);  // 32×32 = 1.8×1.8
        makeSprite("tower", 3.5f, 3.2f);         // 16×32 = 0.9×1.8

        // Row 2 (y ≈ 1.2): Standard 16×16 icons + tiny sprites
        makeSprite("heart", 0.5f, 1.2f);  // 16×16 = 0.9×0.9
        makeSprite("bolt", 1.6f, 1.2f);
        makeSprite("shield", 2.7f, 1.2f);
        makeSprite("coin", 3.8f, 1.2f);  // 8×8 = 0.45×0.45
        makeSprite("gem", 4.5f, 1.2f);

        // Row 3 (y ≈ -0.2): Wide sprites
        makeSprite("sword", 2.0f, -0.2f);   // 32×12 = 1.8×0.675
        makeSprite("banner", 5.8f, -0.2f);  // 48×16 = 2.7×0.9

        // --- Flip demo: original + flipped side by side ---
        {
            auto uv = m_sheet->getUVRect("big_creature");
            float pw = uv.width * static_cast<float>(kTexW) * kPxToWorld;
            float ph = uv.height * static_cast<float>(kTexH) * kPxToWorld;

            auto orig = addEntity<vde::SpriteEntity>();
            orig->setTexture(atlasTex);
            orig->setUVRect(uv.u, uv.v, uv.width, uv.height);
            orig->setScale(pw, ph, 1.0f);
            orig->setPosition(5.5f, 3.2f, 0.0f);
            orig->setAnchor(0.5f, 0.5f);

            auto flip = addEntity<vde::SpriteEntity>();
            flip->setTexture(atlasTex);
            flip->setUVRect(uv.u, uv.v, uv.width, uv.height);
            flip->setFlipX(true);
            flip->setScale(pw, ph, 1.0f);
            flip->setPosition(7.5f, 3.2f, 0.0f);
            flip->setAnchor(0.5f, 0.5f);
        }

        // =================================================================
        // BOTTOM: Interactive player extracted from the atlas.
        // LEFT/RIGHT moves and auto-flips the green character.
        // =================================================================
        auto playerUV = m_sheet->getUVRect("player");

        // Static reference (always facing right)
        auto ref = addEntity<vde::SpriteEntity>();
        ref->setTexture(atlasTex);
        ref->setUVRect(playerUV.u, playerUV.v, playerUV.width, playerUV.height);
        ref->setPosition(-3.0f, -3.8f, 0.0f);
        ref->setScale(1.2f, 1.2f, 1.0f);
        ref->setAnchor(0.5f, 0.5f);

        // Movable character
        m_character = addEntity<vde::SpriteEntity>();
        m_character->setTexture(atlasTex);
        m_character->setUVRect(playerUV.u, playerUV.v, playerUV.width, playerUV.height);
        m_character->setPosition(0.0f, -3.8f, 0.0f);
        m_character->setScale(1.2f, 1.2f, 1.0f);
        m_character->setAnchor(0.5f, 0.5f);
        m_facingRight = true;
    }

    void update(float deltaTime) override {
        BaseExampleScene::update(deltaTime);

        auto* input = dynamic_cast<SheetInputHandler*>(getInputHandler());
        if (!input)
            return;

        float speed = 3.0f;
        auto pos = m_character->getPosition();

        if (input->isLeft()) {
            pos.x -= speed * deltaTime;
            if (m_facingRight) {
                m_character->setFlipX(true);
                m_facingRight = false;
            }
        }
        if (input->isRight()) {
            pos.x += speed * deltaTime;
            if (!m_facingRight) {
                m_character->setFlipX(false);
                m_facingRight = true;
            }
        }

        m_character->setPosition(pos);
    }

  protected:
    std::string getExampleName() const override { return "SpriteSheet & Flip"; }

    std::vector<std::string> getFeatures() const override {
        return {
            "Single atlas with mixed-size sprites: 32x32, 16x16, 32x12, 48x16, 16x32, 8x8",
            "Transparent atlas background — checkerboard proves alpha mask is correct",
            "Named-region extraction via addSprite() with pixel coordinates",
            "Proportional display — each sprite rendered at its true aspect ratio",
            "Original vs. flipped big creature side by side",
            "Interactive player moves over checkerboard to show transparency",
        };
    }

    std::vector<std::string> getExpectedVisuals() const override {
        return {
            "Left: full atlas image with transparent backgrounds on dark scene",
            "Right: sprites on gray checkerboard — opaque shapes, transparent bg",
            "Right-mid: heart, bolt, shield (16x16) + tiny coin & gem (8x8)",
            "Right: wide sword (32x12) and banner (48x16)",
            "Right-top: original + flipped big creature side by side",
            "Bottom: checkerboard strip; green character moves and flips over it",
        };
    }

    std::vector<std::string> getControls() const override {
        return {
            "LEFT/RIGHT - Move character (auto-flips sprite)",
        };
    }

  private:
    vde::SpriteSheet::Ref m_sheet;
    std::shared_ptr<vde::SpriteEntity> m_character;
    bool m_facingRight = true;
};

// ============================================================================
// Game
// ============================================================================

class SpritesheetDemo : public vde::examples::BaseExampleGame<SheetInputHandler, SheetScene> {};

int main(int argc, char** argv) {
    SpritesheetDemo demo;
    return vde::examples::runExample(demo, "VDE SpriteSheet Demo", 1024, 768, argc, argv);
}
