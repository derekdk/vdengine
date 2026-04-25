#pragma once

#include <memory>
#include <string>
#include <vector>

#include "../GameBase.h"

namespace pong {

class PongInput;

class PongScene : public vde::games::BaseGameScene {
  public:
    enum class RoundState {
        WaitingForServe,
        Rally,
        GameOver,
    };

    PongScene();

    void onEnter() override;
    void update(float deltaTime) override;

  protected:
    std::string getGameName() const override;
    std::vector<std::string> getGameplaySummary() const override;
    std::vector<std::string> getGoals() const override;
    std::vector<std::string> getControls() const override;

  private:
    void createArena();
    void createHud();
    void resetMatch();
    void resetRound(float serveDirection);
    void startRound();
    void updatePlayerPaddle(float deltaTime);
    void updateCpuPaddle(float deltaTime);
    void updateBall(float deltaTime);
    void updateHud();
    void setBallVelocity(float directionX, float directionY);
    PongInput* input();

    static bool overlapsAabb(float ax, float ay, float aw, float ah, float bx, float by, float bw,
                             float bh);

    static constexpr float kViewWidth = 14.0f;
    static constexpr float kViewHeight = 10.0f;
    static constexpr float kArenaHalfWidth = 6.7f;
    static constexpr float kArenaHalfHeight = 4.15f;
    static constexpr float kPlayerPaddleX = -6.1f;
    static constexpr float kCpuPaddleX = 6.1f;
    static constexpr float kPaddleWidth = 0.32f;
    static constexpr float kPaddleHeight = 1.65f;
    static constexpr float kBallSize = 0.28f;
    static constexpr float kPlayerSpeed = 7.0f;
    static constexpr float kCpuSpeed = 5.6f;
    static constexpr float kBallSpeed = 6.4f;
    static constexpr int kWinningScore = 5;

    std::shared_ptr<vde::SpriteEntity> m_playerPaddle;
    std::shared_ptr<vde::SpriteEntity> m_cpuPaddle;
    std::shared_ptr<vde::SpriteEntity> m_ball;

    std::shared_ptr<vde::TextEntity> m_titleText;
    std::shared_ptr<vde::TextEntity> m_scoreText;
    std::shared_ptr<vde::TextEntity> m_statusText;
    std::shared_ptr<vde::TextEntity> m_controlText;

    RoundState m_roundState = RoundState::WaitingForServe;
    int m_playerScore = 0;
    int m_cpuScore = 0;
    float m_playerY = 0.0f;
    float m_cpuY = 0.0f;
    float m_ballX = 0.0f;
    float m_ballY = 0.0f;
    float m_ballVX = 0.0f;
    float m_ballVY = 0.0f;
    float m_serveDirection = 1.0f;
    PongInput* m_input = nullptr;
    std::string m_status = "Press SPACE to serve.";
};

}  // namespace pong
