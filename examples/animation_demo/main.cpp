/**
 * @file main.cpp
 * @brief Animation Demo — demonstrates the scene-owned Animator service.
 *
 * Two independent scenes run side by side in split-screen viewports.
 *
 * Scene A (left half):
 *   - A one-shot position tween: a cube slides from top to bottom, then a
 *     completion callback chains into a second tween back to the top.
 *   - A ping-pong scale animation on a second entity.
 *
 * Scene B (right half):
 *   - A looping color-pulse animation (demonstrates Loop mode).
 *   - A separate entity with a speed-scaled animation showing two different
 *     speeds advancing independently.
 *
 * Self-validation:
 *   Scene A verifies the position tween completed at least one round trip and
 *   the ping-pong scale animation completed at least one full cycle.
 *   Scene B verifies the looping animation has completed at least two cycles.
 *   All scenes must pass within the auto-terminate window for exit code 0.
 *   Exit code 1 means one or more checks failed.
 *
 * Controls:
 *   P   - Pause / resume all animations in Scene A
 *   ESC - Exit early (exit code 1)
 *   F   - Report test failure
 */

#include <vde/api/GameAPI.h>

#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "../ExampleBase.h"

using namespace vde;

// ============================================================================
// Layout constants
// ============================================================================

static constexpr float WORLD_W = 8.0f;
static constexpr float WORLD_H = 9.0f;

// ============================================================================
// Input Handler
// ============================================================================

class AnimDemoInputHandler : public vde::examples::BaseExampleInputHandler {
  public:
    AnimDemoInputHandler() { m_keys.bindOneShot(KEY_P, "pause"); }

    void onKeyPress(int key) override {
        BaseExampleInputHandler::onKeyPress(key);
        m_keys.handlePress(key);
    }

    void onKeyRelease(int key) override { m_keys.handleRelease(key); }

    bool consumePause() { return m_keys.consume("pause"); }

  private:
    KeyStateTracker m_keys;
};

// ============================================================================
// Scene A — position tween + ping-pong scale
// ============================================================================

class SceneA : public vde::examples::BaseExampleScene {
  public:
    SceneA() : BaseExampleScene(6.0f) {}

    [[nodiscard]] bool didValidationPass() const {
        // Position tween must have completed at least one round trip.
        // Ping-pong scale must have advanced (cycleIndex > 0).
        return m_positionRoundTrips >= 1 && m_pingPongCycles >= 1;
    }

    void onEnter() override {
        printExampleHeader();
        setup2D(WORLD_W, WORLD_H, Color(0.06f, 0.08f, 0.18f, 1.0f));

        buildUI();
        scheduleAnimations();
    }

    void update(float dt) override {
        BaseExampleScene::update(dt);

        // Check for pause toggle.
        auto* input = dynamic_cast<AnimDemoInputHandler*>(getInputHandler());
        if (input && input->consumePause()) {
            auto& anim = animations();
            if (m_paused) {
                anim.resumeAll();
                m_paused = false;
                if (m_statusText)
                    m_statusText->setText("RUNNING");
            } else {
                anim.pauseAll();
                m_paused = true;
                if (m_statusText)
                    m_statusText->setText("PAUSED");
            }
        }

        // Update status readouts each frame.
        if (m_roundTripText) {
            m_roundTripText->setText("Round trips: " + std::to_string(m_positionRoundTrips));
        }
        if (m_cycleText) {
            m_cycleText->setText("PP cycles: " + std::to_string(m_pingPongCycles));
        }
    }

  protected:
    [[nodiscard]] std::string getExampleName() const override { return "Animation Demo"; }

    [[nodiscard]] std::vector<std::string> getFeatures() const override {
        return {
            "One-shot position tween with completion-chained second tween",
            "Ping-pong scale animation cycling automatically",
            "Pause / resume all Scene A animations",
            "Two independent scenes — Scene B advances regardless of Scene A state",
        };
    }

    [[nodiscard]] std::vector<std::string> getExpectedVisuals() const override {
        return {
            "Left panel: cyan square slides top-to-bottom then bottom-to-top repeatedly",
            "Left panel: yellow square pulses in scale (grows and shrinks)",
            "Right panel: purple square pulses in color (looping color animation)",
        };
    }

    [[nodiscard]] std::vector<std::string> getControls() const override {
        return {"P - Pause / resume Scene A animations"};
    }

  private:
    // -----------------------------------------------------------------------
    // UI helpers
    // -----------------------------------------------------------------------

    std::shared_ptr<TextEntity> makeHeader(const std::string& text, float y) {
        auto e = addEntity<TextEntity>();
        e->setText(text);
        e->setFont(BitmapFont::large());
        e->setStyle({.color = Color::white(), .pixelScale = 2});
        e->setPosition(0.0f, y, 0.0f);
        e->setWorldHeight(0.42f);
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
        makeHeader("SCENE A", 4.1f);

        // -- Position tween section -----------------------------------------
        makeLabel("POSITION TWEEN", 3.30f);
        m_roundTripText = makeValue("Round trips: 0", 2.85f, Color(0.40f, 0.85f, 0.85f, 1.0f));

        // Animated square (position tween target)
        m_slideBox = addEntity<SpriteEntity>();
        m_slideBox->setName("SlideBox");
        m_slideBox->setScale(0.55f, 0.55f, 1.0f);
        m_slideBox->setPosition(-2.5f, 1.8f, 0.0f);
        m_slideBox->setColor(Color(0.25f, 0.85f, 0.85f, 1.0f));

        // -- Ping-pong section ----------------------------------------------
        makeLabel("PING-PONG SCALE", 0.90f);
        m_cycleText = makeValue("PP cycles: 0", 0.45f, Color(0.85f, 0.85f, 0.30f, 1.0f));

        // Scale-animated square
        m_scaleBox = addEntity<SpriteEntity>();
        m_scaleBox->setName("ScaleBox");
        m_scaleBox->setScale(0.45f, 0.45f, 1.0f);
        m_scaleBox->setPosition(-2.5f, -0.60f, 0.0f);
        m_scaleBox->setColor(Color(0.85f, 0.75f, 0.20f, 1.0f));

        // -- Status ---------------------------------------------------------
        makeLabel("P = pause / resume", -2.20f, Color(0.45f, 0.45f, 0.55f, 1.0f));
        m_statusText = makeValue("RUNNING", -2.70f, Color(0.30f, 0.85f, 0.45f, 1.0f));
    }

    // -----------------------------------------------------------------------
    // Animation scheduling
    // -----------------------------------------------------------------------

    void schedulePositionTween() {
        // Slide from y=+1.8 to y=+0.05 over 1.0s then chain back.
        EntityId boxId = m_slideBox->getId();

        animations().schedule<SpriteEntity>(
            *this, AnimationBinding<SpriteEntity>::entity(boxId),
            {.duration = 1.0f, .easing = AnimationEasing::EaseInOutCubic},
            {.onUpdate =
                 [](SpriteEntity& e, const AnimationContext& ctx) {
                     float y = 1.8f + (0.05f - 1.8f) * ctx.easedProgress;
                     e.setPosition(-2.5f, y, 0.0f);
                 },
             .onComplete =
                 [this, boxId](SpriteEntity& e, const AnimationContext&) {
                     // Snap to destination.
                     e.setPosition(-2.5f, 0.05f, 0.0f);
                     // Chain: slide back up.
                     animations().schedule<SpriteEntity>(
                         *this, AnimationBinding<SpriteEntity>::entity(boxId),
                         {.duration = 1.0f, .easing = AnimationEasing::EaseInOutCubic},
                         {.onUpdate =
                              [](SpriteEntity& e2, const AnimationContext& ctx) {
                                  float y = 0.05f + (1.8f - 0.05f) * ctx.easedProgress;
                                  e2.setPosition(-2.5f, y, 0.0f);
                              },
                          .onComplete =
                              [this](SpriteEntity& e2, const AnimationContext&) {
                                  e2.setPosition(-2.5f, 1.8f, 0.0f);
                                  ++m_positionRoundTrips;
                                  // Chain again for continuous demo.
                                  schedulePositionTween();
                              }});
                 }});
    }

    void scheduleAnimations() {
        // 1. Position tween (chaining pattern).
        schedulePositionTween();

        // 2. Ping-pong scale animation.
        EntityId scaleBoxId = m_scaleBox->getId();
        animations().schedule<SpriteEntity>(*this,
                                            AnimationBinding<SpriteEntity>::entity(scaleBoxId),
                                            {.duration = 0.65f,
                                             .playback = AnimationPlayback::PingPong,
                                             .easing = AnimationEasing::EaseInOutSine},
                                            {.onUpdate =
                                                 [](SpriteEntity& e, const AnimationContext& ctx) {
                                                     float s = 0.35f + 0.35f * ctx.easedProgress;
                                                     e.setScale(s, s, 1.0f);
                                                 },
                                             .onComplete = nullptr});

        // Track ping-pong cycles via a separate unbound scheduler.
        animations().schedule({.duration = 0.65f, .playback = AnimationPlayback::PingPong},
                              {.onUpdate = [this](const AnimationContext& ctx) {
                                  // Each forward→reverse pass counts.
                                  if (ctx.reversePass && ctx.cycleIndex > m_lastCycleIndex) {
                                      m_pingPongCycles = ctx.cycleIndex;
                                      m_lastCycleIndex = ctx.cycleIndex;
                                  }
                              }});
    }

    // -----------------------------------------------------------------------
    // State
    // -----------------------------------------------------------------------

    std::shared_ptr<SpriteEntity> m_slideBox;
    std::shared_ptr<SpriteEntity> m_scaleBox;
    std::shared_ptr<TextEntity> m_roundTripText;
    std::shared_ptr<TextEntity> m_cycleText;
    std::shared_ptr<TextEntity> m_statusText;

    int m_positionRoundTrips = 0;
    uint32_t m_pingPongCycles = 0;
    uint32_t m_lastCycleIndex = 0;
    bool m_paused = false;
};

// ============================================================================
// Scene B — looping color animation + independent speed-scaled animation
// ============================================================================

class SceneB : public vde::Scene {
  public:
    [[nodiscard]] bool didValidationPass() const { return m_loopCycles >= 2; }

    void onEnter() override {
        setup2D(WORLD_W, WORLD_H, Color(0.08f, 0.06f, 0.18f, 1.0f));

        buildUI();
        scheduleAnimations();
    }

    void update(float dt) override {
        Scene::update(dt);

        if (m_loopCycleText) {
            m_loopCycleText->setText("Loop cycles: " + std::to_string(m_loopCycles));
        }
    }

  private:
    std::shared_ptr<TextEntity> makeHeader(const std::string& text, float y) {
        auto e = addEntity<TextEntity>();
        e->setText(text);
        e->setFont(BitmapFont::large());
        e->setStyle({.color = Color::white(), .pixelScale = 2});
        e->setPosition(0.0f, y, 0.0f);
        e->setWorldHeight(0.42f);
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
        makeHeader("SCENE B", 4.1f);
        makeLabel("(independent)", 3.55f, Color(0.55f, 0.50f, 0.75f, 1.0f));

        // -- Loop color section ----------------------------------------------
        makeLabel("LOOP COLOR ANIM", 2.90f);
        m_loopCycleText = makeValue("Loop cycles: 0", 2.45f, Color(0.75f, 0.50f, 0.95f, 1.0f));

        // Color-animated square
        m_colorBox = addEntity<SpriteEntity>();
        m_colorBox->setName("ColorBox");
        m_colorBox->setScale(0.70f, 0.70f, 1.0f);
        m_colorBox->setPosition(-1.5f, 1.20f, 0.0f);
        m_colorBox->setColor(Color(0.40f, 0.15f, 0.85f, 1.0f));

        // -- Speed-scaled section -------------------------------------------
        makeLabel("SPEED x2 SCALE", 0.10f);
        makeLabel("(2x faster than Scene A)", -0.38f, Color(0.50f, 0.50f, 0.65f, 1.0f));

        m_fastBox = addEntity<SpriteEntity>();
        m_fastBox->setName("FastBox");
        m_fastBox->setScale(0.45f, 0.45f, 1.0f);
        m_fastBox->setPosition(-1.5f, -1.10f, 0.0f);
        m_fastBox->setColor(Color(0.85f, 0.30f, 0.55f, 1.0f));

        makeLabel("Scene A pausing does", -2.50f, Color(0.45f, 0.40f, 0.60f, 1.0f));
        makeLabel("NOT pause Scene B.", -3.00f, Color(0.45f, 0.40f, 0.60f, 1.0f));
    }

    void scheduleAnimations() {
        // 1. Looping color animation — purple ↔ cyan.
        EntityId colorBoxId = m_colorBox->getId();
        animations().schedule<SpriteEntity>(
            *this, AnimationBinding<SpriteEntity>::entity(colorBoxId),
            {.duration = 0.80f,
             .playback = AnimationPlayback::Loop,
             .easing = AnimationEasing::EaseInOutSine},
            {.onUpdate = [](SpriteEntity& e, const AnimationContext& ctx) {
                Color from{0.40f, 0.15f, 0.85f, 1.0f};
                Color to{0.15f, 0.85f, 0.80f, 1.0f};
                float t = ctx.easedProgress;
                e.setColor({from.r + (to.r - from.r) * t, from.g + (to.g - from.g) * t,
                            from.b + (to.b - from.b) * t, 1.0f});
            }});

        // Track loop cycle count.
        animations().schedule({.duration = 0.80f, .playback = AnimationPlayback::Loop},
                              {.onUpdate = [this](const AnimationContext& ctx) {
                                  if (ctx.cycleIndex > m_loopCycles) {
                                      m_loopCycles = ctx.cycleIndex;
                                  }
                              }});

        // 2. Speed-scaled ping-pong scale animation (speed = 2.0).
        EntityId fastBoxId = m_fastBox->getId();
        animations().schedule<SpriteEntity>(
            *this, AnimationBinding<SpriteEntity>::entity(fastBoxId),
            {.duration = 0.65f,
             .speed = 2.0f,
             .playback = AnimationPlayback::PingPong,
             .easing = AnimationEasing::EaseOutCubic},
            {.onUpdate = [](SpriteEntity& e, const AnimationContext& ctx) {
                float s = 0.30f + 0.45f * ctx.easedProgress;
                e.setScale(s, s, 1.0f);
            }});
    }

    std::shared_ptr<SpriteEntity> m_colorBox;
    std::shared_ptr<SpriteEntity> m_fastBox;
    std::shared_ptr<TextEntity> m_loopCycleText;

    uint32_t m_loopCycles = 0;
};

// ============================================================================
// Game
// ============================================================================

class AnimationDemoGame : public vde::Game {
  public:
    void onStart() override {
        m_input = std::make_unique<AnimDemoInputHandler>();
        setInputHandler(m_input.get());

        auto* a = new SceneA();
        auto* b = new SceneB();
        m_sceneA = a;
        m_sceneB = b;

        addScene("sceneA", a);
        addScene("sceneB", b);

        auto group = SceneGroup::createWithViewports(
            "split", {
                         {.sceneName = "sceneA", .viewport = ViewportRect::leftHalf()},
                         {.sceneName = "sceneB", .viewport = ViewportRect::rightHalf()},
                     });
        setActiveSceneGroup(group);
        setFocusedScene("sceneA");
    }

    void onShutdown() override {
        bool passed = true;

        if (m_sceneA) {
            if (!m_sceneA->didValidationPass()) {
                std::cerr << "[FAIL] Scene A: position round trips or ping-pong cycles not met\n";
                passed = false;
            }
            if (m_sceneA->didTestFail()) {
                passed = false;
            }
        }

        if (m_sceneB) {
            if (!m_sceneB->didValidationPass()) {
                std::cerr << "[FAIL] Scene B: loop cycle count not met\n";
                passed = false;
            }
        }

        if (passed) {
            std::cout << "[PASS] Animation Demo: all animation milestones reached\n";
        }

        m_exitCode = passed ? 0 : 1;
    }

    [[nodiscard]] int getExitCode() const override {
        if (m_exitCode != 0)
            return m_exitCode;
        return Game::getExitCode();
    }

  private:
    std::unique_ptr<AnimDemoInputHandler> m_input;
    SceneA* m_sceneA = nullptr;
    SceneB* m_sceneB = nullptr;
    int m_exitCode = 0;
};

// ============================================================================
// Entry point
// ============================================================================

// NOLINTNEXTLINE(bugprone-exception-escape)
int main(int argc, char** argv) {
    AnimationDemoGame game;
    return vde::examples::runExample(game, "VDE Animation Demo", 1280, 720, argc, argv);
}
