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

// ============================================================================
// Scene
// ============================================================================

class SheetScene : public vde::examples::BaseExampleScene {
  public:
    SheetScene() : BaseExampleScene() {}

    void onEnter() override {
        printExampleHeader();

        // 2D camera: 12 × 9 world units (a little wider to fit the layout)
        auto* camera = new vde::Camera2D(12.0f, 9.0f);
        camera->setPosition(0.0f, 0.0f);
        camera->setZoom(1.0f);
        setCamera(camera);

        setBackgroundColor(vde::Color::fromHex(0x1a1a2e));

        constexpr RGBA kTransparent{0, 0, 0, 0};

        // -------------------------------------------------------------------
        // Row 1 (top, y ≈ 3): Grid spritesheet — 4×2 walk-cycle character
        // Each frame is a 16×16 right-facing creature drawn into an atlas.
        // The 8 frames differ by foot position to simulate a walk cycle.
        // -------------------------------------------------------------------
        constexpr uint32_t kCellW = 16;
        constexpr uint32_t kCellH = 16;
        constexpr int kCols = 4;
        constexpr int kRows = 2;
        constexpr uint32_t kTexW = kCellW * kCols;
        constexpr uint32_t kTexH = kCellH * kRows;

        RGBA bodyColors[8] = {
            {230, 57, 70, 255},  {244, 162, 97, 255},  {233, 196, 106, 255}, {42, 157, 143, 255},
            {69, 123, 157, 255}, {168, 218, 220, 255}, {241, 250, 238, 255}, {200, 80, 200, 255},
        };
        RGBA eyeColor{255, 255, 255, 255};
        RGBA cellBg{20, 20, 40, 255};

        std::vector<uint8_t> atlasPixels(kTexW * kTexH * 4, 0);
        for (int row = 0; row < kRows; ++row) {
            for (int col = 0; col < kCols; ++col) {
                int idx = row * kCols + col;
                uint32_t ox = static_cast<uint32_t>(col) * kCellW;
                uint32_t oy = static_cast<uint32_t>(row) * kCellH;
                drawCharacterRight(atlasPixels, kTexW, ox, oy, bodyColors[idx], eyeColor, cellBg);

                // Vary foot position per frame to suggest a walk cycle:
                // offset one foot up by 1 px on odd frames
                if (idx % 2 == 1) {
                    // Erase original left foot, redraw 1 px higher
                    fillRect(atlasPixels, kTexW, ox + 2, oy + 13, 2, 2, cellBg);
                    fillRect(atlasPixels, kTexW, ox + 2, oy + 12, 2, 2, bodyColors[idx]);
                }
            }
        }

        auto atlasTex = std::make_shared<vde::Texture>();
        atlasTex->loadFromData(atlasPixels.data(), kTexW, kTexH);
        if (auto* ctx = getGame()->getVulkanContext()) {
            atlasTex->uploadToGPU(ctx);
        }

        m_gridSheet = vde::SpriteSheet::createGrid(atlasTex, kCols, kRows);

        float startX = -3.5f;
        for (int i = 0; i < m_gridSheet->getSpriteCount(); ++i) {
            auto sprite = addEntity<vde::SpriteEntity>();
            sprite->setTexture(atlasTex);
            auto uv = m_gridSheet->getUVRect(i);
            sprite->setUVRect(uv.u, uv.v, uv.width, uv.height);
            sprite->setPosition(startX + static_cast<float>(i) * 1.0f, 3.0f, 0.0f);
            sprite->setScale(0.9f, 0.9f, 1.0f);
        }

        // -------------------------------------------------------------------
        // Row 2 (middle, y ≈ 0): Named regions — 3 asymmetric HUD icons
        //   heart (health), lightning bolt (mana), shield (stamina)
        // Each is 16×16, packed into a 48×16 atlas.
        // -------------------------------------------------------------------
        constexpr uint32_t kIconW = 16;
        constexpr uint32_t kIconTexW = kIconW * 3;
        constexpr uint32_t kIconTexH = 16;

        std::vector<uint8_t> iconPixels(kIconTexW * kIconTexH * 4, 0);

        // Heart at (0,0)
        drawHeart(iconPixels, kIconTexW, 0, 0, {220, 40, 60, 255}, {255, 150, 170, 255},
                  kTransparent);

        // Lightning at (16,0)
        drawLightning(iconPixels, kIconTexW, 16, 0, {255, 220, 50, 255}, {255, 255, 200, 255},
                      kTransparent);

        // Shield at (32,0)
        drawShield(iconPixels, kIconTexW, 32, 0, {80, 80, 100, 255}, {60, 120, 200, 255},
                   {255, 215, 0, 255}, kTransparent);

        auto iconTex = std::make_shared<vde::Texture>();
        iconTex->loadFromData(iconPixels.data(), kIconTexW, kIconTexH);
        if (auto* ctx = getGame()->getVulkanContext()) {
            iconTex->uploadToGPU(ctx);
        }

        m_iconSheet = vde::SpriteSheet::create(iconTex);
        m_iconSheet->addSprite("health", 0, 0, 16, 16);
        m_iconSheet->addSprite("mana", 16, 0, 16, 16);
        m_iconSheet->addSprite("stamina", 32, 0, 16, 16);

        // Show icons: original on the left, flipped copy on the right
        const char* names[] = {"health", "mana", "stamina"};
        for (int i = 0; i < 3; ++i) {
            auto uv = m_iconSheet->getUVRect(names[i]);
            float cx = -2.0f + static_cast<float>(i) * 2.0f;

            // Original
            auto orig = addEntity<vde::SpriteEntity>();
            orig->setTexture(iconTex);
            orig->setUVRect(uv.u, uv.v, uv.width, uv.height);
            orig->setPosition(cx - 0.45f, 0.0f, 0.0f);
            orig->setScale(0.8f, 0.8f, 1.0f);

            // Flipped copy (horizontal)
            auto flipped = addEntity<vde::SpriteEntity>();
            flipped->setTexture(iconTex);
            flipped->setUVRect(uv.u, uv.v, uv.width, uv.height);
            flipped->setFlipX(true);
            flipped->setPosition(cx + 0.45f, 0.0f, 0.0f);
            flipped->setScale(0.8f, 0.8f, 1.0f);
        }

        // -------------------------------------------------------------------
        // Row 3 (bottom, y ≈ −3): Flip demo — movable character sprite
        // Uses the same right-facing creature. LEFT/RIGHT movement auto-flips.
        // A static "facing right" reference sprite sits nearby for comparison.
        // -------------------------------------------------------------------
        auto charTex = std::make_shared<vde::Texture>();
        constexpr uint32_t kCharW = 16;
        constexpr uint32_t kCharH = 16;
        std::vector<uint8_t> charPixels(kCharW * kCharH * 4, 0);
        drawCharacterRight(charPixels, kCharW, 0, 0, {50, 180, 80, 255}, {255, 255, 255, 255},
                           kTransparent);
        charTex->loadFromData(charPixels.data(), kCharW, kCharH);
        if (auto* ctx = getGame()->getVulkanContext()) {
            charTex->uploadToGPU(ctx);
        }

        // Static reference (always facing right)
        auto ref = addEntity<vde::SpriteEntity>();
        ref->setTexture(charTex);
        ref->setPosition(-3.0f, -3.0f, 0.0f);
        ref->setScale(1.2f, 1.2f, 1.0f);
        ref->setAnchor(0.5f, 0.5f);

        // Movable character
        m_character = addEntity<vde::SpriteEntity>();
        m_character->setTexture(charTex);
        m_character->setPosition(0.0f, -3.0f, 0.0f);
        m_character->setScale(1.2f, 1.2f, 1.0f);
        m_character->setAnchor(0.5f, 0.5f);
        m_facingRight = true;
    }

    void update(float deltaTime) override {
        BaseExampleScene::update(deltaTime);

        auto* input = dynamic_cast<SheetInputHandler*>(getInputHandler());
        if (!input)
            return;

        // Move character and auto-flip
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

        // Step through grid frames manually
        if (input->consumeSpace()) {
            m_currentFrame = (m_currentFrame + 1) % m_gridSheet->getSpriteCount();
            std::cout << "Frame: " << m_currentFrame << std::endl;
        }
    }

  protected:
    std::string getExampleName() const override { return "SpriteSheet & Flip"; }

    std::vector<std::string> getFeatures() const override {
        return {
            "Grid spritesheet (4x2 walk-cycle creatures with eyes, feet, tails)",
            "Named sprite regions (heart, lightning, shield icons)",
            "Side-by-side original vs. flipped icons to show asymmetry",
            "Movable character with auto-flip on direction change",
        };
    }

    std::vector<std::string> getExpectedVisuals() const override {
        return {"Top: 8 coloured pixel-art creatures facing right (asymmetric — eye+tail on left)",
                "Middle: 3 icon pairs (original | flipped) — heart, lightning bolt, shield",
                "Bottom: green creature moves LEFT/RIGHT and flips; static reference on left"};
    }

    std::vector<std::string> getControls() const override {
        return {
            "LEFT/RIGHT - Move character (auto-flips sprite)",
            "SPACE      - Step through spritesheet frames",
        };
    }

  private:
    vde::SpriteSheet::Ref m_gridSheet;
    vde::SpriteSheet::Ref m_iconSheet;
    std::shared_ptr<vde::SpriteEntity> m_character;
    bool m_facingRight = true;
    int m_currentFrame = 0;
};

// ============================================================================
// Game
// ============================================================================

class SpritesheetDemo : public vde::examples::BaseExampleGame<SheetInputHandler, SheetScene> {};

int main(int argc, char** argv) {
    SpritesheetDemo demo;
    return vde::examples::runExample(demo, "VDE SpriteSheet Demo", 1024, 768, argc, argv);
}
