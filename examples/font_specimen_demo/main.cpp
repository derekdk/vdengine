/**
 * @file main.cpp
 * @brief Font Specimen Demo — side-by-side type specimen viewer.
 *
 * Demonstrates Phase 2 text rendering with:
 * - Left panel: BitmapFont::small() and ::large() full ASCII glyph grids
 * - Right panel: TrueType pangram at six sizes (10–96 px)
 * - Poster section: mixed TTF headline + pixel-font sub-line
 * - Tab key cycles TrueType render through three atlas sizes (24, 48, 72 px)
 */

#include <vde/Texture.h>
#include <vde/api/GameAPI.h>

#include <cmath>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "../ExampleBase.h"

// ---------------------------------------------------------------------------
// Input handler
// ---------------------------------------------------------------------------
class SpecimenInputHandler : public vde::examples::BaseExampleInputHandler {
  public:
    void onKeyPress(int key) override {
        BaseExampleInputHandler::onKeyPress(key);
        if (key == vde::KEY_TAB) {
            m_tabPressed = true;
        }
    }

    bool consumeTab() {
        bool v = m_tabPressed;
        m_tabPressed = false;
        return v;
    }

  private:
    bool m_tabPressed = false;
};

// ---------------------------------------------------------------------------
// Scene
// ---------------------------------------------------------------------------
class SpecimenScene : public vde::examples::BaseExampleScene {
  public:
    SpecimenScene() : BaseExampleScene() {}

    void onEnter() override {
        printExampleHeader();

        auto* camera = new vde::Camera2D(20.0f, 14.0f);
        camera->setPosition(0.0f, 0.0f);
        camera->setZoom(1.0f);
        setCamera(camera);

        setBackgroundColor(vde::Color::fromRGB8(30, 30, 40));

        m_ctx = getGame()->getVulkanContext();

        buildLeftPanel();

        // Pre-build all three TTF panel variants and poster headlines
        for (int i = 0; i < 3; ++i) {
            buildRightPanelVariant(i);
            buildPosterVariant(i);
        }

        // Show only the first variant
        setVariantVisible(0, true);
        setVariantVisible(1, false);
        setVariantVisible(2, false);

        // Pixel-font poster sub-line (always visible, shared across all variants)
        {
            const auto& small = vde::BitmapFont::small();
            vde::TextStyle style{
                .color = vde::Color::fromRGB8(160, 160, 180), .pixelScale = 2, .letterSpacing = 1};
            auto tex = vde::TextRenderer::createTexture(
                m_ctx, "PHASE 2: TTF BACKEND VIA STB_TRUETYPE", small, style);
            auto sprite = addEntity<vde::SpriteEntity>();
            float aspect =
                static_cast<float>(tex->getWidth()) / static_cast<float>(tex->getHeight());
            float h = 0.4f;
            sprite->setTexture(tex);
            sprite->setScale(h * aspect, h, 1.0f);
            sprite->setPosition(0.0f, -5.5f, 0.0f);
        }
    }

    void update(float deltaTime) override {
        BaseExampleScene::update(deltaTime);

        auto* input = dynamic_cast<SpecimenInputHandler*>(getInputHandler());
        if (input && input->consumeTab()) {
            setVariantVisible(m_fontIndex, false);
            m_fontIndex = (m_fontIndex + 1) % 3;
            setVariantVisible(m_fontIndex, true);
        }
    }

  protected:
    std::string getExampleName() const override { return "Font Specimen Demo"; }

    std::vector<std::string> getFeatures() const override {
        return {
            "BitmapFont full ASCII glyph grid (small + large)",
            "TrueType rendering at six sizes (10-96 px)",
            "Mixed TTF headline + pixel-font sub-line poster",
            "Tab cycles TTF atlas through 24/48/72 px sizes",
        };
    }

    std::vector<std::string> getExpectedVisuals() const override {
        return {
            "Left panel: two glyph grids (small 5x7, large 8x13)",
            "Right panel: pangram at increasing TTF sizes",
            "Bottom poster: large TTF title + small pixel sub-line",
        };
    }

    std::vector<std::string> getControls() const override { return {"TAB - Cycle TTF font size"}; }

  private:
    static constexpr int kVariantCount = 3;
    static constexpr float kAtlasSizes[kVariantCount] = {24.0f, 48.0f, 72.0f};

    void buildLeftPanel() {
        const auto& small = vde::BitmapFont::small();
        const auto& large = vde::BitmapFont::large();

        // --- Small font glyph grid ---
        {
            vde::TextStyle style{.color = vde::Color::cyan(), .pixelScale = 2, .letterSpacing = 1};
            auto tex = vde::TextRenderer::createTexture(m_ctx, "BITMAP 5x7", small, style);
            auto sprite = addEntity<vde::SpriteEntity>();
            float aspect =
                static_cast<float>(tex->getWidth()) / static_cast<float>(tex->getHeight());
            float h = 0.45f;
            sprite->setTexture(tex);
            sprite->setScale(h * aspect, h, 1.0f);
            sprite->setPosition(-6.5f, 6.0f, 0.0f);
            sprite->setAnchor(0.0f, 0.5f);
        }

        for (int row = 0; row < 6; ++row) {
            std::string line;
            for (int col = 0; col < 16; ++col) {
                int charIdx = row * 16 + col;
                char c = static_cast<char>(0x20 + charIdx);
                if (c > 0x7E)
                    break;
                line += c;
            }
            vde::TextStyle style{.color = vde::Color::white(), .pixelScale = 2, .letterSpacing = 1};
            auto tex = vde::TextRenderer::createTexture(m_ctx, line, small, style);
            auto sprite = addEntity<vde::SpriteEntity>();
            float aspect =
                static_cast<float>(tex->getWidth()) / static_cast<float>(tex->getHeight());
            float h = 0.4f;
            sprite->setTexture(tex);
            sprite->setScale(h * aspect, h, 1.0f);
            sprite->setPosition(-6.5f, 5.3f - row * 0.55f, 0.0f);
            sprite->setAnchor(0.0f, 0.5f);
        }

        // --- Large font glyph grid ---
        {
            vde::TextStyle style{.color = vde::Color::cyan(), .pixelScale = 2, .letterSpacing = 1};
            auto tex = vde::TextRenderer::createTexture(m_ctx, "BITMAP 8x13", small, style);
            auto sprite = addEntity<vde::SpriteEntity>();
            float aspect =
                static_cast<float>(tex->getWidth()) / static_cast<float>(tex->getHeight());
            float h = 0.45f;
            sprite->setTexture(tex);
            sprite->setScale(h * aspect, h, 1.0f);
            sprite->setPosition(-6.5f, 1.8f, 0.0f);
            sprite->setAnchor(0.0f, 0.5f);
        }

        for (int row = 0; row < 6; ++row) {
            std::string line;
            for (int col = 0; col < 16; ++col) {
                int charIdx = row * 16 + col;
                char c = static_cast<char>(0x20 + charIdx);
                if (c > 0x7E)
                    break;
                line += c;
            }
            vde::TextStyle style{
                .color = vde::Color::fromRGB8(200, 220, 255), .pixelScale = 1, .letterSpacing = 1};
            auto tex = vde::TextRenderer::createTexture(m_ctx, line, large, style);
            auto sprite = addEntity<vde::SpriteEntity>();
            float aspect =
                static_cast<float>(tex->getWidth()) / static_cast<float>(tex->getHeight());
            float h = 0.35f;
            sprite->setTexture(tex);
            sprite->setScale(h * aspect, h, 1.0f);
            sprite->setPosition(-6.5f, 1.1f - row * 0.50f, 0.0f);
            sprite->setAnchor(0.0f, 0.5f);
        }
    }

    void buildRightPanelVariant(int variantIdx) {
        auto& sprites = m_ttfVariants[variantIdx];

        // Load TTF font at this atlas size
        auto ttfFont = std::make_unique<vde::TrueTypeFont>();
        if (!ttfFont->loadFromFile(m_ctx, "assets/fonts/VDE_default.ttf",
                                   kAtlasSizes[variantIdx])) {
            std::cerr << "WARNING: Failed to load TTF font at " << kAtlasSizes[variantIdx]
                      << "px\n";
            return;
        }

        // Label
        {
            const auto& small = vde::BitmapFont::small();
            std::ostringstream label;
            label << "TTF " << kAtlasSizes[variantIdx] << "px";
            vde::TextStyle style{
                .color = vde::Color::yellow(), .pixelScale = 2, .letterSpacing = 1};
            auto tex = vde::TextRenderer::createTexture(m_ctx, label.str(), small, style);
            auto sprite = addEntity<vde::SpriteEntity>();
            float aspect =
                static_cast<float>(tex->getWidth()) / static_cast<float>(tex->getHeight());
            float h = 0.45f;
            sprite->setTexture(tex);
            sprite->setScale(h * aspect, h, 1.0f);
            sprite->setPosition(2.5f, 6.0f, 0.0f);
            sprite->setAnchor(0.0f, 0.5f);
            sprites.push_back(sprite);
        }

        // Pangram at six display sizes
        const std::string pangram = "The quick brown fox jumps over the lazy dog 0123456789";
        const float displaySizes[] = {0.3f, 0.45f, 0.6f, 0.8f, 1.0f, 1.3f};
        float y = 5.2f;

        for (int i = 0; i < 6; ++i) {
            std::string text = (i < 3) ? pangram : pangram.substr(0, 30);

            vde::TextStyle style;
            style.color = vde::Color::white();
            style.pixelScale = 1;
            style.letterSpacing = 0;

            auto tex = vde::TextRenderer::createTexture(m_ctx, text, *ttfFont, style);
            if (!tex || tex->getWidth() == 0 || tex->getHeight() == 0)
                continue;

            auto sprite = addEntity<vde::SpriteEntity>();
            float aspect =
                static_cast<float>(tex->getWidth()) / static_cast<float>(tex->getHeight());
            float h = displaySizes[i];
            float w = h * aspect;
            if (w > 7.5f) {
                w = 7.5f;
                h = w / aspect;
            }
            sprite->setTexture(tex);
            sprite->setScale(w, h, 1.0f);
            sprite->setPosition(2.5f, y, 0.0f);
            sprite->setAnchor(0.0f, 0.5f);
            sprites.push_back(sprite);
            y -= h + 0.15f;
        }

        // Keep font alive so textures referencing its atlas remain valid
        m_ttfFonts[variantIdx] = std::move(ttfFont);
    }

    void buildPosterVariant(int variantIdx) {
        if (!m_ttfFonts[variantIdx] || !m_ttfFonts[variantIdx]->isLoaded())
            return;

        vde::TextStyle style{
            .color = vde::Color::fromRGB8(255, 200, 100), .pixelScale = 1, .letterSpacing = 0};
        auto tex = vde::TextRenderer::createTexture(m_ctx, "VDE TEXT RENDERING",
                                                    *m_ttfFonts[variantIdx], style);
        if (!tex || tex->getWidth() == 0 || tex->getHeight() == 0)
            return;

        auto sprite = addEntity<vde::SpriteEntity>();
        float aspect = static_cast<float>(tex->getWidth()) / static_cast<float>(tex->getHeight());
        float h = 1.2f;
        float w = h * aspect;
        if (w > 18.0f) {
            w = 18.0f;
            h = w / aspect;
        }
        sprite->setTexture(tex);
        sprite->setScale(w, h, 1.0f);
        sprite->setPosition(0.0f, -4.5f, 0.0f);
        m_posterVariants[variantIdx] = sprite;
    }

    void setVariantVisible(int idx, bool visible) {
        for (auto& s : m_ttfVariants[idx]) {
            if (s)
                s->setVisible(visible);
        }
        if (m_posterVariants[idx]) {
            m_posterVariants[idx]->setVisible(visible);
        }
    }

    vde::VulkanContext* m_ctx = nullptr;
    int m_fontIndex = 0;

    // Pre-built sprite sets for each atlas size variant
    std::vector<std::shared_ptr<vde::SpriteEntity>> m_ttfVariants[kVariantCount];
    std::shared_ptr<vde::SpriteEntity> m_posterVariants[kVariantCount];
    std::unique_ptr<vde::TrueTypeFont> m_ttfFonts[kVariantCount];
};

// ---------------------------------------------------------------------------
// Game
// ---------------------------------------------------------------------------
class FontSpecimenDemo
    : public vde::examples::BaseExampleGame<SpecimenInputHandler, SpecimenScene> {};

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main(int argc, char** argv) {
    FontSpecimenDemo demo;
    return vde::examples::runExample(demo, "VDE Font Specimen Demo", 1280, 720, argc, argv);
}
