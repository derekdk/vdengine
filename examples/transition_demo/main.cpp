/**
 * @file main.cpp
 * @brief Transition Demo - demonstrates screen transition effects
 *
 * This example demonstrates:
 * - Fade transition (cross-fade between scenes)
 * - Wipe transition (directional edge wipe, left and right)
 * - Circle reveal transition (expanding circle from center)
 * - Adjustable transition duration
 * - Transition cancellation
 * - Pause & frame-step for debugging transitions
 * - Adjustable playback speed
 * - onEnter/onExit lifecycle logging
 *
 * Controls:
 * - 1: Fade to GameScene
 * - 2: Wipe Left to CreditsScene
 * - 3: Wipe Right to MainMenuScene
 * - 4: Circle Reveal to GameScene
 * - 5: Random Block Fall to ShowcaseScene
 * - +/=: Increase transition duration (+0.25s)
 * - -: Decrease transition duration (-0.25s, min 0.25s)
 * - C: Cancel in-flight transition
 * - SPACE: Pause / unpause transition
 * - . (Period): Step forward one frame (while paused)
 * - S: Cycle playback speed (1x -> 0.5x -> 0.25x -> 0.1x -> 1x)
 * - ESC: Exit
 * - F: Report failure
 */

#include <vde/api/GameAPI.h>

#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "../ExampleBase.h"

using namespace vde;

// ============================================================================
// Input Handler
// ============================================================================

class TransitionInputHandler : public vde::examples::BaseExampleInputHandler {
  public:
    void onKeyPress(int key) override {
        BaseExampleInputHandler::onKeyPress(key);

        if (key == KEY_1)
            m_triggerFade = true;
        if (key == KEY_2)
            m_triggerWipeLeft = true;
        if (key == KEY_3)
            m_triggerWipeRight = true;
        if (key == KEY_4)
            m_triggerCircleReveal = true;
        if (key == KEY_5)
            m_triggerBlockFall = true;
        if (key == KEY_EQUAL || key == KEY_KP_ADD)
            m_increaseDuration = true;
        if (key == KEY_MINUS || key == KEY_KP_SUBTRACT)
            m_decreaseDuration = true;
        if (key == KEY_C)
            m_cancelTransition = true;
        if (key == KEY_SPACE)
            m_togglePause = true;
        if (key == KEY_PERIOD)
            m_stepFrame = true;
        if (key == KEY_S)
            m_cycleSpeed = true;
    }

    bool consumeFade() {
        bool v = m_triggerFade;
        m_triggerFade = false;
        return v;
    }
    bool consumeWipeLeft() {
        bool v = m_triggerWipeLeft;
        m_triggerWipeLeft = false;
        return v;
    }
    bool consumeWipeRight() {
        bool v = m_triggerWipeRight;
        m_triggerWipeRight = false;
        return v;
    }
    bool consumeCircleReveal() {
        bool v = m_triggerCircleReveal;
        m_triggerCircleReveal = false;
        return v;
    }
    bool consumeBlockFall() {
        bool v = m_triggerBlockFall;
        m_triggerBlockFall = false;
        return v;
    }
    bool consumeIncreaseDuration() {
        bool v = m_increaseDuration;
        m_increaseDuration = false;
        return v;
    }
    bool consumeDecreaseDuration() {
        bool v = m_decreaseDuration;
        m_decreaseDuration = false;
        return v;
    }
    bool consumeCancel() {
        bool v = m_cancelTransition;
        m_cancelTransition = false;
        return v;
    }
    bool consumeTogglePause() {
        bool v = m_togglePause;
        m_togglePause = false;
        return v;
    }
    bool consumeStepFrame() {
        bool v = m_stepFrame;
        m_stepFrame = false;
        return v;
    }
    bool consumeCycleSpeed() {
        bool v = m_cycleSpeed;
        m_cycleSpeed = false;
        return v;
    }

  private:
    bool m_triggerFade = false;
    bool m_triggerWipeLeft = false;
    bool m_triggerWipeRight = false;
    bool m_triggerCircleReveal = false;
    bool m_triggerBlockFall = false;
    bool m_increaseDuration = false;
    bool m_decreaseDuration = false;
    bool m_cancelTransition = false;
    bool m_togglePause = false;
    bool m_stepFrame = false;
    bool m_cycleSpeed = false;
};

// ============================================================================
// MainMenu Scene — solid blue background with title-like entities
// ============================================================================

class MainMenuScene : public vde::examples::BaseExampleScene {
  public:
    MainMenuScene() : BaseExampleScene(60.0f) {}

    void onEnter() override {
        printExampleHeader();
        std::cout << "[MainMenuScene] onEnter()" << std::endl;

        setBackgroundColor({0.1f, 0.15f, 0.4f, 1.0f});

        // Create some visual entities
        auto camera = std::make_unique<Camera2D>(20.0f, 20.0f);
        setCamera(std::move(camera));
        auto boxMesh = Mesh::createCube(1.0f);

        // Title entity (large box)
        auto title = std::make_unique<MeshEntity>();
        title->setMesh(boxMesh);
        title->setPosition(0.0f, 2.0f, 0.0f);
        title->setScale(6.0f, 1.5f, 0.5f);
        title->setColor(Color{0.9f, 0.9f, 0.2f, 1.0f});
        addEntity(std::move(title));

        // Subtitle entity
        auto subtitle = std::make_unique<MeshEntity>();
        subtitle->setMesh(boxMesh);
        subtitle->setPosition(0.0f, 0.0f, 0.0f);
        subtitle->setScale(4.0f, 0.8f, 0.5f);
        subtitle->setColor(Color{0.7f, 0.7f, 0.9f, 1.0f});
        addEntity(std::move(subtitle));

        // Decorative entities
        for (int i = -3; i <= 3; ++i) {
            auto dot = std::make_unique<MeshEntity>();
            dot->setMesh(boxMesh);
            dot->setPosition(static_cast<float>(i) * 1.5f, -2.5f, 0.0f);
            dot->setScale(0.4f, 0.4f, 0.4f);
            dot->setColor(Color{0.3f, 0.6f, 1.0f, 1.0f});
            addEntity(std::move(dot));
        }
    }

    void onExit() override { std::cout << "[MainMenuScene] onExit()" << std::endl; }

    void update(float deltaTime) override {
        BaseExampleScene::update(deltaTime);

        // Animate the title entity with a gentle bob
        if (!getEntities().empty()) {
            float t = static_cast<float>(getGame()->getTotalTime());
            auto& title = getEntities()[0];
            float y = 2.0f + std::sin(t * 1.5f) * 0.3f;
            title->setPosition(0.0f, y, 0.0f);
        }
    }

  protected:
    std::string getExampleName() const override { return "Screen Transition Demo"; }

    std::vector<std::string> getFeatures() const override {
        return {"Fade transition (cross-fade)", "Wipe transition (left/right)",
                "Circle reveal transition",     "Random block-fall transition (32x32)",
                "Adjustable duration (+/-)",    "Cancel mid-transition (C)"};
    }

    std::vector<std::string> getExpectedVisuals() const override {
        return {"Blue scene (MainMenu) with yellow/purple boxes",
                "Green scene (Game) with moving orange entities",
                "Dark red scene (Credits) with white/gray boxes",
                "Dark blue showcase scene with dense animated meshes",
                "Smooth transitions between scenes"};
    }

    std::vector<std::string> getControls() const override {
        return {"1 - Fade to GameScene",       "2 - Wipe Left to CreditsScene",
                "3 - Wipe Right to MainMenu",  "4 - Circle Reveal to GameScene",
                "5 - Block Fall to Showcase",  "+/- - Adjust duration",
                "C - Cancel transition",       "SPACE - Pause/unpause transition",
                ". (Period) - Step one frame", "S - Cycle speed (1x/0.5x/0.25x/0.1x)"};
    }
};

// ============================================================================
// Game Scene — green background with moving entities
// ============================================================================

class GameScene : public Scene {
  public:
    void onEnter() override {
        std::cout << "[GameScene] onEnter()" << std::endl;

        setBackgroundColor({0.1f, 0.35f, 0.15f, 1.0f});

        auto camera = std::make_unique<Camera2D>(20.0f, 20.0f);
        setCamera(std::move(camera));
        auto boxMesh = Mesh::createCube(1.0f);

        // Several moving entities
        for (int i = 0; i < 8; ++i) {
            auto entity = std::make_unique<MeshEntity>();
            entity->setMesh(boxMesh);
            float x = static_cast<float>(i - 4) * 2.0f;
            float y = (i % 2 == 0) ? 1.5f : -1.5f;
            entity->setPosition(x, y, 0.0f);
            entity->setScale(0.8f);
            entity->setColor(Color{0.9f, 0.5f + (i * 0.05f), 0.1f, 1.0f});
            addEntity(std::move(entity));
        }

        // Central entity
        auto center = std::make_unique<MeshEntity>();
        center->setMesh(boxMesh);
        center->setPosition(0.0f, 0.0f, 0.0f);
        center->setScale(2.0f, 2.0f, 0.5f);
        center->setColor(Color{1.0f, 0.8f, 0.2f, 1.0f});
        addEntity(std::move(center));
    }

    void onExit() override { std::cout << "[GameScene] onExit()" << std::endl; }

    void update(float deltaTime) override {
        (void)deltaTime;
        float t = static_cast<float>(getGame()->getTotalTime());

        // Animate entities in a circular motion
        auto& entities = getEntities();
        for (size_t i = 0; i < entities.size() - 1; ++i) {
            float angle = t * 1.2f + static_cast<float>(i) * 0.8f;
            float radius = 3.0f + static_cast<float>(i % 3);
            float x = std::cos(angle) * radius;
            float y = std::sin(angle) * radius;
            entities[i]->setPosition(x, y, 0.0f);
        }

        // Pulse the center entity
        float scale = 1.5f + std::sin(t * 3.0f) * 0.5f;
        entities.back()->setScale(scale, scale, 0.5f);
    }
};

// ============================================================================
// Credits Scene — dark red background with static layout
// ============================================================================

class CreditsScene : public Scene {
  public:
    void onEnter() override {
        std::cout << "[CreditsScene] onEnter()" << std::endl;

        setBackgroundColor({0.35f, 0.1f, 0.1f, 1.0f});

        auto camera = std::make_unique<Camera2D>(20.0f, 20.0f);
        setCamera(std::move(camera));
        auto boxMesh = Mesh::createCube(1.0f);

        // Title bar
        auto title = std::make_unique<MeshEntity>();
        title->setMesh(boxMesh);
        title->setPosition(0.0f, 3.5f, 0.0f);
        title->setScale(8.0f, 1.0f, 0.5f);
        title->setColor(Color{0.95f, 0.95f, 0.95f, 1.0f});
        addEntity(std::move(title));

        // Credit lines
        for (int i = 0; i < 5; ++i) {
            auto line = std::make_unique<MeshEntity>();
            line->setMesh(boxMesh);
            line->setPosition(0.0f, 1.5f - static_cast<float>(i) * 1.2f, 0.0f);
            float width = 5.0f - static_cast<float>(i) * 0.5f;
            line->setScale(width, 0.5f, 0.5f);
            float gray = 0.6f + (i * 0.05f);
            line->setColor(Color{gray, gray, gray, 1.0f});
            addEntity(std::move(line));
        }

        // Footer
        auto footer = std::make_unique<MeshEntity>();
        footer->setMesh(boxMesh);
        footer->setPosition(0.0f, -4.0f, 0.0f);
        footer->setScale(3.0f, 0.6f, 0.5f);
        footer->setColor(Color{0.8f, 0.3f, 0.3f, 1.0f});
        addEntity(std::move(footer));
    }

    void onExit() override { std::cout << "[CreditsScene] onExit()" << std::endl; }

    void update(float deltaTime) override {
        (void)deltaTime;
        // Static scene — credits don't need animation
    }
};

// ============================================================================
// Showcase Scene — dense animated geometry for transition visibility
// ============================================================================

class ShowcaseScene : public Scene {
  public:
    void onEnter() override {
        std::cout << "[ShowcaseScene] onEnter()" << std::endl;

        setBackgroundColor({0.05f, 0.08f, 0.16f, 1.0f});

        auto camera = std::make_unique<Camera2D>(24.0f, 18.0f);
        setCamera(std::move(camera));

        auto cubeMesh = Mesh::createCube(1.0f);
        auto sphereMesh = Mesh::createSphere(0.65f, 20, 20);
        auto pyramidMesh = Mesh::createPyramid(1.0f, 1.2f);

        auto center = std::make_unique<MeshEntity>();
        center->setMesh(cubeMesh);
        center->setPosition(0.0f, 0.0f, 0.0f);
        center->setScale(2.1f, 2.1f, 0.7f);
        center->setColor(Color{0.95f, 0.75f, 0.2f, 1.0f});
        addEntity(std::move(center));

        for (int i = 0; i < 18; ++i) {
            auto entity = std::make_unique<MeshEntity>();
            if (i % 3 == 0) {
                entity->setMesh(cubeMesh);
            } else if (i % 3 == 1) {
                entity->setMesh(sphereMesh);
            } else {
                entity->setMesh(pyramidMesh);
            }

            const float angle = static_cast<float>(i) * (6.2831853f / 18.0f);
            const float radius = 4.5f + static_cast<float>(i % 4) * 0.85f;
            entity->setPosition(std::cos(angle) * radius, std::sin(angle) * radius, 0.0f);
            entity->setScale(0.5f + static_cast<float>(i % 5) * 0.08f);
            entity->setColor(Color{0.2f + 0.04f * static_cast<float>(i),
                                   0.9f - 0.03f * static_cast<float>(i),
                                   0.45f + 0.02f * static_cast<float>(i), 1.0f});
            addEntity(std::move(entity));
        }

        for (int i = 0; i < 10; ++i) {
            auto topNode = std::make_unique<MeshEntity>();
            topNode->setMesh(cubeMesh);
            topNode->setPosition(-9.0f + static_cast<float>(i) * 2.0f, 6.3f, 0.0f);
            topNode->setScale(0.8f, 0.35f, 0.3f);
            topNode->setColor(Color{0.3f, 0.8f, 1.0f, 1.0f});
            addEntity(std::move(topNode));

            auto bottomNode = std::make_unique<MeshEntity>();
            bottomNode->setMesh(cubeMesh);
            bottomNode->setPosition(9.0f - static_cast<float>(i) * 2.0f, -6.3f, 0.0f);
            bottomNode->setScale(0.8f, 0.35f, 0.3f);
            bottomNode->setColor(Color{1.0f, 0.5f, 0.4f, 1.0f});
            addEntity(std::move(bottomNode));
        }
    }

    void onExit() override { std::cout << "[ShowcaseScene] onExit()" << std::endl; }

    void update(float deltaTime) override {
        (void)deltaTime;
        const float t = static_cast<float>(getGame()->getTotalTime());

        auto& entities = getEntities();
        if (entities.empty()) {
            return;
        }

        const float pulse = 1.8f + std::sin(t * 2.4f) * 0.35f;
        entities[0]->setScale(pulse, pulse, 0.7f);
        entities[0]->setRotation(0.0f, 0.0f, t * 90.0f);

        for (size_t i = 1; i <= 18; ++i) {
            const float phase = static_cast<float>(i) * 0.45f;
            const float radius = 4.2f + std::sin(t * 0.8f + phase) * 1.5f;
            const float angle = t * (0.7f + (static_cast<float>(i % 5) * 0.12f)) + phase;
            const float x = std::cos(angle) * radius;
            const float y = std::sin(angle) * radius;
            entities[i]->setPosition(x, y, 0.0f);
            entities[i]->setRotation(0.0f, 0.0f, (t * 50.0f) + phase * 35.0f);
        }

        const size_t topStart = 19;
        const size_t bottomStart = 29;
        for (size_t i = 0; i < 10; ++i) {
            const float f = static_cast<float>(i);
            const float xTop = -9.5f + std::fmod((t * 4.0f) + f * 1.7f, 20.0f);
            const float xBottom = 9.5f - std::fmod((t * 4.0f) + f * 1.7f, 20.0f);
            const float yOffset = std::sin(t * 3.0f + f) * 0.25f;

            entities[topStart + i]->setPosition(xTop, 6.3f + yOffset, 0.0f);
            entities[bottomStart + i]->setPosition(xBottom, -6.3f - yOffset, 0.0f);
        }
    }
};

// ============================================================================
// Transition Demo Game
// ============================================================================

class TransitionDemoGame
    : public vde::examples::BaseExampleGame<TransitionInputHandler, MainMenuScene> {
  public:
    TransitionDemoGame() = default;

    void onStart() override {
        BaseExampleGame::onStart();

        // Add additional scenes
        addScene("game", new GameScene());
        addScene("credits", new CreditsScene());
        addScene("showcase", new ShowcaseScene());

        std::cout << "\n[TransitionDemo] Duration: " << std::fixed << std::setprecision(2)
                  << m_transitionDuration << "s\n"
                  << std::endl;
    }

    void onUpdate(float deltaTime) override {
        (void)deltaTime;

        auto* input = dynamic_cast<TransitionInputHandler*>(getInputHandler());
        if (!input) {
            return;
        }

        // Duration controls
        if (input->consumeIncreaseDuration()) {
            m_transitionDuration += 0.25f;
            std::cout << "[TransitionDemo] Duration: " << std::fixed << std::setprecision(2)
                      << m_transitionDuration << "s" << std::endl;
        }
        if (input->consumeDecreaseDuration()) {
            m_transitionDuration = std::max(0.25f, m_transitionDuration - 0.25f);
            std::cout << "[TransitionDemo] Duration: " << std::fixed << std::setprecision(2)
                      << m_transitionDuration << "s" << std::endl;
        }

        // Cancel
        if (input->consumeCancel()) {
            if (isTransitioning()) {
                std::cout << "[TransitionDemo] Cancelling transition" << std::endl;
                cancelTransition();
            }
        }

        // Pause / unpause
        if (input->consumeTogglePause()) {
            if (isTransitioning()) {
                bool nowPaused = !isTransitionPaused();
                setTransitionPaused(nowPaused);
                std::cout << "[TransitionDemo] " << (nowPaused ? "PAUSED" : "RESUMED")
                          << " (progress: " << std::fixed << std::setprecision(3)
                          << getTransitionProgress() << ")" << std::endl;
            }
        }

        // Step one frame
        if (input->consumeStepFrame()) {
            if (isTransitioning() && isTransitionPaused()) {
                stepTransitionOneFrame();
                std::cout << "[TransitionDemo] STEP -> progress: " << std::fixed
                          << std::setprecision(3) << getTransitionProgress() << std::endl;
            }
        }

        // Cycle playback speed
        if (input->consumeCycleSpeed()) {
            constexpr float speeds[] = {1.0f, 0.5f, 0.25f, 0.1f};
            constexpr int numSpeeds = 4;
            float current = getTransitionSpeed();
            int idx = 0;
            for (int i = 0; i < numSpeeds; ++i) {
                if (std::abs(current - speeds[i]) < 0.01f) {
                    idx = (i + 1) % numSpeeds;
                    break;
                }
            }
            setTransitionSpeed(speeds[idx]);
            std::cout << "[TransitionDemo] Speed: " << std::fixed << std::setprecision(2)
                      << speeds[idx] << "x" << std::endl;
        }

        // Transition triggers
        if (input->consumeFade()) {
            std::cout << "[TransitionDemo] Fade -> GameScene (" << std::fixed
                      << std::setprecision(2) << m_transitionDuration << "s)" << std::endl;
            transitionToScene("game", std::make_unique<FadeTransition>(), m_transitionDuration);
        }
        if (input->consumeWipeLeft()) {
            std::cout << "[TransitionDemo] Wipe Left -> CreditsScene (" << std::fixed
                      << std::setprecision(2) << m_transitionDuration << "s)" << std::endl;
            transitionToScene("credits",
                              std::make_unique<WipeTransition>(TransitionDirection::Left),
                              m_transitionDuration);
        }
        if (input->consumeWipeRight()) {
            std::cout << "[TransitionDemo] Wipe Right -> MainMenuScene (" << std::fixed
                      << std::setprecision(2) << m_transitionDuration << "s)" << std::endl;
            transitionToScene("main", std::make_unique<WipeTransition>(TransitionDirection::Right),
                              m_transitionDuration);
        }
        if (input->consumeCircleReveal()) {
            std::cout << "[TransitionDemo] Circle Reveal -> GameScene (" << std::fixed
                      << std::setprecision(2) << m_transitionDuration << "s)" << std::endl;
            transitionToScene("game", std::make_unique<CircleRevealTransition>(),
                              m_transitionDuration);
        }
        if (input->consumeBlockFall()) {
            std::cout << "[TransitionDemo] Block Fall -> ShowcaseScene (" << std::fixed
                      << std::setprecision(2) << m_transitionDuration << "s)" << std::endl;
            transitionToScene("showcase", std::make_unique<BlockFallTransition>(),
                              m_transitionDuration);
        }
    }

  private:
    float m_transitionDuration = 1.0f;
};

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    TransitionDemoGame demo;
    return vde::examples::runExample(demo, "VDE Transition Demo", 1280, 720, argc, argv);
}
