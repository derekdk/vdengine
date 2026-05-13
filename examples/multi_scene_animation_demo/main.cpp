/**
 * @file main.cpp
 * @brief Multi-Scene Animation Demo — two independent scenes, each combining
 *        timed events and animations.
 *
 * Scene A (left half):
 *   - A one-shot timed callback after 0.8 s triggers a position animation on a cyan square.
 *   - A repeating timer (every 0.6 s) alternates the color of a second square between
 *     orange and purple.
 *
 * Scene B (right half):
 *   - An independent looping animation pulses the scale of a green square.
 *   - A repeating timed event (every 0.5 s) increments a counter displayed as text.
 *
 * Self-validation:
 *   Both scenes must complete their expected cycles before the auto-terminate deadline.
 *   Exit code 0 on success, 1 on failure.
 *
 * Controls:
 *   ESC - Exit early (exit code 1)
 *   F   - Report test failure
 */

#include <vde/api/GameAPI.h>

#include <iostream>
#include <string>
#include <vector>

#include "../ExampleBase.h"

using namespace vde;

// ============================================================================
// Layout
// ============================================================================

static constexpr float WORLD_W = 8.0f;
static constexpr float WORLD_H = 9.0f;

// ============================================================================
// Scene A — timed delay triggers animation; repeating timer alternates color
// ============================================================================

class SceneA : public vde::examples::BaseExampleScene {
  public:
    SceneA() : BaseExampleScene(7.0f) {}

    [[nodiscard]] bool didValidationPass() const {
        // One-shot must have fired and the animation it launched must have completed at least once.
        // Color alternation timer must have fired at least 4 times.
        return m_animTriggered && m_animCompletions >= 1 && m_colorFlipCount >= 4;
    }

    void onEnter() override {
        printExampleHeader();
        setup2D(WORLD_W, WORLD_H, Color(0.06f, 0.08f, 0.18f, 1.0f));

        buildUI();

        // Animated position square (starts off the top)
        m_moverBox = addEntity<SpriteEntity>();
        m_moverBox->setName("MoverBox");
        m_moverBox->setScale(0.6f, 0.6f, 1.0f);
        m_moverBox->setPosition(0.0f, 3.5f, 0.0f);
        m_moverBox->setColor(Color(0.25f, 0.85f, 0.85f, 1.0f));

        // Color-alternating square
        m_colorBox = addEntity<SpriteEntity>();
        m_colorBox->setName("ColorBox");
        m_colorBox->setScale(0.55f, 0.55f, 1.0f);
        m_colorBox->setPosition(0.0f, -1.5f, 0.0f);
        m_colorBox->setColor(m_colorA);

        auto& t = getTimedEvents();

        // One-shot: after 0.8 s, trigger a looping position animation.
        t.after(0.8f, [this]() {
            m_animTriggered = true;
            if (m_triggerStatus) {
                m_triggerStatus->setText("Animation started!");
            }
            launchPositionLoop();
        });

        // Repeating: alternate color every 0.6 s.
        t.every(0.6f, [this]() {
            ++m_colorFlipCount;
            m_useColorA = !m_useColorA;
            if (m_colorBox) {
                m_colorBox->setColor(m_useColorA ? m_colorA : m_colorB);
            }
            if (m_colorStatus) {
                m_colorStatus->setText("Flips: " + std::to_string(m_colorFlipCount));
            }
        });
    }

  protected:
    [[nodiscard]] std::string getExampleName() const override {
        return "Multi-Scene Animation Demo";
    }

    [[nodiscard]] std::vector<std::string> getFeatures() const override {
        return {
            "One-shot timed event triggers an animation after a delay",
            "Repeating timer alternates entity color each tick",
            "Animation completion callback chains the next animation cycle",
            "Scene B runs fully independently in the right viewport",
        };
    }

    [[nodiscard]] std::vector<std::string> getExpectedVisuals() const override {
        return {
            "Left panel: cyan square slides up and down continuously after 0.8 s",
            "Left panel: small square alternates orange/purple every 0.6 s",
            "Right panel: green square pulses in scale; counter increments every 0.5 s",
        };
    }

  private:
    void launchPositionLoop() {
        if (!m_moverBox) {
            return;
        }
        EntityId id = m_moverBox->getId();

        // Slide from y=+3.0 to y=-0.5 then back — one full round-trip per chain pair.
        animations().schedule<SpriteEntity>(
            *this, AnimationBinding<SpriteEntity>::entity(id),
            {.duration = 0.9f, .easing = AnimationEasing::EaseInOutCubic},
            {.onUpdate =
                 [](SpriteEntity& e, const AnimationContext& ctx) {
                     float y = 3.0f + (-0.5f - 3.0f) * ctx.easedProgress;
                     e.setPosition(0.0f, y, 0.0f);
                 },
             .onComplete =
                 [this, id](SpriteEntity& e, const AnimationContext&) {
                     e.setPosition(0.0f, -0.5f, 0.0f);
                     // Return tween.
                     animations().schedule<SpriteEntity>(
                         *this, AnimationBinding<SpriteEntity>::entity(id),
                         {.duration = 0.9f, .easing = AnimationEasing::EaseInOutCubic},
                         {.onUpdate =
                              [](SpriteEntity& e2, const AnimationContext& ctx) {
                                  float y = -0.5f + (3.0f - (-0.5f)) * ctx.easedProgress;
                                  e2.setPosition(0.0f, y, 0.0f);
                              },
                          .onComplete =
                              [this](SpriteEntity& e2, const AnimationContext&) {
                                  e2.setPosition(0.0f, 3.0f, 0.0f);
                                  ++m_animCompletions;
                                  if (m_animStatus) {
                                      m_animStatus->setText(
                                          "Round trips: " + std::to_string(m_animCompletions));
                                  }
                                  launchPositionLoop();
                              }});
                 }});
    }

    // ---- UI ----------------------------------------------------------------

    std::shared_ptr<TextEntity> makeHeader(const std::string& text, float y) {
        auto e = addEntity<TextEntity>();
        e->setText(text);
        e->setFont(BitmapFont::large());
        e->setStyle({.color = Color::white(), .pixelScale = 2});
        e->setPosition(0.0f, y, 0.0f);
        e->setWorldHeight(0.40f);
        return e;
    }

    std::shared_ptr<TextEntity> makeLabel(const std::string& text, float y,
                                          Color col = Color(0.75f, 0.75f, 0.85f, 1.0f)) {
        auto e = addEntity<TextEntity>();
        e->setText(text);
        e->setFont(BitmapFont::small());
        e->setStyle({.color = col, .pixelScale = 2});
        e->setPosition(0.0f, y, 0.0f);
        e->setWorldHeight(0.28f);
        return e;
    }

    std::shared_ptr<TextEntity> makeValue(const std::string& text, float y,
                                          Color col = Color(0.40f, 0.85f, 0.50f, 1.0f)) {
        auto e = addEntity<TextEntity>();
        e->setText(text);
        e->setFont(BitmapFont::small());
        e->setStyle({.color = col, .pixelScale = 2});
        e->setPosition(0.0f, y, 0.0f);
        e->setWorldHeight(0.28f);
        return e;
    }

    void buildUI() {
        makeHeader("SCENE A", 4.2f);

        makeLabel("TIMED → ANIM (delay 0.8s)", 3.1f);
        m_triggerStatus = makeValue("Waiting for timer...", 2.65f, Color(0.95f, 0.75f, 0.30f, 1.0f));
        m_animStatus = makeValue("Round trips: 0", 2.20f);

        makeLabel("COLOR FLIP (every 0.6s)", 0.2f);
        m_colorStatus = makeValue("Flips: 0", -0.25f, Color(0.75f, 0.50f, 0.95f, 1.0f));
    }

    // ---- State -------------------------------------------------------------

    std::shared_ptr<SpriteEntity> m_moverBox;
    std::shared_ptr<SpriteEntity> m_colorBox;
    std::shared_ptr<TextEntity> m_triggerStatus;
    std::shared_ptr<TextEntity> m_animStatus;
    std::shared_ptr<TextEntity> m_colorStatus;

    const Color m_colorA{0.95f, 0.60f, 0.20f, 1.0f};
    const Color m_colorB{0.65f, 0.35f, 0.95f, 1.0f};

    bool m_useColorA = true;
    bool m_animTriggered = false;
    int m_animCompletions = 0;
    int m_colorFlipCount = 0;
};

// ============================================================================
// Scene B — looping scale animation + repeating timed counter
// ============================================================================

class SceneB : public vde::Scene {
  public:
    [[nodiscard]] bool didValidationPass() const {
        // Scale animation must have completed at least 3 cycles; counter ≥ 5 ticks.
        return m_scaleCycles >= 3 && m_timerCount >= 5;
    }

    void onEnter() override {
        setup2D(WORLD_W, WORLD_H, Color(0.07f, 0.14f, 0.09f, 1.0f));

        buildUI();

        // Scale-pulsing square
        m_pulseBox = addEntity<SpriteEntity>();
        m_pulseBox->setName("PulseBox");
        m_pulseBox->setScale(0.5f, 0.5f, 1.0f);
        m_pulseBox->setPosition(0.0f, 1.0f, 0.0f);
        m_pulseBox->setColor(Color(0.30f, 0.85f, 0.45f, 1.0f));

        // Looping scale animation (each loop = one cycle).
        EntityId pulseId = m_pulseBox->getId();
        animations().schedule<SpriteEntity>(
            *this, AnimationBinding<SpriteEntity>::entity(pulseId),
            {.duration = 0.8f, .playback = AnimationPlayback::Loop,
             .easing = AnimationEasing::EaseInOutSine},
            {.onUpdate =
                 [](SpriteEntity& e, const AnimationContext& ctx) {
                     float s = 0.3f + 0.5f * ctx.easedProgress;
                     e.setScale(s, s, 1.0f);
                 },
             .onComplete =
                 [this](SpriteEntity&, const AnimationContext& ctx) {
                     m_scaleCycles = static_cast<int>(ctx.cycleIndex);
                     if (m_cycleText) {
                         m_cycleText->setText("Cycles: " + std::to_string(m_scaleCycles));
                     }
                 }});

        // Repeating timed event every 0.5 s.
        getTimedEvents().every(0.5f, [this]() {
            ++m_timerCount;
            if (m_timerText) {
                m_timerText->setText("Ticks: " + std::to_string(m_timerCount));
            }
        });
    }

    void update(float dt) override { Scene::update(dt); }

  private:
    std::shared_ptr<TextEntity> makeHeader(const std::string& text, float y) {
        auto e = addEntity<TextEntity>();
        e->setText(text);
        e->setFont(BitmapFont::large());
        e->setStyle({.color = Color::white(), .pixelScale = 2});
        e->setPosition(0.0f, y, 0.0f);
        e->setWorldHeight(0.40f);
        return e;
    }

    std::shared_ptr<TextEntity> makeLabel(const std::string& text, float y,
                                          Color col = Color(0.75f, 0.85f, 0.75f, 1.0f)) {
        auto e = addEntity<TextEntity>();
        e->setText(text);
        e->setFont(BitmapFont::small());
        e->setStyle({.color = col, .pixelScale = 2});
        e->setPosition(0.0f, y, 0.0f);
        e->setWorldHeight(0.28f);
        return e;
    }

    std::shared_ptr<TextEntity> makeValue(const std::string& text, float y,
                                          Color col = Color(0.40f, 0.85f, 0.50f, 1.0f)) {
        auto e = addEntity<TextEntity>();
        e->setText(text);
        e->setFont(BitmapFont::small());
        e->setStyle({.color = col, .pixelScale = 2});
        e->setPosition(0.0f, y, 0.0f);
        e->setWorldHeight(0.28f);
        return e;
    }

    void buildUI() {
        makeHeader("SCENE B", 4.2f);

        makeLabel("SCALE PULSE (loop 0.8s)", 3.1f);
        m_cycleText = makeValue("Cycles: 0", 2.65f);

        makeLabel("TIMER (every 0.5s)", 1.5f);
        m_timerText = makeValue("Ticks: 0", 1.05f, Color(0.40f, 0.85f, 0.50f, 1.0f));

        makeLabel("Independent of Scene A", -1.8f, Color(0.45f, 0.60f, 0.45f, 1.0f));
    }

    std::shared_ptr<SpriteEntity> m_pulseBox;
    std::shared_ptr<TextEntity> m_cycleText;
    std::shared_ptr<TextEntity> m_timerText;

    int m_scaleCycles = 0;
    int m_timerCount = 0;
};

// ============================================================================
// Game — split-screen, two scenes
// ============================================================================

class MultiSceneAnimGame : public vde::Game {
  public:
    void onStart() override {
        auto* a = new SceneA();
        auto* b = new SceneB();
        m_sceneA = a;
        m_sceneB = b;

        addScene("sceneA", a);
        addScene("sceneB", b);

        auto group = SceneGroup::createWithViewports(
            "split",
            {
                {.sceneName = "sceneA", .viewport = ViewportRect::leftHalf()},
                {.sceneName = "sceneB", .viewport = ViewportRect::rightHalf()},
            });
        setActiveSceneGroup(group);
        setFocusedScene("sceneA");
    }

    void onShutdown() override {
        bool passed = true;

        if (m_sceneA && !m_sceneA->didValidationPass()) {
            std::cerr << "[FAIL] Scene A: timed trigger or animation round-trip or color-flip "
                         "count not met\n";
            passed = false;
        }
        if (m_sceneA && m_sceneA->didTestFail()) {
            passed = false;
        }
        if (m_sceneB && !m_sceneB->didValidationPass()) {
            std::cerr << "[FAIL] Scene B: scale cycles or timer count not met\n";
            passed = false;
        }

        if (passed) {
            std::cout << "[PASS] Multi-Scene Animation Demo: all expected events completed\n";
        }

        m_exitCode = passed ? 0 : 1;
    }

    [[nodiscard]] int getExitCode() const override {
        if (m_exitCode != 0) {
            return m_exitCode;
        }
        return Game::getExitCode();
    }

  private:
    SceneA* m_sceneA = nullptr;
    SceneB* m_sceneB = nullptr;
    int m_exitCode = 0;
};

// ============================================================================
// Entry point
// ============================================================================

// NOLINTNEXTLINE(bugprone-exception-escape)
int main(int argc, char** argv) {
    MultiSceneAnimGame demo;
    return vde::examples::runExample(demo, "VDE Multi-Scene Animation Demo", 1280, 720, argc,
                                     argv);
}
