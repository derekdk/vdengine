/**
 * @file main.cpp
 * @brief Emoji demo — demonstrates color emoji rendering in engine text and ImGui.
 *
 * Shows:
 * - Color emoji loaded from the system emoji font (Segoe UI Emoji on Windows)
 * - Engine TextRenderer compositing emoji inline with regular TrueType text
 * - ImGui debug UI with emoji characters rendered via custom atlas glyphs
 * - Multiple emoji at different sizes
 */

#include <vde/api/EmojiFont.h>
#include <vde/api/TextEntity.h>
#include <vde/api/TextRenderer.h>
#include <vde/api/TrueTypeFont.h>

#include "../ExampleBase.h"

// ============================================================================
// Input Handler
// ============================================================================

class EmojiDemoInputHandler : public vde::examples::BaseExampleInputHandler {
  public:
    void onKeyPress(int key) override {
        BaseExampleInputHandler::onKeyPress(key);
        if (key == vde::KEY_TAB) {
            m_tabPressed = true;
        }
    }

    bool isTabPressed() {
        bool val = m_tabPressed;
        m_tabPressed = false;
        return val;
    }

  private:
    bool m_tabPressed = false;
};

// ============================================================================
// Scene
// ============================================================================

class EmojiDemoScene : public vde::examples::BaseExampleScene {
  public:
    EmojiDemoScene() : BaseExampleScene(15.0f) {}

    void onEnter() override {
        printExampleHeader();

        auto* game = getGame();
        auto* ctx = game ? game->getVulkanContext() : nullptr;
        if (!ctx) {
            std::cerr << "No VulkanContext available\n";
            return;
        }

        // Load TrueType font for regular text
        m_ttfFont = std::make_unique<vde::TrueTypeFont>();
        if (!m_ttfFont->loadFromFile(ctx, "assets/fonts/VDE_default.ttf", 32.0f)) {
            std::cerr << "Failed to load TrueType font: " << m_ttfFont->getLastError() << "\n";
            m_ttfFont.reset();
        }

        // Load color emoji font from system
        std::string emojiPath = vde::EmojiFont::findSystemEmojiFont();
        if (!emojiPath.empty()) {
            m_emojiFont = std::make_unique<vde::EmojiFont>();
            if (!m_emojiFont->loadFromFile(ctx, emojiPath, 32)) {
                std::cerr << "Failed to load emoji font: " << m_emojiFont->getLastError() << "\n";
                m_emojiFont.reset();
            }
        } else {
            std::cerr << "No system emoji font found\n";
        }

        buildScene(ctx);
    }

    void update(float deltaTime) override {
        BaseExampleScene::update(deltaTime);

        auto* input = dynamic_cast<EmojiDemoInputHandler*>(getInputHandler());
        if (input && input->isTabPressed()) {
            m_displayPage = (m_displayPage + 1) % 3;
            auto* game = getGame();
            if (game) {
                rebuildScene(game->getVulkanContext());
            }
        }
    }

#ifdef VDE_EXAMPLE_USE_IMGUI
    void drawDebugUI() override {
        BaseExampleScene::drawDebugUI();

        auto* game = getGame();
        if (!game)
            return;

        ImGui::SetNextWindowPos(ImVec2(10, 160), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Emoji in ImGui")) {
            ImGui::TextWrapped("Color emoji rendered via custom font atlas glyphs:");
            ImGui::Separator();

            // These emoji will render in color if the system emoji font was loaded
            ImGui::Text("Faces:  \xF0\x9F\x98\x80 \xF0\x9F\x98\x83 \xF0\x9F\x98\x84 "
                        "\xF0\x9F\x98\x81 \xF0\x9F\x98\x86");
            ImGui::Text("Hearts: \xE2\x9D\xA4 \xF0\x9F\x92\x9B \xF0\x9F\x92\x9A "
                        "\xF0\x9F\x92\x99 \xF0\x9F\x92\x9C");
            ImGui::Text("Hands:  \xF0\x9F\x91\x8D \xF0\x9F\x91\x8E \xF0\x9F\x91\x8B "
                        "\xE2\x9C\x8C");
            ImGui::Text("Nature: \xF0\x9F\x94\xA5 \xF0\x9F\x92\xA7 \xE2\xAD\x90 "
                        "\xE2\x98\x80 \xE2\x98\x81");
            ImGui::Text("Objects: \xF0\x9F\x8E\xAE \xF0\x9F\x8E\xAF \xF0\x9F\x8F\x86 "
                        "\xF0\x9F\x92\x8E");
            ImGui::Text("Food:   \xF0\x9F\x8D\x95 \xF0\x9F\x8D\x94 \xF0\x9F\x8D\x9F "
                        "\xF0\x9F\x8C\xAD \xF0\x9F\x8D\xA9");
            ImGui::Text("Travel: \xF0\x9F\x9A\x80 \xF0\x9F\x9A\x97 \xE2\x9C\x88 "
                        "\xF0\x9F\x9A\xA2 \xF0\x9F\x9A\x82");

            ImGui::Separator();
            ImGui::Text("Mixed text: Hello \xF0\x9F\x91\x8B World \xF0\x9F\x8C\x8D!");

            if (m_emojiFont) {
                ImGui::Separator();
                ImGui::TextColored(ImVec4(0.5f, 0.8f, 0.5f, 1.0f),
                                   "Emoji atlas: %dx%d (%zu glyphs)", m_emojiFont->atlasWidth(),
                                   m_emojiFont->atlasHeight(),
                                   m_emojiFont->getAvailableCodepoints().size());
            } else {
                ImGui::Separator();
                ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "No system emoji font found");
            }
        }
        ImGui::End();
    }
#endif

  protected:
    std::string getExampleName() const override { return "Color Emoji Rendering"; }

    std::vector<std::string> getFeatures() const override {
        return {
            "Color emoji loaded from system font (COLR/CPAL)",
            "Engine TextRenderer compositing emoji inline with TrueType text",
            "ImGui color emoji via custom font atlas glyphs",
            "UTF-8 text processing",
        };
    }

    std::vector<std::string> getExpectedVisuals() const override {
        return {
            "Engine text labels with inline color emoji on the left",
            "ImGui window with color emoji text on the right",
            "Multiple emoji categories (faces, hearts, objects, etc.)",
        };
    }

    std::vector<std::string> getControls() const override {
        return {
            "TAB  - Cycle display page",
        };
    }

  private:
    void buildScene(vde::VulkanContext* ctx) {
        if (!ctx || !m_ttfFont || !m_ttfFont->isLoaded())
            return;

        const vde::TextStyle whiteStyle = {.color = vde::Color::white(), .pixelScale = 1};

        // Page 0: Basic emoji text
        // Page 1: Mixed text+emoji
        // Page 2: Emoji categories

        float yPos = 4.0f;
        float yStep = -1.5f;

        switch (m_displayPage) {
        case 0:
            addTextLabel(ctx, "Page 1: Emoji Faces", whiteStyle, 0.0f, yPos);
            yPos += yStep;
            addEmojiLabel(ctx, "Smiley: \xF0\x9F\x98\x80\xF0\x9F\x98\x83\xF0\x9F\x98\x84",
                          whiteStyle, 0.0f, yPos);
            yPos += yStep;
            addEmojiLabel(ctx, "Hearts: \xE2\x9D\xA4\xF0\x9F\x92\x9B\xF0\x9F\x92\x99", whiteStyle,
                          0.0f, yPos);
            yPos += yStep;
            addEmojiLabel(ctx, "Stars: \xE2\xAD\x90\xF0\x9F\x8C\x9F", whiteStyle, 0.0f, yPos);
            break;
        case 1:
            addTextLabel(ctx, "Page 2: Mixed Text", whiteStyle, 0.0f, yPos);
            yPos += yStep;
            addEmojiLabel(ctx, "Hello \xF0\x9F\x91\x8B World!", whiteStyle, 0.0f, yPos);
            yPos += yStep;
            addEmojiLabel(ctx, "Game Over \xF0\x9F\x8E\xAE", whiteStyle, 0.0f, yPos);
            yPos += yStep;
            addEmojiLabel(ctx, "You Win! \xF0\x9F\x8F\x86", whiteStyle, 0.0f, yPos);
            break;
        case 2:
            addTextLabel(ctx, "Page 3: Categories", whiteStyle, 0.0f, yPos);
            yPos += yStep;
            addEmojiLabel(ctx, "Fire \xF0\x9F\x94\xA5 Water \xF0\x9F\x92\xA7", whiteStyle, 0.0f,
                          yPos);
            yPos += yStep;
            addEmojiLabel(ctx, "Rocket \xF0\x9F\x9A\x80 Car \xF0\x9F\x9A\x97", whiteStyle, 0.0f,
                          yPos);
            yPos += yStep;
            addEmojiLabel(ctx, "Pizza \xF0\x9F\x8D\x95 Burger \xF0\x9F\x8D\x94", whiteStyle, 0.0f,
                          yPos);
            break;
        }
    }

    void rebuildScene(vde::VulkanContext* ctx) {
        // Remove old labels
        for (auto& e : m_labels) {
            if (e) {
                removeEntity(e->getId());
            }
        }
        m_labels.clear();
        buildScene(ctx);
    }

    void addTextLabel(vde::VulkanContext* ctx, const std::string& text, const vde::TextStyle& style,
                      float x, float y) {
        if (!m_ttfFont || !m_ttfFont->isLoaded())
            return;
        auto tex = vde::TextRenderer::createTexture(ctx, text, *m_ttfFont, style);
        auto entity = addEntity<vde::SpriteEntity>();
        entity->setTexture(tex);
        entity->setPosition(x, y, 0.0f);
        entity->setScale(0.01f);
        m_labels.push_back(entity);
    }

    void addEmojiLabel(vde::VulkanContext* ctx, const std::string& utf8Text,
                       const vde::TextStyle& style, float x, float y) {
        if (!m_ttfFont || !m_ttfFont->isLoaded())
            return;
        auto tex =
            vde::TextRenderer::createTexture(ctx, utf8Text, *m_ttfFont, m_emojiFont.get(), style);
        auto entity = addEntity<vde::SpriteEntity>();
        entity->setTexture(tex);
        entity->setPosition(x, y, 0.0f);
        entity->setScale(0.01f);
        m_labels.push_back(entity);
    }

    std::unique_ptr<vde::TrueTypeFont> m_ttfFont;
    std::unique_ptr<vde::EmojiFont> m_emojiFont;
    std::vector<std::shared_ptr<vde::SpriteEntity>> m_labels;
    int m_displayPage = 0;
};

// ============================================================================
// Game
// ============================================================================

class EmojiDemoGame : public vde::examples::BaseExampleGame<EmojiDemoInputHandler, EmojiDemoScene> {
  public:
    EmojiDemoGame() = default;
};

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    EmojiDemoGame game;
    return vde::examples::runExample(game, "Emoji Demo", 1280, 720, argc, argv);
}
