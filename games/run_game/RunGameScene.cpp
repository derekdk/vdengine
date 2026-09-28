#include "RunGameScene.h"

#include <algorithm>
#include <cmath>

#include "Input.h"
#include "ObstacleCourse.h"
#include "ParallaxBackground.h"
#include "RunnerCharacter.h"

namespace rungame {

RunGameScene::RunGameScene() = default;
RunGameScene::~RunGameScene() = default;

void RunGameScene::onEnter() {
    printGameHeader();
    setup2D(24.0f, 13.5f, vde::Color::fromHex(0x83c9ef));

    m_background = std::make_unique<ParallaxBackground>(*this);
    m_obstacles = std::make_unique<ObstacleCourse>(*this);
    m_runner = std::make_unique<RunnerCharacter>(*this);
    createHud();
    resetGame();
}

void RunGameScene::update(float deltaTime) {
    BaseGameScene::update(deltaTime);

    auto* controls = dynamic_cast<RunGameInput*>(getInputHandler());
    if (!controls || !m_background || !m_runner || !m_obstacles) {
        return;
    }

    const bool jumpPressed = controls->keys.consume("jump");
    if (controls->keys.consume("restart") || (m_gameOver && jumpPressed)) {
        resetGame();
        return;
    }

    if (m_gameOver) {
        return;
    }

    if (jumpPressed) {
        m_runner->jump();
    }

    m_elapsed += deltaTime;
    const float runSpeed = std::min(10.5f, 6.0f + m_elapsed * 0.12f);
    m_distance += runSpeed * deltaTime;
    m_background->update(deltaTime, runSpeed);
    m_obstacles->update(deltaTime, runSpeed);
    m_runner->update(deltaTime);

    if (m_obstacles->collides(*m_runner)) {
        m_gameOver = true;
        m_statusText->setText("CRASH! Press SPACE to run again.");
    }
    updateHud();
}

std::string RunGameScene::getGameName() const {
    return "RunGame";
}

std::vector<std::string> RunGameScene::getGameplaySummary() const {
    return {
        "An endless runner with a looping, multi-layer parallax landscape.",
        "Jump over obstacles while the world speeds up around you.",
    };
}

std::vector<std::string> RunGameScene::getGoals() const {
    return {
        "Run as far as possible without hitting an obstacle.",
        "Your distance is your score.",
    };
}

std::vector<std::string> RunGameScene::getControls() const {
    return {
        "SPACE / UP / W - Jump",
        "R              - Restart",
    };
}

std::optional<double> RunGameScene::getScriptStateValue(const std::string& key) const {
    if (key == "game_over") {
        return m_gameOver ? 1.0 : 0.0;
    }
    if (key == "jumping") {
        return m_runner && m_runner->isJumping() ? 1.0 : 0.0;
    }
    if (key == "score") {
        return std::floor(m_distance);
    }
    return std::nullopt;
}

void RunGameScene::createHud() {
    m_titleText = addEntity<vde::TextEntity>();
    m_titleText->setText("RUN GAME");
    m_titleText->setFont(vde::BitmapFont::large());
    m_titleText->setStyle({.color = vde::Color::fromHex(0x17384b), .pixelScale = 2});
    m_titleText->setAnchor(0.0f, 0.5f);
    m_titleText->setPosition(-11.3f, 5.9f, 0.8f);
    m_titleText->setWorldHeight(0.55f);

    m_scoreText = addEntity<vde::TextEntity>();
    m_scoreText->setFont(vde::BitmapFont::small());
    m_scoreText->setStyle({.color = vde::Color::fromHex(0x17384b), .pixelScale = 2});
    m_scoreText->setAnchor(1.0f, 0.5f);
    m_scoreText->setPosition(11.3f, 5.9f, 0.8f);
    m_scoreText->setWorldHeight(0.38f);

    m_statusText = addEntity<vde::TextEntity>();
    m_statusText->setFont(vde::BitmapFont::small());
    m_statusText->setStyle({.color = vde::Color::fromHex(0x17384b), .pixelScale = 2});
    m_statusText->setPosition(0.0f, 5.15f, 0.8f);
    m_statusText->setWorldHeight(0.36f);
    m_statusText->setMaxWidth(20.0f);
}

void RunGameScene::resetGame() {
    m_elapsed = 0.0f;
    m_distance = 0.0f;
    m_displayedScore = -1;
    m_gameOver = false;
    if (m_background) {
        m_background->reset();
    }
    if (m_runner) {
        m_runner->reset();
    }
    if (m_obstacles) {
        m_obstacles->reset();
    }
    if (m_statusText) {
        m_statusText->setText("SPACE / UP / W to jump   -   R to restart");
    }
    updateHud();
}

void RunGameScene::updateHud() {
    const int score = static_cast<int>(std::floor(m_distance));
    if (score != m_displayedScore && m_scoreText) {
        m_displayedScore = score;
        m_scoreText->setText("DISTANCE  " + std::to_string(score));
    }
}

}  // namespace rungame
