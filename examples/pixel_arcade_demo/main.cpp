/**
 * @file main.cpp
 * @brief Pixel Arcade Demo — retro arcade attract screen using BitmapFont + TextRenderer.
 *
 * Demonstrates Phase 1 text rendering with:
 * - "INSERT COIN" banner using BitmapFont::large() at 4× scale
 * - Flashing high-score table (10 entries) in alternating colors using BitmapFont::small()
 * - "PLAYER 1 READY" banner with per-letter color cycling (green, yellow, red, cyan)
 * - Scrolling marquee of game instructions at the bottom
 * - ~30 text textures created at startup — stress test of batch creation
 * - Varied pixelScale (1–4×) and letterSpacing values
 */

#include <vde/Texture.h>
#include <vde/api/GameAPI.h>

#include <cmath>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

#include "../ExampleBase.h"

// ---------------------------------------------------------------------------
// Input handler — no custom keys needed beyond the base
// ---------------------------------------------------------------------------
class ArcadeInputHandler : public vde::examples::BaseExampleInputHandler {};

// ---------------------------------------------------------------------------
// Scene
// ---------------------------------------------------------------------------
class ArcadeScene : public vde::examples::BaseExampleScene {
  public:
    ArcadeScene() : BaseExampleScene() {}

    void onEnter() override {
        printExampleHeader();

        auto* camera = new vde::Camera2D(16.0f, 12.0f);
        camera->setPosition(0.0f, 0.0f);
        camera->setZoom(1.0f);
        setCamera(camera);

        setBackgroundColor(vde::Color::black());

        auto* ctx = getGame()->getVulkanContext();

        const auto& small = vde::BitmapFont::small();
        const auto& large = vde::BitmapFont::large();

        // ---- INSERT COIN banner (large font, 4× scale, centered near top) ----
        {
            vde::TextStyle style{
                .color = vde::Color::yellow(), .pixelScale = 4, .letterSpacing = 2};
            auto tex = vde::TextRenderer::createTexture(ctx, "INSERT COIN", large, style);
            auto sprite = addEntity<vde::SpriteEntity>();
            float aspect =
                static_cast<float>(tex->getWidth()) / static_cast<float>(tex->getHeight());
            float h = 1.8f;
            sprite->setTexture(tex);
            sprite->setScale(h * aspect, h, 1.0f);
            sprite->setPosition(0.0f, 4.8f, 0.0f);
            sprite->setAnchor(0.5f, 0.5f);
            m_insertCoinSprite = sprite;
        }

        // ---- HIGH SCORES title (large font, 2× scale) ----
        {
            vde::TextStyle style{.color = vde::Color::cyan(), .pixelScale = 2, .letterSpacing = 1};
            auto tex = vde::TextRenderer::createTexture(ctx, "HIGH SCORES", large, style);
            auto sprite = addEntity<vde::SpriteEntity>();
            float aspect =
                static_cast<float>(tex->getWidth()) / static_cast<float>(tex->getHeight());
            float h = 0.8f;
            sprite->setTexture(tex);
            sprite->setScale(h * aspect, h, 1.0f);
            sprite->setPosition(0.0f, 3.4f, 0.0f);
            sprite->setAnchor(0.5f, 0.5f);
        }

        // ---- 10 high-score entries (small font, 2× scale, alternating colors) ----
        {
            const vde::Color colors[] = {
                vde::Color::white(),
                vde::Color::fromRGB8(255, 200, 50),  // gold
            };

            const char* names[] = {"AAA", "BOB", "CAT", "DAN", "EVE",
                                   "FOX", "GUS", "HAL", "IVY", "JAX"};
            int scores[] = {99900, 87650, 76500, 65400, 54300, 43200, 32100, 21000, 10500, 5000};

            for (int i = 0; i < 10; ++i) {
                char buf[32];
                snprintf(buf, sizeof(buf), "%2d. %s %6d", i + 1, names[i], scores[i]);

                vde::TextStyle style{.color = colors[i % 2], .pixelScale = 2, .letterSpacing = 1};
                auto tex = vde::TextRenderer::createTexture(ctx, buf, small, style);
                auto sprite = addEntity<vde::SpriteEntity>();
                float aspect =
                    static_cast<float>(tex->getWidth()) / static_cast<float>(tex->getHeight());
                float h = 0.45f;
                sprite->setTexture(tex);
                sprite->setScale(h * aspect, h, 1.0f);
                float y = 2.6f - i * 0.50f;
                sprite->setPosition(0.0f, y, 0.0f);
                sprite->setAnchor(0.5f, 0.5f);
                m_scoreSprites.push_back(sprite);
            }
        }

        // ---- PLAYER 1 READY — per-letter color cycling ----
        {
            const std::string text = "PLAYER 1 READY";
            const vde::Color letterColors[] = {
                vde::Color::green(),
                vde::Color::yellow(),
                vde::Color::red(),
                vde::Color::cyan(),
            };
            constexpr int numColors = 4;

            float totalWidth = 0.0f;
            struct LetterInfo {
                std::shared_ptr<vde::Texture> tex;
                float w, h;
            };
            std::vector<LetterInfo> letters;
            letters.reserve(text.size());

            for (size_t i = 0; i < text.size(); ++i) {
                std::string ch(1, text[i]);
                vde::TextStyle style{
                    .color = letterColors[i % numColors], .pixelScale = 3, .letterSpacing = 0};
                auto tex = vde::TextRenderer::createTexture(ctx, ch, large, style);
                float aspect =
                    static_cast<float>(tex->getWidth()) / static_cast<float>(tex->getHeight());
                float h = 1.2f;
                float w = h * aspect;
                letters.push_back({tex, w, h});
                totalWidth += w;
            }

            // Add small gap between letters
            float gap = 0.05f;
            totalWidth += gap * static_cast<float>(text.size() - 1);

            float x = -totalWidth / 2.0f;
            for (size_t i = 0; i < letters.size(); ++i) {
                auto sprite = addEntity<vde::SpriteEntity>();
                sprite->setTexture(letters[i].tex);
                sprite->setScale(letters[i].w, letters[i].h, 1.0f);
                sprite->setPosition(x + letters[i].w / 2.0f, -2.8f, 0.0f);
                sprite->setAnchor(0.5f, 0.5f);
                m_playerReadySprites.push_back(sprite);
                x += letters[i].w + gap;
            }
        }

        // ---- Scrolling marquee at bottom (small font, 1× scale) ----
        {
            vde::TextStyle style{
                .color = vde::Color::fromRGB8(180, 180, 255), .pixelScale = 1, .letterSpacing = 1};
            auto tex = vde::TextRenderer::createTexture(
                ctx,
                "USE ARROW KEYS TO MOVE --- PRESS SPACE TO FIRE --- COLLECT POWER-UPS FOR BONUS "
                "POINTS --- AVOID ENEMY SHIPS",
                small, style);
            auto sprite = addEntity<vde::SpriteEntity>();
            float aspect =
                static_cast<float>(tex->getWidth()) / static_cast<float>(tex->getHeight());
            float h = 0.35f;
            sprite->setTexture(tex);
            sprite->setScale(h * aspect, h, 1.0f);
            sprite->setAnchor(0.0f, 0.5f);
            sprite->setPosition(8.5f, -4.8f, 0.0f);
            m_marqueeSprite = sprite;
            m_marqueeWidth = h * aspect;
        }

        // ---- Extra text textures to reach ~30 batch count ----
        // (already created: 1 + 1 + 10 + 14 + 1 = 27, add a few more decorative labels)
        {
            vde::TextStyle style{
                .color = vde::Color::magenta(), .pixelScale = 1, .letterSpacing = 1};
            auto tex = vde::TextRenderer::createTexture(ctx, "CREDIT 00", small, style);
            auto sprite = addEntity<vde::SpriteEntity>();
            float aspect =
                static_cast<float>(tex->getWidth()) / static_cast<float>(tex->getHeight());
            float h = 0.3f;
            sprite->setTexture(tex);
            sprite->setScale(h * aspect, h, 1.0f);
            sprite->setPosition(-6.5f, -5.5f, 0.0f);
            sprite->setAnchor(0.0f, 0.5f);
        }
        {
            vde::TextStyle style{
                .color = vde::Color::magenta(), .pixelScale = 1, .letterSpacing = 1};
            auto tex = vde::TextRenderer::createTexture(ctx, "(C) 2026 VDE", small, style);
            auto sprite = addEntity<vde::SpriteEntity>();
            float aspect =
                static_cast<float>(tex->getWidth()) / static_cast<float>(tex->getHeight());
            float h = 0.3f;
            sprite->setTexture(tex);
            sprite->setScale(h * aspect, h, 1.0f);
            sprite->setPosition(6.5f, -5.5f, 0.0f);
            sprite->setAnchor(1.0f, 0.5f);
        }
        {
            vde::TextStyle style{.color = vde::Color::white(), .pixelScale = 2, .letterSpacing = 2};
            auto tex = vde::TextRenderer::createTexture(ctx, "1UP", small, style);
            auto sprite = addEntity<vde::SpriteEntity>();
            float aspect =
                static_cast<float>(tex->getWidth()) / static_cast<float>(tex->getHeight());
            float h = 0.4f;
            sprite->setTexture(tex);
            sprite->setScale(h * aspect, h, 1.0f);
            sprite->setPosition(-6.0f, 5.5f, 0.0f);
            sprite->setAnchor(0.0f, 0.5f);
        }
    }

    void update(float deltaTime) override {
        BaseExampleScene::update(deltaTime);
        m_time += deltaTime;

        // Flash INSERT COIN (toggle visibility every 0.5s)
        if (m_insertCoinSprite) {
            bool visible = static_cast<int>(m_time * 2.0f) % 2 == 0;
            m_insertCoinSprite->setVisible(visible);
        }

        // Flash high-score table rows (staggered blink)
        for (size_t i = 0; i < m_scoreSprites.size(); ++i) {
            float phase = m_time * 3.0f + static_cast<float>(i) * 0.3f;
            bool visible = std::fmod(phase, 2.0f) < 1.6f;
            m_scoreSprites[i]->setVisible(visible);
        }

        // Scroll marquee from right to left
        if (m_marqueeSprite) {
            float scrollSpeed = 3.0f;
            auto pos = m_marqueeSprite->getPosition();
            pos.x -= scrollSpeed * deltaTime;
            // Reset once fully off-screen left
            if (pos.x + m_marqueeWidth < -8.5f) {
                pos.x = 8.5f;
            }
            m_marqueeSprite->setPosition(pos);
        }
    }

  protected:
    std::string getExampleName() const override { return "Pixel Arcade Demo"; }

    std::vector<std::string> getFeatures() const override {
        return {
            "BitmapFont::small() and BitmapFont::large() usage",
            "TextRenderer::createTexture() batch creation (~30 textures)",
            "Varied pixelScale (1-4x) and letterSpacing",
            "Per-letter color cycling (green, yellow, red, cyan)",
            "Flashing text and scrolling marquee animation",
        };
    }

    std::vector<std::string> getExpectedVisuals() const override {
        return {
            "Flashing yellow INSERT COIN banner at top",
            "HIGH SCORES title in cyan",
            "10 high-score entries in alternating white/gold",
            "PLAYER 1 READY in cycling green/yellow/red/cyan",
            "Scrolling instruction marquee at the bottom",
            "CREDIT 00 and (C) 2026 VDE at bottom corners",
        };
    }

  private:
    float m_time = 0.0f;
    std::shared_ptr<vde::SpriteEntity> m_insertCoinSprite;
    std::vector<std::shared_ptr<vde::SpriteEntity>> m_scoreSprites;
    std::vector<std::shared_ptr<vde::SpriteEntity>> m_playerReadySprites;
    std::shared_ptr<vde::SpriteEntity> m_marqueeSprite;
    float m_marqueeWidth = 0.0f;
};

// ---------------------------------------------------------------------------
// Game
// ---------------------------------------------------------------------------
class PixelArcadeDemo : public vde::examples::BaseExampleGame<ArcadeInputHandler, ArcadeScene> {};

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main(int argc, char** argv) {
    PixelArcadeDemo demo;
    return vde::examples::runExample(demo, "VDE Pixel Arcade Demo", 1280, 720, argc, argv);
}
