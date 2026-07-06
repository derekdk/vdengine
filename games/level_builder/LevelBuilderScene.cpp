#include "LevelBuilderScene.h"

#include <vde/Texture.h>

#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
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
constexpr float kCameraTargetYOffset = 1.1f;
constexpr float kRespawnFloorY = -5.0f;
constexpr float kModeTextX = -10.6f;
constexpr float kModeTextY = 5.55f;
constexpr float kModeTextHeight = 0.28f;
constexpr float kSelectionTextX = -10.6f;
constexpr float kSelectionTextY = 5.10f;
constexpr float kSelectionTextHeight = 0.22f;
constexpr float kActionLegendX = 6.2f;
constexpr float kActionLegendTopY = 5.35f;
constexpr float kActionLegendLineHeight = 0.22f;
constexpr float kActionLegendLineSpacing = 0.32f;

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
    m_tileMapSession.load(getGame() ? getGame()->getVulkanContext() : nullptr);

    addEntity(std::static_pointer_cast<vde::Entity>(m_tileMapSession.tileMap()));
    m_tileMapSession.tileMap()->setPosition(0.0f, 0.0f, -0.4f);

    m_playerController.createEntities(*this);
    m_playerController.reset(m_tileMapSession, camera);
    m_devModeController.setPosition(m_playerController.getPosition());

    createHud();
    // Create the selection cursor after the HUD so the white outline renders on top of text.
    m_tileCursor.initialize(*this);
    setSelectTileUiVisible(false);

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
        setDevelopmentMode(!m_devModeController.isEnabled());
    }

    auto* camera = currentCamera();
    if (camera == nullptr || m_tileMapSession.tileMap() == nullptr) {
        finishFrame();
        return;
    }

    if (m_devModeController.isEnabled()) {
        const DevelopmentSubmode previousSubmode = m_devModeController.activeSubmode();
        if (actions.consumePressed("prev_submode")) {
            m_devModeController.cycleToPreviousAvailableSubmode();
        }
        if (actions.consumePressed("next_submode")) {
            m_devModeController.cycleToNextAvailableSubmode();
        }

        if (previousSubmode != m_devModeController.activeSubmode()) {
            if (m_devModeController.activeSubmode() == DevelopmentSubmode::SelectTileMode) {
                initializeSelectTileMode();
            } else {
                setSelectTileUiVisible(false);
            }
            updateModeText();
        }
    }

    if (actions.consumePressed("reset")) {
        m_playerController.reset(m_tileMapSession, camera);
        m_devModeController.setPosition(m_playerController.getPosition());
        m_playerController.stopMotion();
        if (m_devModeController.isEnabled() &&
            m_devModeController.activeSubmode() == DevelopmentSubmode::SelectTileMode) {
            initializeSelectTileMode();
        }
        finishFrame();
        return;
    }

    glm::vec2 moveAxis(0.0f, 0.0f);
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

    if (m_devModeController.isEnabled()) {
        switch (m_devModeController.activeSubmode()) {
        case DevelopmentSubmode::MoveMode:
            m_devModeController.updateMoveMode(deltaTime, moveAxis);
            m_playerController.stopMotion();
            m_playerController.setPosition(m_devModeController.position());
            m_playerController.setFacingFromHorizontal(moveAxis.x);
            setSelectTileUiVisible(false);
            camera->followTarget(m_playerController.getPosition() +
                                     glm::vec2(0.0f, kCameraTargetYOffset),
                                 kCameraFollowSpeed);
            finishFrame();
            return;

        case DevelopmentSubmode::SelectTileMode: {
            if (!m_devModeController.hasSelection()) {
                initializeSelectTileMode();
            }

            const glm::ivec2 selectionAxis(moveAxis.x < 0.0f ? -1 : (moveAxis.x > 0.0f ? 1 : 0),
                                           moveAxis.y < 0.0f ? -1 : (moveAxis.y > 0.0f ? 1 : 0));
            const bool selectionChanged = m_devModeController.updateSelectTileMode(
                deltaTime, selectionAxis, m_tileMapSession.maxTileCoordinate());
            if (selectionChanged || !m_tileCursor.isVisible()) {
                updateSelectTileUi();
            }

            if (m_devModeController.hasSelection()) {
                camera->followTarget(
                    m_tileMapSession.tileCenterWorld(m_devModeController.selectedTile()),
                    kCameraFollowSpeed);
            }
            m_playerController.stopMotion();
            finishFrame();
            return;
        }
        }
    }

    m_playerController.update(deltaTime, moveAxis.x, actions.consumePressed("jump"),
                              m_tileMapSession);

    if (m_playerController.getPosition().y < kRespawnFloorY) {
        m_playerController.reset(m_tileMapSession, camera);
        m_devModeController.setPosition(m_playerController.getPosition());
        finishFrame();
        return;
    }

    camera->followTarget(m_playerController.getPosition() + glm::vec2(0.0f, kCameraTargetYOffset),
                         kCameraFollowSpeed);
    finishFrame();
}

std::string LevelBuilderScene::getGameName() const {
    return "Level Builder";
}

std::vector<std::string> LevelBuilderScene::getGameplaySummary() const {
    return {
        "Phase 4 adds Select Tile Mode on top of the Development submode controller.",
        "The nearest tile to the player becomes the initial selection when Select Tile Mode "
        "starts.",
        "A white outline and on-screen action legend make controller-driven tile navigation "
        "visible.",
    };
}

std::vector<std::string> LevelBuilderScene::getGoals() const {
    return {
        "Make Select Tile Mode navigable before tile-mutation actions are added.",
        "Keep tile selection state isolated from play-mode physics and camera rules.",
    };
}

std::vector<std::string> LevelBuilderScene::getControls() const {
    return {
        "A / D or Left / Right - Move across the tilemap",
        "W / Up - Jump in Play mode, move up in Development Move Mode, or move tile selection up",
        "S / Down - Move down in Development Move Mode or move tile selection down",
        std::string("Gamepad D-pad / Left Stick - Move player in Move Mode or tile selection in "
                    "Select Tile Mode"),
        "Space - Jump",
        "Gamepad A or D-pad Up - Jump",
        "R - Reset to the spawn point",
        "Gamepad Back - Reset",
        "Enter / Gamepad Start - Toggle Development mode",
        "Q / E or Gamepad LB / RB - Cycle Development submodes",
    };
}

void LevelBuilderScene::drawDebugUI() {
    BaseGameScene::drawDebugUI();

#ifdef VDE_GAME_USE_IMGUI
    ImGui::SetNextWindowPos(ImVec2(10, 170), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(360, 165), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Level Builder")) {
        ImGui::Text("Mode: %s", m_devModeController.isEnabled() ? "Development" : "Play");
        ImGui::Text("Submode: %s", m_devModeController.isEnabled()
                                       ? m_devModeController.activeSubmodeName()
                                       : "N/A");
        ImGui::Text("Player: %.2f, %.2f", m_playerController.getPosition().x,
                    m_playerController.getPosition().y);
        if (m_devModeController.hasSelection()) {
            ImGui::Text("Selected Tile: %d, %d", m_devModeController.selectedTile().x,
                        m_devModeController.selectedTile().y);
        }
        ImGui::TextColored(ImVec4(0.65f, 0.82f, 0.95f, 1.0f),
                           "Start / Enter toggles Development mode");
        if (m_devModeController.isEnabled() &&
            m_devModeController.activeSubmode() == DevelopmentSubmode::MoveMode) {
            ImGui::Text("Move Mode: free movement, no collisions, no gravity");
        }
        if (m_devModeController.isEnabled() &&
            m_devModeController.activeSubmode() == DevelopmentSubmode::SelectTileMode) {
            ImGui::Text("Select Tile Mode: movement controls step the tile selection");
            ImGui::Text("Tile editing buttons arrive in Phase 5");
        }
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

    m_selectionText = addEntity<vde::TextEntity>();
    m_selectionText->setFont(vde::BitmapFont::small());
    m_selectionText->setStyle({.color = vde::Color::white(), .pixelScale = 1});
    m_selectionText->setAnchor(0.0f, 0.5f);
    m_selectionText->setPosition(kSelectionTextX, kSelectionTextY, 1.2f);
    m_selectionText->setWorldHeight(kSelectionTextHeight);

    const std::vector<std::string> actionLines = {
        "SELECT TILE MODE",      "DPAD / STICK - MOVE TILE",    "LB / RB - CHANGE SUBMODE",
        "START - EXIT DEV MODE", "A/B/X/Y - TILE ACTIONS NEXT",
    };

    m_actionLegendLines.clear();
    for (size_t index = 0; index < actionLines.size(); ++index) {
        auto line = addEntity<vde::TextEntity>();
        line->setText(actionLines.at(index));
        line->setFont(vde::BitmapFont::small());
        line->setStyle({.color = vde::Color(0.88f, 0.94f, 0.99f, 1.0f), .pixelScale = 1});
        line->setAnchor(0.0f, 0.5f);
        line->setPosition(
            kActionLegendX,
            kActionLegendTopY - (static_cast<float>(index) * kActionLegendLineSpacing), 1.2f);
        line->setWorldHeight(kActionLegendLineHeight);
        m_actionLegendLines.push_back(line);
    }

    updateModeText();
    setSelectTileUiVisible(false);
}

void LevelBuilderScene::initializeSelectTileMode() {
    m_devModeController.setSelectedTile(
        m_tileMapSession.nearestTileToWorld(m_playerController.getPosition()));
    updateSelectTileUi();
}

void LevelBuilderScene::updateSelectTileUi() {
    if (!m_devModeController.hasSelection()) {
        setSelectTileUiVisible(false);
        return;
    }

    setSelectTileUiVisible(true);
    const glm::ivec2 selectedTile = m_devModeController.selectedTile();
    const glm::vec2 tileCenter = m_tileMapSession.tileCenterWorld(selectedTile);
    m_tileCursor.show(tileCenter, m_tileMapSession.tileMap()->getTileWidth(),
                      m_tileMapSession.tileMap()->getTileHeight());

    if (m_selectionText != nullptr) {
        m_selectionText->setText("TILE: " + std::to_string(selectedTile.x) + ", " +
                                 std::to_string(selectedTile.y));
    }
}

void LevelBuilderScene::setSelectTileUiVisible(bool visible) {
    if (!visible) {
        m_tileCursor.hide();
    }

    if (m_selectionText != nullptr) {
        m_selectionText->setVisible(visible);
    }

    for (const auto& line : m_actionLegendLines) {
        if (line != nullptr) {
            line->setVisible(visible);
        }
    }
}

void LevelBuilderScene::updateModeText() {
    if (m_modeText == nullptr) {
        return;
    }

    if (m_devModeController.isEnabled()) {
        m_modeText->setStyle({.color = vde::Color::fromHex(0xffd36b), .pixelScale = 1});
        m_modeText->setText(std::string("MODE: DEVELOPMENT / ") +
                            m_devModeController.activeSubmodeName());
        return;
    }

    m_modeText->setStyle({.color = vde::Color(0.84f, 0.92f, 0.98f, 1.0f), .pixelScale = 1});
    m_modeText->setText("MODE: PLAY");
}

void LevelBuilderScene::setDevelopmentMode(bool enabled) {
    if (enabled) {
        m_devModeController.enter(m_playerController.getPosition());
        m_playerController.stopMotion();
    } else {
        m_devModeController.exit();
        m_playerController.stopMotion();
        setSelectTileUiVisible(false);
    }

    updateModeText();
    std::cout << (enabled ? "Development mode enabled\n" : "Development mode disabled\n");
}

LevelBuilderInput* LevelBuilderScene::input() {
    return dynamic_cast<LevelBuilderInput*>(getInputHandler());
}

vde::Camera2D* LevelBuilderScene::currentCamera() {
    return dynamic_cast<vde::Camera2D*>(getCamera());
}

}  // namespace levelbuilder