#include <vde/api/GameAPI.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>

#include "../ExampleBase.h"

namespace {

constexpr float kViewWidth = 18.0f;
constexpr float kViewHeight = 10.0f;
constexpr float kWorldLimitX = 34.0f;
constexpr float kWorldLimitY = 18.0f;
constexpr float kPlayerSpeed = 7.0f;
constexpr float kFrameThickness = 0.08f;
constexpr float kDeadzoneWidth = 4.6f;
constexpr float kDeadzoneHeight = 2.6f;
constexpr float kLookAheadDistance = 2.5f;
constexpr float kLookAheadSeconds = 0.18f;
constexpr float kLookAheadSmoothing = 9.0f;
constexpr float kShakeFlashDuration = 0.22f;

enum class CameraFeelMode : std::uint8_t {
    Base,
    Deadzone,
    LookAhead,
    Zoom,
    Combined,
};

bool usesDeadzone(CameraFeelMode mode) {
    return mode == CameraFeelMode::Deadzone || mode == CameraFeelMode::Combined;
}

bool usesLookAhead(CameraFeelMode mode) {
    return mode == CameraFeelMode::LookAhead || mode == CameraFeelMode::Combined;
}

const char* modeName(CameraFeelMode mode) {
    switch (mode) {
    case CameraFeelMode::Base:
        return "Follow";
    case CameraFeelMode::Deadzone:
        return "Deadzone";
    case CameraFeelMode::LookAhead:
        return "Look-Ahead";
    case CameraFeelMode::Zoom:
        return "Smooth Zoom";
    case CameraFeelMode::Combined:
        return "Combined";
    }

    return "Unknown";
}

vde::Color modeColor(CameraFeelMode mode) {
    switch (mode) {
    case CameraFeelMode::Base:
        return {0.68f, 0.74f, 0.84f, 0.95f};
    case CameraFeelMode::Deadzone:
        return {0.26f, 0.64f, 1.0f, 0.95f};
    case CameraFeelMode::LookAhead:
        return {1.0f, 0.66f, 0.26f, 0.95f};
    case CameraFeelMode::Zoom:
        return {0.45f, 0.96f, 0.72f, 0.95f};
    case CameraFeelMode::Combined:
        return {0.92f, 0.38f, 0.84f, 0.98f};
    }

    return {1.0f, 1.0f, 1.0f, 1.0f};
}

glm::vec2 clampMagnitude(const glm::vec2& value, float maxMagnitude) {
    if (maxMagnitude <= 0.0f) {
        return glm::vec2(0.0f);
    }

    const float lengthSquared = glm::dot(value, value);
    if (lengthSquared <= (maxMagnitude * maxMagnitude)) {
        return value;
    }

    return glm::normalize(value) * maxMagnitude;
}

}  // namespace

// ---------------------------------------------------------------------------
// Input handler
// ---------------------------------------------------------------------------

class CameraFeelInputHandler : public vde::examples::BaseExampleInputHandler {
  public:
    CameraFeelInputHandler() {
        keys.bindHeld(vde::KEY_A, "left");
        keys.bindHeld(vde::KEY_LEFT, "left");
        keys.bindHeld(vde::KEY_D, "right");
        keys.bindHeld(vde::KEY_RIGHT, "right");
        keys.bindHeld(vde::KEY_W, "up");
        keys.bindHeld(vde::KEY_UP, "up");
        keys.bindHeld(vde::KEY_S, "down");
        keys.bindHeld(vde::KEY_DOWN, "down");

        keys.bindOneShot(vde::KEY_1, "deadzone_mode");
        keys.bindOneShot(vde::KEY_2, "lookahead_mode");
        keys.bindOneShot(vde::KEY_3, "zoom_mode");
        keys.bindOneShot(vde::KEY_4, "combined_mode");
        keys.bindOneShot(vde::KEY_SPACE, "shake");
        keys.bindOneShot(vde::KEY_R, "reset");
    }

    void onKeyPress(int key) override {
        BaseExampleInputHandler::onKeyPress(key);
        keys.handlePress(key);
    }

    void onKeyRelease(int key) override { keys.handleRelease(key); }

    vde::KeyStateTracker keys;
};

// ---------------------------------------------------------------------------
// Scene
// ---------------------------------------------------------------------------

class CameraFeelScene : public vde::examples::BaseExampleScene {
  public:
    CameraFeelScene() : BaseExampleScene(15.0f) {}

    void onEnter() override {
        printExampleHeader();

        setup2D(kViewWidth, kViewHeight, vde::Color::fromHex(0x0d1422));
        m_camera2D = dynamic_cast<vde::Camera2D*>(getCamera());

        createBackdrop();
        createPlayer();
        createOverlay();
        resetDemo();
    }

    void update(float deltaTime) override {
        BaseExampleScene::update(deltaTime);

        auto* input = dynamic_cast<CameraFeelInputHandler*>(getInputHandler());
        if (!input || !m_camera2D) {
            return;
        }

        if (input->keys.consume("reset")) {
            resetDemo();
            std::cout << "Reset camera feel demo\n";
        }

        if (input->keys.consume("deadzone_mode")) {
            applyMode(CameraFeelMode::Deadzone);
        }
        if (input->keys.consume("lookahead_mode")) {
            applyMode(CameraFeelMode::LookAhead);
        }
        if (input->keys.consume("zoom_mode")) {
            applyMode(CameraFeelMode::Zoom);
        }
        if (input->keys.consume("combined_mode")) {
            applyMode(CameraFeelMode::Combined);
        }
        if (input->keys.consume("shake")) {
            triggerShake();
        }

        glm::vec2 moveAxis(0.0f);
        if (input->keys.isHeld("left")) {
            moveAxis.x -= 1.0f;
        }
        if (input->keys.isHeld("right")) {
            moveAxis.x += 1.0f;
        }
        if (input->keys.isHeld("up")) {
            moveAxis.y += 1.0f;
        }
        if (input->keys.isHeld("down")) {
            moveAxis.y -= 1.0f;
        }

        const glm::vec2 moveDirection = vde::math2d::normalizeOrZero(moveAxis);
        const glm::vec2 previousPosition = m_playerPosition;

        m_playerPosition += moveDirection * kPlayerSpeed * deltaTime;
        m_playerPosition.x = vde::math2d::clamp(m_playerPosition.x, -kWorldLimitX, kWorldLimitX);
        m_playerPosition.y = vde::math2d::clamp(m_playerPosition.y, -kWorldLimitY, kWorldLimitY);

        if (deltaTime > 0.0001f) {
            m_playerVelocity = (m_playerPosition - previousPosition) / deltaTime;
        } else {
            m_playerVelocity = glm::vec2(0.0f);
        }

        if (vde::math2d::lengthSquared(moveDirection) > 0.0001f) {
            m_lastMoveDirection = moveDirection;
        }

        m_camera2D->followTarget(m_playerPosition, m_followSpeed);

        m_shakeFlashTimer = std::max(0.0f, m_shakeFlashTimer - deltaTime);
        syncVisuals();
    }

  protected:
    [[nodiscard]] std::string getExampleName() const override { return "Camera Feel Demo"; }

    [[nodiscard]] std::vector<std::string> getFeatures() const override {
        return {
            "Camera2D follow deadzone mode for jitter-free movement",
            "Velocity look-ahead and smooth zoom target interpolation",
            "Decaying shake trigger plus a combined camera-feel preset",
        };
    }

    [[nodiscard]] std::vector<std::string> getExpectedVisuals() const override {
        return {
            "A bright player square moving through a striped world with landmark columns",
            "A viewport frame, mode indicator bars, and a center marker that reveal camera motion",
            "A cyan deadzone frame in mode 1, an amber lead marker in mode 2, and a pink combined "
            "preset in mode 4",
        };
    }

    [[nodiscard]] std::vector<std::string> getControls() const override {
        return {
            "WASD / Arrow Keys - Move the target",
            "1 - Deadzone only, 2 - Look-ahead only, 3 - Smooth zoom only, 4 - Combined preset",
            "SPACE - Trigger shake, R - Reset to the base follow mode",
        };
    }

  private:
    struct FrameOverlay {
        std::shared_ptr<vde::SpriteEntity> top;
        std::shared_ptr<vde::SpriteEntity> bottom;
        std::shared_ptr<vde::SpriteEntity> left;
        std::shared_ptr<vde::SpriteEntity> right;
    };

    struct ModeIndicator {
        CameraFeelMode mode = CameraFeelMode::Base;
        std::shared_ptr<vde::SpriteEntity> sprite;
    };

    void createBackdrop() {
        auto background = addEntity<vde::SpriteEntity>();
        background->setPosition(0.0f, 0.0f, -0.8f);
        background->setScale(86.0f, 44.0f, 1.0f);
        background->setColor(vde::Color::fromHex(0x09111d));

        auto horizon = addEntity<vde::SpriteEntity>();
        horizon->setPosition(0.0f, 0.0f, -0.7f);
        horizon->setScale(84.0f, 0.18f, 1.0f);
        horizon->setColor(vde::Color(0.55f, 0.66f, 0.84f, 0.28f));

        for (int index = -8; index <= 8; ++index) {
            const float x = static_cast<float>(index) * 4.5f;

            auto stripe = addEntity<vde::SpriteEntity>();
            stripe->setPosition(x, 0.0f, -0.65f);
            stripe->setScale(0.45f, 19.0f, 1.0f);
            stripe->setColor(index % 2 == 0 ? vde::Color(0.11f, 0.20f, 0.33f, 0.75f)
                                            : vde::Color(0.08f, 0.14f, 0.24f, 0.75f));

            auto pylon = addEntity<vde::SpriteEntity>();
            pylon->setPosition(x, (index % 2 == 0) ? 3.1f : -3.1f, -0.35f);
            pylon->setScale(0.9f, 2.6f + (static_cast<float>(std::abs(index % 3)) * 0.5f), 1.0f);
            pylon->setColor(index % 3 == 0
                                ? vde::Color(0.20f, 0.82f, 1.0f, 0.88f)
                                : (index % 3 == 1 ? vde::Color(1.0f, 0.70f, 0.22f, 0.88f)
                                                  : vde::Color(0.52f, 0.98f, 0.62f, 0.88f)));
        }

        for (int row = -2; row <= 2; ++row) {
            auto lane = addEntity<vde::SpriteEntity>();
            lane->setPosition(0.0f, static_cast<float>(row) * 3.0f, -0.68f);
            lane->setScale(84.0f, 0.12f, 1.0f);
            lane->setColor(vde::Color(0.30f, 0.38f, 0.55f, row == 0 ? 0.32f : 0.18f));
        }
    }

    void createPlayer() {
        m_player = addEntity<vde::SpriteEntity>();
        m_player->setPosition(0.0f, 0.0f, 0.18f);
        m_player->setScale(0.62f, 0.62f, 1.0f);

        m_trailGhost = addEntity<vde::SpriteEntity>();
        m_trailGhost->setPosition(-0.55f, 0.0f, 0.12f);
        m_trailGhost->setScale(0.32f, 0.32f, 1.0f);
        m_trailGhost->setColor(vde::Color(0.52f, 0.86f, 1.0f, 0.38f));

        m_lookAheadGhost = addEntity<vde::SpriteEntity>();
        m_lookAheadGhost->setPosition(0.0f, 0.0f, 0.14f);
        m_lookAheadGhost->setScale(0.28f, 0.28f, 1.0f);
        m_lookAheadGhost->setColor(vde::Color(1.0f, 0.72f, 0.25f, 0.0f));
    }

    void createOverlay() {
        createFrame(m_viewportFrame, 0.28f, vde::Color(0.82f, 0.88f, 0.97f, 0.65f));
        createFrame(m_deadzoneFrame, 0.26f, vde::Color(0.25f, 0.74f, 1.0f, 0.0f));

        m_cameraCenter = addEntity<vde::SpriteEntity>();
        m_cameraCenter->setScale(0.18f, 0.18f, 1.0f);
        m_cameraCenter->setColor(vde::Color(1.0f, 0.95f, 0.45f, 0.9f));

        m_shakeFlash = addEntity<vde::SpriteEntity>();
        m_shakeFlash->setScale(kViewWidth, kViewHeight, 1.0f);
        m_shakeFlash->setColor(vde::Color(1.0f, 0.36f, 0.28f, 0.0f));

        for (auto& indicator : m_modeIndicators) {
            indicator.sprite = addEntity<vde::SpriteEntity>();
            indicator.sprite->setScale(1.45f, 0.20f, 1.0f);
        }
    }

    void initializeFrameEdge(const std::shared_ptr<vde::SpriteEntity>& edge, float z,
                             const vde::Color& color) {
        edge->setPosition(0.0f, 0.0f, z);
        edge->setScale(0.1f, 0.1f, 1.0f);
        edge->setColor(color);
    }

    void createFrame(FrameOverlay& frame, float z, const vde::Color& color) {
        frame.top = addEntity<vde::SpriteEntity>();
        frame.bottom = addEntity<vde::SpriteEntity>();
        frame.left = addEntity<vde::SpriteEntity>();
        frame.right = addEntity<vde::SpriteEntity>();

        initializeFrameEdge(frame.top, z, color);
        initializeFrameEdge(frame.bottom, z, color);
        initializeFrameEdge(frame.left, z, color);
        initializeFrameEdge(frame.right, z, color);
    }

    void layoutFrame(const FrameOverlay& frame, const glm::vec2& center, float width, float height,
                     float z, const vde::Color& color) {
        const float halfWidth = width * 0.5f;
        const float halfHeight = height * 0.5f;

        frame.top->setPosition(center.x, center.y + halfHeight, z);
        frame.top->setScale(width, kFrameThickness, 1.0f);
        frame.top->setColor(color);

        frame.bottom->setPosition(center.x, center.y - halfHeight, z);
        frame.bottom->setScale(width, kFrameThickness, 1.0f);
        frame.bottom->setColor(color);

        frame.left->setPosition(center.x - halfWidth, center.y, z);
        frame.left->setScale(kFrameThickness, height, 1.0f);
        frame.left->setColor(color);

        frame.right->setPosition(center.x + halfWidth, center.y, z);
        frame.right->setScale(kFrameThickness, height, 1.0f);
        frame.right->setColor(color);
    }

    void applyMode(CameraFeelMode mode) {
        if (!m_camera2D) {
            return;
        }

        m_mode = mode;
        m_followSpeed = 7.0f;
        m_deadzoneVisualSize = glm::vec2(0.0f);

        m_camera2D->setDeadzone(0.0f, 0.0f);
        m_camera2D->setLookAhead(0.0f);
        m_camera2D->setZoom(1.0f);

        switch (m_mode) {
        case CameraFeelMode::Base:
            break;
        case CameraFeelMode::Deadzone:
            m_camera2D->setDeadzone(kDeadzoneWidth, kDeadzoneHeight);
            m_deadzoneVisualSize = glm::vec2(kDeadzoneWidth, kDeadzoneHeight);
            m_followSpeed = 8.5f;
            break;
        case CameraFeelMode::LookAhead:
            m_camera2D->setLookAhead(kLookAheadDistance, kLookAheadSeconds, kLookAheadSmoothing);
            m_followSpeed = 9.0f;
            break;
        case CameraFeelMode::Zoom:
            m_camera2D->zoomTo(1.65f, 8.0f);
            m_followSpeed = 6.5f;
            break;
        case CameraFeelMode::Combined:
            m_camera2D->setDeadzone(kDeadzoneWidth, kDeadzoneHeight);
            m_camera2D->setLookAhead(kLookAheadDistance, kLookAheadSeconds, kLookAheadSmoothing);
            m_camera2D->zoomTo(1.30f, 7.0f);
            m_deadzoneVisualSize = glm::vec2(kDeadzoneWidth, kDeadzoneHeight);
            m_followSpeed = 8.5f;
            break;
        }

        std::cout << "Camera mode: " << modeName(m_mode) << '\n';
    }

    void triggerShake() {
        if (!m_camera2D) {
            return;
        }

        const bool amplified = m_mode == CameraFeelMode::Combined;
        m_camera2D->shake(amplified ? 0.55f : 0.32f, amplified ? 0.42f : 0.28f, 2.2f);
        m_shakeFlashTimer = kShakeFlashDuration;
        std::cout << "Triggered camera shake\n";
    }

    void resetDemo() {
        m_playerPosition = glm::vec2(0.0f);
        m_playerVelocity = glm::vec2(0.0f);
        m_lastMoveDirection = glm::vec2(1.0f, 0.0f);
        m_shakeFlashTimer = 0.0f;

        if (m_camera2D) {
            m_camera2D->setPosition(0.0f, 0.0f);
            m_camera2D->setZoom(1.0f);
            m_camera2D->shake(0.0f, 0.0f);
        }

        applyMode(CameraFeelMode::Base);
        syncVisuals();
    }

    [[nodiscard]] glm::vec2 getLookAheadPreview() const {
        if (!usesLookAhead(m_mode)) {
            return glm::vec2(0.0f);
        }

        return clampMagnitude(m_playerVelocity * kLookAheadSeconds, kLookAheadDistance);
    }

    void syncVisuals() {
        const float speedRatio = vde::math2d::saturate(glm::length(m_playerVelocity) / 9.0f);
        const vde::Color activeColor = modeColor(m_mode);

        m_player->setPosition(m_playerPosition.x, m_playerPosition.y, 0.18f);
        m_player->setScale(0.60f + (speedRatio * 0.10f), 0.60f - (speedRatio * 0.05f), 1.0f);
        m_player->setColor(activeColor);

        const glm::vec2 trailOffset = m_lastMoveDirection * 0.65f;
        m_trailGhost->setPosition(m_playerPosition.x - trailOffset.x,
                                  m_playerPosition.y - trailOffset.y, 0.12f);
        m_trailGhost->setScale(0.30f + (speedRatio * 0.06f), 0.30f + (speedRatio * 0.06f), 1.0f);
        m_trailGhost->setColor(
            vde::Color(activeColor.r, activeColor.g, activeColor.b, 0.18f + (speedRatio * 0.20f)));

        const glm::vec2 lookAhead = getLookAheadPreview();
        m_lookAheadGhost->setPosition(m_playerPosition.x + lookAhead.x,
                                      m_playerPosition.y + lookAhead.y, 0.14f);
        m_lookAheadGhost->setScale(0.26f, 0.26f, 1.0f);
        m_lookAheadGhost->setColor(
            vde::Color(1.0f, 0.72f, 0.25f, usesLookAhead(m_mode) ? 0.85f : 0.0f));

        if (!m_camera2D) {
            return;
        }

        const vde::Rect2D visible = m_camera2D->getVisibleRect();
        const float visibleWidth = visible.right - visible.left;
        const float visibleHeight = visible.top - visible.bottom;
        const glm::vec2 center((visible.left + visible.right) * 0.5f,
                               (visible.bottom + visible.top) * 0.5f);

        layoutFrame(m_viewportFrame, center, visibleWidth - 0.18f, visibleHeight - 0.18f, 0.28f,
                    vde::Color(0.84f, 0.88f, 0.97f, 0.52f));

        const vde::Color deadzoneColor = usesDeadzone(m_mode)
                                             ? vde::Color(0.24f, 0.74f, 1.0f, 0.95f)
                                             : vde::Color(0.24f, 0.74f, 1.0f, 0.0f);
        const glm::vec2 deadzoneSize =
            usesDeadzone(m_mode) ? m_deadzoneVisualSize : glm::vec2(0.2f);
        layoutFrame(m_deadzoneFrame, center, deadzoneSize.x, deadzoneSize.y, 0.26f, deadzoneColor);

        m_cameraCenter->setPosition(center.x, center.y, 0.30f);
        m_cameraCenter->setColor(vde::Color(1.0f, 0.95f, 0.42f, 0.92f));

        m_shakeFlash->setPosition(center.x, center.y, 0.34f);
        m_shakeFlash->setScale(visibleWidth, visibleHeight, 1.0f);
        m_shakeFlash->setColor(
            vde::Color(1.0f, 0.35f, 0.28f,
                       vde::math2d::saturate(m_shakeFlashTimer / kShakeFlashDuration) * 0.14f));

        const float topY = visible.top - 0.48f;
        const float startX = center.x - 3.0f;
        float indicatorX = startX;
        for (auto& indicator : m_modeIndicators) {
            const bool active = indicator.mode == m_mode;
            const vde::Color indicatorColor = modeColor(indicator.mode);
            indicator.sprite->setPosition(indicatorX, topY, 0.33f);
            indicator.sprite->setScale(active ? 1.7f : 1.45f, active ? 0.24f : 0.18f, 1.0f);
            indicator.sprite->setColor(vde::Color(indicatorColor.r, indicatorColor.g,
                                                  indicatorColor.b, active ? 0.95f : 0.26f));
            indicatorX += 2.0f;
        }
    }

    vde::Camera2D* m_camera2D = nullptr;
    CameraFeelMode m_mode = CameraFeelMode::Base;
    float m_followSpeed = 7.0f;
    float m_shakeFlashTimer = 0.0f;
    glm::vec2 m_playerPosition{0.0f};
    glm::vec2 m_playerVelocity{0.0f};
    glm::vec2 m_lastMoveDirection{1.0f, 0.0f};
    glm::vec2 m_deadzoneVisualSize{0.0f};

    std::shared_ptr<vde::SpriteEntity> m_player;
    std::shared_ptr<vde::SpriteEntity> m_trailGhost;
    std::shared_ptr<vde::SpriteEntity> m_lookAheadGhost;
    std::shared_ptr<vde::SpriteEntity> m_cameraCenter;
    std::shared_ptr<vde::SpriteEntity> m_shakeFlash;
    FrameOverlay m_viewportFrame;
    FrameOverlay m_deadzoneFrame;
    std::array<ModeIndicator, 4> m_modeIndicators{{
        {.mode = CameraFeelMode::Deadzone, .sprite = nullptr},
        {.mode = CameraFeelMode::LookAhead, .sprite = nullptr},
        {.mode = CameraFeelMode::Zoom, .sprite = nullptr},
        {.mode = CameraFeelMode::Combined, .sprite = nullptr},
    }};
};

// ---------------------------------------------------------------------------
// Game class
// ---------------------------------------------------------------------------

class CameraFeelGame
    : public vde::examples::BaseExampleGame<CameraFeelInputHandler, CameraFeelScene> {};

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

// NOLINTNEXTLINE(bugprone-exception-escape)
int main(int argc, char** argv) {
    CameraFeelGame demo;
    return vde::examples::runExample(demo, "VDE Camera Feel Demo", 1280, 720, argc, argv);
}
