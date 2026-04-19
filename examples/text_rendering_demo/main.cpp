/**
 * @file main.cpp
 * @brief Text Rendering Showcase — the canonical reference for TextEntity usage.
 *
 * Demonstrates the recommended text rendering patterns in VDE:
 * - Automatic sizing via setWorldHeight() (recommended approach)
 * - Max-width clamping via setMaxWidth() with animated oscillation
 * - Dynamic text updates that auto-resize every frame
 * - Scrolling text area using a ring-buffer pattern
 * - Left / center / right alignment via setAnchor()
 * - BitmapFont::small() and BitmapFont::large() at multiple pixelScales
 * - Edge cases: single character, long overflow text with maxWidth
 * - Size comparison: same text at different worldHeight values
 */

#include <vde/api/GameAPI.h>

#include <cmath>
#include <cstdio>
#include <deque>
#include <string>
#include <vector>

#include "../ExampleBase.h"

using namespace vde;

// ============================================================================
// Constants
// ============================================================================

static constexpr float VIEW_W = 16.0f;
static constexpr float VIEW_H = 9.0f;
static constexpr int LOG_LINES = 8;

// ============================================================================
// Input handler
// ============================================================================

class ShowcaseInputHandler : public vde::examples::BaseExampleInputHandler {
  public:
    void onKeyPress(int key) override {
        BaseExampleInputHandler::onKeyPress(key);
        if (key == vde::KEY_SPACE)
            m_spacePressed = true;
    }

    bool consumeSpace() {
        bool v = m_spacePressed;
        m_spacePressed = false;
        return v;
    }

  private:
    bool m_spacePressed = false;
};

// ============================================================================
// Scene
// ============================================================================

class ShowcaseScene : public vde::examples::BaseExampleScene {
  public:
    ShowcaseScene() : BaseExampleScene() {}

    void onEnter() override {
        printExampleHeader();

        auto* camera = new Camera2D(VIEW_W, VIEW_H);
        camera->setPosition(0.0f, 0.0f);
        camera->setZoom(1.0f);
        setCamera(camera);
        setBackgroundColor(Color(0.05f, 0.05f, 0.12f, 1.0f));
        setLightBox(std::make_unique<SimpleColorLightBox>(Color::white()));

        // ── Title ───────────────────────────────────────────────
        float y = VIEW_H * 0.5f - 0.45f;

        auto title = addEntity<TextEntity>();
        title->setText("TEXT RENDERING SHOWCASE");
        title->setFont(BitmapFont::large());
        title->setStyle({.color = Color::white(), .pixelScale = 2});
        title->setAnchor(0.5f, 0.5f);
        title->setPosition(0.0f, y, 0.0f);
        title->setWorldHeight(0.50f);

        y -= 0.50f;

        auto subtitle = addEntity<TextEntity>();
        subtitle->setText("Automatic sizing with setWorldHeight()");
        subtitle->setFont(BitmapFont::small());
        subtitle->setStyle({.color = Color(0.6f, 0.6f, 0.7f, 1.0f), .pixelScale = 1});
        subtitle->setAnchor(0.5f, 0.5f);
        subtitle->setPosition(0.0f, y, 0.0f);
        subtitle->setWorldHeight(0.25f);

        float sectionTop = y - 0.55f;

        buildLeftColumn(sectionTop);
        buildRightColumn(sectionTop);
        buildStatusBar();
    }

    void update(float deltaTime) override {
        BaseExampleScene::update(deltaTime);
        m_time += deltaTime;

        updateDynamic();
        updateResizeAnimation();
        updateAutoLog(deltaTime);

        auto* input = dynamic_cast<ShowcaseInputHandler*>(getInputHandler());
        if (input && input->consumeSpace()) {
            addLogMessage("User pressed SPACE at " + formatTime(m_time));
        }

        updateStatusBar();
    }

  protected:
    std::string getExampleName() const override { return "Text Rendering Showcase"; }

    std::vector<std::string> getFeatures() const override {
        return {
            "Automatic sizing with setWorldHeight()",
            "Max-width clamping with setMaxWidth()",
            "Dynamic text updates that auto-resize each frame",
            "Scrolling text area with ring-buffer pattern",
            "Left / center / right text alignment",
            "BitmapFont::small() and BitmapFont::large() comparison",
            "Edge cases: single char, long overflow text",
            "Animated max-width oscillation",
        };
    }

    std::vector<std::string> getExpectedVisuals() const override {
        return {
            "Title and subtitle centered at top",
            "Left/center/right aligned labels in ALIGNMENT section",
            "Small and large font samples in FONTS section",
            "Frame counter, timer, and FPS updating every frame",
            "Scrolling log with auto-generated messages (new every 1.5s)",
            "Long text with oscillating width in ANIMATED MAX-WIDTH section",
            "Same text at three different sizes in SIZE COMPARISON",
            "Status bar at bottom with entity count and FPS",
        };
    }

    std::vector<std::string> getControls() const override {
        return {"SPACE - Add a message to the scrolling log"};
    }

  private:
    // ── Section header helper ───────────────────────────────────

    std::shared_ptr<TextEntity> addSectionHeader(const std::string& text, float x, float& y) {
        auto e = addEntity<TextEntity>();
        e->setText(text);
        e->setFont(BitmapFont::small());
        e->setStyle({.color = Color::cyan(), .pixelScale = 1, .letterSpacing = 2});
        e->setAnchor(0.0f, 0.5f);
        e->setPosition(x, y, 0.1f);
        e->setWorldHeight(0.28f);
        y -= 0.40f;
        return e;
    }

    // ── Left Column ─────────────────────────────────────────────

    void buildLeftColumn(float y) {
        constexpr float COL_L = -VIEW_W * 0.5f + 0.3f;
        constexpr float COL_MID = -VIEW_W * 0.5f + 3.8f;
        constexpr float COL_R = -0.3f;

        // ── Alignment ───────────────────────────────────────────
        addSectionHeader("ALIGNMENT", COL_L, y);

        auto left = addEntity<TextEntity>();
        left->setText("Left-aligned");
        left->setFont(BitmapFont::small());
        left->setStyle({.color = Color::green(), .pixelScale = 1});
        left->setAnchor(0.0f, 0.5f);
        left->setPosition(COL_L, y, 0.0f);
        left->setWorldHeight(0.28f);
        y -= 0.38f;

        auto center = addEntity<TextEntity>();
        center->setText("Center-aligned");
        center->setFont(BitmapFont::small());
        center->setStyle({.color = Color::yellow(), .pixelScale = 1});
        center->setAnchor(0.5f, 0.5f);
        center->setPosition(COL_MID, y, 0.0f);
        center->setWorldHeight(0.28f);
        y -= 0.38f;

        auto right = addEntity<TextEntity>();
        right->setText("Right-aligned");
        right->setFont(BitmapFont::small());
        right->setStyle({.color = Color(1.0f, 0.5f, 0.3f, 1.0f), .pixelScale = 1});
        right->setAnchor(1.0f, 0.5f);
        right->setPosition(COL_R, y, 0.0f);
        right->setWorldHeight(0.28f);
        y -= 0.38f;

        y -= 0.15f;

        // ── Fonts ───────────────────────────────────────────────
        addSectionHeader("FONTS", COL_L, y);

        auto f1 = addEntity<TextEntity>();
        f1->setText("small() px=1");
        f1->setFont(BitmapFont::small());
        f1->setStyle({.color = Color::white(), .pixelScale = 1});
        f1->setAnchor(0.0f, 0.5f);
        f1->setPosition(COL_L, y, 0.0f);
        f1->setWorldHeight(0.28f);
        y -= 0.38f;

        auto f2 = addEntity<TextEntity>();
        f2->setText("small() px=2");
        f2->setFont(BitmapFont::small());
        f2->setStyle({.color = Color::white(), .pixelScale = 2});
        f2->setAnchor(0.0f, 0.5f);
        f2->setPosition(COL_L, y, 0.0f);
        f2->setWorldHeight(0.28f);
        y -= 0.38f;

        auto f3 = addEntity<TextEntity>();
        f3->setText("large() px=2");
        f3->setFont(BitmapFont::large());
        f3->setStyle({.color = Color::white(), .pixelScale = 2});
        f3->setAnchor(0.0f, 0.5f);
        f3->setPosition(COL_L, y, 0.0f);
        f3->setWorldHeight(0.28f);
        y -= 0.38f;

        y -= 0.15f;

        // ── Dynamic Text ────────────────────────────────────────
        addSectionHeader("DYNAMIC TEXT", COL_L, y);

        m_frameCounter = addEntity<TextEntity>();
        m_frameCounter->setText("Frame: 0");
        m_frameCounter->setFont(BitmapFont::small());
        m_frameCounter->setStyle({.color = Color::green(), .pixelScale = 1});
        m_frameCounter->setAnchor(0.0f, 0.5f);
        m_frameCounter->setPosition(COL_L, y, 0.0f);
        m_frameCounter->setWorldHeight(0.28f);
        y -= 0.38f;

        m_timerLabel = addEntity<TextEntity>();
        m_timerLabel->setText("Time: 0.00s");
        m_timerLabel->setFont(BitmapFont::small());
        m_timerLabel->setStyle({.color = Color::green(), .pixelScale = 1});
        m_timerLabel->setAnchor(0.0f, 0.5f);
        m_timerLabel->setPosition(COL_L, y, 0.0f);
        m_timerLabel->setWorldHeight(0.28f);
        y -= 0.38f;

        m_fpsLabel = addEntity<TextEntity>();
        m_fpsLabel->setText("FPS: 0.0");
        m_fpsLabel->setFont(BitmapFont::small());
        m_fpsLabel->setStyle({.color = Color::green(), .pixelScale = 1});
        m_fpsLabel->setAnchor(0.0f, 0.5f);
        m_fpsLabel->setPosition(COL_L, y, 0.0f);
        m_fpsLabel->setWorldHeight(0.28f);
        y -= 0.38f;

        y -= 0.15f;

        // ── Edge Cases ──────────────────────────────────────────
        addSectionHeader("EDGE CASES", COL_L, y);

        auto singleChar = addEntity<TextEntity>();
        singleChar->setText("X");
        singleChar->setFont(BitmapFont::small());
        singleChar->setStyle({.color = Color::yellow(), .pixelScale = 1});
        singleChar->setAnchor(0.0f, 0.5f);
        singleChar->setPosition(COL_L, y, 0.0f);
        singleChar->setWorldHeight(0.25f);
        y -= 0.35f;

        auto overflow = addEntity<TextEntity>();
        overflow->setText("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");
        overflow->setFont(BitmapFont::small());
        overflow->setStyle({.color = Color(1.0f, 0.4f, 0.4f, 1.0f), .pixelScale = 1});
        overflow->setAnchor(0.0f, 0.5f);
        overflow->setPosition(COL_L, y, 0.0f);
        overflow->setWorldHeight(0.25f);
        overflow->setMaxWidth(7.0f);
    }

    // ── Right Column ────────────────────────────────────────────

    void buildRightColumn(float y) {
        constexpr float COL = 0.5f;

        // ── Scrolling Log ───────────────────────────────────────
        addSectionHeader("SCROLLING LOG", COL, y);

        for (int i = 0; i < LOG_LINES; ++i) {
            m_logEntities[i] = addEntity<TextEntity>();
            m_logEntities[i]->setText("---");
            m_logEntities[i]->setFont(BitmapFont::small());
            m_logEntities[i]->setStyle({.color = Color(0.8f, 0.8f, 0.8f, 1.0f), .pixelScale = 1});
            m_logEntities[i]->setAnchor(0.0f, 0.5f);
            m_logEntities[i]->setPosition(COL, y, 0.0f);
            m_logEntities[i]->setWorldHeight(0.25f);
            m_logEntities[i]->setMaxWidth(7.0f);
            y -= 0.33f;
        }

        y -= 0.20f;

        // ── Animated Max-Width ──────────────────────────────────
        addSectionHeader("ANIMATED MAX-WIDTH", COL, y);

        m_resizeLabel = addEntity<TextEntity>();
        m_resizeLabel->setText(
            "This long text demonstrates dynamic max-width clamping as it oscillates");
        m_resizeLabel->setFont(BitmapFont::small());
        m_resizeLabel->setStyle({.color = Color(1.0f, 0.8f, 0.2f, 1.0f), .pixelScale = 1});
        m_resizeLabel->setAnchor(0.0f, 0.5f);
        m_resizeLabel->setPosition(COL, y, 0.0f);
        m_resizeLabel->setWorldHeight(0.28f);
        m_resizeLabel->setMaxWidth(7.0f);
        y -= 0.38f;

        m_resizeInfo = addEntity<TextEntity>();
        m_resizeInfo->setText("maxWidth: 7.0");
        m_resizeInfo->setFont(BitmapFont::small());
        m_resizeInfo->setStyle({.color = Color(0.5f, 0.5f, 0.6f, 1.0f), .pixelScale = 1});
        m_resizeInfo->setAnchor(0.0f, 0.5f);
        m_resizeInfo->setPosition(COL, y, 0.0f);
        m_resizeInfo->setWorldHeight(0.22f);
        y -= 0.35f;

        y -= 0.15f;

        // ── Size Comparison ─────────────────────────────────────
        addSectionHeader("SIZE COMPARISON", COL, y);

        auto s1 = addEntity<TextEntity>();
        s1->setText("worldHeight = 0.20");
        s1->setFont(BitmapFont::small());
        s1->setStyle({.color = Color(0.7f, 0.9f, 1.0f, 1.0f), .pixelScale = 1});
        s1->setAnchor(0.0f, 0.5f);
        s1->setPosition(COL, y, 0.0f);
        s1->setWorldHeight(0.20f);
        y -= 0.35f;

        auto s2 = addEntity<TextEntity>();
        s2->setText("worldHeight = 0.35");
        s2->setFont(BitmapFont::small());
        s2->setStyle({.color = Color(0.7f, 0.9f, 1.0f, 1.0f), .pixelScale = 1});
        s2->setAnchor(0.0f, 0.5f);
        s2->setPosition(COL, y, 0.0f);
        s2->setWorldHeight(0.35f);
        y -= 0.50f;

        auto s3 = addEntity<TextEntity>();
        s3->setText("worldHeight = 0.50");
        s3->setFont(BitmapFont::large());
        s3->setStyle({.color = Color(0.7f, 0.9f, 1.0f, 1.0f), .pixelScale = 2});
        s3->setAnchor(0.0f, 0.5f);
        s3->setPosition(COL, y, 0.0f);
        s3->setWorldHeight(0.50f);
    }

    // ── Status Bar ──────────────────────────────────────────────

    void buildStatusBar() {
        float y = -VIEW_H * 0.5f + 0.25f;
        m_statusLabel = addEntity<TextEntity>();
        m_statusLabel->setText(" ");
        m_statusLabel->setFont(BitmapFont::small());
        m_statusLabel->setStyle({.color = Color(0.4f, 0.4f, 0.5f, 1.0f), .pixelScale = 1});
        m_statusLabel->setAnchor(0.0f, 0.5f);
        m_statusLabel->setPosition(-VIEW_W * 0.5f + 0.3f, y, 0.0f);
        m_statusLabel->setWorldHeight(0.20f);
    }

    // ── Update Logic ────────────────────────────────────────────

    void updateDynamic() {
        if (m_frameCounter) {
            char buf[64];
            std::snprintf(
                buf, sizeof(buf), "Frame: %llu",
                static_cast<unsigned long long>(getGame() ? getGame()->getFrameCount() : 0));
            m_frameCounter->setText(buf);
        }
        if (m_timerLabel) {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "Time: %.2fs", m_time);
            m_timerLabel->setText(buf);
        }
        if (m_fpsLabel) {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "FPS: %.1f", getGame() ? getGame()->getFPS() : 0.0f);
            m_fpsLabel->setText(buf);
        }
    }

    void updateResizeAnimation() {
        if (!m_resizeLabel || !m_resizeInfo)
            return;

        // Oscillate maxWidth between 2.0 and 7.0
        float t = (std::sin(m_time * 1.5f) + 1.0f) * 0.5f;
        float maxW = 2.0f + t * 5.0f;
        m_resizeLabel->setMaxWidth(maxW);

        char buf[64];
        std::snprintf(buf, sizeof(buf), "maxWidth: %.1f (oscillating)", maxW);
        m_resizeInfo->setText(buf);
    }

    void updateAutoLog(float deltaTime) {
        m_logAccum += deltaTime;
        if (m_logAccum < 1.5f)
            return;
        m_logAccum -= 1.5f;
        ++m_logCounter;

        static const char* prefixes[] = {
            "System check",  "Sensor ping",     "Data received",  "Alert cleared",
            "Module loaded", "Signal acquired", "Buffer flushed", "Task completed",
        };
        int idx = m_logCounter % 8;
        addLogMessage(std::string(prefixes[idx]) + " #" + std::to_string(m_logCounter));
    }

    void addLogMessage(const std::string& msg) {
        m_logMessages.push_front(msg);
        while (static_cast<int>(m_logMessages.size()) > LOG_LINES)
            m_logMessages.pop_back();

        for (int i = 0; i < LOG_LINES; ++i) {
            if (i < static_cast<int>(m_logMessages.size()))
                m_logEntities[i]->setText(m_logMessages[i]);
            else
                m_logEntities[i]->setText("---");
        }
    }

    void updateStatusBar() {
        if (!m_statusLabel || !getGame())
            return;
        char buf[128];
        std::snprintf(buf, sizeof(buf), "Entities: %zu  |  FPS: %.0f  |  Elapsed: %.1fs",
                      getEntities().size(), getGame()->getFPS(), m_time);
        m_statusLabel->setText(buf);
    }

    std::string formatTime(float t) {
        int sec = static_cast<int>(t);
        int ms = static_cast<int>((t - sec) * 100.0f);
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%d.%02ds", sec, ms);
        return buf;
    }

    // ── Members ─────────────────────────────────────────────────

    float m_time = 0.0f;
    float m_logAccum = 0.0f;
    int m_logCounter = 0;

    std::shared_ptr<TextEntity> m_frameCounter;
    std::shared_ptr<TextEntity> m_timerLabel;
    std::shared_ptr<TextEntity> m_fpsLabel;

    std::shared_ptr<TextEntity> m_logEntities[LOG_LINES];
    std::deque<std::string> m_logMessages;

    std::shared_ptr<TextEntity> m_resizeLabel;
    std::shared_ptr<TextEntity> m_resizeInfo;

    std::shared_ptr<TextEntity> m_statusLabel;
};

// ============================================================================
// Game
// ============================================================================

class ShowcaseGame : public vde::examples::BaseExampleGame<ShowcaseInputHandler, ShowcaseScene> {
  public:
    ShowcaseGame() = default;
};

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    ShowcaseGame game;
    return vde::examples::runExample(game, "VDE Text Rendering Showcase", 1280, 720, argc, argv);
}
