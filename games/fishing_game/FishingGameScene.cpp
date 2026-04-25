#include "FishingGameScene.h"

#include "FishingInput.h"

#ifdef VDE_GAME_USE_IMGUI
#include <imgui.h>
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <utility>

using namespace vde;

namespace fishing {

namespace {

Color fishColor(size_t index) {
    static const std::array<Color, 5> colors = {
        Color(0.95f, 0.72f, 0.22f, 1.0f), Color(0.32f, 0.76f, 0.95f, 1.0f),
        Color(0.95f, 0.45f, 0.30f, 1.0f), Color(0.45f, 0.88f, 0.58f, 1.0f),
        Color(0.72f, 0.58f, 0.95f, 1.0f),
    };
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
    return colors[index % colors.size()];
}

std::string lineStateLabel(FishingGameScene::LineState state) {
    switch (state) {
    case FishingGameScene::LineState::Stowed:
        return "stowed";
    case FishingGameScene::LineState::InWater:
        return "waiting";
    case FishingGameScene::LineState::Hooked:
        return "hooked";
    }
    return "unknown";
}

}  // namespace

FishingGameScene::FishingGameScene() : m_rng(1337) {}  // NOLINT(bugprone-random-generator-seed)

void FishingGameScene::onEnter() {
    printGameHeader();
    m_input = dynamic_cast<FishingInput*>(getInputHandler());
    setup2D(kViewWidth, kViewHeight, Color(0.72f, 0.88f, 0.98f, 1.0f));

    createEnvironment();
    createHud();
    resetRound();
}

void FishingGameScene::update(float deltaTime) {
    BaseGameScene::update(deltaTime);
    m_elapsed += deltaTime;

    auto* controls = input();
    if (controls && controls->keys.consume("restart")) {
        resetRound();
        return;
    }

    updateBoat(deltaTime);
    updateBobber(deltaTime);
    updateFish(deltaTime);
    updateHud();
}

void FishingGameScene::drawDebugUI() {
    BaseGameScene::drawDebugUI();

#ifdef VDE_GAME_USE_IMGUI
    ImGui::SetNextWindowPos(ImVec2(10, 180), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(320, 220), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Fishing State")) {
        ImGui::Text("Score: %d", m_score);
        ImGui::Text("Caught: %d / %d", m_caughtCount, kGoalFish);
        ImGui::Text("Boat X: %.2f", m_boatX);
        ImGui::Text("Bobber depth: %.2f", m_bobberDepth);
        ImGui::Text("Line: %s", lineStateLabel(m_lineState).c_str());
        ImGui::TextWrapped("Status: %s", m_status.c_str());
        ImGui::Separator();
        for (size_t index = 0; index < m_fish.size(); ++index) {
            const auto& fish = m_fish
                [index];  // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
            ImGui::Text("Fish %zu  x=%.2f y=%.2f nibble=%.2f", index, fish.x, fish.y,
                        fish.nibbleProgress);
        }
    }
    ImGui::End();
#endif
}

std::string FishingGameScene::getGameName() const {
    return "Pond Fisher";
}

std::vector<std::string> FishingGameScene::getGameplaySummary() const {
    return {
        "A compact 2D fishing game built as a multi-file VDE game application.",
        "Move the boat, drop the bobber to different depths, and reel in fish.",
        "Uses the same scripted-input workflow as examples for smoke coverage.",
    };
}

std::vector<std::string> FishingGameScene::getGoals() const {
    return {
        "Catch three fish to complete the round.",
        "Match the bobber depth to a fish lane and wait for a bite.",
        "Reel in when the bobber flashes gold to secure the catch.",
    };
}

std::vector<std::string> FishingGameScene::getControls() const {
    return {
        "LEFT/RIGHT - Move the boat",
        "UP/DOWN    - Raise or lower the bobber while the line is in the water",
        "SPACE      - Cast, reel in, or land a hooked fish",
        "R          - Reset the round",
    };
}

void FishingGameScene::createEnvironment() {
    m_water = addEntity<SpriteEntity>();
    m_water->setPosition(0.0f, -1.25f, 0.0f);
    m_water->setScale(kViewWidth, 5.8f, 1.0f);
    m_water->setColor(Color(0.10f, 0.32f, 0.62f, 1.0f));

    m_surface = addEntity<SpriteEntity>();
    m_surface->setPosition(0.0f, kWaterSurfaceY, 0.0f);
    m_surface->setScale(kViewWidth, 0.18f, 1.0f);
    m_surface->setColor(Color(0.84f, 0.95f, 1.0f, 1.0f));

    auto dock = addEntity<SpriteEntity>();
    dock->setPosition(0.0f, 3.6f, 0.0f);
    dock->setScale(kViewWidth, 1.0f, 1.0f);
    dock->setColor(Color(0.67f, 0.53f, 0.33f, 1.0f));

    m_boatHull = addEntity<SpriteEntity>();
    m_boatHull->setPosition(0.0f, 2.4f, 0.0f);
    m_boatHull->setScale(1.7f, 0.55f, 1.0f);
    m_boatHull->setColor(Color(0.58f, 0.28f, 0.14f, 1.0f));

    m_boatCabin = addEntity<SpriteEntity>();
    m_boatCabin->setPosition(0.0f, 2.78f, 0.0f);
    m_boatCabin->setScale(0.75f, 0.35f, 1.0f);
    m_boatCabin->setColor(Color(0.95f, 0.92f, 0.80f, 1.0f));

    m_bobber = addEntity<SpriteEntity>();
    m_bobber->setScale(0.22f, 0.35f, 1.0f);
    m_bobber->setColor(Color(0.95f, 0.28f, 0.24f, 1.0f));
    m_bobber->setVisible(false);

    m_fish.resize(5);
    for (size_t index = 0; index < m_fish.size(); ++index) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
        auto& fish = m_fish[index];
        fish.entity = addEntity<SpriteEntity>();
        fish.entity->setScale(0.65f, 0.24f, 1.0f);
        fish.entity->setColor(fishColor(index));
        respawnFish(fish, index == 0);
    }
}

void FishingGameScene::createHud() {
    m_titleText = addEntity<TextEntity>();
    m_titleText->setText("POND FISHER");
    m_titleText->setFont(BitmapFont::large());
    m_titleText->setStyle({.color = Color(0.16f, 0.21f, 0.30f, 1.0f), .pixelScale = 2});
    m_titleText->setPosition(-6.2f, 4.35f, 0.0f);
    m_titleText->setWorldHeight(0.55f);

    m_scoreText = addEntity<TextEntity>();
    m_scoreText->setFont(BitmapFont::small());
    m_scoreText->setStyle({.color = Color(0.08f, 0.18f, 0.28f, 1.0f), .pixelScale = 2});
    m_scoreText->setPosition(-6.2f, 3.72f, 0.0f);
    m_scoreText->setWorldHeight(0.30f);

    m_statusText = addEntity<TextEntity>();
    m_statusText->setFont(BitmapFont::small());
    m_statusText->setStyle({.color = Color::white(), .pixelScale = 2});
    m_statusText->setPosition(-6.2f, -4.55f, 0.0f);
    m_statusText->setWorldHeight(0.28f);

    m_controlText = addEntity<TextEntity>();
    m_controlText->setText("SPACE cast/reel   arrows move + depth   R reset");
    m_controlText->setFont(BitmapFont::small());
    m_controlText->setStyle({.color = Color(0.88f, 0.94f, 0.98f, 1.0f), .pixelScale = 1});
    m_controlText->setPosition(-6.2f, -4.9f, 0.0f);
    m_controlText->setWorldHeight(0.22f);
}

void FishingGameScene::resetRound() {
    m_lineState = LineState::Stowed;
    m_hookedFishIndex = -1;
    m_boatX = 0.0f;
    m_bobberDepth = 2.3f;
    m_score = 0;
    m_caughtCount = 0;
    m_elapsed = 0.0f;
    m_status = "Move to a fish, cast the line, and wait for a bite.";
    m_bobber->setVisible(false);
    m_bobber->setColor(Color(0.95f, 0.28f, 0.24f, 1.0f));
    m_boatHull->setPosition(m_boatX, 2.4f, 0.0f);
    m_boatCabin->setPosition(m_boatX, 2.78f, 0.0f);

    m_rng.seed(1337);  // NOLINT(bugprone-random-generator-seed)
    for (size_t index = 0; index < m_fish.size(); ++index) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
        respawnFish(m_fish[index], index == 0);
    }

    updateHud();
}

void FishingGameScene::respawnFish(FishState& fish, bool guaranteeStartFish) {
    if (guaranteeStartFish) {
        fish.x = 0.0f;
        fish.y = kWaterSurfaceY - 2.3f;
        fish.speed = 0.18f;
        fish.width = 0.65f;
        fish.scoreValue = 25;
        fish.goingRight = true;
    } else {
        std::uniform_real_distribution<float> xDist(kBoatMinX + 0.3f, kBoatMaxX - 0.3f);
        std::uniform_real_distribution<float> yDist(kWaterFloorY + 0.5f, kWaterSurfaceY - 0.9f);
        std::uniform_real_distribution<float> speedDist(0.35f, 0.95f);
        std::uniform_int_distribution<int> scoreDist(10, 40);
        std::uniform_int_distribution<int> dirDist(0, 1);
        fish.x = xDist(m_rng);
        fish.y = yDist(m_rng);
        fish.speed = speedDist(m_rng);
        fish.width = 0.65f;
        fish.scoreValue = scoreDist(m_rng);
        fish.goingRight = dirDist(m_rng) == 0;
    }

    fish.nibbleProgress = 0.0f;
    fish.entity->setPosition(fish.x, fish.y, 0.0f);
    fish.entity->setVisible(true);
}

void FishingGameScene::updateBoat(float deltaTime) {
    auto* controls = input();
    if (!controls) {
        return;
    }

    float moveDir = 0.0f;
    if (controls->keys.isHeld("left")) {
        moveDir -= 1.0f;
    }
    if (controls->keys.isHeld("right")) {
        moveDir += 1.0f;
    }

    m_boatX = std::clamp(m_boatX + moveDir * kBoatSpeed * deltaTime, kBoatMinX, kBoatMaxX);
    m_boatHull->setPosition(m_boatX, 2.4f, 0.0f);
    m_boatCabin->setPosition(m_boatX, 2.78f, 0.0f);
}

void FishingGameScene::updateBobber(float deltaTime) {
    auto* controls = input();
    if (!controls) {
        return;
    }

    if (controls->keys.consume("action")) {
        if (m_lineState == LineState::Stowed) {
            deployLine();
        } else {
            reelLine();
        }
    }

    if (m_lineState == LineState::Stowed) {
        return;
    }

    if (m_lineState == LineState::InWater) {
        float depthDelta = 0.0f;
        if (controls->keys.isHeld("up")) {
            depthDelta -= kDepthAdjustSpeed * deltaTime;
        }
        if (controls->keys.isHeld("down")) {
            depthDelta += kDepthAdjustSpeed * deltaTime;
        }
        m_bobberDepth = std::clamp(m_bobberDepth + depthDelta, kMinDepth, kMaxDepth);
        m_bobber->setColor(Color(0.95f, 0.28f, 0.24f, 1.0f));
        m_bobber->setPosition(m_boatX, kWaterSurfaceY - m_bobberDepth, 0.0f);
    } else if (m_hookedFishIndex >= 0 && std::cmp_less(m_hookedFishIndex, m_fish.size())) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
        const auto& hooked = m_fish[static_cast<size_t>(m_hookedFishIndex)];
        m_bobber->setColor(Color(0.98f, 0.88f, 0.16f, 1.0f));
        m_bobber->setPosition(hooked.x, hooked.y, 0.0f);
    }
}

void FishingGameScene::updateFish(float deltaTime) {
    for (size_t index = 0; index < m_fish.size(); ++index) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
        auto& fish = m_fish[index];
        if (!fish.entity->isVisible()) {
            continue;
        }

        if (m_lineState != LineState::Hooked || std::cmp_not_equal(index, m_hookedFishIndex)) {
            float direction = fish.goingRight ? 1.0f : -1.0f;
            fish.x += direction * fish.speed * deltaTime;

            if (fish.x <= kBoatMinX) {
                fish.x = kBoatMinX;
                fish.goingRight = true;
            } else if (fish.x >= kBoatMaxX) {
                fish.x = kBoatMaxX;
                fish.goingRight = false;
            }
        }

        if (m_lineState == LineState::InWater) {
            float bobberY = kWaterSurfaceY - m_bobberDepth;
            if (std::fabs(fish.x - m_boatX) <= kHookDistanceX &&
                std::fabs(fish.y - bobberY) <= kHookDistanceY) {
                fish.nibbleProgress += deltaTime;
                fish.entity->setColor(Color(1.0f, 0.97f, 0.45f, 1.0f));
                if (fish.nibbleProgress >= kNibbleTime) {
                    m_lineState = LineState::Hooked;
                    m_hookedFishIndex = static_cast<int>(index);
                    m_status = "Fish on! Press SPACE again to land it.";
                }
            } else {
                fish.nibbleProgress = 0.0f;
                fish.entity->setColor(fishColor(index));
            }
        } else if (m_lineState != LineState::Hooked ||
                   std::cmp_not_equal(index, m_hookedFishIndex)) {
            fish.nibbleProgress = 0.0f;
            fish.entity->setColor(fishColor(index));
        } else {
            fish.entity->setColor(Color(1.0f, 0.86f, 0.18f, 1.0f));
        }

        fish.entity->setPosition(fish.x, fish.y, 0.0f);
    }
}

void FishingGameScene::updateHud() {
    std::ostringstream scoreStream;
    scoreStream << "Score: " << m_score << "   Fish: " << m_caughtCount << '/' << kGoalFish;
    m_scoreText->setText(scoreStream.str());

    std::ostringstream statusStream;
    statusStream << m_status << "  Depth " << std::fixed << std::setprecision(1) << m_bobberDepth;
    m_statusText->setText(statusStream.str());

    if (m_caughtCount >= kGoalFish) {
        m_statusText->setText("Round clear. Keep fishing or press R to reset.");
    }
}

void FishingGameScene::deployLine() {
    m_lineState = LineState::InWater;
    m_hookedFishIndex = -1;
    m_bobber->setVisible(true);
    m_bobber->setPosition(m_boatX, kWaterSurfaceY - m_bobberDepth, 0.0f);
    m_status = "Line in the water. Match depth and wait for a bite.";
}

void FishingGameScene::reelLine() {
    if (m_lineState == LineState::Hooked && m_hookedFishIndex >= 0 &&
        std::cmp_less(m_hookedFishIndex, m_fish.size())) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
        catchFish(m_fish[static_cast<size_t>(m_hookedFishIndex)]);
        return;
    }

    m_lineState = LineState::Stowed;
    m_hookedFishIndex = -1;
    m_bobber->setVisible(false);
    for (size_t index = 0; index < m_fish.size(); ++index) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
        m_fish[index].nibbleProgress = 0.0f;
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
        m_fish[index].entity->setColor(fishColor(index));
    }
    m_status = "Reeled in. Try another lane.";
}

void FishingGameScene::catchFish(FishState& fish) {
    m_score += fish.scoreValue;
    ++m_caughtCount;
    m_status = "Caught one! Cast again for another fish.";
    m_lineState = LineState::Stowed;
    m_hookedFishIndex = -1;
    m_bobber->setVisible(false);
    respawnFish(fish, false);
}

FishingInput* FishingGameScene::input() {
    return m_input;
}

}  // namespace fishing