/**
 * @file main.cpp
 * @brief Multi-Scene Animation Demo — timed events and animations across two independent scenes.
 *
 * Two scenes run side by side in split-screen viewports.
 *
 * Scene A (left half):
 *   - A timed event fires after 1.0 s and starts a position animation on a square.
 *   - The position animation chains back when it completes, creating a round trip.
 *   - Demonstrates: timed-event → animation handoff in the same scene.
 *
 * Scene B (right half):
 *   - A repeating timed event fires every 0.5 s and toggles the color of a square.
 *   - A separate looping rotation-scale animation runs concurrently.
 *   - Scene B advances independently — its timer and animation are not gated on Scene A.
 *
 * Self-validation:
 *   Scene A must complete at least two position round trips within the run window.
 *   Scene B must fire its repeating timer at least four times and complete at least
 *   two animation cycles within the run window.
 *   Exit code 0 on success, 1 on failure.
 *
 * Controls:
 *   ESC - Exit early (exit code 1)
 *   F   - Report test failure
 */

#include <vde/api/GameAPI.h>

#include <iostream>
#include <sstream>
#include <string>

#include "../ExampleBase.h"

using namespace vde;

// ============================================================================
// Layout constants — each scene uses an 8×9 world (aspect matches 640×720 px)
// ============================================================================

static constexpr float WORLD_W = 8.0f;
static constexpr float WORLD_H = 9.0f;

// ============================================================================
// Scene A — timed event triggers an animation
// ============================================================================

class SceneA : public vde::examples::BaseExampleScene {
  public:
    SceneA() : BaseExampleScene(6.0f) {}

    [[nodiscard]] bool didValidationPass() const { return m_roundTrips >= 2; }

    void onEnter() override {
        printExampleHeader();
        setup2D(WORLD_W, WORLD_H, Color(0.06f, 0.10f, 0.20f, 1.0f));

        buildUI();

        // Timed event fires after 1.0 s and starts the position animation.
        getTimedEvents().after(1.0f, [this]() { startPositionAnimation(); });
    }

    void update(float dt) override {
        BaseExampleScene::update(dt);
        if (m_countText) {
            m_countText->setText("Round trips: " + std::to_string(m_roundTrips));
        }
    }

  protected:
    [[nodiscard]] std::string getExampleName() const override {
        return "Multi-Scene Animation Demo";
    }

    [[nodiscard]] std::vector<std::string> getFeatures() const override {
        return {
            "Timed event after 1 s triggers a position animation on Scene A",
            "Animation chains back on completion (round trip)",
            "Scene B repeating timer runs independently every 0.5 s",
            "Both scenes validated independently — neither gates the other",
        };
    }

    [[nodiscard]] std::vector<std::string> getExpectedVisuals() const override {
        return {
            "Left panel: cyan square begins sliding after 1 s and continues in a loop",
            "Right panel: orange square alternates colors every 0.5 s from the start",
            "Right panel: purple square pulses in scale independently",
        };
    }

    [[nodiscard]] std::vector<std::string> getControls() const override { return {}; }

  private:
    void buildUI() {
        auto makeHeader = [this](const std::string& text, float y) {
            auto e = addEntity<TextEntity>();
            e->setText(text);
            e->setFont(BitmapFont::large());
            e->setStyle({.color = Color::white(), .pixelScale = 2});
            e->setPosition(0.0f, y, 0.0f);
            e->setWorldHeight(0.42f);
            return e;
        };

        auto makeLabel = [this](const std::string& text, float y,
                                Color col = Color(0.75f, 0.75f, 0.85f, 1.0f)) {
            auto e = addEntity<TextEntity>();
            e->setText(text);
            e->setFont(BitmapFont::small());
            e->setStyle({.color = col, .pixelScale = 2});
            e->setPosition(0.0f, y, 0.0f);
            e->setWorldHeight(0.28f);
            return e;
        };

        auto makeValue = [this](const std::string& text, float y,
                                Color col = Color(0.40f, 0.85f, 0.50f, 1.0f)) {
            auto e = addEntity<TextEntity>();
            e->setText(text);
            e->setFont(BitmapFont::small());
            e->setStyle({.color = col, .pixelScale = 2});
            e->setPosition(0.0f, y, 0.0f);
            e->setWorldHeight(0.28f);
            return e;
        };

        makeHeader("SCENE A", 4.1f);
        makeLabel("Timed event @1s", 3.50f);
        makeLabel("triggers animation", 3.05f);

        m_countText = makeValue("Round trips: 0", 2.50f, Color(0.40f, 0.85f, 0.85f, 1.0f));

        // The sliding square.
        m_slideBox = addEntity<SpriteEntity>();
        m_slideBox->setName("SlideBox");
        m_slideBox->setScale(0.55f, 0.55f, 1.0f);
        m_slideBox->setPosition(0.0f, 2.0f, 0.0f);
        m_slideBox->setColor(Color(0.25f, 0.85f, 0.85f, 1.0f));

        makeLabel("(starts after 1 s)", -1.50f, Color(0.50f, 0.50f, 0.65f, 1.0f));
        makeLabel("Goal: 2 round trips", -2.00f, Color(0.50f, 0.50f, 0.65f, 1.0f));
    }

    void startPositionAnimation() {
        EntityId boxId = m_slideBox->getId();

        // Slide down from y=+2.0 to y=-1.5 over 0.8 s, then chain back.
        animations().schedule<SpriteEntity>(
            *this, AnimationBinding<SpriteEntity>::entity(boxId),
            {.duration = 0.8f, .easing = AnimationEasing::EaseInOutCubic, .customEasing = nullptr},
            {.onStart = nullptr,
             .onUpdate =
                 [](SpriteEntity& e, const AnimationContext& ctx) {
                     float y = 2.0f + (-1.5f - 2.0f) * ctx.easedProgress;
                     e.setPosition(0.0f, y, 0.0f);
                 },
             .onComplete =
                 [this, boxId](SpriteEntity& e, const AnimationContext&) {
                     e.setPosition(0.0f, -1.5f, 0.0f);
                     // Chain: slide back up.
                     animations().schedule<SpriteEntity>(
                         *this, AnimationBinding<SpriteEntity>::entity(boxId),
                         {.duration = 0.8f,
                          .easing = AnimationEasing::EaseInOutCubic,
                          .customEasing = nullptr},
                         {.onStart = nullptr,
                          .onUpdate =
                              [](SpriteEntity& e2, const AnimationContext& ctx) {
                                  float y = -1.5f + (2.0f - (-1.5f)) * ctx.easedProgress;
                                  e2.setPosition(0.0f, y, 0.0f);
                              },
                          .onComplete =
                              [this](SpriteEntity& e2, const AnimationContext&) {
                                  e2.setPosition(0.0f, 2.0f, 0.0f);
                                  ++m_roundTrips;
                                  // Keep looping.
                                  startPositionAnimation();
                              }});
                 }});
    }

    std::shared_ptr<SpriteEntity> m_slideBox;
    std::shared_ptr<TextEntity> m_countText;
    int m_roundTrips = 0;
};

// ============================================================================
// Scene B — repeating timer + independent looping animation
// ============================================================================

class SceneB : public vde::Scene {
  public:
    [[nodiscard]] bool didValidationPass() const { return m_timerFires >= 4 && m_animCycles >= 2; }

    void onEnter() override {
        setup2D(WORLD_W, WORLD_H, Color(0.10f, 0.06f, 0.18f, 1.0f));

        buildUI();

        // Repeating timer — toggles square color every 0.5 s.
        getTimedEvents().every(0.5f, [this]() {
            ++m_timerFires;
            m_colorToggle = !m_colorToggle;
            if (m_toggleBox) {
                m_toggleBox->setColor(m_colorToggle ? Color(0.95f, 0.55f, 0.10f, 1.0f)
                                                    : Color(0.95f, 0.90f, 0.20f, 1.0f));
            }
        });

        scheduleAnimations();
    }

    void update(float dt) override {
        Scene::update(dt);
        if (m_timerText) {
            m_timerText->setText("Timer fires: " + std::to_string(m_timerFires));
        }
        if (m_cycleText) {
            m_cycleText->setText("Anim cycles: " + std::to_string(m_animCycles));
        }
    }

  private:
    void buildUI() {
        auto makeHeader = [this](const std::string& text, float y) {
            auto e = addEntity<TextEntity>();
            e->setText(text);
            e->setFont(BitmapFont::large());
            e->setStyle({.color = Color::white(), .pixelScale = 2});
            e->setPosition(0.0f, y, 0.0f);
            e->setWorldHeight(0.42f);
            return e;
        };

        auto makeLabel = [this](const std::string& text, float y,
                                Color col = Color(0.75f, 0.75f, 0.85f, 1.0f)) {
            auto e = addEntity<TextEntity>();
            e->setText(text);
            e->setFont(BitmapFont::small());
            e->setStyle({.color = col, .pixelScale = 2});
            e->setPosition(0.0f, y, 0.0f);
            e->setWorldHeight(0.28f);
            return e;
        };

        auto makeValue = [this](const std::string& text, float y,
                                Color col = Color(0.40f, 0.85f, 0.50f, 1.0f)) {
            auto e = addEntity<TextEntity>();
            e->setText(text);
            e->setFont(BitmapFont::small());
            e->setStyle({.color = col, .pixelScale = 2});
            e->setPosition(0.0f, y, 0.0f);
            e->setWorldHeight(0.28f);
            return e;
        };

        makeHeader("SCENE B", 4.1f);
        makeLabel("(independent)", 3.55f, Color(0.65f, 0.50f, 0.85f, 1.0f));

        // Color-toggle section.
        makeLabel("REPEATING TIMER", 3.00f);
        makeLabel("every 0.5 s", 2.55f, Color(0.60f, 0.60f, 0.70f, 1.0f));
        m_timerText = makeValue("Timer fires: 0", 2.05f, Color(0.95f, 0.75f, 0.30f, 1.0f));

        m_toggleBox = addEntity<SpriteEntity>();
        m_toggleBox->setName("ToggleBox");
        m_toggleBox->setScale(0.55f, 0.55f, 1.0f);
        m_toggleBox->setPosition(0.0f, 1.10f, 0.0f);
        m_toggleBox->setColor(Color(0.95f, 0.90f, 0.20f, 1.0f));

        // Pulse animation section.
        makeLabel("LOOP SCALE ANIM", -0.10f);
        m_cycleText = makeValue("Anim cycles: 0", -0.55f, Color(0.65f, 0.50f, 0.95f, 1.0f));

        m_pulseBox = addEntity<SpriteEntity>();
        m_pulseBox->setName("PulseBox");
        m_pulseBox->setScale(0.45f, 0.45f, 1.0f);
        m_pulseBox->setPosition(0.0f, -1.30f, 0.0f);
        m_pulseBox->setColor(Color(0.55f, 0.25f, 0.85f, 1.0f));

        makeLabel("Goal: 4 fires + 2 cycles", -2.70f, Color(0.45f, 0.40f, 0.60f, 1.0f));
    }

    void scheduleAnimations() {
        // Looping scale animation on the pulse box.
        EntityId pulseId = m_pulseBox->getId();
        animations().schedule<SpriteEntity>(*this, AnimationBinding<SpriteEntity>::entity(pulseId),
                                            {.duration = 0.60f,
                                             .playback = AnimationPlayback::Loop,
                                             .easing = AnimationEasing::EaseInOutSine,
                                             .customEasing = nullptr},
                                            {.onStart = nullptr,
                                             .onUpdate =
                                                 [](SpriteEntity& e, const AnimationContext& ctx) {
                                                     float s = 0.30f + 0.40f * ctx.easedProgress;
                                                     e.setScale(s, s, 1.0f);
                                                 },
                                             .onComplete = nullptr});

        // Unbound tracker to count completed animation cycles.
        animations().schedule(
            {.duration = 0.60f, .playback = AnimationPlayback::Loop, .customEasing = nullptr},
            {.onStart = nullptr,
             .onUpdate =
                 [this](const AnimationContext& ctx) {
                     if (ctx.cycleIndex > m_animCycles) {
                         m_animCycles = ctx.cycleIndex;
                     }
                 },
             .onComplete = nullptr});
    }

    std::shared_ptr<SpriteEntity> m_toggleBox;
    std::shared_ptr<SpriteEntity> m_pulseBox;
    std::shared_ptr<TextEntity> m_timerText;
    std::shared_ptr<TextEntity> m_cycleText;

    int m_timerFires = 0;
    uint32_t m_animCycles = 0;
    bool m_colorToggle = false;
};

// ============================================================================
// Game
// ============================================================================

class MultiSceneAnimationDemoGame : public vde::Game {
  public:
    void onStart() override {
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

        if (m_sceneA && !m_sceneA->didValidationPass()) {
            std::cerr << "[FAIL] Scene A: position round trip goal not met"
                         " (need >= 2 round trips)\n";
            passed = false;
        }
        if (m_sceneA && m_sceneA->didTestFail()) {
            passed = false;
        }
        if (m_sceneB && !m_sceneB->didValidationPass()) {
            std::cerr << "[FAIL] Scene B: repeating timer or animation cycle goal not met"
                         " (need >= 4 fires and >= 2 cycles)\n";
            passed = false;
        }

        if (passed) {
            std::cout << "[PASS] Multi-Scene Animation Demo: all milestones reached\n";
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
    MultiSceneAnimationDemoGame game;
    return vde::examples::runExample(game, "VDE Multi-Scene Animation Demo", 1280, 720, argc, argv);
}
