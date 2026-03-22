/**
 * @file main.cpp
 * @brief Text Metrics Demo — demonstrates text layout, alignment, wrapping, and fit-to-rect.
 *
 * Showcases the VDE text rendering API with correct font metrics:
 * - Horizontal alignment: left, center, right
 * - Vertical alignment: top, center, bottom
 * - Word wrapping within a bounded rectangle
 * - Fit-to-rectangle scaling
 * - Animated container resizing with text accommodating changes
 * - ImGui debug controls for interactive manipulation
 *
 * Controls:
 *   F1    - Toggle debug UI (ImGui panels)
 *   SPACE - Pause/resume animation
 *   ESC   - Exit
 *   F     - Fail test (if visuals are incorrect)
 */

#include <vde/Texture.h>
#include <vde/api/GameAPI.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "../ExampleBase.h"

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
static constexpr float kCameraWidth = 24.0f;
static constexpr float kCameraHeight = 16.0f;
static constexpr float kBoxBorderThickness = 0.04f;

// ---------------------------------------------------------------------------
// Font enumeration
// ---------------------------------------------------------------------------

struct FontEntry {
    std::string displayName;  ///< Friendly name shown in the UI
    std::string path;         ///< Absolute path to the .ttf file
};

static std::string toLowerCopy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

/// Scan common font directories for .ttf files and return sorted list.
static std::vector<FontEntry> enumerateSystemFonts() {
    std::vector<FontEntry> fonts;

    // Gather candidate directories
    std::vector<std::filesystem::path> dirs;
#ifdef _WIN32
    if (const char* windir = std::getenv("WINDIR")) {
        dirs.emplace_back(std::filesystem::path(windir) / "Fonts");
    }
    if (const char* localAppData = std::getenv("LOCALAPPDATA")) {
        dirs.emplace_back(std::filesystem::path(localAppData) / "Microsoft" / "Windows" / "Fonts");
    }
#else
    dirs.emplace_back("/usr/share/fonts");
    dirs.emplace_back("/usr/local/share/fonts");
    if (const char* home = std::getenv("HOME")) {
        dirs.emplace_back(std::filesystem::path(home) / ".fonts");
    }
#endif

    for (auto& dir : dirs) {
        std::error_code ec;
        if (!std::filesystem::is_directory(dir, ec))
            continue;

        for (auto& entry : std::filesystem::directory_iterator(
                 dir, std::filesystem::directory_options::skip_permission_denied, ec)) {
            if (ec)
                break;
            if (!entry.is_regular_file(ec))
                continue;
            auto ext = toLowerCopy(entry.path().extension().string());
            if (ext == ".ttf") {
                FontEntry fe;
                fe.path = entry.path().string();
                fe.displayName = entry.path().stem().string();
                fonts.push_back(std::move(fe));
            }
        }
    }

    // Sort by display name (case-insensitive)
    std::sort(fonts.begin(), fonts.end(), [](const FontEntry& a, const FontEntry& b) {
        return toLowerCopy(a.displayName) < toLowerCopy(b.displayName);
    });

    return fonts;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/// Create a solid-color 4x4 texture (used for box backgrounds and borders).
static std::shared_ptr<vde::Texture> createSolidTexture(vde::VulkanContext* ctx, uint8_t r,
                                                        uint8_t g, uint8_t b, uint8_t a = 255) {
    std::vector<uint8_t> pixels(4 * 4 * 4);
    for (size_t i = 0; i < 4 * 4; ++i) {
        pixels[i * 4 + 0] = r;
        pixels[i * 4 + 1] = g;
        pixels[i * 4 + 2] = b;
        pixels[i * 4 + 3] = a;
    }
    auto tex = std::make_shared<vde::Texture>();
    tex->loadFromData(pixels.data(), 4, 4);
    tex->uploadToGPU(ctx);
    return tex;
}

/// Measure text width in pixels using TrueTypeFont glyph metrics.
static float measureTextWidthPx(const vde::TrueTypeFont& font, const std::string& text) {
    float width = 0.0f;
    for (char c : text) {
        if (auto* g = font.getGlyph(c)) {
            width += g->advanceX;
        }
    }
    return width;
}

/// Word-wrap text into lines that fit within maxWidthPx pixels.
static std::vector<std::string> wordWrap(const vde::TrueTypeFont& font, const std::string& text,
                                         float maxWidthPx) {
    std::vector<std::string> lines;
    std::istringstream stream(text);
    std::string word;
    std::string currentLine;

    while (stream >> word) {
        std::string testLine = currentLine.empty() ? word : currentLine + " " + word;
        if (measureTextWidthPx(font, testLine) > maxWidthPx && !currentLine.empty()) {
            lines.push_back(currentLine);
            currentLine = word;
        } else {
            currentLine = testLine;
        }
    }
    if (!currentLine.empty()) {
        lines.push_back(currentLine);
    }
    if (lines.empty()) {
        lines.push_back("");
    }
    return lines;
}

// ---------------------------------------------------------------------------
// A visual box: background fill + border outline made of sprites
// ---------------------------------------------------------------------------
struct VisualBox {
    std::shared_ptr<vde::SpriteEntity> background;
    std::shared_ptr<vde::SpriteEntity> borderTop;
    std::shared_ptr<vde::SpriteEntity> borderBottom;
    std::shared_ptr<vde::SpriteEntity> borderLeft;
    std::shared_ptr<vde::SpriteEntity> borderRight;

    /// Update the box position and size. (x,y) is box center.
    void update(float x, float y, float w, float h) {
        float halfW = w * 0.5f;
        float halfH = h * 0.5f;
        float bt = kBoxBorderThickness;

        if (background) {
            background->setPosition(x, y, -0.02f);
            background->setScale(w, h, 1.0f);
        }
        if (borderTop) {
            borderTop->setPosition(x, y + halfH, -0.01f);
            borderTop->setScale(w + bt, bt, 1.0f);
        }
        if (borderBottom) {
            borderBottom->setPosition(x, y - halfH, -0.01f);
            borderBottom->setScale(w + bt, bt, 1.0f);
        }
        if (borderLeft) {
            borderLeft->setPosition(x - halfW, y, -0.01f);
            borderLeft->setScale(bt, h + bt, 1.0f);
        }
        if (borderRight) {
            borderRight->setPosition(x + halfW, y, -0.01f);
            borderRight->setScale(bt, h + bt, 1.0f);
        }
    }
};

// ---------------------------------------------------------------------------
// Input handler
// ---------------------------------------------------------------------------
class TextMetricsInputHandler : public vde::examples::BaseExampleInputHandler {
  public:
    void onKeyPress(int key) override {
        BaseExampleInputHandler::onKeyPress(key);
        if (key == vde::KEY_SPACE) {
            m_spacePressed = true;
        }
    }

    bool consumeSpace() {
        bool v = m_spacePressed;
        m_spacePressed = false;
        return v;
    }

  private:
    bool m_spacePressed = false;
};

// ---------------------------------------------------------------------------
// Scene
// ---------------------------------------------------------------------------
class TextMetricsScene : public vde::examples::BaseExampleScene {
  public:
    TextMetricsScene() : BaseExampleScene() {}

    void onEnter() override {
        printExampleHeader();

        auto* camera = new vde::Camera2D(kCameraWidth, kCameraHeight);
        camera->setPosition(0.0f, 0.0f);
        camera->setZoom(1.0f);
        setCamera(camera);

        setBackgroundColor(vde::Color::fromRGB8(25, 25, 35));

        m_ctx = getGame()->getVulkanContext();

        // Enumerate available system fonts
        m_systemFonts = enumerateSystemFonts();

        // Insert the bundled VDE font at position 0 so it's always available
        m_fontPaths.push_back("assets/fonts/VDE_default.ttf");
        m_fontNames.push_back("VDE_default (bundled)");
        for (auto& fe : m_systemFonts) {
            m_fontPaths.push_back(fe.path);
            m_fontNames.push_back(fe.displayName);
        }
        m_selectedFontIdx = 0;

        std::cout << "Found " << m_systemFonts.size() << " system TTF fonts." << std::endl;

        // Load TrueType font
        m_ttfFont = std::make_unique<vde::TrueTypeFont>();
        if (!m_ttfFont->loadFromFile(m_ctx, m_fontPaths[0], m_ttfSizePx)) {
            std::cerr << "ERROR: Failed to load TTF font. Demo requires TrueType font.\n";
            if (getGame())
                getGame()->quit();
            return;
        }

        // Create shared solid textures
        m_whiteTex = createSolidTexture(m_ctx, 255, 255, 255);
        m_bgTex = createSolidTexture(m_ctx, 40, 42, 58);
        m_borderTex = createSolidTexture(m_ctx, 100, 120, 180);

        // Build all panels
        buildAlignmentPanel();
        buildWrapPanel();
        buildFitPanel();
        buildSectionLabels();
    }

    void update(float deltaTime) override {
        BaseExampleScene::update(deltaTime);

        auto* input = dynamic_cast<TextMetricsInputHandler*>(getInputHandler());
        if (input && input->consumeSpace()) {
            m_paused = !m_paused;
        }

        if (!m_paused) {
            m_time += deltaTime * m_animSpeed;
        }

        // Animate wrap panel box width
        float wrapWidthNow = m_wrapBaseWidth + m_wrapWidthAmplitude * std::sin(m_time * 0.8f);
        wrapWidthNow = std::max(wrapWidthNow, 2.0f);

        float wrapHeightNow = m_wrapBaseHeight + m_wrapHeightAmplitude * std::sin(m_time * 0.5f);
        wrapHeightNow = std::max(wrapHeightNow, 1.5f);

        if (std::abs(wrapWidthNow - m_wrapCurrentWidth) > 0.05f ||
            std::abs(wrapHeightNow - m_wrapCurrentHeight) > 0.05f || m_wrapDirty) {
            m_wrapCurrentWidth = wrapWidthNow;
            m_wrapCurrentHeight = wrapHeightNow;
            updateWrapPanel();
            m_wrapDirty = false;
        }

        // Animate fit panel box
        float fitW = m_fitBaseWidth + m_fitWidthAmplitude * std::sin(m_time * 1.2f);
        float fitH = m_fitBaseHeight + m_fitHeightAmplitude * std::sin(m_time * 0.7f);
        fitW = std::max(fitW, 1.5f);
        fitH = std::max(fitH, 1.0f);

        if (std::abs(fitW - m_fitCurrentWidth) > 0.02f ||
            std::abs(fitH - m_fitCurrentHeight) > 0.02f || m_fitDirty) {
            m_fitCurrentWidth = fitW;
            m_fitCurrentHeight = fitH;
            updateFitPanel();
            m_fitDirty = false;
        }

        // Animate alignment panel boxes
        float alignH = m_alignBaseHeight + m_alignHeightAmplitude * std::sin(m_time * 0.6f);
        alignH = std::max(alignH, 2.0f);

        if (std::abs(alignH - m_alignCurrentHeight) > 0.02f || m_alignDirty) {
            m_alignCurrentHeight = alignH;
            updateAlignmentPanel();
            m_alignDirty = false;
        }
    }

    // --- ImGui debug UI ---
    void drawDebugUI() override {
#ifdef VDE_EXAMPLE_USE_IMGUI
        auto* game = getGame();
        float scale = game ? game->getDPIScale() : 1.0f;

        ImGui::SetNextWindowPos(ImVec2(10 * scale, 10 * scale), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(320 * scale, 520 * scale), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Text Metrics Controls")) {
            // --- Engine Stats ---
            if (ImGui::CollapsingHeader("Engine Stats", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Text("FPS: %.1f", game ? game->getFPS() : 0.0f);
                ImGui::Text("Frame: %llu", game ? game->getFrameCount() : 0ULL);
                ImGui::Text("Time: %.1f s", m_time);
            }

            // --- Font Selection ---
            if (ImGui::CollapsingHeader("Font", ImGuiTreeNodeFlags_DefaultOpen)) {
                // Build a combined string for the combo (display names only)
                // We use a listbox with filter for large font lists
                ImGui::Text("Fonts: %d", static_cast<int>(m_fontNames.size()));

                // Filter input
                ImGui::InputTextWithHint("##filter", "Filter fonts...", m_fontFilter,
                                         sizeof(m_fontFilter));

                std::string filterLower = toLowerCopy(m_fontFilter);

                // Listbox with filtered font names
                if (ImGui::BeginListBox("##fontlist", ImVec2(-FLT_MIN, 150 * scale))) {
                    for (int i = 0; i < static_cast<int>(m_fontNames.size()); ++i) {
                        // Apply filter
                        if (!filterLower.empty()) {
                            std::string nameLower = toLowerCopy(m_fontNames[i]);
                            if (nameLower.find(filterLower) == std::string::npos)
                                continue;
                        }

                        bool selected = (i == m_selectedFontIdx);
                        if (ImGui::Selectable(m_fontNames[i].c_str(), selected)) {
                            if (i != m_selectedFontIdx) {
                                m_selectedFontIdx = i;
                                deferCommand([this]() { reloadFont(); });
                            }
                        }
                        if (selected) {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndListBox();
                }

                // Font size slider
                if (ImGui::SliderFloat("Size (px)", &m_ttfSizePx, 12.0f, 96.0f, "%.0f")) {
                    deferCommand([this]() { reloadFont(); });
                }

                // Show current font path
                ImGui::TextWrapped("Path: %s", m_fontPaths[m_selectedFontIdx].c_str());
            }

            // --- Animation ---
            if (ImGui::CollapsingHeader("Animation", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Checkbox("Paused (SPACE)", &m_paused);
                ImGui::SliderFloat("Speed", &m_animSpeed, 0.1f, 3.0f);
                if (ImGui::Button("Reset Time")) {
                    m_time = 0.0f;
                }
            }

            // --- Alignment Panel ---
            if (ImGui::CollapsingHeader("Alignment Boxes", ImGuiTreeNodeFlags_DefaultOpen)) {
                if (ImGui::SliderFloat("Box Height##align", &m_alignBaseHeight, 1.5f, 5.0f)) {
                    m_alignDirty = true;
                }
                if (ImGui::SliderFloat("Height Amplitude##align", &m_alignHeightAmplitude, 0.0f,
                                       2.0f)) {
                    m_alignDirty = true;
                }
            }

            // --- Wrap Panel ---
            if (ImGui::CollapsingHeader("Word Wrap Box", ImGuiTreeNodeFlags_DefaultOpen)) {
                if (ImGui::SliderFloat("Base Width##wrap", &m_wrapBaseWidth, 3.0f, 12.0f)) {
                    m_wrapDirty = true;
                }
                if (ImGui::SliderFloat("Width Amplitude##wrap", &m_wrapWidthAmplitude, 0.0f,
                                       4.0f)) {
                    m_wrapDirty = true;
                }
                if (ImGui::SliderFloat("Base Height##wrap", &m_wrapBaseHeight, 2.0f, 8.0f)) {
                    m_wrapDirty = true;
                }
                if (ImGui::SliderFloat("Height Amplitude##wrap", &m_wrapHeightAmplitude, 0.0f,
                                       3.0f)) {
                    m_wrapDirty = true;
                }
            }

            // --- Fit Panel ---
            if (ImGui::CollapsingHeader("Fit-to-Rect Box", ImGuiTreeNodeFlags_DefaultOpen)) {
                if (ImGui::SliderFloat("Base Width##fit", &m_fitBaseWidth, 2.0f, 10.0f)) {
                    m_fitDirty = true;
                }
                if (ImGui::SliderFloat("Width Amplitude##fit", &m_fitWidthAmplitude, 0.0f, 3.0f)) {
                    m_fitDirty = true;
                }
                if (ImGui::SliderFloat("Base Height##fit", &m_fitBaseHeight, 1.5f, 5.0f)) {
                    m_fitDirty = true;
                }
                if (ImGui::SliderFloat("Height Amplitude##fit", &m_fitHeightAmplitude, 0.0f,
                                       2.0f)) {
                    m_fitDirty = true;
                }
            }

            // --- Text Colors ---
            if (ImGui::CollapsingHeader("Colors")) {
                if (ImGui::ColorEdit3("Text##col", m_textColor)) {
                    deferCommand([this]() { rebuildAll(); });
                }
                if (ImGui::ColorEdit3("Box BG##col", m_boxBgColor)) {
                    applyBoxColors();
                }
                if (ImGui::ColorEdit3("Border##col", m_borderColor)) {
                    applyBoxColors();
                }
            }
        }
        ImGui::End();
#endif
    }

  protected:
    std::string getExampleName() const override { return "Text Metrics Demo"; }

    std::vector<std::string> getFeatures() const override {
        return {
            "Horizontal alignment: left, center, right",
            "Vertical alignment: top, center, bottom",
            "Word wrapping within bounded rectangles",
            "Fit-to-rectangle text scaling",
            "Animated container resizing",
            "ImGui debug controls for manipulation",
        };
    }

    std::vector<std::string> getExpectedVisuals() const override {
        return {
            "Top row: three boxes with left / center / right aligned text",
            "Each alignment box also shows top, center, and bottom vertical alignment",
            "Bottom-left: a box with word-wrapped text that reflows as box animates",
            "Bottom-right: text that scales to fill an animated rectangle",
            "All boxes animate their size smoothly",
        };
    }

    std::vector<std::string> getControls() const override {
        return {
            "SPACE - Pause/resume animation",
        };
    }

  private:
    vde::VulkanContext* m_ctx = nullptr;
    std::unique_ptr<vde::TrueTypeFont> m_ttfFont;
    std::shared_ptr<vde::Texture> m_whiteTex;
    std::shared_ptr<vde::Texture> m_bgTex;
    std::shared_ptr<vde::Texture> m_borderTex;
    float m_ttfSizePx = 32.0f;

    // Font enumeration
    std::vector<FontEntry> m_systemFonts;
    std::vector<std::string> m_fontPaths;
    std::vector<std::string> m_fontNames;
    int m_selectedFontIdx = 0;
    int m_lastWorkingFontIdx = 0;
    char m_fontFilter[128] = {};

    float m_time = 0.0f;
    bool m_paused = false;
    float m_animSpeed = 1.0f;

    // Colors (ImGui-editable)
    float m_textColor[3] = {1.0f, 1.0f, 1.0f};
    float m_boxBgColor[3] = {0.16f, 0.165f, 0.227f};
    float m_borderColor[3] = {0.39f, 0.47f, 0.71f};

    // =====================================================================
    // ALIGNMENT PANEL (top row: three boxes)
    // =====================================================================
    static constexpr float kAlignBoxWidth = 6.0f;
    static constexpr float kAlignY = 3.5f;
    static constexpr float kAlignBoxSpacing = 0.6f;

    float m_alignBaseHeight = 3.5f;
    float m_alignHeightAmplitude = 1.0f;
    float m_alignCurrentHeight = 3.5f;
    bool m_alignDirty = false;

    // Three boxes: left-align, center-align, right-align
    VisualBox m_alignBoxes[3];

    // Text sprites for alignment demo:
    // Each box has 3 lines: top-aligned, center-aligned, bottom-aligned
    // [box][vertical_position]
    struct AlignTextSprite {
        std::shared_ptr<vde::SpriteEntity> sprite;
        std::shared_ptr<vde::Texture> texture;
        float aspectRatio = 1.0f;
    };
    AlignTextSprite m_alignTexts[3][3];  // [hAlign][vAlign]
    // Also a label sprite per box
    std::shared_ptr<vde::SpriteEntity> m_alignLabels[3];

    void buildAlignmentPanel() {
        const char* labels[] = {"LEFT ALIGN", "CENTER ALIGN", "RIGHT ALIGN"};
        const char* lines[] = {"Top line", "Middle line", "Bottom line"};

        for (int h = 0; h < 3; ++h) {
            // Create box
            m_alignBoxes[h] = createBox();

            // Create label (bitmap font, small, above box)
            {
                const auto& small = vde::BitmapFont::small();
                vde::TextStyle style{
                    .color = vde::Color::yellow(), .pixelScale = 2, .letterSpacing = 1};
                auto tex = vde::TextRenderer::createTexture(m_ctx, labels[h], small, style);
                auto sprite = addEntity<vde::SpriteEntity>();
                sprite->setTexture(tex);
                sprite->setAnchor(0.5f, 0.5f);
                m_alignLabels[h] = sprite;
            }

            // Create three text lines per box (top, middle, bottom)
            for (int v = 0; v < 3; ++v) {
                vde::TextStyle style;
                style.color = vde::Color(m_textColor[0], m_textColor[1], m_textColor[2]);
                style.pixelScale = 1;
                style.letterSpacing = 0;

                auto tex = vde::TextRenderer::createTexture(m_ctx, lines[v], *m_ttfFont, style);
                auto sprite = addEntity<vde::SpriteEntity>();
                sprite->setTexture(tex);
                sprite->setAnchor(0.5f, 0.5f);  // will be adjusted per alignment

                m_alignTexts[h][v].sprite = sprite;
                m_alignTexts[h][v].texture = tex;
                float w = static_cast<float>(tex->getWidth());
                float ht = static_cast<float>(tex->getHeight());
                m_alignTexts[h][v].aspectRatio = (ht > 0) ? w / ht : 1.0f;
            }
        }

        updateAlignmentPanel();
    }

    void updateAlignmentPanel() {
        float boxW = kAlignBoxWidth;
        float boxH = m_alignCurrentHeight;
        float totalW = 3.0f * boxW + 2.0f * kAlignBoxSpacing;
        float startX = -totalW * 0.5f + boxW * 0.5f;

        float textHeight = 0.45f;  // world-unit height for each text line
        float padding = 0.15f;

        for (int h = 0; h < 3; ++h) {
            float cx = startX + h * (boxW + kAlignBoxSpacing);
            float cy = kAlignY;

            m_alignBoxes[h].update(cx, cy, boxW, boxH);

            // Position label above box
            if (m_alignLabels[h]) {
                auto tex = m_alignLabels[h]->getTexture();
                if (tex) {
                    float aspect =
                        static_cast<float>(tex->getWidth()) / static_cast<float>(tex->getHeight());
                    float lh = 0.35f;
                    m_alignLabels[h]->setScale(lh * aspect, lh, 1.0f);
                    m_alignLabels[h]->setPosition(cx, cy + boxH * 0.5f + 0.35f, 0.0f);
                }
            }

            // Position text lines
            for (int v = 0; v < 3; ++v) {
                auto& at = m_alignTexts[h][v];
                if (!at.sprite)
                    continue;

                float tw = textHeight * at.aspectRatio;
                at.sprite->setScale(tw, textHeight, 1.0f);

                // Horizontal alignment
                float tx;
                float anchorX;
                switch (h) {
                case 0:  // left
                    tx = cx - boxW * 0.5f + padding;
                    anchorX = 0.0f;
                    break;
                case 1:  // center
                    tx = cx;
                    anchorX = 0.5f;
                    break;
                case 2:  // right
                    tx = cx + boxW * 0.5f - padding;
                    anchorX = 1.0f;
                    break;
                default:
                    tx = cx;
                    anchorX = 0.5f;
                    break;
                }

                // Vertical alignment
                float ty;
                float anchorY;
                switch (v) {
                case 0:  // top
                    ty = cy + boxH * 0.5f - padding;
                    anchorY = 1.0f;
                    break;
                case 1:  // center
                    ty = cy;
                    anchorY = 0.5f;
                    break;
                case 2:  // bottom
                    ty = cy - boxH * 0.5f + padding;
                    anchorY = 0.0f;
                    break;
                default:
                    ty = cy;
                    anchorY = 0.5f;
                    break;
                }

                at.sprite->setAnchor(anchorX, anchorY);
                at.sprite->setPosition(tx, ty, 0.0f);
            }
        }
    }

    // =====================================================================
    // WORD WRAP PANEL (bottom-left)
    // =====================================================================
    static constexpr float kWrapCenterX = -4.5f;
    static constexpr float kWrapCenterY = -3.5f;

    float m_wrapBaseWidth = 7.0f;
    float m_wrapWidthAmplitude = 2.5f;
    float m_wrapBaseHeight = 4.5f;
    float m_wrapHeightAmplitude = 1.0f;
    float m_wrapCurrentWidth = 7.0f;
    float m_wrapCurrentHeight = 4.5f;
    bool m_wrapDirty = false;

    VisualBox m_wrapBox;
    std::shared_ptr<vde::SpriteEntity> m_wrapLabel;
    std::vector<std::shared_ptr<vde::SpriteEntity>> m_wrapLineSprites;
    size_t m_wrapActiveCount = 0;
    std::vector<std::string> m_wrapCachedLines;
    std::unordered_map<std::string, std::shared_ptr<vde::Texture>> m_wrapTextureCache;

    const std::string m_wrapText =
        "The quick brown fox jumps over the lazy dog. "
        "Text wrapping adjusts automatically as the container resizes, "
        "demonstrating proper word-break behavior with correct font metrics.";

    void buildWrapPanel() {
        m_wrapBox = createBox();

        // Label
        {
            const auto& small = vde::BitmapFont::small();
            vde::TextStyle style{.color = vde::Color::cyan(), .pixelScale = 2, .letterSpacing = 1};
            auto tex = vde::TextRenderer::createTexture(m_ctx, "WORD WRAP", small, style);
            auto sprite = addEntity<vde::SpriteEntity>();
            sprite->setTexture(tex);
            sprite->setAnchor(0.5f, 0.5f);
            m_wrapLabel = sprite;
        }

        m_wrapCurrentWidth = m_wrapBaseWidth;
        m_wrapCurrentHeight = m_wrapBaseHeight;
        updateWrapPanel();
    }

    void updateWrapPanel() {
        float boxW = m_wrapCurrentWidth;
        float boxH = m_wrapCurrentHeight;
        float cx = kWrapCenterX;
        float cy = kWrapCenterY;

        m_wrapBox.update(cx, cy, boxW, boxH);

        // Position label
        if (m_wrapLabel) {
            auto tex = m_wrapLabel->getTexture();
            if (tex) {
                float aspect =
                    static_cast<float>(tex->getWidth()) / static_cast<float>(tex->getHeight());
                float lh = 0.35f;
                m_wrapLabel->setScale(lh * aspect, lh, 1.0f);
                m_wrapLabel->setPosition(cx, cy + boxH * 0.5f + 0.35f, 0.0f);
            }
        }

        // Calculate text line height in world units
        float lineWorldH = 0.4f;
        float padding = 0.15f;
        float innerW = boxW - 2.0f * padding;

        // Convert inner width to pixel space for word wrapping
        // The font is rendered at m_ttfSizePx. A rendered line that is
        // lineWorldH tall in world units corresponds to lineWorldH * (fontSize / lineWorldH)
        // ratio. We need: pixelsPerWorldUnit = fontSize / lineWorldH
        float pixelsPerWorldUnit = m_ttfFont->fontSize() / lineWorldH;
        float maxWidthPx = innerW * pixelsPerWorldUnit;

        // Word-wrap
        auto newLines = wordWrap(*m_ttfFont, m_wrapText, maxWidthPx);

        // Check if lines actually changed
        bool linesChanged = (newLines.size() != m_wrapCachedLines.size());
        if (!linesChanged) {
            for (size_t i = 0; i < newLines.size(); ++i) {
                if (newLines[i] != m_wrapCachedLines[i]) {
                    linesChanged = true;
                    break;
                }
            }
        }

        if (linesChanged) {
            m_wrapCachedLines = newLines;

            vde::TextStyle style;
            style.color = vde::Color(m_textColor[0], m_textColor[1], m_textColor[2]);
            style.pixelScale = 1;
            style.letterSpacing = 0;

            // Reuse pooled sprites; grow pool if needed; cache textures by content
            size_t activeCount = 0;
            for (auto& line : newLines) {
                if (line.empty())
                    continue;

                // Grow sprite pool if needed
                if (activeCount >= m_wrapLineSprites.size()) {
                    auto sprite = addEntity<vde::SpriteEntity>();
                    sprite->setAnchor(0.0f, 1.0f);
                    m_wrapLineSprites.push_back(sprite);
                }

                // Get or create cached texture for this line content
                auto& cachedTex = m_wrapTextureCache[line];
                if (!cachedTex) {
                    cachedTex = vde::TextRenderer::createTexture(m_ctx, line, *m_ttfFont, style);
                }
                m_wrapLineSprites[activeCount]->setTexture(cachedTex);
                ++activeCount;
            }
            m_wrapActiveCount = activeCount;

            // Hide excess pooled sprites
            for (size_t i = activeCount; i < m_wrapLineSprites.size(); ++i) {
                m_wrapLineSprites[i]->setVisible(false);
            }
        }

        // Position all line sprites (left-aligned, top-to-bottom)
        float topY = cy + boxH * 0.5f - padding;
        float leftX = cx - boxW * 0.5f + padding;
        float lineSpacing = lineWorldH * 1.15f;

        // Calculate how many lines fit
        float availH = boxH - 2.0f * padding;
        int maxVisibleLines = static_cast<int>(availH / lineSpacing);

        for (size_t i = 0; i < m_wrapLineSprites.size(); ++i) {
            auto& sprite = m_wrapLineSprites[i];
            if (i >= m_wrapActiveCount || static_cast<int>(i) >= maxVisibleLines) {
                sprite->setVisible(false);
                continue;
            }
            sprite->setVisible(true);

            auto tex = sprite->getTexture();
            if (!tex)
                continue;

            float aspect =
                static_cast<float>(tex->getWidth()) / static_cast<float>(tex->getHeight());
            float w = lineWorldH * aspect;
            // Clamp width to fit inside box
            if (w > innerW) {
                w = innerW;
            }
            sprite->setScale(w, lineWorldH, 1.0f);
            sprite->setPosition(leftX, topY - i * lineSpacing, 0.0f);
        }
    }

    // =====================================================================
    // FIT-TO-RECT PANEL (bottom-right)
    // =====================================================================
    static constexpr float kFitCenterX = 5.5f;
    static constexpr float kFitCenterY = -3.5f;

    float m_fitBaseWidth = 6.0f;
    float m_fitWidthAmplitude = 2.0f;
    float m_fitBaseHeight = 3.0f;
    float m_fitHeightAmplitude = 1.0f;
    float m_fitCurrentWidth = 6.0f;
    float m_fitCurrentHeight = 3.0f;
    bool m_fitDirty = false;

    VisualBox m_fitBox;
    std::shared_ptr<vde::SpriteEntity> m_fitLabel;
    std::shared_ptr<vde::SpriteEntity> m_fitTextSprite;
    std::shared_ptr<vde::Texture> m_fitTextTexture;
    float m_fitTextAspect = 1.0f;

    void buildFitPanel() {
        m_fitBox = createBox();

        // Label
        {
            const auto& small = vde::BitmapFont::small();
            vde::TextStyle style{
                .color = vde::Color::fromRGB8(255, 160, 100), .pixelScale = 2, .letterSpacing = 1};
            auto tex = vde::TextRenderer::createTexture(m_ctx, "FIT TO RECT", small, style);
            auto sprite = addEntity<vde::SpriteEntity>();
            sprite->setTexture(tex);
            sprite->setAnchor(0.5f, 0.5f);
            m_fitLabel = sprite;
        }

        // Pre-render the fit text
        {
            vde::TextStyle style;
            style.color = vde::Color(m_textColor[0], m_textColor[1], m_textColor[2]);
            style.pixelScale = 1;
            style.letterSpacing = 0;

            m_fitTextTexture =
                vde::TextRenderer::createTexture(m_ctx, "FIT ME!", *m_ttfFont, style);
            float w = static_cast<float>(m_fitTextTexture->getWidth());
            float h = static_cast<float>(m_fitTextTexture->getHeight());
            m_fitTextAspect = (h > 0) ? w / h : 1.0f;

            auto sprite = addEntity<vde::SpriteEntity>();
            sprite->setTexture(m_fitTextTexture);
            sprite->setAnchor(0.5f, 0.5f);
            m_fitTextSprite = sprite;
        }

        m_fitCurrentWidth = m_fitBaseWidth;
        m_fitCurrentHeight = m_fitBaseHeight;
        updateFitPanel();
    }

    void updateFitPanel() {
        float boxW = m_fitCurrentWidth;
        float boxH = m_fitCurrentHeight;
        float cx = kFitCenterX;
        float cy = kFitCenterY;

        m_fitBox.update(cx, cy, boxW, boxH);

        // Position label
        if (m_fitLabel) {
            auto tex = m_fitLabel->getTexture();
            if (tex) {
                float aspect =
                    static_cast<float>(tex->getWidth()) / static_cast<float>(tex->getHeight());
                float lh = 0.35f;
                m_fitLabel->setScale(lh * aspect, lh, 1.0f);
                m_fitLabel->setPosition(cx, cy + boxH * 0.5f + 0.35f, 0.0f);
            }
        }

        // Scale text to fill the box (with padding)
        if (m_fitTextSprite) {
            float padding = 0.2f;
            float innerW = boxW - 2.0f * padding;
            float innerH = boxH - 2.0f * padding;

            // Fit maintaining aspect ratio
            float fitW, fitH;
            if (innerW / m_fitTextAspect <= innerH) {
                // Width-limited
                fitW = innerW;
                fitH = innerW / m_fitTextAspect;
            } else {
                // Height-limited
                fitH = innerH;
                fitW = innerH * m_fitTextAspect;
            }

            m_fitTextSprite->setScale(fitW, fitH, 1.0f);
            m_fitTextSprite->setPosition(cx, cy, 0.0f);
        }
    }

    // =====================================================================
    // Section Labels
    // =====================================================================
    std::shared_ptr<vde::SpriteEntity> m_titleSprite;

    void buildSectionLabels() {
        // Main title
        {
            vde::TextStyle style;
            style.color = vde::Color::fromRGB8(255, 220, 120);
            style.pixelScale = 1;
            style.letterSpacing = 0;

            auto tex =
                vde::TextRenderer::createTexture(m_ctx, "Text Metrics Demo", *m_ttfFont, style);
            auto sprite = addEntity<vde::SpriteEntity>();
            float aspect =
                static_cast<float>(tex->getWidth()) / static_cast<float>(tex->getHeight());
            float h = 0.7f;
            sprite->setTexture(tex);
            sprite->setScale(h * aspect, h, 1.0f);
            sprite->setPosition(0.0f, kCameraHeight * 0.5f - 0.55f, 0.0f);
            sprite->setAnchor(0.5f, 0.5f);
            m_titleSprite = sprite;
        }
    }

    // =====================================================================
    // Box creation helper
    // =====================================================================
    VisualBox createBox() {
        VisualBox box;
        auto makePart = [&](std::shared_ptr<vde::Texture> tex, const vde::Color& color) {
            auto sprite = addEntity<vde::SpriteEntity>();
            sprite->setTexture(tex);
            sprite->setColor(color);
            sprite->setAnchor(0.5f, 0.5f);
            return sprite;
        };

        vde::Color bgColor(m_boxBgColor[0], m_boxBgColor[1], m_boxBgColor[2]);
        vde::Color borderColor(m_borderColor[0], m_borderColor[1], m_borderColor[2]);

        box.background = makePart(m_whiteTex, bgColor);
        box.borderTop = makePart(m_whiteTex, borderColor);
        box.borderBottom = makePart(m_whiteTex, borderColor);
        box.borderLeft = makePart(m_whiteTex, borderColor);
        box.borderRight = makePart(m_whiteTex, borderColor);
        return box;
    }

    // =====================================================================
    // Rebuild helpers (for color changes)
    // =====================================================================
    /// Reload the TrueType font from the currently selected path/size and rebuild.
    void reloadFont() {
        auto newFont = std::make_unique<vde::TrueTypeFont>();
        if (!newFont->loadFromFile(m_ctx, m_fontPaths[m_selectedFontIdx], m_ttfSizePx)) {
            std::cerr << "WARNING: Failed to load font: " << m_fontPaths[m_selectedFontIdx]
                      << std::endl;
            // Revert to the last working font index
            m_selectedFontIdx = m_lastWorkingFontIdx;
            return;
        }
        m_ttfFont = std::move(newFont);
        m_lastWorkingFontIdx = m_selectedFontIdx;
        rebuildAll();
    }

    void rebuildAll() {
        // Rebuild alignment text textures
        const char* lines[] = {"Top line", "Middle line", "Bottom line"};
        for (int h = 0; h < 3; ++h) {
            for (int v = 0; v < 3; ++v) {
                auto& at = m_alignTexts[h][v];
                if (!at.sprite)
                    continue;

                vde::TextStyle style;
                style.color = vde::Color(m_textColor[0], m_textColor[1], m_textColor[2]);
                style.pixelScale = 1;
                style.letterSpacing = 0;

                auto tex = vde::TextRenderer::createTexture(m_ctx, lines[v], *m_ttfFont, style);
                at.sprite->setTexture(tex);
                at.texture = tex;
                float w = static_cast<float>(tex->getWidth());
                float ht = static_cast<float>(tex->getHeight());
                at.aspectRatio = (ht > 0) ? w / ht : 1.0f;
            }
        }

        // Rebuild fit text
        {
            vde::TextStyle style;
            style.color = vde::Color(m_textColor[0], m_textColor[1], m_textColor[2]);
            style.pixelScale = 1;
            style.letterSpacing = 0;

            m_fitTextTexture =
                vde::TextRenderer::createTexture(m_ctx, "FIT ME!", *m_ttfFont, style);
            float w = static_cast<float>(m_fitTextTexture->getWidth());
            float h = static_cast<float>(m_fitTextTexture->getHeight());
            m_fitTextAspect = (h > 0) ? w / h : 1.0f;
            if (m_fitTextSprite) {
                m_fitTextSprite->setTexture(m_fitTextTexture);
            }
        }

        // Rebuild title
        if (m_titleSprite) {
            vde::TextStyle titleStyle;
            titleStyle.color = vde::Color::fromRGB8(255, 220, 120);
            titleStyle.pixelScale = 1;
            titleStyle.letterSpacing = 0;

            auto tex = vde::TextRenderer::createTexture(m_ctx, "Text Metrics Demo", *m_ttfFont,
                                                        titleStyle);
            float aspect =
                static_cast<float>(tex->getWidth()) / static_cast<float>(tex->getHeight());
            float h = 0.7f;
            m_titleSprite->setTexture(tex);
            m_titleSprite->setScale(h * aspect, h, 1.0f);
        }

        // Force wrap panel to regenerate line textures
        m_wrapTextureCache.clear();
        m_wrapCachedLines.clear();
        m_wrapDirty = true;
        m_alignDirty = true;
        m_fitDirty = true;
    }

    void applyBoxColors() {
        vde::Color bgColor(m_boxBgColor[0], m_boxBgColor[1], m_boxBgColor[2]);
        vde::Color borderColor(m_borderColor[0], m_borderColor[1], m_borderColor[2]);

        auto applyToBox = [&](VisualBox& box) {
            if (box.background)
                box.background->setColor(bgColor);
            if (box.borderTop)
                box.borderTop->setColor(borderColor);
            if (box.borderBottom)
                box.borderBottom->setColor(borderColor);
            if (box.borderLeft)
                box.borderLeft->setColor(borderColor);
            if (box.borderRight)
                box.borderRight->setColor(borderColor);
        };

        for (int i = 0; i < 3; ++i) {
            applyToBox(m_alignBoxes[i]);
        }
        applyToBox(m_wrapBox);
        applyToBox(m_fitBox);
    }
};

// ---------------------------------------------------------------------------
// Game
// ---------------------------------------------------------------------------
class TextMetricsDemo
    : public vde::examples::BaseExampleGame<TextMetricsInputHandler, TextMetricsScene> {};

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main(int argc, char** argv) {
    TextMetricsDemo demo;
    float dpiScale = vde::Window::getPrimaryMonitorDPIScale();
    uint32_t width = static_cast<uint32_t>(1280 * dpiScale);
    uint32_t height = static_cast<uint32_t>(720 * dpiScale);
    return vde::examples::runExample(demo, "VDE Text Metrics Demo", width, height, argc, argv);
}
