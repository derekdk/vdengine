/**
 * @file main.cpp
 * @brief Timed Events Demo — demonstrates scene-owned TimedEvents service.
 *
 * Two independent scenes run side by side in split-screen viewports.
 * Scene A (left half) shows one-shot callbacks, repeating timers,
 * pause/resume, and mid-flight cancellation.
 * Scene B (right half) shows a separate set of timers that continue
 * independently when Scene A is paused.
 *
 * Self-validation: the demo tracks expected callback counts and exits
 * with code 0 on success or code 1 if any expected event did not fire.
 *
 * Controls:
 *   P   - Pause / resume Scene A timers (Scene B keeps running)
 *   C   - Cancel the cancellable timer in Scene A
 *   ESC - Exit early
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
// Layout constants — each scene uses an 8×9 world (aspect matches 640×720 px)
// ============================================================================

static constexpr float WORLD_W = 8.0f;
static constexpr float WORLD_H = 9.0f;

// ============================================================================
// Input Handler
// ============================================================================

class TimedEventsInputHandler : public vde::examples::BaseExampleInputHandler {
  public:
    TimedEventsInputHandler() {
        m_keys.bindOneShot(KEY_P, "pause");
        m_keys.bindOneShot(KEY_C, "cancel");
    }

    void onKeyPress(int key) override {
        BaseExampleInputHandler::onKeyPress(key);
        m_keys.handlePress(key);
    }

    void onKeyRelease(int key) override { m_keys.handleRelease(key); }

    bool consumePause() { return m_keys.consume("pause"); }
    bool consumeCancel() { return m_keys.consume("cancel"); }

  private:
    KeyStateTracker m_keys;
};

// ============================================================================
// Scene A — main timed events scene (left half)
// ============================================================================

class SceneA : public vde::examples::BaseExampleScene {
  public:
    SceneA() : BaseExampleScene(10.0f) {}

    // Returns true if the demo's expected callbacks all fired.
    [[nodiscard]] bool didValidationPass() const { return m_oneShotFired && m_repeatCount >= 4; }

    void onEnter() override {
        printExampleHeader();
        setup2D(WORLD_W, WORLD_H, Color(0.07f, 0.09f, 0.18f, 1.0f));

        buildUI();

        auto& t = getTimedEvents();

        // One-shot: fires once after 1.0 second
        t.after(1.0f, [this]() {
            m_oneShotFired = true;
            if (m_oneShotStatus) {
                m_oneShotStatus->setText("Fired!");
            }
        });

        // Repeating: every 500 ms
        t.every(0.5f, [this]() {
            ++m_repeatCount;
            if (m_counterStatus) {
                m_counterStatus->setText("Count: " + std::to_string(m_repeatCount));
            }
        });

        // Cancellable: every 300 ms — user can cancel with C key
        m_cancelHandle = t.every(0.3f, [this]() {
            ++m_cancelCount;
            if (m_cancelStatus) {
                m_cancelStatus->setText("Count: " + std::to_string(m_cancelCount));
            }
        });
    }

    void update(float dt) override {
        BaseExampleScene::update(dt);

        auto* input = dynamic_cast<TimedEventsInputHandler*>(getInputHandler());
        if (!input) {
            return;
        }

        if (input->consumePause()) {
            auto& t = getTimedEvents();
            if (t.isPaused()) {
                t.resume();
                if (m_pauseStatus) {
                    m_pauseStatus->setText("RUNNING");
                }
            } else {
                t.pause();
                if (m_pauseStatus) {
                    m_pauseStatus->setText("PAUSED");
                }
            }
        }

        if (input->consumeCancel() && !m_cancelled) {
            m_cancelled = true;
            getTimedEvents().cancel(m_cancelHandle);
            if (m_cancelStatus) {
                m_cancelStatus->setText("Count: " + std::to_string(m_cancelCount) + " [cancelled]");
            }
        }
    }

  protected:
    [[nodiscard]] std::string getExampleName() const override { return "Timed Events Demo"; }

    [[nodiscard]] std::vector<std::string> getFeatures() const override {
        return {
            "One-shot callback after a configurable delay",
            "Repeating callback at a fixed interval",
            "Pause and resume all scene timers",
            "Mid-flight cancellation of a single timer",
            "Two independent scenes — Scene B unaffected by Scene A pause",
        };
    }

    [[nodiscard]] std::vector<std::string> getExpectedVisuals() const override {
        return {
            "Left panel (Scene A): one-shot label changes to 'Fired!' after 1 second",
            "Left panel: counter increments every 500 ms",
            "Right panel (Scene B): counter increments independently every 400 ms",
        };
    }

    [[nodiscard]] std::vector<std::string> getControls() const override {
        return {
            "P - Pause / resume Scene A timers",
            "C - Cancel the cancellable timer in Scene A",
        };
    }

  private:
    // ---- UI helpers -------------------------------------------------------

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
        e->setWorldHeight(0.30f);
        return e;
    }

    std::shared_ptr<TextEntity> makeValue(const std::string& text, float y,
                                          Color col = Color(0.40f, 0.85f, 0.50f, 1.0f)) {
        auto e = addEntity<TextEntity>();
        e->setText(text);
        e->setFont(BitmapFont::small());
        e->setStyle({.color = col, .pixelScale = 2});
        e->setPosition(0.0f, y, 0.0f);
        e->setWorldHeight(0.30f);
        return e;
    }

    void buildUI() {
        makeHeader("SCENE A", 3.9f);

        // -- One-shot section ------------------------------------------------
        makeLabel("ONE-SHOT  (delay 1.0s)", 2.90f);
        m_oneShotStatus = makeValue("Waiting...", 2.42f, Color(0.95f, 0.75f, 0.30f, 1.0f));

        // -- Repeating section -----------------------------------------------
        makeLabel("COUNTER  (every 500 ms)", 1.60f);
        m_counterStatus = makeValue("Count: 0", 1.12f);

        // -- Cancellable section ---------------------------------------------
        makeLabel("CANCELLABLE  (every 300 ms)", -0.10f);
        m_cancelStatus = makeValue("Count: 0", -0.58f, Color(0.70f, 0.55f, 0.95f, 1.0f));

        // -- Pause status ---------------------------------------------------
        makeLabel("STATUS", -1.70f);
        m_pauseStatus = makeValue("RUNNING", -2.18f, Color(0.30f, 0.85f, 0.70f, 1.0f));

        // -- Hint -----------------------------------------------------------
        makeLabel("P=pause/resume  C=cancel", -3.30f, Color(0.45f, 0.45f, 0.55f, 1.0f));
    }

    // ---- State ------------------------------------------------------------
    std::shared_ptr<TextEntity> m_oneShotStatus;
    std::shared_ptr<TextEntity> m_counterStatus;
    std::shared_ptr<TextEntity> m_cancelStatus;
    std::shared_ptr<TextEntity> m_pauseStatus;

    int m_repeatCount = 0;
    int m_cancelCount = 0;
    bool m_oneShotFired = false;
    bool m_cancelled = false;
    TimedEventHandle m_cancelHandle = INVALID_TIMED_EVENT_HANDLE;
};

// ============================================================================
// Scene B — independent timed events scene (right half)
// ============================================================================

class SceneB : public vde::Scene {
  public:
    [[nodiscard]] bool didValidationPass() const { return m_repeatCount >= 4; }

    void onEnter() override {
        setup2D(WORLD_W, WORLD_H, Color(0.07f, 0.18f, 0.09f, 1.0f));

        buildUI();

        auto& t = getTimedEvents();

        // Independent repeating counter: every 400 ms
        t.every(0.4f, [this]() {
            ++m_repeatCount;
            if (m_counterStatus) {
                m_counterStatus->setText("Count: " + std::to_string(m_repeatCount));
            }
        });

        // Independent one-shot: fires after 1.5 seconds
        t.after(1.5f, [this]() {
            m_oneShotFired = true;
            if (m_oneShotStatus) {
                m_oneShotStatus->setText("Fired!");
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
        e->setWorldHeight(0.42f);
        return e;
    }

    std::shared_ptr<TextEntity> makeLabel(const std::string& text, float y,
                                          Color col = Color(0.75f, 0.85f, 0.75f, 1.0f)) {
        auto e = addEntity<TextEntity>();
        e->setText(text);
        e->setFont(BitmapFont::small());
        e->setStyle({.color = col, .pixelScale = 2});
        e->setPosition(0.0f, y, 0.0f);
        e->setWorldHeight(0.30f);
        return e;
    }

    std::shared_ptr<TextEntity> makeValue(const std::string& text, float y,
                                          Color col = Color(0.40f, 0.85f, 0.50f, 1.0f)) {
        auto e = addEntity<TextEntity>();
        e->setText(text);
        e->setFont(BitmapFont::small());
        e->setStyle({.color = col, .pixelScale = 2});
        e->setPosition(0.0f, y, 0.0f);
        e->setWorldHeight(0.30f);
        return e;
    }

    void buildUI() {
        makeHeader("SCENE B", 3.9f);
        makeLabel("(independent timers)", 3.30f, Color(0.55f, 0.70f, 0.55f, 1.0f));

        // -- One-shot section ------------------------------------------------
        makeLabel("ONE-SHOT  (delay 1.5s)", 2.20f);
        m_oneShotStatus = makeValue("Waiting...", 1.72f, Color(0.95f, 0.75f, 0.30f, 1.0f));

        // -- Repeating section -----------------------------------------------
        makeLabel("COUNTER  (every 400 ms)", 0.90f);
        m_counterStatus = makeValue("Count: 0", 0.42f);

        // -- Independence note -----------------------------------------------
        makeLabel("Pausing Scene A does", -1.00f, Color(0.45f, 0.60f, 0.45f, 1.0f));
        makeLabel("NOT pause Scene B.", -1.50f, Color(0.45f, 0.60f, 0.45f, 1.0f));
    }

    std::shared_ptr<TextEntity> m_oneShotStatus;
    std::shared_ptr<TextEntity> m_counterStatus;

    int m_repeatCount = 0;
    bool m_oneShotFired = false;
};

// ============================================================================
// Game class — sets up both scenes with split-screen viewports
// ============================================================================

class TimedEventsGame : public vde::Game {
  public:
    void onStart() override {
        m_input = std::make_unique<TimedEventsInputHandler>();
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
                std::cerr << "[FAIL] Scene A: one-shot or repeat count requirement not met\n";
                passed = false;
            }
            if (m_sceneA->didTestFail()) {
                passed = false;
            }
        }

        if (m_sceneB) {
            if (!m_sceneB->didValidationPass()) {
                std::cerr << "[FAIL] Scene B: repeat count requirement not met\n";
                passed = false;
            }
        }

        if (passed) {
            std::cout << "[PASS] Timed Events Demo: all expected callbacks fired\n";
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
    std::unique_ptr<TimedEventsInputHandler> m_input;
    SceneA* m_sceneA = nullptr;
    SceneB* m_sceneB = nullptr;
    int m_exitCode = 0;
};

// ============================================================================
// Entry point
// ============================================================================

// NOLINTNEXTLINE(bugprone-exception-escape)
int main(int argc, char** argv) {
    TimedEventsGame demo;
    return vde::examples::runExample(demo, "VDE Timed Events Demo", 1280, 720, argc, argv);
}
