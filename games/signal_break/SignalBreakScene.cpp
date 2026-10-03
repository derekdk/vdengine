#include "SignalBreakScene.h"

#include "Input.h"

namespace signalbreak {

void SignalBreakScene::onEnter() {
    printGameHeader();
    setDebugUIVisible(false);
    setup2D(24.0f, 14.0f, vde::Color::fromHex(0xaecbc9));
    m_art = createArtwork(*getGame());
    createWorldVisuals();
    createHud();
    updatePresentation();
}

void SignalBreakScene::update(float deltaTime) {
    auto* input = dynamic_cast<SignalBreakInput*>(getInputHandler());
    if (input) {
        m_gameplay.update(deltaTime, input->sample());
    }
    updatePresentation();
    BaseGameScene::update(deltaTime);
}

std::string SignalBreakScene::getGameName() const {
    return "Signal Break";
}

std::vector<std::string> SignalBreakScene::getGameplaySummary() const {
    return {"Run-and-gun platform combat through a scrolling industrial outpost",
            "Elevated sentries, aimed enemy fire, checkpoints and extraction"};
}

std::vector<std::string> SignalBreakScene::getGoals() const {
    return {"Defeat the commander at the far right and reach the extraction beacon",
            "Checkpoints restore health; jump after defeat to retry from the last checkpoint"};
}

std::vector<std::string> SignalBreakScene::getControls() const {
    return {"A/D or arrows - Move | W/S or up/down - Aim diagonally",
            "SPACE - Jump / Retry | J or X (hold) - Fire | P - Pause | R - New run",
            "Gamepad: left stick / D-pad - Move and aim | A - Jump / Retry",
            "Gamepad: X / RB / RT (hold) - Fire | Start - Pause | Y - New run"};
}

std::optional<double> SignalBreakScene::getScriptStateValue(const std::string& key) const {
    if (key == "player_x") {
        return m_gameplay.playerPosition().x;
    }
    if (key == "player_y") {
        return m_gameplay.playerPosition().y;
    }
    if (key == "health") {
        return m_gameplay.health();
    }
    if (key == "shots") {
        return m_gameplay.shots();
    }
    if (key == "enemy_shots") {
        return m_gameplay.enemyShots();
    }
    if (key == "kills") {
        return m_gameplay.kills();
    }
    if (key == "jumps") {
        return m_gameplay.jumps();
    }
    if (key == "grounded") {
        return m_gameplay.grounded() ? 1.0 : 0.0;
    }
    if (key == "paused") {
        return m_gameplay.paused() ? 1.0 : 0.0;
    }
    if (key == "phase") {
        return static_cast<double>(m_gameplay.phase());
    }
    if (key == "checkpoint") {
        return m_gameplay.checkpoint();
    }
    if (key == "camera_x") {
        return m_cameraX;
    }
    return std::nullopt;
}

}  // namespace signalbreak
