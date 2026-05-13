#pragma once

#include <array>
#include <memory>
#include <string>
#include <vector>

#include "../GameBase.h"

namespace pacman {

class PacManInput;

// One ghost actor: position, direction, and sprite.
struct Ghost {
    float worldX = 0.0f;
    float worldY = 0.0f;
    int   tileX  = 0;
    int   tileY  = 0;
    int   dirX   = 1;
    int   dirY   = 0;
    float progress = 0.0f;  // 0..1 interpolation between current and next tile
    std::shared_ptr<vde::SpriteEntity> entity;
    vde::Color color;
};

class PacManScene : public vde::games::BaseGameScene {
  public:
    enum class GameState { Playing, Dead, GameOver, Win };

    // Public so PacManScene.cpp can use them in static array declarations
    static constexpr int   kCols       = 21;
    static constexpr int   kRows       = 15;
    static constexpr float kViewWidth  = 21.0f;
    static constexpr float kViewHeight = 17.0f;
    static constexpr float kHalfCols   = kCols  * 0.5f;
    static constexpr float kHalfRows   = kRows  * 0.5f;

    PacManScene();

    void onEnter() override;
    void update(float deltaTime) override;

  protected:
    std::string              getGameName()         const override;
    std::vector<std::string> getGameplaySummary()  const override;
    std::vector<std::string> getGoals()            const override;
    std::vector<std::string> getControls()         const override;

  private:
    // ----- Gameplay constants -----
    static constexpr float kPacSpeed    = 5.5f;
    static constexpr float kGhostSpeed  = 3.5f;
    static constexpr int   kNumGhosts   = 4;
    static constexpr int   kPelletScore = 10;
    static constexpr int   kStartLives  = 3;
    static constexpr float kDeadDelay   = 1.5f;

    // ----- Coordinate helpers -----
    static float tileToWorldX(int col);
    static float tileToWorldY(int row);

    // ----- Maze helpers -----
    bool isWall(int col, int row) const;
    bool canMove(int col, int row, int dx, int dy) const;

    // ----- Setup -----
    void initMaze();
    void createWalls();
    void createPellets();
    void createActors();
    void createHud();
    void resetRound();
    void fullReset();

    // ----- Update -----
    void updatePacMan(float dt);
    void updateGhosts(float dt);
    void checkGhostCollisions();
    void updateHud();
    void handleDeath();

    // ----- Ghost AI -----
    void chooseGhostDirection(Ghost& g);

    // ----- Maze data -----
    char m_maze[kRows][kCols + 1];
    bool m_hasPellet[kRows][kCols];
    std::shared_ptr<vde::SpriteEntity> m_pelletSprite[kRows][kCols];
    int  m_pelletsLeft  = 0;
    int  m_totalPellets = 0;

    // ----- Pac-Man state -----
    float m_pacWorldX   = 0.0f;
    float m_pacWorldY   = 0.0f;
    int   m_pacTileX    = 0;
    int   m_pacTileY    = 0;
    int   m_dirX        = 1;
    int   m_dirY        = 0;
    int   m_nextDirX    = 1;
    int   m_nextDirY    = 0;
    float m_pacProgress = 0.0f;
    std::shared_ptr<vde::SpriteEntity> m_pacMan;

    // ----- Ghosts -----
    Ghost m_ghosts[kNumGhosts];

    // ----- HUD -----
    std::shared_ptr<vde::TextEntity> m_scoreText;
    std::shared_ptr<vde::TextEntity> m_livesText;
    std::shared_ptr<vde::TextEntity> m_statusText;

    // ----- Game state -----
    GameState m_state    = GameState::Playing;
    int       m_score    = 0;
    int       m_lives    = kStartLives;
    float     m_deadTimer = 0.0f;

    PacManInput* m_input = nullptr;
};

}  // namespace pacman
