#include "PongScene.h"

#include <algorithm>
#include <cmath>

#include "Input.h"

namespace pong {

using namespace vde;

PongScene::PongScene() = default;

void PongScene::onEnter() {
    printGameHeader();

    setup2D(kViewWidth, kViewHeight, Color::fromHex(0x08131f));
    createArena();
    createHud();
    resetMatch();
}

void PongScene::update(float deltaTime) {
    BaseGameScene::update(deltaTime);

    auto* input = dynamic_cast<PongInput*>(getInputHandler());
    if (!input) {
        return;
    }

    if (input->keys.consume("restart")) {
        resetMatch();
        return;
    }

    if (m_roundState == RoundState::GameOver) {
        if (input->keys.consume("serve")) {
            resetMatch();
            return;
        }
    } else if (m_roundState == RoundState::WaitingForServe && input->keys.consume("serve")) {
        startRound();
    }

    updatePlayerPaddle(deltaTime);
    updateCpuPaddle(deltaTime);
    updateBall(deltaTime);
    updateHud();
}

std::string PongScene::getGameName() const {
    return "Pong";
}

std::vector<std::string> PongScene::getGameplaySummary() const {
    return {
        "Classic one-player Pong with a CPU paddle defending the far side.",
        "Built as a full VDE game using 2D sprites, text HUD, and scripted smoke coverage.",
    };
}

std::vector<std::string> PongScene::getGoals() const {
    return {
        "Score by sending the ball past the CPU paddle.",
        "Reach five points before the CPU does.",
    };
}

std::vector<std::string> PongScene::getControls() const {
    return {
        "W / UP    - Move paddle up",
        "S / DOWN  - Move paddle down",
        "SPACE     - Serve the ball or start a new match after game over",
        "R         - Restart the match",
    };
}

void PongScene::createArena() {
    auto backdrop = addEntity<SpriteEntity>();
    backdrop->setScale(kViewWidth, kViewHeight, 1.0f);
    backdrop->setColor(Color::fromHex(0x08131f));

    auto court = addEntity<SpriteEntity>();
    court->setScale(kArenaHalfWidth * 2.0f, kArenaHalfHeight * 2.0f, 1.0f);
    court->setColor(Color::fromHex(0x0d1d2c));

    auto topRail = addEntity<SpriteEntity>();
    topRail->setPosition(0.0f, kArenaHalfHeight + 0.17f, 0.0f);
    topRail->setScale(kArenaHalfWidth * 2.0f, 0.12f, 1.0f);
    topRail->setColor(Color::fromHex(0x8ee3ef));

    auto bottomRail = addEntity<SpriteEntity>();
    bottomRail->setPosition(0.0f, -kArenaHalfHeight - 0.17f, 0.0f);
    bottomRail->setScale(kArenaHalfWidth * 2.0f, 0.12f, 1.0f);
    bottomRail->setColor(Color::fromHex(0x8ee3ef));

    for (int index = 0; index < 9; ++index) {
        auto mark = addEntity<SpriteEntity>();
        mark->setPosition(0.0f, 3.2f - static_cast<float>(index) * 0.8f, 0.0f);
        mark->setScale(0.12f, 0.45f, 1.0f);
        mark->setColor(Color(0.76f, 0.87f, 0.95f, 0.55f));
    }

    m_playerPaddle = addEntity<SpriteEntity>();
    m_playerPaddle->setScale(kPaddleWidth, kPaddleHeight, 1.0f);
    m_playerPaddle->setColor(Color::fromHex(0x7ef9c6));

    m_cpuPaddle = addEntity<SpriteEntity>();
    m_cpuPaddle->setScale(kPaddleWidth, kPaddleHeight, 1.0f);
    m_cpuPaddle->setColor(Color::fromHex(0xff9f68));

    m_ball = addEntity<SpriteEntity>();
    m_ball->setScale(kBallSize, kBallSize, 1.0f);
    m_ball->setColor(Color::white());
}

void PongScene::createHud() {
    m_titleText = addEntity<TextEntity>();
    m_titleText->setText("PONG");
    m_titleText->setFont(BitmapFont::large());
    m_titleText->setStyle({.color = Color(0.83f, 0.93f, 0.98f, 1.0f), .pixelScale = 2});
    m_titleText->setAnchor(0.0f, 0.5f);
    m_titleText->setPosition(-6.5f, 4.55f, 0.0f);
    m_titleText->setWorldHeight(0.55f);

    m_scoreText = addEntity<TextEntity>();
    m_scoreText->setFont(BitmapFont::large());
    m_scoreText->setStyle({.color = Color::white(), .pixelScale = 2});
    m_scoreText->setPosition(0.0f, 4.3f, 0.0f);
    m_scoreText->setWorldHeight(0.5f);

    m_statusText = addEntity<TextEntity>();
    m_statusText->setFont(BitmapFont::small());
    m_statusText->setStyle({.color = Color(0.93f, 0.97f, 0.99f, 1.0f), .pixelScale = 1});
    m_statusText->setPosition(0.0f, -4.55f, 0.0f);
    m_statusText->setWorldHeight(0.26f);
    m_statusText->setMaxWidth(12.4f);

    m_controlText = addEntity<TextEntity>();
    m_controlText->setText("W/S or arrows move   SPACE serve   R reset");
    m_controlText->setFont(BitmapFont::small());
    m_controlText->setStyle({.color = Color(0.67f, 0.79f, 0.90f, 1.0f), .pixelScale = 1});
    m_controlText->setAnchor(0.0f, 0.5f);
    m_controlText->setPosition(-6.5f, -4.92f, 0.0f);
    m_controlText->setWorldHeight(0.22f);
}

void PongScene::resetMatch() {
    m_playerScore = 0;
    m_cpuScore = 0;
    m_status = "Press SPACE to serve. First to five wins.";
    resetRound(1.0f);
}

void PongScene::resetRound(float serveDirection) {
    m_roundState = RoundState::WaitingForServe;
    m_serveDirection = serveDirection >= 0.0f ? 1.0f : -1.0f;
    m_playerY = 0.0f;
    m_cpuY = 0.0f;
    m_ballX = 0.0f;
    m_ballY = 0.0f;
    m_ballVX = 0.0f;
    m_ballVY = 0.0f;

    if (m_playerPaddle) {
        m_playerPaddle->setPosition(kPlayerPaddleX, m_playerY, 0.0f);
    }
    if (m_cpuPaddle) {
        m_cpuPaddle->setPosition(kCpuPaddleX, m_cpuY, 0.0f);
    }
    if (m_ball) {
        m_ball->setPosition(m_ballX, m_ballY, 0.0f);
    }

    updateHud();
}

void PongScene::startRound() {
    m_roundState = RoundState::Rally;
    m_status = "Rally on. Beat the CPU to five points.";
    float openingAngle = ((m_playerScore + m_cpuScore) % 2 == 0) ? 0.42f : -0.42f;
    setBallVelocity(m_serveDirection, openingAngle);
}

void PongScene::updatePlayerPaddle(float deltaTime) {
    auto* controls = input();
    if (!controls || !m_playerPaddle) {
        return;
    }

    float moveDir = 0.0f;
    if (controls->keys.isHeld("up")) {
        moveDir += 1.0f;
    }
    if (controls->keys.isHeld("down")) {
        moveDir -= 1.0f;
    }

    float limit = kArenaHalfHeight - (kPaddleHeight * 0.5f);
    m_playerY = std::clamp(m_playerY + moveDir * kPlayerSpeed * deltaTime, -limit, limit);
    m_playerPaddle->setPosition(kPlayerPaddleX, m_playerY, 0.0f);
}

void PongScene::updateCpuPaddle(float deltaTime) {
    if (!m_cpuPaddle) {
        return;
    }

    float targetY = 0.0f;
    if (m_roundState == RoundState::Rally) {
        targetY = (m_ballVX > 0.0f || m_ballX > 0.0f) ? m_ballY * 0.92f : 0.0f;
    }

    float maxStep = kCpuSpeed * deltaTime;
    float delta = std::clamp(targetY - m_cpuY, -maxStep, maxStep);
    float limit = kArenaHalfHeight - (kPaddleHeight * 0.5f);
    m_cpuY = std::clamp(m_cpuY + delta, -limit, limit);
    m_cpuPaddle->setPosition(kCpuPaddleX, m_cpuY, 0.0f);
}

void PongScene::updateBall(float deltaTime) {
    if (!m_ball || m_roundState != RoundState::Rally) {
        return;
    }

    m_ballX += m_ballVX * deltaTime;
    m_ballY += m_ballVY * deltaTime;

    float ballHalf = kBallSize * 0.5f;
    if (m_ballY + ballHalf >= kArenaHalfHeight) {
        m_ballY = kArenaHalfHeight - ballHalf;
        m_ballVY = -std::abs(m_ballVY);
    } else if (m_ballY - ballHalf <= -kArenaHalfHeight) {
        m_ballY = -kArenaHalfHeight + ballHalf;
        m_ballVY = std::abs(m_ballVY);
    }

    if (m_ballVX < 0.0f && overlapsAabb(m_ballX, m_ballY, kBallSize, kBallSize, kPlayerPaddleX,
                                        m_playerY, kPaddleWidth, kPaddleHeight)) {
        m_ballX = kPlayerPaddleX + (kPaddleWidth * 0.5f) + ballHalf + 0.01f;
        float hitOffset = (m_ballY - m_playerY) / (kPaddleHeight * 0.5f);
        setBallVelocity(1.0f, std::clamp(hitOffset, -0.95f, 0.95f));
    } else if (m_ballVX > 0.0f && overlapsAabb(m_ballX, m_ballY, kBallSize, kBallSize, kCpuPaddleX,
                                               m_cpuY, kPaddleWidth, kPaddleHeight)) {
        m_ballX = kCpuPaddleX - (kPaddleWidth * 0.5f) - ballHalf - 0.01f;
        float hitOffset = (m_ballY - m_cpuY) / (kPaddleHeight * 0.5f);
        setBallVelocity(-1.0f, std::clamp(hitOffset, -0.95f, 0.95f));
    }

    if (m_ballX < -kArenaHalfWidth - ballHalf) {
        ++m_cpuScore;
        if (m_cpuScore >= kWinningScore) {
            m_roundState = RoundState::GameOver;
            m_ballVX = 0.0f;
            m_ballVY = 0.0f;
            m_status = "CPU wins the match. Press SPACE for a rematch.";
        } else {
            m_status = "CPU scores. Press SPACE for the next serve.";
            resetRound(-1.0f);
        }
    } else if (m_ballX > kArenaHalfWidth + ballHalf) {
        ++m_playerScore;
        if (m_playerScore >= kWinningScore) {
            m_roundState = RoundState::GameOver;
            m_ballVX = 0.0f;
            m_ballVY = 0.0f;
            m_status = "You win the match. Press SPACE to play again.";
        } else {
            m_status = "Point scored. Press SPACE for the next serve.";
            resetRound(1.0f);
        }
    }

    m_ball->setPosition(m_ballX, m_ballY, 0.0f);
}

void PongScene::updateHud() {
    if (m_scoreText) {
        m_scoreText->setText("YOU " + std::to_string(m_playerScore) + "   CPU " +
                             std::to_string(m_cpuScore));
    }

    if (!m_statusText) {
        return;
    }

    if (m_roundState == RoundState::WaitingForServe && m_playerScore == 0 && m_cpuScore == 0) {
        m_statusText->setText("Press SPACE to serve. First to five wins.");
        return;
    }

    if (m_roundState == RoundState::WaitingForServe && m_playerScore < kWinningScore &&
        m_cpuScore < kWinningScore) {
        m_statusText->setText(m_status + "  Score from the open side to earn a point.");
        return;
    }

    m_statusText->setText(m_status);
}

void PongScene::setBallVelocity(float directionX, float directionY) {
    float length = std::sqrt(directionX * directionX + directionY * directionY);
    if (length <= 0.0001f) {
        directionX = m_serveDirection;
        directionY = 0.0f;
        length = 1.0f;
    }

    m_ballVX = (directionX / length) * kBallSpeed;
    m_ballVY = (directionY / length) * kBallSpeed;
}

PongInput* PongScene::input() {
    return dynamic_cast<PongInput*>(getInputHandler());
}

bool PongScene::overlapsAabb(float ax, float ay, float aw, float ah, float bx, float by, float bw,
                             float bh) {
    float aHalfW = aw * 0.5f;
    float aHalfH = ah * 0.5f;
    float bHalfW = bw * 0.5f;
    float bHalfH = bh * 0.5f;

    return !(ax + aHalfW < bx - bHalfW || ax - aHalfW > bx + bHalfW || ay + aHalfH < by - bHalfH ||
             ay - aHalfH > by + bHalfH);
}

}  // namespace pong
