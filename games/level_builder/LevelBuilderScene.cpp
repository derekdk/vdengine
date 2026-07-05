#include "LevelBuilderScene.h"

#include <vde/Texture.h>

#include <cstdint>
#include <iostream>
#include <memory>
#include <vector>

#include "Input.h"

namespace {

constexpr float kViewWidth = 22.0f;
constexpr float kViewHeight = 12.0f;
constexpr float kCameraDeadzoneWidth = 4.0f;
constexpr float kCameraDeadzoneHeight = 2.6f;
constexpr float kCameraLookAheadDistance = 2.1f;
constexpr float kCameraLookAheadSmoothing = 0.18f;
constexpr float kCameraFollowSpeed = 7.5f;
constexpr float kCameraZoom = 1.08f;
constexpr float kRespawnFloorY = -5.0f;
constexpr float kModeTextX = -10.6f;
constexpr float kModeTextY = 5.55f;
constexpr float kModeTextHeight = 0.28f;

struct RGBA {
    constexpr RGBA() = default;
    constexpr RGBA(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha = 255)
        : r(red), g(green), b(blue), a(alpha) {}

    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t a = 255;
};

void putPixel(std::vector<uint8_t>& buffer, uint32_t stride, uint32_t x, uint32_t y, RGBA color) {
    const size_t offset = (static_cast<size_t>(y) * stride + x) * 4;
    buffer.at(offset + 0) = color.r;
    buffer.at(offset + 1) = color.g;
    buffer.at(offset + 2) = color.b;
    buffer.at(offset + 3) = color.a;
}

std::shared_ptr<vde::Texture> createBackdropTexture(uint32_t width, uint32_t height, RGBA base,
                                                    RGBA stripe, RGBA highlight, bool diagonal) {
    std::vector<uint8_t> pixels(static_cast<size_t>(width) * height * 4, 0);

    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            RGBA color = base;
            if (((x / 8) + (diagonal ? (y / 6) : 0)) % 3 == 0) {
                color = stripe;
            }
            if ((y < (height / 4)) && ((x + y) % 17 < 5)) {
                color = highlight;
            }
            putPixel(pixels, width, x, y, color);
        }
    }

    auto texture = std::make_shared<vde::Texture>();
    texture->loadFromData(pixels.data(), width, height);
    return texture;
}

}  // namespace

namespace levelbuilder {

LevelBuilderScene::LevelBuilderScene() = default;

void LevelBuilderScene::onEnter() {
    printGameHeader();

    setup2D(kViewWidth, kViewHeight, vde::Color::fromHex(0x08121f));
    auto* camera = currentCamera();
    if (camera != nullptr) {
        camera->setDeadzone(kCameraDeadzoneWidth, kCameraDeadzoneHeight);
        camera->setLookAhead(kCameraLookAheadDistance, kCameraLookAheadSmoothing,
                             kCameraFollowSpeed);
        camera->setZoom(kCameraZoom);
    }

    createBackgrounds();
    createHud();
    m_tileMapSession.load(getGame() ? getGame()->getVulkanContext() : nullptr);

    addEntity(std::static_pointer_cast<vde::Entity>(m_tileMapSession.tileMap()));
    m_tileMapSession.tileMap()->setPosition(0.0f, 0.0f, -0.4f);

    m_playerController.createEntities(*this);
    m_playerController.reset(m_tileMapSession, camera);

    std::cout << "Level builder baseline: " << m_tileMapSession.tileMap()->getColumnCount() << 'x'
              << m_tileMapSession.tileMap()->getRowCount() << " imported tiles across "
              << m_tileMapSession.tileMap()->getLayerCount() << " layers, "
              << m_tileMapSession.importedObjectCount() << " imported objects, extracted "
              << m_tileMapSession.solidRects().size() << " solid regions and "
              << m_tileMapSession.oneWayRects().size() << " one-way regions\n";
}

void LevelBuilderScene::update(float deltaTime) {
    BaseGameScene::update(deltaTime);

    auto* controls = input();
    const auto finishFrame = [&controls]() {
        if (controls != nullptr) {
            controls->finishFrame();
        }
    };

    if (controls == nullptr) {
        finishFrame();
        return;
    }

    auto& actions = controls->actions();
    if (actions.consumePressed("toggle_dev_mode")) {
        setDevelopmentMode(!m_developmentMode);
    }

    auto* camera = currentCamera();
    if (camera == nullptr || m_tileMapSession.tileMap() == nullptr) {
        finishFrame();
        return;
    }

    if (actions.consumePressed("reset")) {
        m_playerController.reset(m_tileMapSession, camera);
        finishFrame();
        return;
    }

    if (m_developmentMode) {
        finishFrame();
        return;
    }

    float moveAxis = 0.0f;
    if (actions.isHeld("move_left")) {
        moveAxis -= 1.0f;
    }
    if (actions.isHeld("move_right")) {
        moveAxis += 1.0f;
    }

    m_playerController.update(deltaTime, moveAxis, actions.consumePressed("jump"),
                              m_tileMapSession);

    if (m_playerController.getPosition().y < kRespawnFloorY) {
        m_playerController.reset(m_tileMapSession, camera);
        finishFrame();
        return;
    }

    camera->followTarget(m_playerController.getPosition() + glm::vec2(0.0f, 1.1f),
                         kCameraFollowSpeed);
    finishFrame();
}

std::string LevelBuilderScene::getGameName() const {
    return "Level Builder";
}

std::vector<std::string> LevelBuilderScene::getGameplaySummary() const {
    return {
        "Phase 2 unifies keyboard and gamepad input through named actions.",
        "Imports a checked-in Tiled map, collision data, and object-layer spawn metadata.",
        "Adds a Development mode toggle that pauses play-mode movement while editor controls grow.",
    };
}

std::vector<std::string> LevelBuilderScene::getGoals() const {
    return {
        "Validate the unified input layer before adding tile-selection behavior.",
        "Keep Development mode state in the scene while the input layer stays declarative.",
    };
}

std::vector<std::string> LevelBuilderScene::getControls() const {
    return {
        "A / D or Left / Right - Move across the tilemap",
        "Gamepad D-pad Left / Right or Left Stick - Move",
        "Space / W / Up - Jump",
        "Gamepad A or D-pad Up - Jump",
        "R - Reset to the spawn point",
        "Gamepad Back - Reset",
        "Enter / Gamepad Start - Toggle Development mode",
    };
}

void LevelBuilderScene::drawDebugUI() {
    BaseGameScene::drawDebugUI();

#ifdef VDE_GAME_USE_IMGUI
    ImGui::SetNextWindowPos(ImVec2(10, 170), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(310, 105), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Level Builder")) {
        ImGui::Text("Mode: %s", m_developmentMode ? "Development" : "Play");
        ImGui::Text("Player: %.2f, %.2f", m_playerController.getPosition().x,
                    m_playerController.getPosition().y);
        ImGui::TextColored(ImVec4(0.65f, 0.82f, 0.95f, 1.0f),
                           "Start / Enter toggles Development mode");
    }
    ImGui::End();
#endif
}

void LevelBuilderScene::createBackgrounds() {
    auto skyTexture = createBackdropTexture(64, 64, RGBA{10, 22, 38, 255}, RGBA{18, 38, 62, 255},
                                            RGBA{44, 74, 104, 255}, false);
    auto ridgeTexture = createBackdropTexture(64, 48, RGBA{18, 31, 46, 255}, RGBA{28, 52, 72, 255},
                                              RGBA{54, 92, 118, 255}, true);

    auto sky = addEntity<vde::RepeatingBackground>(skyTexture, 8.0f, 8.0f, 8, 3);
    sky->setPosition(0.0f, 0.0f, -3.0f);
    sky->setParallaxFactor(0.18f, 0.06f);
    sky->setScrollVelocity(0.22f, 0.0f);

    auto ridges = addEntity<vde::RepeatingBackground>(ridgeTexture, 6.0f, 4.0f, 10, 4);
    ridges->setPosition(0.0f, -1.5f, -2.2f);
    ridges->setParallaxFactor(0.42f, 0.12f);
    ridges->setScrollVelocity(0.48f, 0.0f);
}

void LevelBuilderScene::createHud() {
    m_modeText = addEntity<vde::TextEntity>();
    m_modeText->setFont(vde::BitmapFont::small());
    m_modeText->setStyle({.color = vde::Color(0.84f, 0.92f, 0.98f, 1.0f), .pixelScale = 1});
    m_modeText->setAnchor(0.0f, 0.5f);
    m_modeText->setPosition(kModeTextX, kModeTextY, 1.2f);
    m_modeText->setWorldHeight(kModeTextHeight);
    setDevelopmentMode(false);
}

void LevelBuilderScene::setDevelopmentMode(bool enabled) {
    m_developmentMode = enabled;

    if (m_modeText != nullptr) {
        const vde::Color textColor =
            enabled ? vde::Color::fromHex(0xffd36b) : vde::Color(0.84f, 0.92f, 0.98f, 1.0f);
        m_modeText->setStyle({.color = textColor, .pixelScale = 1});
        m_modeText->setText(enabled ? "MODE: DEVELOPMENT" : "MODE: PLAY");
    }

    std::cout << (enabled ? "Development mode enabled\n" : "Development mode disabled\n");
}

LevelBuilderInput* LevelBuilderScene::input() {
    return dynamic_cast<LevelBuilderInput*>(getInputHandler());
}

vde::Camera2D* LevelBuilderScene::currentCamera() {
    return dynamic_cast<vde::Camera2D*>(getCamera());
}

}  // namespace levelbuilder