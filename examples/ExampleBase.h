/**
 * @file ExampleBase.h
 * @brief Shared base classes and utilities for VDE examples.
 *
 * This header provides common functionality for all VDE examples:
 * - Auto-termination after a configured duration
 * - User verification via 'F' key to report failures
 * - Early exit via ESC key
 * - Standardized console output
 * - Clear pass/fail reporting
 *
 * Usage:
 * 1. Inherit from BaseExampleInputHandler for your input handler
 * 2. Inherit from BaseExampleScene for your scene
 * 3. Inherit from BaseExampleGame for your game (Game API examples only)
 * 4. Call printExampleHeader() in onEnter() to display standard info
 * 5. Override getExampleName(), getFeatures(), getExpectedVisuals(), and getControls()
 */

#pragma once

#include <vde/Window.h>
#include <vde/api/GameAPI.h>

#include <iostream>
#include <string>
#include <vector>

namespace vde {
namespace examples {

/**
 * @brief Base input handler with escape and fail key support.
 *
 * Provides standard functionality for:
 * - ESC key for early exit
 * - F key for reporting test failures
 *
 * Derive from this class and add your own keys as needed.
 */
class BaseExampleInputHandler : public vde::InputHandler {
  public:
    BaseExampleInputHandler() = default;
    virtual ~BaseExampleInputHandler() = default;

    void onKeyPress(int key) override {
        if (key == vde::KEY_ESCAPE) {
            m_escapePressed = true;
        }
        if (key == vde::KEY_F) {
            m_failPressed = true;
        }
        if (key == vde::KEY_F11) {
            m_fullscreenTogglePressed = true;
        }
    }

    /**
     * @brief Check if escape was pressed (clears flag).
     */
    bool isEscapePressed() {
        bool val = m_escapePressed;
        m_escapePressed = false;
        return val;
    }

    /**
     * @brief Check if fail key was pressed (clears flag).
     */
    bool isFailPressed() {
        bool val = m_failPressed;
        m_failPressed = false;
        return val;
    }

    /**
     * @brief Check if fullscreen toggle was pressed (clears flag).
     */
    bool isFullscreenTogglePressed() {
        bool val = m_fullscreenTogglePressed;
        m_fullscreenTogglePressed = false;
        return val;
    }

  protected:
    bool m_escapePressed = false;
    bool m_failPressed = false;
    bool m_fullscreenTogglePressed = false;
};

/**
 * @brief Base scene class with standard example testing pattern.
 *
 * Provides:
 * - Auto-termination after configured time
 * - Escape key for early exit
 * - F key for reporting failures
 * - Standardized console output
 * - Test pass/fail tracking
 *
 * To use:
 * 1. Inherit from this class
 * 2. Override getExampleName(), getFeatures(), getExpectedVisuals(), getControls()
 * 3. Call printExampleHeader() in your onEnter()
 * 4. Call BaseExampleScene::update(deltaTime) in your update()
 */
class BaseExampleScene : public vde::Scene {
  public:
    /**
     * @brief Construct a scene with auto-termination time.
     * @param autoTerminateSeconds Time in seconds before auto-closing (default: 15.0)
     */
    explicit BaseExampleScene(float autoTerminateSeconds = 15.0f)
        : m_autoTerminateSeconds(autoTerminateSeconds) {}

    virtual ~BaseExampleScene() = default;

    void update(float deltaTime) override {
        Scene::update(deltaTime);
        m_elapsedTime += deltaTime;

        // Check for input (works with any BaseExampleInputHandler)
        auto* input = dynamic_cast<BaseExampleInputHandler*>(getInputHandler());
        if (input) {
            // Check for fail key
            if (input->isFailPressed()) {
                handleTestFailure();
                return;
            }

            // Check for fullscreen toggle
            if (input->isFullscreenTogglePressed()) {
                if (getGame() && getGame()->getWindow()) {
                    auto* window = getGame()->getWindow();
                    window->setFullscreen(!window->isFullscreen());
                }
            }

            // Check for escape key
            if (input->isEscapePressed()) {
                handleEarlyExit();
                return;
            }
        }

        // Auto-terminate after configured time
        if (m_elapsedTime >= m_autoTerminateSeconds) {
            handleTestSuccess();
        }
    }

    /**
     * @brief Check if the test failed.
     */
    bool didTestFail() const { return m_testFailed; }

  protected:
    /**
     * @brief Get the example name (e.g., "Simple Game", "Sprite System").
     */
    virtual std::string getExampleName() const = 0;

    /**
     * @brief Get list of features demonstrated.
     */
    virtual std::vector<std::string> getFeatures() const = 0;

    /**
     * @brief Get description of expected visuals.
     */
    virtual std::vector<std::string> getExpectedVisuals() const = 0;

    /**
     * @brief Get list of controls (excluding standard ESC/F).
     */
    virtual std::vector<std::string> getControls() const { return {}; }

    /**
     * @brief Get custom failure message (optional).
     */
    virtual std::string getFailureMessage() const { return ""; }

    /**
     * @brief Print the standard example header with instructions.
     */
    void printExampleHeader() {
        std::cout << "\n========================================" << std::endl;
        std::cout << "  VDE Example: " << getExampleName() << std::endl;
        std::cout << "========================================\n" << std::endl;

        std::cout << "Features demonstrated:" << std::endl;
        for (const auto& feature : getFeatures()) {
            std::cout << "  - " << feature << std::endl;
        }

        std::cout << "\nYou should see:" << std::endl;
        for (const auto& visual : getExpectedVisuals()) {
            std::cout << "  - " << visual << std::endl;
        }

        std::cout << "\nControls:" << std::endl;
        auto customControls = getControls();
        for (const auto& control : customControls) {
            std::cout << "  " << control << std::endl;
        }

        std::cout << "  F11   - Toggle fullscreen" << std::endl;
        std::cout << "  F     - Fail test (if visuals are incorrect)" << std::endl;
        std::cout << "  ESC   - Exit early" << std::endl;
        std::cout << "  (Auto-closes in " << m_autoTerminateSeconds << " seconds)\n" << std::endl;
    }

    /**
     * @brief Handle test failure (F key pressed).
     */
    virtual void handleTestFailure() {
        std::cerr << "\n========================================" << std::endl;
        std::cerr << "  TEST FAILED: User reported issue" << std::endl;

        std::string customMsg = getFailureMessage();
        if (!customMsg.empty()) {
            std::cerr << "  " << customMsg << std::endl;
        } else {
            std::cerr << "  Expected: " << std::endl;
            for (const auto& visual : getExpectedVisuals()) {
                std::cerr << "    - " << visual << std::endl;
            }
        }

        std::cerr << "========================================\n" << std::endl;
        m_testFailed = true;
        if (getGame())
            getGame()->quit();
    }

    /**
     * @brief Handle early exit (ESC key pressed).
     */
    virtual void handleEarlyExit() {
        std::cout << "User requested early exit." << std::endl;
        if (getGame())
            getGame()->quit();
    }

    /**
     * @brief Handle test success (auto-termination).
     */
    virtual void handleTestSuccess() {
        std::cout << "\n========================================" << std::endl;
        std::cout << "  TEST PASSED: Demo completed successfully" << std::endl;
        std::cout << "  Duration: " << m_elapsedTime << " seconds" << std::endl;
        std::cout << "========================================\n" << std::endl;
        if (getGame())
            getGame()->quit();
    }

    float m_elapsedTime = 0.0f;
    float m_autoTerminateSeconds;
    bool m_testFailed = false;
};

/**
 * @brief Base game class for examples using the Game API.
 *
 * Provides:
 * - Input handler management
 * - Scene reference for test failure checking
 * - Exit code handling (0 for pass, 1 for fail)
 *
 * Template parameter TInputHandler: Your input handler type (must derive from
 * BaseExampleInputHandler) Template parameter TScene: Your scene type (must derive from
 * BaseExampleScene)
 */
template <typename TInputHandler, typename TScene>
class BaseExampleGame : public vde::Game {
    static_assert(std::is_base_of<BaseExampleInputHandler, TInputHandler>::value,
                  "TInputHandler must derive from BaseExampleInputHandler");
    static_assert(std::is_base_of<BaseExampleScene, TScene>::value,
                  "TScene must derive from BaseExampleScene");

  public:
    BaseExampleGame() = default;
    virtual ~BaseExampleGame() = default;

    void onStart() override {
        // Set up input handler
        m_inputHandler = std::make_unique<TInputHandler>();
        setInputHandler(m_inputHandler.get());

        // Create scene - addScene takes ownership
        auto* scene = new TScene();
        m_scenePtr = scene;
        addScene("main", scene);
        setActiveScene("main");
    }

    void onShutdown() override {
        if (m_scenePtr && m_scenePtr->didTestFail()) {
            m_exitCode = 1;
        }
    }

    /**
     * @brief Get the exit code (0 for success, 1 for failure).
     */
    int getExitCode() const { return m_exitCode; }

  protected:
    /**
     * @brief Get the input handler (for derived classes).
     */
    TInputHandler* getExampleInputHandler() { return m_inputHandler.get(); }

    /**
     * @brief Get the scene (for derived classes).
     */
    TScene* getExampleScene() { return m_scenePtr; }

  private:
    std::unique_ptr<TInputHandler> m_inputHandler;
    TScene* m_scenePtr = nullptr;  // Non-owning reference
    int m_exitCode = 0;
};

/**
 * @brief Helper function to run an example game with standard settings.
 *
 * @param game The game instance to run
 * @param gameName The name of the game/example
 * @param width Window width (default: 1280)
 * @param height Window height (default: 720)
 * @return Exit code (0 for success, 1 for failure)
 */
template <typename TGame>
int runExample(TGame& game, const std::string& gameName, uint32_t width = 1280,
               uint32_t height = 720) {
    vde::GameSettings settings;
    settings.gameName = gameName;
    settings.display.windowWidth = width;
    settings.display.windowHeight = height;
    settings.display.fullscreen = false;

    try {
        if (!game.initialize(settings)) {
            std::cerr << "Failed to initialize " << gameName << "!" << std::endl;
            return 1;
        }

        game.run();
        return game.getExitCode();

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
}

}  // namespace examples
}  // namespace vde
