#include "LevelBuilderScene.h"

#include <vde/Texture.h>

#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
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
constexpr float kPersistenceTextX = -10.6f;
constexpr float kPersistenceTextY = 4.76f;
constexpr float kPersistenceTextHeight = 0.20f;
constexpr float kActionLegendX = 6.2f;
constexpr float kActionLegendTopY = 5.35f;
constexpr float kActionLegendLineHeight = 0.22f;
constexpr float kActionLegendLineSpacing = 0.32f;
constexpr float kLayerDepthAdjustStep = 0.06f;

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

std::string formatTileId(int tileId) {
    return tileId == vde::TileMap::kEmptyTile ? "EMPTY" : std::to_string(tileId);
}

std::string formatDepth(float depth) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(2) << depth;
    return stream.str();
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

    (void)m_tileMapSession.setMapPosition({0.0f, 0.0f, -0.4f});
    rebuildLayerRuntimes();

    m_playerController.createEntities(*this);
    m_playerController.reset(m_tileMapSession, camera);
    m_devModeController.setPosition(m_playerController.getPosition());

    createHud();
    m_tilePalette.initialize(*this);
    m_tilePalette.setTileSet(m_tileMapSession.tileMap()->getTileSet(),
                             m_tileMapSession.tileMap()->getTileWidth(),
                             m_tileMapSession.tileMap()->getTileHeight());
    // Create the selection cursor after the HUD so the white outline renders on top of text.
    m_tileCursor.initialize(*this);
    setSelectTileUiVisible(false);

    std::cout << "Level builder baseline: " << m_tileMapSession.tileMap()->getColumnCount() << 'x'
              << m_tileMapSession.tileMap()->getRowCount() << " imported tiles across "
              << m_tileMapSession.layerCount() << " authorable layers, "
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
    const auto finishSceneFrame = [this, &finishFrame, deltaTime]() {
        synchronizeLayerRuntimes();
        advanceLayerRuntimeScroll(deltaTime);
        finishFrame();
    };

    bool persistenceActionConsumed = false;
    if (m_devModeController.isEnabled()) {
        if (actions.consumePressed("save_overlay")) {
            (void)m_tileMapSession.saveEditableLayerOverlay();
            persistenceActionConsumed = true;
        }
        if (actions.consumePressed("load_overlay")) {
            const bool reloaded = m_tileMapSession.reloadEditableLayerOverlay();
            if (reloaded) {
                persistenceActionConsumed = true;
            }
        }
        if (persistenceActionConsumed) {
            updateModeText();
            updatePersistenceText();
        }
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
            syncInputMode();
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
        finishSceneFrame();
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
            if (actions.consumePressed("add_layer")) {
                const size_t newLayerIndex = m_tileMapSession.addLayer();
                (void)m_tileMapSession.setActiveLayerIndex(newLayerIndex);
                updateModeText();
                updateLayerStatusText();
                updatePersistenceText();
            }
            if (actions.consumePressed("previous_layer") && m_tileMapSession.layerCount() > 0) {
                const size_t newIndex =
                    (m_tileMapSession.activeLayerIndex() + m_tileMapSession.layerCount() - 1) %
                    m_tileMapSession.layerCount();
                (void)m_tileMapSession.setActiveLayerIndex(newIndex);
                updateModeText();
                updateLayerStatusText();
            }
            if (actions.consumePressed("next_layer") && m_tileMapSession.layerCount() > 0) {
                const size_t newIndex =
                    (m_tileMapSession.activeLayerIndex() + 1) % m_tileMapSession.layerCount();
                (void)m_tileMapSession.setActiveLayerIndex(newIndex);
                updateModeText();
                updateLayerStatusText();
            }
            if (actions.consumePressed("toggle_layer_visibility")) {
                if (m_tileMapSession.toggleLayerVisibility(m_tileMapSession.activeLayerIndex())) {
                    updateModeText();
                    updateLayerStatusText();
                    updatePersistenceText();
                }
            }
            if (actions.consumePressed("layer_depth_down")) {
                if (m_tileMapSession.adjustLayerDepthZ(m_tileMapSession.activeLayerIndex(),
                                                       -kLayerDepthAdjustStep)) {
                    updateModeText();
                    updateLayerStatusText();
                    updatePersistenceText();
                }
            }
            if (actions.consumePressed("layer_depth_up")) {
                if (m_tileMapSession.adjustLayerDepthZ(m_tileMapSession.activeLayerIndex(),
                                                       kLayerDepthAdjustStep)) {
                    updateModeText();
                    updateLayerStatusText();
                    updatePersistenceText();
                }
            }
            if (actions.consumePressed("previous_scroll_preset")) {
                if (m_tileMapSession.cycleLayerScrollPreset(m_tileMapSession.activeLayerIndex(),
                                                            -1)) {
                    resetLayerRuntimeScroll(m_tileMapSession.activeLayerIndex());
                    updateLayerStatusText();
                    updatePersistenceText();
                }
            }
            if (actions.consumePressed("next_scroll_preset")) {
                if (m_tileMapSession.cycleLayerScrollPreset(m_tileMapSession.activeLayerIndex(),
                                                            1)) {
                    resetLayerRuntimeScroll(m_tileMapSession.activeLayerIndex());
                    updateLayerStatusText();
                    updatePersistenceText();
                }
            }

            m_devModeController.updateMoveMode(deltaTime, moveAxis);
            m_playerController.stopMotion();
            m_playerController.setPosition(m_devModeController.position());
            m_playerController.setFacingFromHorizontal(moveAxis.x);
            m_tileCursor.hide();
            updateLayerStatusText();
            updateActionLegendText();
            camera->followTarget(m_playerController.getPosition() +
                                     glm::vec2(0.0f, kCameraTargetYOffset),
                                 kCameraFollowSpeed);
            finishSceneFrame();
            return;

        case DevelopmentSubmode::SelectTileMode: {
            if (!m_devModeController.hasSelection()) {
                initializeSelectTileMode();
            }

            const glm::ivec2 selectionAxis(moveAxis.x < 0.0f ? -1 : (moveAxis.x > 0.0f ? 1 : 0),
                                           moveAxis.y < 0.0f ? -1 : (moveAxis.y > 0.0f ? 1 : 0));
            bool selectionUiChanged = m_devModeController.updateSelectTileMode(
                deltaTime, selectionAxis, m_tileMapSession.maxTileCoordinate());
            selectionUiChanged = selectionUiChanged || persistenceActionConsumed;

            if (m_devModeController.hasSelection()) {
                const glm::ivec2 selectedTile = m_devModeController.selectedTile();
                const int selectedTileId = m_tileMapSession.editableTileId(selectedTile);
                if (actions.consumePressed("next_tile")) {
                    const int paletteBaseTile =
                        m_devModeController.clipboardTile().value_or(selectedTileId);
                    const auto nextPaletteTile =
                        m_tileMapSession.cycledEditableTileId(paletteBaseTile, 1);
                    if (nextPaletteTile.has_value()) {
                        m_devModeController.setClipboardTile(nextPaletteTile.value());
                        selectionUiChanged = true;
                    }
                }
                if (actions.consumePressed("previous_tile")) {
                    const int paletteBaseTile =
                        m_devModeController.clipboardTile().value_or(selectedTileId);
                    const auto previousPaletteTile =
                        m_tileMapSession.cycledEditableTileId(paletteBaseTile, -1);
                    if (previousPaletteTile.has_value()) {
                        m_devModeController.setClipboardTile(previousPaletteTile.value());
                        selectionUiChanged = true;
                    }
                }
                if (actions.consumePressed("copy_tile")) {
                    m_devModeController.setClipboardTile(selectedTileId);
                    selectionUiChanged = true;
                }
                if (actions.consumePressed("paste_tile")) {
                    const auto clipboardTile = m_devModeController.clipboardTile();
                    if (clipboardTile.has_value()) {
                        const bool painted =
                            m_tileMapSession.setEditableTileId(selectedTile, clipboardTile.value());
                        selectionUiChanged |= painted;
                    }
                }
                if (actions.consumePressed("undo_tile_edit")) {
                    const bool undid = m_tileMapSession.undoLastEditableEdit();
                    selectionUiChanged |= undid;
                }
                if (actions.consumePressed("redo_tile_edit")) {
                    const bool redid = m_tileMapSession.redoLastEditableEdit();
                    selectionUiChanged |= redid;
                }
            }

            if (selectionUiChanged || !m_tileCursor.isVisible()) {
                updateSelectTileUi();
            }

            if (m_devModeController.hasSelection()) {
                camera->followTarget(
                    m_tileMapSession.tileCenterWorld(m_devModeController.selectedTile()),
                    kCameraFollowSpeed);
            }
            m_playerController.stopMotion();
            finishSceneFrame();
            return;
        }
        }
    }

    m_playerController.update(deltaTime, moveAxis.x, actions.consumePressed("jump"),
                              m_tileMapSession);

    if (m_playerController.getPosition().y < kRespawnFloorY) {
        m_playerController.reset(m_tileMapSession, camera);
        m_devModeController.setPosition(m_playerController.getPosition());
        finishSceneFrame();
        return;
    }

    camera->followTarget(m_playerController.getPosition() + glm::vec2(0.0f, kCameraTargetYOffset),
                         kCameraFollowSpeed);
    finishSceneFrame();
}

void LevelBuilderScene::updateCameraDependentVisuals([[maybe_unused]] float deltaTime) {
    if (const auto* camera = currentCamera(); camera != nullptr) {
        applyLayerRuntimeTransforms(camera->getPosition());
        m_tilePalette.updatePosition(camera->getVisibleRect());
    }
}

std::string LevelBuilderScene::getGameName() const {
    return "Level Builder";
}

std::vector<std::string> LevelBuilderScene::getGameplaySummary() const {
    return {
        "The post-Phase-6 slices turn Select Tile Mode into a palette-driven paint workflow on "
        "top of the persisted ground-layer overlay.",
        "Controller and keyboard actions now cycle the active palette tile, copy from the map, "
        "paint with the palette, and undo or redo edits.",
        "The HUD and debug overlay expose the selected tile ID, palette state, history depth, "
        "and overlay save status while you edit.",
    };
}

std::vector<std::string> LevelBuilderScene::getGoals() const {
    return {
        "Keep edit history and persisted overlay state behind TileMapSession instead of leaking "
        "authoring bookkeeping into the scene.",
        "Reuse one visible palette or clipboard concept instead of adding a second brush state "
        "model that controller-first users would have to learn separately.",
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
        "N / Gamepad A (Dev Move Mode) - Add a new layer and select it",
        "J / K or Gamepad X / Y (Dev Move Mode) - Select previous or next layer",
        "H / Gamepad B (Dev Move Mode) - Toggle the active layer visibility",
        "O / P or Gamepad LT / RT (Dev Move Mode) - Move the active layer depth down or up",
        "[ / ] or Gamepad Right Stick Left / Right (Dev Move Mode) - Cycle layer scroll preset",
        "Z / X or Gamepad B / A - Previous or next palette tile in Select Tile Mode",
        "C / V or Gamepad X / Y - Copy a tile to the palette or paint the selection",
        "U / I or Gamepad LT / RT - Undo or redo the last tile edit",
        "F5 / F9 or Gamepad L3 / R3 - Save or reload the editable ground-layer overlay",
    };
}

void LevelBuilderScene::drawDebugUI() {
    BaseGameScene::drawDebugUI();

#ifdef VDE_GAME_USE_IMGUI
    ImGui::SetNextWindowPos(ImVec2(10, 170), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(360, 165), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Level Builder")) {
        const std::string clipboardState = formatClipboardState();
        ImGui::Text("Mode: %s", m_devModeController.isEnabled() ? "Development" : "Play");
        ImGui::Text("Submode: %s", m_devModeController.isEnabled()
                                       ? m_devModeController.activeSubmodeName()
                                       : "N/A");
        ImGui::Text("Player: %.2f, %.2f", m_playerController.getPosition().x,
                    m_playerController.getPosition().y);
        if (const LayerDefinition* activeLayer =
                m_tileMapSession.layerDefinition(m_tileMapSession.activeLayerIndex());
            activeLayer != nullptr) {
            ImGui::Text("Active Layer: %llu / %llu",
                        static_cast<unsigned long long>(m_tileMapSession.activeLayerIndex() + 1),
                        static_cast<unsigned long long>(m_tileMapSession.layerCount()));
            ImGui::Text("Layer Name: %s", activeLayer->name.c_str());
            ImGui::Text("Depth: %.2f  Visible: %s  Collision: %s", activeLayer->depthZ,
                        activeLayer->visible ? "ON" : "OFF",
                        activeLayer->collisionEnabled ? "ON" : "OFF");
            ImGui::Text("Scroll: %s", activeLayerScrollPresetName().c_str());
            ImGui::Text("Follow: %.2f, %.2f  Velocity: %.2f, %.2f", activeLayer->followFactorX,
                        activeLayer->followFactorY, activeLayer->scrollVelocityX,
                        activeLayer->scrollVelocityY);
        }
        if (m_devModeController.hasSelection()) {
            const std::string editableLayer = m_tileMapSession.editableLayerName();
            const int tileId = m_tileMapSession.editableTileId(m_devModeController.selectedTile());
            ImGui::Text("Selected Tile: %d, %d", m_devModeController.selectedTile().x,
                        m_devModeController.selectedTile().y);
            ImGui::Text("Editable Layer: %s", editableLayer.c_str());
            ImGui::Text("Tile ID: %s", formatTileId(tileId).c_str());
        }
        ImGui::Text("Palette: %s", clipboardState.c_str());
        ImGui::Text("Undo / Redo: %llu / %llu",
                    static_cast<unsigned long long>(m_tileMapSession.undoDepth()),
                    static_cast<unsigned long long>(m_tileMapSession.redoDepth()));
        ImGui::Text("Overlay: %s (%s)", m_tileMapSession.hasUnsavedChanges() ? "Dirty" : "Clean",
                    m_tileMapSession.overlayFileName().c_str());
        ImGui::TextWrapped("Persistence: %s", m_tileMapSession.lastPersistenceStatus().c_str());
        ImGui::TextColored(ImVec4(0.65f, 0.82f, 0.95f, 1.0f),
                           "Start / Enter toggles Development mode");
        if (m_devModeController.isEnabled() &&
            m_devModeController.activeSubmode() == DevelopmentSubmode::MoveMode) {
            ImGui::Text("Move Mode: free movement, no collisions, no gravity");
            ImGui::Text("L3 / R3: save / reload overlay");
        }
        if (m_devModeController.isEnabled() &&
            m_devModeController.activeSubmode() == DevelopmentSubmode::SelectTileMode) {
            ImGui::Text("Select Tile Mode: movement controls step the tile selection");
            ImGui::Text("A / B: next / previous palette tile, X / Y: copy / paint");
            ImGui::Text("LT / RT: undo / redo, L3 / R3: save / reload overlay");
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

    m_persistenceText = addEntity<vde::TextEntity>();
    m_persistenceText->setFont(vde::BitmapFont::small());
    m_persistenceText->setStyle({.color = vde::Color(0.76f, 0.93f, 0.80f, 1.0f), .pixelScale = 1});
    m_persistenceText->setAnchor(0.0f, 0.5f);
    m_persistenceText->setPosition(kPersistenceTextX, kPersistenceTextY, 1.2f);
    m_persistenceText->setWorldHeight(kPersistenceTextHeight);

    m_actionLegendLines.clear();
    constexpr size_t kActionLegendLineCount = 11;
    for (size_t index = 0; index < kActionLegendLineCount; ++index) {
        auto line = addEntity<vde::TextEntity>();
        line->setFont(vde::BitmapFont::small());
        line->setStyle({.color = vde::Color(0.88f, 0.94f, 0.99f, 1.0f), .pixelScale = 1});
        line->setAnchor(0.0f, 0.5f);
        line->setPosition(
            kActionLegendX,
            kActionLegendTopY - (static_cast<float>(index) * kActionLegendLineSpacing), 1.2f);
        line->setWorldHeight(kActionLegendLineHeight);
        m_actionLegendLines.push_back(line);
    }

    updateActionLegendText();
    updateModeText();
    updateLayerStatusText();
    updatePersistenceText();
    setSelectTileUiVisible(false);
    setActionLegendVisible(false);
}

void LevelBuilderScene::initializeSelectTileMode() {
    m_devModeController.setSelectedTile(
        m_tileMapSession.nearestTileToWorld(m_playerController.getPosition()));
    updateSelectTileUi();
}

void LevelBuilderScene::updateSelectTileUi() {
    if (!m_devModeController.hasSelection()) {
        setSelectTileUiVisible(false);
        updateLayerStatusText();
        return;
    }

    setSelectTileUiVisible(true);
    m_tilePalette.setCurrentTile(m_devModeController.clipboardTile());
    const glm::ivec2 selectedTile = m_devModeController.selectedTile();
    const glm::vec2 tileCenter = m_tileMapSession.tileCenterWorld(selectedTile);
    m_tileCursor.show(tileCenter, m_tileMapSession.tileMap()->getTileWidth(),
                      m_tileMapSession.tileMap()->getTileHeight());

    updateLayerStatusText();
    updateActionLegendText();
    updatePersistenceText();
}

void LevelBuilderScene::updateActionLegendText() {
    std::vector<std::string> actionLines;
    if (m_devModeController.isEnabled() &&
        m_devModeController.activeSubmode() == DevelopmentSubmode::MoveMode) {
        actionLines = {
            "MOVE MODE",
            "DPAD / STICK - MOVE",
            "A - ADD LAYER",
            "X / Y - PREV / NEXT",
            "B - TOGGLE VISIBILITY",
            "LT / RT - DEPTH - / +",
            "RIGHT STICK L / R - SCROLL PRESET",
            "L3 - SAVE OVERLAY",
            "R3 - RELOAD OVERLAY",
            "LB / RB - CHANGE SUBMODE",
            "START - EXIT DEV MODE",
        };
    } else {
        const char* paintActionText = m_devModeController.hasClipboardTile()
                                          ? "Y - PAINT TILE"
                                          : "Y - PAINT TILE (COPY FIRST)";
        actionLines = {
            "SELECT TILE MODE",         "DPAD / STICK - MOVE TILE", "A - NEXT PALETTE",
            "B - PREV PALETTE",         "X - COPY TILE TO PALETTE", paintActionText,
            "LT / RT - UNDO / REDO",    "L3 - SAVE OVERLAY",        "R3 - RELOAD OVERLAY",
            "LB / RB - CHANGE SUBMODE", "START - EXIT DEV MODE",
        };
    }

    for (size_t index = 0; index < m_actionLegendLines.size(); ++index) {
        if (m_actionLegendLines.at(index) != nullptr) {
            m_actionLegendLines.at(index)->setText(
                index < actionLines.size() ? actionLines.at(index) : "");
        }
    }
}

void LevelBuilderScene::updateLayerStatusText() {
    if (m_selectionText == nullptr) {
        return;
    }

    if (!m_devModeController.isEnabled()) {
        m_selectionText->setVisible(false);
        return;
    }

    const LayerDefinition* activeLayer =
        m_tileMapSession.layerDefinition(m_tileMapSession.activeLayerIndex());
    if (activeLayer == nullptr) {
        m_selectionText->setVisible(false);
        return;
    }

    std::string text = "LAYER " + std::to_string(m_tileMapSession.activeLayerIndex() + 1) + "/" +
                       std::to_string(m_tileMapSession.layerCount()) + ": " + activeLayer->name +
                       "  Z: " + formatDepth(activeLayer->depthZ) +
                       "  VIS: " + std::string(activeLayer->visible ? "ON" : "OFF") +
                       "  COLL: " + std::string(activeLayer->collisionEnabled ? "ON" : "OFF") +
                       "  SCROLL: " + activeLayerScrollPresetName();

    if (m_devModeController.activeSubmode() == DevelopmentSubmode::SelectTileMode &&
        m_devModeController.hasSelection()) {
        const glm::ivec2 selectedTile = m_devModeController.selectedTile();
        const int tileId = m_tileMapSession.editableTileId(selectedTile);
        text += "  TILE: " + std::to_string(selectedTile.x) + ", " +
                std::to_string(selectedTile.y) + "  ID: " + formatTileId(tileId) +
                "  PAL: " + formatClipboardState();
    }

    m_selectionText->setText(text);
    m_selectionText->setVisible(true);
}

void LevelBuilderScene::updatePersistenceText() {
    if (m_persistenceText == nullptr) {
        return;
    }

    const bool hasUnsavedChanges = m_tileMapSession.hasUnsavedChanges();
    const vde::Color textColor =
        hasUnsavedChanges ? vde::Color::fromHex(0xffb56a) : vde::Color(0.76f, 0.93f, 0.80f, 1.0f);
    m_persistenceText->setStyle({.color = textColor, .pixelScale = 1});
    m_persistenceText->setText(std::string("OVERLAY: ") + (hasUnsavedChanges ? "DIRTY" : "CLEAN") +
                               "  FILE: " + m_tileMapSession.overlayFileName());
}

void LevelBuilderScene::setSelectTileUiVisible(bool visible) {
    if (!visible) {
        m_tileCursor.hide();
        m_tilePalette.hide();
    } else {
        m_tilePalette.show();
    }

    if (m_selectionText != nullptr) {
        m_selectionText->setVisible(visible);
    }
}

void LevelBuilderScene::setActionLegendVisible(bool visible) {
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
        updateLayerStatusText();
        updateActionLegendText();
        return;
    }

    m_modeText->setStyle({.color = vde::Color(0.84f, 0.92f, 0.98f, 1.0f), .pixelScale = 1});
    m_modeText->setText("MODE: PLAY");
    updateLayerStatusText();
}

void LevelBuilderScene::clearLayerRuntimes() {
    for (const auto& runtime : m_layerRuntimes) {
        if (runtime.tileMap != nullptr) {
            retireResource(runtime.tileMap);
            removeEntity(runtime.tileMap->getId());
        }
    }
    m_layerRuntimes.clear();
}

void LevelBuilderScene::rebuildLayerRuntimes() {
    clearLayerRuntimes();

    m_layerRuntimes.reserve(m_tileMapSession.layerCount());
    for (size_t layerOffset = 0; layerOffset < m_tileMapSession.layerCount(); ++layerOffset) {
        const size_t layerIndex = m_tileMapSession.layerCount() - layerOffset - 1;
        auto runtimeTileMap = m_tileMapSession.createRuntimeTileMap(layerIndex);
        if (runtimeTileMap == nullptr) {
            continue;
        }

        addEntity(std::static_pointer_cast<vde::Entity>(runtimeTileMap));
        (void)moveEntityToBack(runtimeTileMap->getId());
        m_layerRuntimes.push_back(
            {.layerIndex = layerIndex,
             .tileMap = std::move(runtimeTileMap),
             .scrollOffset = glm::vec2(0.0f),
             .appliedSyncRevision = m_tileMapSession.runtimeLayerSyncRevision(layerIndex)});
    }
    std::ranges::reverse(m_layerRuntimes);
    if (m_layerRuntimes.size() == m_tileMapSession.layerCount()) {
        m_appliedRuntimeLayoutRevision = m_tileMapSession.runtimeLayoutRevision();
    }
}

void LevelBuilderScene::synchronizeLayerRuntimes() {
    if (m_layerRuntimes.size() != m_tileMapSession.layerCount() ||
        m_appliedRuntimeLayoutRevision != m_tileMapSession.runtimeLayoutRevision()) {
        rebuildLayerRuntimes();
        return;
    }

    for (auto& runtime : m_layerRuntimes) {
        const size_t revision = m_tileMapSession.runtimeLayerSyncRevision(runtime.layerIndex);
        if (runtime.appliedSyncRevision == revision) {
            continue;
        }
        if (runtime.tileMap == nullptr ||
            !m_tileMapSession.syncRuntimeTileMap(runtime.layerIndex, *runtime.tileMap)) {
            rebuildLayerRuntimes();
            return;
        }
        runtime.appliedSyncRevision = revision;
    }
}

void LevelBuilderScene::resetLayerRuntimeScroll(size_t layerIndex) {
    const auto runtimeIt =
        std::ranges::find_if(m_layerRuntimes, [layerIndex](const LayerRuntime& runtime) {
            return runtime.layerIndex == layerIndex;
        });
    if (runtimeIt == m_layerRuntimes.end()) {
        return;
    }
    runtimeIt->scrollOffset = glm::vec2(0.0f);
}

void LevelBuilderScene::advanceLayerRuntimeScroll(float deltaTime) {
    for (auto& runtime : m_layerRuntimes) {
        const LayerDefinition* layer = m_tileMapSession.layerDefinition(runtime.layerIndex);
        if (layer == nullptr) {
            continue;
        }

        runtime.scrollOffset +=
            glm::vec2(layer->scrollVelocityX, layer->scrollVelocityY) * deltaTime;
    }
}

void LevelBuilderScene::applyLayerRuntimeTransforms(const glm::vec2& cameraPosition) {
    for (auto& runtime : m_layerRuntimes) {
        if (runtime.tileMap == nullptr) {
            continue;
        }
        const auto position = m_tileMapSession.runtimeLayerPosition(
            runtime.layerIndex, cameraPosition, runtime.scrollOffset);
        if (position.has_value()) {
            runtime.tileMap->setPosition(position->x, position->y, position->z);
        }
    }
}

std::string LevelBuilderScene::activeLayerScrollPresetName() const {
    const auto preset = m_tileMapSession.layerScrollPreset(m_tileMapSession.activeLayerIndex());
    return preset.has_value() ? TileMapSession::layerScrollPresetName(preset.value()) : "Custom";
}

std::string LevelBuilderScene::formatClipboardState() const {
    const auto clipboardTile = m_devModeController.clipboardTile();
    if (!clipboardTile.has_value()) {
        return "UNSET";
    }

    return formatTileId(clipboardTile.value());
}

void LevelBuilderScene::setDevelopmentMode(bool enabled) {
    if (enabled) {
        m_devModeController.enter(m_playerController.getPosition());
        m_playerController.stopMotion();
        setActionLegendVisible(true);
    } else {
        m_devModeController.exit();
        m_playerController.stopMotion();
        setSelectTileUiVisible(false);
        setActionLegendVisible(false);
    }

    syncInputMode();
    updateModeText();
    updatePersistenceText();
    std::cout << (enabled ? "Development mode enabled\n" : "Development mode disabled\n");
}

void LevelBuilderScene::syncInputMode() {
    auto* controls = input();
    if (controls == nullptr) {
        return;
    }

    if (!m_devModeController.isEnabled()) {
        controls->setMode(LevelBuilderInputMode::Play);
        return;
    }

    controls->setMode(m_devModeController.activeSubmode() == DevelopmentSubmode::MoveMode
                          ? LevelBuilderInputMode::DevelopmentMove
                          : LevelBuilderInputMode::DevelopmentSelectTile);
}

LevelBuilderInput* LevelBuilderScene::input() {
    return dynamic_cast<LevelBuilderInput*>(getInputHandler());
}

vde::Camera2D* LevelBuilderScene::currentCamera() {
    return dynamic_cast<vde::Camera2D*>(getCamera());
}

}  // namespace levelbuilder