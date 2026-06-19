#include <vde/api/GameAPI.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>

#include "../ExampleBase.h"

namespace {

constexpr const char* kStorageAppName = "vde_input_actions_demo";
constexpr const char* kBindingsStorageKey = "bindings";
constexpr const char* kLayoutStorageKey = "layout_name";

constexpr float kArenaHalfWidth = 4.2f;
constexpr float kArenaHalfHeight = 2.5f;
constexpr float kMoveSpeed = 2.35f;
constexpr float kBoostMultiplier = 1.8f;
constexpr float kPulseDuration = 0.55f;
constexpr float kReleaseDuration = 0.35f;

void addSharedBindings(vde::InputActionMap& actions) {
    actions.addBinding("move_left",
                       vde::InputActionBinding::gamepadButton(vde::GAMEPAD_BUTTON_DPAD_LEFT));
    actions.addBinding(
        "move_left", vde::InputActionBinding::gamepadAxisNegative(vde::GAMEPAD_AXIS_LEFT_X, 0.45f));
    actions.addBinding("move_right",
                       vde::InputActionBinding::gamepadButton(vde::GAMEPAD_BUTTON_DPAD_RIGHT));
    actions.addBinding("move_right", vde::InputActionBinding::gamepadAxisPositive(
                                         vde::GAMEPAD_AXIS_LEFT_X, 0.45f));
    actions.addBinding("move_up",
                       vde::InputActionBinding::gamepadButton(vde::GAMEPAD_BUTTON_DPAD_UP));
    actions.addBinding(
        "move_up", vde::InputActionBinding::gamepadAxisNegative(vde::GAMEPAD_AXIS_LEFT_Y, 0.45f));
    actions.addBinding("move_down",
                       vde::InputActionBinding::gamepadButton(vde::GAMEPAD_BUTTON_DPAD_DOWN));
    actions.addBinding(
        "move_down", vde::InputActionBinding::gamepadAxisPositive(vde::GAMEPAD_AXIS_LEFT_Y, 0.45f));

    actions.addBinding("pulse", vde::InputActionBinding::key(vde::KEY_SPACE));
    actions.addBinding("pulse", vde::InputActionBinding::gamepadButton(vde::GAMEPAD_BUTTON_A));

    actions.addBinding("boost", vde::InputActionBinding::key(vde::KEY_LEFT_SHIFT));
    actions.addBinding("boost",
                       vde::InputActionBinding::gamepadButton(vde::GAMEPAD_BUTTON_RIGHT_BUMPER));

    actions.addBinding("swap_layout", vde::InputActionBinding::key(vde::KEY_TAB));
    actions.addBinding("swap_layout",
                       vde::InputActionBinding::gamepadButton(vde::GAMEPAD_BUTTON_START));

    actions.addBinding("reset_layout", vde::InputActionBinding::key(vde::KEY_R));
    actions.addBinding("reset_layout",
                       vde::InputActionBinding::gamepadButton(vde::GAMEPAD_BUTTON_BACK));
}

}  // namespace

// ---------------------------------------------------------------------------
// Input handler
// ---------------------------------------------------------------------------

class InputActionsDemoInputHandler : public vde::examples::BaseExampleInputHandler {
  public:
    InputActionsDemoInputHandler() { applyClassicPreset(); }

    void onKeyPress(int key) override {
        BaseExampleInputHandler::onKeyPress(key);
        m_actions.handleKeyPress(key);
    }

    void onKeyRelease(int key) override { m_actions.handleKeyRelease(key); }

    void onGamepadButtonPress(int gamepadId, int button) override {
        m_actions.handleGamepadButtonPress(gamepadId, button);
    }

    void onGamepadButtonRelease(int gamepadId, int button) override {
        m_actions.handleGamepadButtonRelease(gamepadId, button);
    }

    void onGamepadAxis(int gamepadId, int axis, float value) override {
        m_actions.handleGamepadAxis(gamepadId, axis, value);
    }

    vde::InputActionMap& actions() { return m_actions; }
    [[nodiscard]] const vde::InputActionMap& actions() const { return m_actions; }

    void finishFrame() { m_actions.advanceFrame(); }

    void applyClassicPreset() {
        applyPreset("Classic", vde::KEY_A, vde::KEY_D, vde::KEY_W, vde::KEY_S, true);
    }

    void applyArcadePreset() {
        applyPreset("Arcade", vde::KEY_J, vde::KEY_L, vde::KEY_I, vde::KEY_K, false);
    }

    void togglePreset() {
        if (m_layoutName == "Arcade") {
            applyClassicPreset();
            return;
        }
        applyArcadePreset();
    }

    bool loadPersistedBindings() {
        if (!m_actions.loadBindings(kBindingsStorageKey)) {
            return false;
        }

        auto storedLayout = vde::StorageManager::getInstance().getStringData(kLayoutStorageKey);
        m_layoutName = storedLayout.value_or("Persisted");
        return true;
    }

    [[nodiscard]] bool savePersistedBindings() const {
        if (!m_actions.saveBindings(kBindingsStorageKey)) {
            return false;
        }
        return vde::StorageManager::getInstance().setStringData(kLayoutStorageKey, m_layoutName);
    }

    [[nodiscard]] const std::string& getLayoutName() const { return m_layoutName; }

    [[nodiscard]] bool isArcadePreset() const { return m_layoutName == "Arcade"; }

  private:
    void applyPreset(const std::string& layoutName, int leftKey, int rightKey, int upKey,
                     int downKey, bool includeArrows) {
        m_actions.clear();

        m_actions.addBinding("move_left", vde::InputActionBinding::key(leftKey));
        m_actions.addBinding("move_right", vde::InputActionBinding::key(rightKey));
        m_actions.addBinding("move_up", vde::InputActionBinding::key(upKey));
        m_actions.addBinding("move_down", vde::InputActionBinding::key(downKey));

        if (includeArrows) {
            m_actions.addBinding("move_left", vde::InputActionBinding::key(vde::KEY_LEFT));
            m_actions.addBinding("move_right", vde::InputActionBinding::key(vde::KEY_RIGHT));
            m_actions.addBinding("move_up", vde::InputActionBinding::key(vde::KEY_UP));
            m_actions.addBinding("move_down", vde::InputActionBinding::key(vde::KEY_DOWN));
        }

        addSharedBindings(m_actions);
        m_layoutName = layoutName;
    }

    vde::InputActionMap m_actions;
    std::string m_layoutName;
};

// ---------------------------------------------------------------------------
// Scene
// ---------------------------------------------------------------------------

class InputActionsDemoScene : public vde::examples::BaseExampleScene {
  public:
    InputActionsDemoScene() : BaseExampleScene(15.0f) {}

    void onEnter() override {
        printExampleHeader();

        auto* camera = new vde::Camera2D(10.0f, 7.5f);
        camera->setPosition(0.0f, 0.0f);
        camera->setZoom(1.0f);
        setCamera(camera);

        initializeStorageAndBindings();
        createArena();
        createIndicators();
        createPlayer();
        applyLayoutVisuals();
    }

    void update(float deltaTime) override {
        BaseExampleScene::update(deltaTime);  // handles ESC, F, auto-terminate

        auto* input = dynamic_cast<InputActionsDemoInputHandler*>(getInputHandler());
        if (!input) {
            return;
        }

        auto& actions = input->actions();

        if (actions.consumePressed("reset_layout")) {
            input->applyClassicPreset();
            persistBindings(*input);
            applyLayoutVisuals();
            std::cout << "Reset bindings to Classic preset\n";
        }

        if (actions.consumePressed("swap_layout")) {
            input->togglePreset();
            persistBindings(*input);
            applyLayoutVisuals();
            std::cout << "Swapped to " << input->getLayoutName() << " preset\n";
        }

        if (actions.consumePressed("pulse")) {
            m_pulseTimer = kPulseDuration;
            std::cout << "Pulse action fired\n";
        }

        if (actions.consumeReleased("boost")) {
            m_releaseFlashTimer = kReleaseDuration;
            std::cout << "Boost released\n";
        }

        glm::vec2 moveAxis(0.0f);
        if (actions.isHeld("move_left")) {
            moveAxis.x -= 1.0f;
        }
        if (actions.isHeld("move_right")) {
            moveAxis.x += 1.0f;
        }
        if (actions.isHeld("move_up")) {
            moveAxis.y += 1.0f;
        }
        if (actions.isHeld("move_down")) {
            moveAxis.y -= 1.0f;
        }

        const bool boosting = actions.isHeld("boost");
        const glm::vec2 moveDirection = vde::math2d::normalizeOrZero(moveAxis);
        const float moveSpeed = boosting ? kMoveSpeed * kBoostMultiplier : kMoveSpeed;
        m_playerPosition += moveDirection * moveSpeed * deltaTime;

        m_playerPosition.x = vde::math2d::clamp(m_playerPosition.x, -kArenaHalfWidth + 0.35f,
                                                kArenaHalfWidth - 0.35f);
        m_playerPosition.y = vde::math2d::clamp(m_playerPosition.y, -kArenaHalfHeight + 0.35f,
                                                kArenaHalfHeight - 0.35f);

        if (vde::math2d::lengthSquared(moveDirection) > 0.0001f) {
            m_lastDirection = moveDirection;
        }

        updatePlayerVisuals(deltaTime, boosting);
        input->finishFrame();
    }

  protected:
    [[nodiscard]] std::string getExampleName() const override { return "Input Actions Demo"; }

    [[nodiscard]] std::vector<std::string> getFeatures() const override {
        return {
            "Named actions with held, pressed, and released states",
            "Multiple bindings per action, including keyboard and gamepad inputs",
            "Binding persistence through StorageManager with preset swapping",
        };
    }

    [[nodiscard]] std::vector<std::string> getExpectedVisuals() const override {
        return {
            "A movable square inside a framed 2D arena",
            "A blue or amber top indicator showing the active binding preset",
            "A cyan pulse ring on pulse press and a red flash when boost is released",
        };
    }

    [[nodiscard]] std::vector<std::string> getControls() const override {
        return {
            "ESC - Exit",
            "F   - Report failure",
            "WASD or Arrows - Move in Classic preset",
            "IJKL - Move in Arcade preset",
            "SPACE - Pulse action",
            "LEFT SHIFT - Hold boost",
            "TAB - Swap and save the active preset",
            "R - Reset to Classic preset for deterministic automation",
        };
    }

  private:
    void initializeStorageAndBindings() {
        auto* input = dynamic_cast<InputActionsDemoInputHandler*>(getInputHandler());
        if (!input) {
            return;
        }

        if (!vde::StorageManager::getInstance().init_storage(kStorageAppName)) {
            input->applyClassicPreset();
            std::cout << "WARNING: Storage unavailable, using in-memory bindings only\n";
            return;
        }

        if (input->loadPersistedBindings()) {
            std::cout << "Loaded persisted bindings (layout: " << input->getLayoutName() << ")"
                      << '\n';
            return;
        }

        input->applyClassicPreset();
        persistBindings(*input);
        std::cout << "Initialized default Classic preset bindings\n";
    }

    void persistBindings(const InputActionsDemoInputHandler& input) const {
        if (vde::StorageManager::getInstance().isInitialized() && !input.savePersistedBindings()) {
            std::cout << "WARNING: Failed to save input bindings\n";
        }
    }

    void createArena() {
        setBackgroundColor(vde::Color::fromHex(0x14213d));

        auto addWall = [&](float x, float y, float sx, float sy, const vde::Color& color) {
            auto wall = addEntity<vde::SpriteEntity>();
            wall->setPosition(x, y, -0.1f);
            wall->setScale(sx, sy, 1.0f);
            wall->setColor(color);
        };

        addWall(0.0f, kArenaHalfHeight + 0.2f, 4.6f, 0.09f, vde::Color(0.95f, 0.95f, 0.98f, 0.9f));
        addWall(0.0f, -kArenaHalfHeight - 0.2f, 4.6f, 0.09f, vde::Color(0.95f, 0.95f, 0.98f, 0.9f));
        addWall(-kArenaHalfWidth - 0.2f, 0.0f, 0.09f, 2.9f, vde::Color(0.95f, 0.95f, 0.98f, 0.9f));
        addWall(kArenaHalfWidth + 0.2f, 0.0f, 0.09f, 2.9f, vde::Color(0.95f, 0.95f, 0.98f, 0.9f));

        auto lane = addEntity<vde::SpriteEntity>();
        lane->setPosition(0.0f, 0.0f, -0.2f);
        lane->setScale(4.0f, 2.35f, 1.0f);
        lane->setColor(vde::Color(0.05f, 0.08f, 0.14f, 0.55f));
    }

    void createIndicators() {
        m_classicIndicator = addEntity<vde::SpriteEntity>();
        m_classicIndicator->setPosition(-2.2f, 3.0f, 0.05f);
        m_classicIndicator->setScale(1.35f, 0.2f, 1.0f);

        m_arcadeIndicator = addEntity<vde::SpriteEntity>();
        m_arcadeIndicator->setPosition(2.2f, 3.0f, 0.05f);
        m_arcadeIndicator->setScale(1.35f, 0.2f, 1.0f);

        m_boostIndicator = addEntity<vde::SpriteEntity>();
        m_boostIndicator->setPosition(0.0f, -3.0f, 0.05f);
        m_boostIndicator->setScale(1.3f, 0.18f, 1.0f);
        m_boostIndicator->setColor(vde::Color(0.35f, 0.27f, 0.08f, 0.35f));
    }

    void createPlayer() {
        m_player = addEntity<vde::SpriteEntity>();
        m_player->setName("ActionPlayer");
        m_player->setPosition(0.0f, 0.0f, 0.2f);
        m_player->setScale(0.35f, 0.35f, 1.0f);

        m_directionGhost = addEntity<vde::SpriteEntity>();
        m_directionGhost->setPosition(0.8f, 0.0f, 0.15f);
        m_directionGhost->setScale(0.22f, 0.22f, 1.0f);
        m_directionGhost->setColor(vde::Color(0.6f, 0.8f, 1.0f, 0.5f));

        m_pulseRing = addEntity<vde::SpriteEntity>();
        m_pulseRing->setPosition(0.0f, 0.0f, 0.1f);
        m_pulseRing->setScale(0.4f, 0.4f, 1.0f);
        m_pulseRing->setColor(vde::Color(0.25f, 0.9f, 1.0f, 0.0f));

        m_releaseFlash = addEntity<vde::SpriteEntity>();
        m_releaseFlash->setPosition(0.0f, 0.0f, 0.18f);
        m_releaseFlash->setScale(0.25f, 0.25f, 1.0f);
        m_releaseFlash->setColor(vde::Color(1.0f, 0.25f, 0.3f, 0.0f));
    }

    void applyLayoutVisuals() {
        auto* input = dynamic_cast<InputActionsDemoInputHandler*>(getInputHandler());
        const bool arcade = input && input->isArcadePreset();

        setBackgroundColor(arcade ? vde::Color::fromHex(0x2d1b12) : vde::Color::fromHex(0x14213d));

        if (m_classicIndicator) {
            m_classicIndicator->setColor(arcade ? vde::Color(0.24f, 0.38f, 0.58f, 0.35f)
                                                : vde::Color(0.30f, 0.62f, 1.0f, 0.95f));
        }
        if (m_arcadeIndicator) {
            m_arcadeIndicator->setColor(arcade ? vde::Color(1.0f, 0.66f, 0.22f, 0.95f)
                                               : vde::Color(0.58f, 0.36f, 0.18f, 0.35f));
        }
    }

    void updatePlayerVisuals(float deltaTime, bool boosting) {
        m_player->setPosition(m_playerPosition.x, m_playerPosition.y, 0.2f);

        const vde::Color baseColor =
            boosting ? vde::Color(1.0f, 0.84f, 0.25f, 1.0f) : vde::Color(0.45f, 0.96f, 0.72f, 1.0f);
        m_player->setColor(baseColor);
        const float playerScale = boosting ? 0.42f : 0.35f;
        m_player->setScale(playerScale, playerScale, 1.0f);

        const glm::vec2 ghostOffset = m_lastDirection * (boosting ? 0.95f : 0.7f);
        m_directionGhost->setPosition(m_playerPosition.x + ghostOffset.x,
                                      m_playerPosition.y + ghostOffset.y, 0.15f);
        m_directionGhost->setColor(vde::Color(0.55f, 0.82f, 1.0f, boosting ? 0.9f : 0.45f));
        const float ghostScale = boosting ? 0.30f : 0.22f;
        m_directionGhost->setScale(ghostScale, ghostScale, 1.0f);

        m_boostIndicator->setColor(boosting ? vde::Color(1.0f, 0.86f, 0.25f, 0.95f)
                                            : vde::Color(0.35f, 0.27f, 0.08f, 0.35f));

        m_pulseTimer = std::max(0.0f, m_pulseTimer - deltaTime);
        const float pulseT = 1.0f - (m_pulseTimer / kPulseDuration);
        m_pulseRing->setPosition(m_playerPosition.x, m_playerPosition.y, 0.1f);
        m_pulseRing->setScale(0.45f + pulseT * 0.95f, 0.45f + pulseT * 0.95f, 1.0f);
        m_pulseRing->setColor(
            vde::Color(0.25f, 0.9f, 1.0f, vde::math2d::saturate(m_pulseTimer / kPulseDuration)));

        m_releaseFlashTimer = std::max(0.0f, m_releaseFlashTimer - deltaTime);
        const float releaseScale =
            0.24f + (1.0f - (m_releaseFlashTimer / kReleaseDuration)) * 0.55f;
        m_releaseFlash->setPosition(m_playerPosition.x, m_playerPosition.y, 0.18f);
        m_releaseFlash->setScale(releaseScale, releaseScale, 1.0f);
        m_releaseFlash->setColor(vde::Color(
            1.0f, 0.25f, 0.3f, vde::math2d::saturate(m_releaseFlashTimer / kReleaseDuration)));
    }

    glm::vec2 m_playerPosition{0.0f, 0.0f};
    glm::vec2 m_lastDirection{1.0f, 0.0f};
    float m_pulseTimer = 0.0f;
    float m_releaseFlashTimer = 0.0f;

    std::shared_ptr<vde::SpriteEntity> m_player;
    std::shared_ptr<vde::SpriteEntity> m_directionGhost;
    std::shared_ptr<vde::SpriteEntity> m_pulseRing;
    std::shared_ptr<vde::SpriteEntity> m_releaseFlash;
    std::shared_ptr<vde::SpriteEntity> m_classicIndicator;
    std::shared_ptr<vde::SpriteEntity> m_arcadeIndicator;
    std::shared_ptr<vde::SpriteEntity> m_boostIndicator;
};

// ---------------------------------------------------------------------------
// Game class
// ---------------------------------------------------------------------------

class InputActionsDemoGame
    : public vde::examples::BaseExampleGame<InputActionsDemoInputHandler, InputActionsDemoScene> {};

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

// NOLINTNEXTLINE(bugprone-exception-escape)
int main(int argc, char** argv) {
    InputActionsDemoGame demo;
    return vde::examples::runExample(demo, "VDE Input Actions Demo", 1280, 720, argc, argv);
}
