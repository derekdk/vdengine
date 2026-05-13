#include "PacManScene.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>

#include "Input.h"

namespace pacman {

using namespace vde;

// ---------------------------------------------------------------------------
//  Maze template — 21 columns x 15 rows.
//  '#' = wall, '.' = pellet, ' ' = open corridor (no pellet).
// ---------------------------------------------------------------------------
static const char kMazeTemplate[PacManScene::kRows][PacManScene::kCols + 1] = {
    "#####################",   // row  0
    "#.........#.........#",   // row  1
    "#.##.####.#.####.##.#",   // row  2
    "#...................#",   // row  3
    "###.##.#.###.#.##.###",   // row  4
    "#...##.#.....#.##...#",   // row  5
    "#.###..#.###.#..###.#",   // row  6
    "#......#     #......#",   // row  7  (ghost-house opening, spaces = open)
    "#.###..#.###.#..###.#",   // row  8
    "#...##.#.....#.##...#",   // row  9
    "###.##.#.###.#.##.###",   // row 10
    "#...................#",   // row 11
    "#.##.####.#.####.##.#",   // row 12
    "#.........#.........#",   // row 13
    "#####################",   // row 14
};

// Starting tile positions for Pac-Man and the four ghosts
static constexpr int kPacStartCol = 10, kPacStartRow = 11;
static constexpr int kGhostStartCols[4] = {  3, 10, 17, 10 };
static constexpr int kGhostStartRows[4] = {  7,  5,  7,  9 };
static const vde::Color kGhostColors[4] = {
    Color(1.0f, 0.4f, 0.4f),   // red
    Color(1.0f, 0.7f, 0.9f),   // pink
    Color(0.4f, 0.9f, 1.0f),   // cyan
    Color(1.0f, 0.65f, 0.1f),  // orange
};

// ---------------------------------------------------------------------------
//  Helpers
// ---------------------------------------------------------------------------

float PacManScene::tileToWorldX(int col) {
    return static_cast<float>(col) - kHalfCols + 0.5f;
}

float PacManScene::tileToWorldY(int row) {
    // row 0 is at the top; world Y increases upward.
    // +1.0 offsets the maze downward to leave room for the HUD strip.
    return kHalfRows - static_cast<float>(row) - 0.5f + 1.0f;
}

bool PacManScene::isWall(int col, int row) const {
    if (col < 0 || col >= kCols || row < 0 || row >= kRows) return true;
    return m_maze[row][col] == '#';
}

bool PacManScene::canMove(int col, int row, int dx, int dy) const {
    return !isWall(col + dx, row + dy);
}

// ---------------------------------------------------------------------------
//  Constructor / metadata
// ---------------------------------------------------------------------------

PacManScene::PacManScene() = default;

std::string PacManScene::getGameName() const { return "Pac-Man"; }

std::vector<std::string> PacManScene::getGameplaySummary() const {
    return {
        "Classic Pac-Man clone. Eat all the pellets while avoiding the four ghosts.",
        "Built with VDE SpriteEntity, TextEntity, and grid-based movement.",
    };
}

std::vector<std::string> PacManScene::getGoals() const {
    return {
        "Eat every pellet to advance.",
        "Avoid the four coloured ghosts.",
        "You have three lives.",
    };
}

std::vector<std::string> PacManScene::getControls() const {
    return {
        "Arrow keys / WASD  — steer Pac-Man",
        "SPACE              — restart after game over",
        "ESC                — quit",
    };
}

// ---------------------------------------------------------------------------
//  onEnter
// ---------------------------------------------------------------------------

void PacManScene::onEnter() {
    printGameHeader();
    m_input = dynamic_cast<PacManInput*>(getInputHandler());
    setup2D(kViewWidth, kViewHeight, Color(0.0f, 0.0f, 0.0f));
    initMaze();
    createWalls();
    createPellets();
    createActors();
    createHud();
}

// ---------------------------------------------------------------------------
//  initMaze — copy template into mutable array and count pellets
// ---------------------------------------------------------------------------

void PacManScene::initMaze() {
    std::memcpy(m_maze, kMazeTemplate, sizeof(m_maze));
    m_pelletsLeft  = 0;
    m_totalPellets = 0;
    for (int r = 0; r < kRows; ++r) {
        for (int c = 0; c < kCols; ++c) {
            m_hasPellet[r][c]    = (m_maze[r][c] == '.');
            m_pelletSprite[r][c] = nullptr;
            if (m_hasPellet[r][c]) {
                ++m_pelletsLeft;
                ++m_totalPellets;
            }
        }
    }
}

// ---------------------------------------------------------------------------
//  Build visual tiles
// ---------------------------------------------------------------------------

void PacManScene::createWalls() {
    for (int r = 0; r < kRows; ++r) {
        for (int c = 0; c < kCols; ++c) {
            if (m_maze[r][c] == '#') {
                auto wall = addEntity<SpriteEntity>();
                wall->setPosition(tileToWorldX(c), tileToWorldY(r), 0.0f);
                wall->setScale(1.0f, 1.0f, 1.0f);
                wall->setColor(Color(0.1f, 0.2f, 0.8f));
            }
        }
    }
}

void PacManScene::createPellets() {
    for (int r = 0; r < kRows; ++r) {
        for (int c = 0; c < kCols; ++c) {
            if (m_hasPellet[r][c]) {
                auto dot = addEntity<SpriteEntity>();
                dot->setPosition(tileToWorldX(c), tileToWorldY(r), 0.0f);
                dot->setScale(0.22f, 0.22f, 1.0f);
                dot->setColor(Color(1.0f, 0.9f, 0.7f));
                m_pelletSprite[r][c] = dot;
            }
        }
    }
}

void PacManScene::createActors() {
    m_pacTileX    = kPacStartCol;
    m_pacTileY    = kPacStartRow;
    m_pacProgress = 0.0f;
    m_dirX        = 1; m_dirY = 0;
    m_nextDirX    = 1; m_nextDirY = 0;
    m_pacWorldX   = tileToWorldX(m_pacTileX);
    m_pacWorldY   = tileToWorldY(m_pacTileY);

    m_pacMan = addEntity<SpriteEntity>();
    m_pacMan->setPosition(m_pacWorldX, m_pacWorldY, 0.0f);
    m_pacMan->setScale(0.85f, 0.85f, 1.0f);
    m_pacMan->setColor(Color(1.0f, 0.9f, 0.0f));

    for (int i = 0; i < kNumGhosts; ++i) {
        Ghost& g   = m_ghosts[i];
        g.tileX    = kGhostStartCols[i];
        g.tileY    = kGhostStartRows[i];
        g.progress = 0.0f;
        g.color    = kGhostColors[i];
        g.worldX   = tileToWorldX(g.tileX);
        g.worldY   = tileToWorldY(g.tileY);

        static constexpr int initDX[4] = { 1, -1,  0,  0 };
        static constexpr int initDY[4] = { 0,  0,  1, -1 };
        g.dirX = initDX[i];
        g.dirY = initDY[i];

        g.entity = addEntity<SpriteEntity>();
        g.entity->setPosition(g.worldX, g.worldY, 0.0f);
        g.entity->setScale(0.85f, 0.85f, 1.0f);
        g.entity->setColor(g.color);
    }
}

void PacManScene::createHud() {
    m_scoreText = addEntity<TextEntity>();
    m_scoreText->setFont(BitmapFont::large());
    m_scoreText->setStyle({.color = Color::white(), .pixelScale = 2});
    m_scoreText->setAnchor(0.0f, 0.5f);
    m_scoreText->setPosition(-10.0f, 7.5f, 0.0f);
    m_scoreText->setWorldHeight(0.55f);

    m_livesText = addEntity<TextEntity>();
    m_livesText->setFont(BitmapFont::large());
    m_livesText->setStyle({.color = Color(1.0f, 0.9f, 0.0f), .pixelScale = 2});
    m_livesText->setAnchor(1.0f, 0.5f);
    m_livesText->setPosition(10.0f, 7.5f, 0.0f);
    m_livesText->setWorldHeight(0.55f);

    m_statusText = addEntity<TextEntity>();
    m_statusText->setFont(BitmapFont::small());
    m_statusText->setStyle({.color = Color(0.8f, 0.8f, 0.8f), .pixelScale = 1});
    m_statusText->setAnchor(0.5f, 0.5f);
    m_statusText->setPosition(0.0f, -7.5f, 0.0f);
    m_statusText->setWorldHeight(0.3f);

    updateHud();
}

// ---------------------------------------------------------------------------
//  resetRound — reposition actors, keep pellet and score state intact
// ---------------------------------------------------------------------------

void PacManScene::resetRound() {
    m_pacTileX    = kPacStartCol;
    m_pacTileY    = kPacStartRow;
    m_pacProgress = 0.0f;
    m_dirX        = 1; m_dirY = 0;
    m_nextDirX    = 1; m_nextDirY = 0;
    m_pacWorldX   = tileToWorldX(m_pacTileX);
    m_pacWorldY   = tileToWorldY(m_pacTileY);
    if (m_pacMan) {
        m_pacMan->setPosition(m_pacWorldX, m_pacWorldY, 0.0f);
        m_pacMan->setVisible(true);
    }

    for (int i = 0; i < kNumGhosts; ++i) {
        Ghost& g   = m_ghosts[i];
        g.tileX    = kGhostStartCols[i];
        g.tileY    = kGhostStartRows[i];
        g.progress = 0.0f;
        g.worldX   = tileToWorldX(g.tileX);
        g.worldY   = tileToWorldY(g.tileY);
        static constexpr int initDX[4] = { 1, -1,  0,  0 };
        static constexpr int initDY[4] = { 0,  0,  1, -1 };
        g.dirX = initDX[i];
        g.dirY = initDY[i];
        if (g.entity) g.entity->setPosition(g.worldX, g.worldY, 0.0f);
    }

    m_state = GameState::Playing;
    updateHud();
}

// ---------------------------------------------------------------------------
//  fullReset — restore all pellets, reset score and lives, reposition actors
// ---------------------------------------------------------------------------

void PacManScene::fullReset() {
    for (int r = 0; r < kRows; ++r) {
        for (int c = 0; c < kCols; ++c) {
            if (kMazeTemplate[r][c] == '.') {
                m_hasPellet[r][c] = true;
                if (m_pelletSprite[r][c]) m_pelletSprite[r][c]->setVisible(true);
            }
        }
    }
    m_pelletsLeft = m_totalPellets;
    m_score       = 0;
    m_lives       = kStartLives;
    resetRound();
}

// ---------------------------------------------------------------------------
//  update
// ---------------------------------------------------------------------------

void PacManScene::update(float deltaTime) {
    BaseGameScene::update(deltaTime);
    if (!m_input) return;

    if (m_state == GameState::GameOver || m_state == GameState::Win) {
        if (m_input->keys.consume("restart")) {
            fullReset();
        }
        return;
    }

    if (m_state == GameState::Dead) {
        m_deadTimer -= deltaTime;
        if (m_deadTimer <= 0.0f) {
            if (m_lives <= 0) {
                m_state = GameState::GameOver;
                updateHud();
            } else {
                resetRound();
            }
        }
        return;
    }

    // Queue direction from held keys
    if (m_input->keys.isHeld("left"))  { m_nextDirX = -1; m_nextDirY =  0; }
    if (m_input->keys.isHeld("right")) { m_nextDirX =  1; m_nextDirY =  0; }
    if (m_input->keys.isHeld("up"))    { m_nextDirX =  0; m_nextDirY =  1; }
    if (m_input->keys.isHeld("down"))  { m_nextDirX =  0; m_nextDirY = -1; }

    updatePacMan(deltaTime);
    updateGhosts(deltaTime);
    checkGhostCollisions();
    updateHud();
}

// ---------------------------------------------------------------------------
//  Pac-Man tile-to-tile movement with smooth interpolation
// ---------------------------------------------------------------------------

void PacManScene::updatePacMan(float deltaTime) {
    m_pacProgress += kPacSpeed * deltaTime;

    while (m_pacProgress >= 1.0f) {
        m_pacProgress -= 1.0f;

        // Arrive at next tile
        m_pacTileX += m_dirX;
        m_pacTileY -= m_dirY;  // world-Y up → tile-row increases downward

        // Eat pellet
        if (m_pacTileY >= 0 && m_pacTileY < kRows &&
            m_pacTileX >= 0 && m_pacTileX < kCols &&
            m_hasPellet[m_pacTileY][m_pacTileX]) {
            m_hasPellet[m_pacTileY][m_pacTileX] = false;
            if (m_pelletSprite[m_pacTileY][m_pacTileX]) {
                m_pelletSprite[m_pacTileY][m_pacTileX]->setVisible(false);
            }
            m_score += kPelletScore;
            --m_pelletsLeft;
            if (m_pelletsLeft == 0) {
                m_state = GameState::Win;
                updateHud();
                return;
            }
        }

        // Try queued direction, then current direction
        if (canMove(m_pacTileX, m_pacTileY, m_nextDirX, -m_nextDirY)) {
            m_dirX = m_nextDirX;
            m_dirY = m_nextDirY;
        } else if (!canMove(m_pacTileX, m_pacTileY, m_dirX, -m_dirY)) {
            m_pacProgress = 0.0f;
            break;
        }
    }

    // Smooth position lerp
    float fromX = tileToWorldX(m_pacTileX);
    float fromY = tileToWorldY(m_pacTileY);
    float toX   = tileToWorldX(m_pacTileX + m_dirX);
    float toY   = tileToWorldY(m_pacTileY - m_dirY);
    float t     = std::min(m_pacProgress, 1.0f);

    m_pacWorldX = fromX + (toX - fromX) * t;
    m_pacWorldY = fromY + (toY - fromY) * t;
    if (m_pacMan) m_pacMan->setPosition(m_pacWorldX, m_pacWorldY, 0.0f);
}

// ---------------------------------------------------------------------------
//  Ghost movement
// ---------------------------------------------------------------------------

void PacManScene::updateGhosts(float deltaTime) {
    for (int i = 0; i < kNumGhosts; ++i) {
        Ghost& g   = m_ghosts[i];
        g.progress += kGhostSpeed * deltaTime;

        while (g.progress >= 1.0f) {
            g.progress -= 1.0f;
            g.tileX    += g.dirX;
            g.tileY    -= g.dirY;
            chooseGhostDirection(g);
        }

        float fromX = tileToWorldX(g.tileX);
        float fromY = tileToWorldY(g.tileY);
        float toX   = tileToWorldX(g.tileX + g.dirX);
        float toY   = tileToWorldY(g.tileY - g.dirY);
        float t     = std::min(g.progress, 1.0f);

        g.worldX = fromX + (toX - fromX) * t;
        g.worldY = fromY + (toY - fromY) * t;
        if (g.entity) g.entity->setPosition(g.worldX, g.worldY, 0.0f);
    }
}

// ---------------------------------------------------------------------------
//  Ghost AI: chase 60%, random 40%
// ---------------------------------------------------------------------------

void PacManScene::chooseGhostDirection(Ghost& g) {
    // Build list of valid directions (exclude direct reversal unless forced)
    static const int kCandDX[4] = { 1, -1,  0,  0 };
    static const int kCandDY[4] = { 0,  0,  1, -1 };

    int validDX[4], validDY[4];
    int nValid = 0;
    for (int k = 0; k < 4; ++k) {
        int nx = kCandDX[k], ny = kCandDY[k];
        if (nx == -g.dirX && ny == -g.dirY) continue;  // skip reverse
        if (canMove(g.tileX, g.tileY, nx, -ny)) {
            validDX[nValid] = nx;
            validDY[nValid] = ny;
            ++nValid;
        }
    }

    if (nValid == 0) {
        // Dead end: reverse
        g.dirX = -g.dirX;
        g.dirY = -g.dirY;
        return;
    }

    // Chase: find direction minimising Manhattan distance to Pac-Man
    int bestIdx  = 0;
    int bestDist = 99999;
    for (int k = 0; k < nValid; ++k) {
        int nx   = g.tileX + validDX[k];
        int ny   = g.tileY - validDY[k];
        int dist = std::abs(nx - m_pacTileX) + std::abs(ny - m_pacTileY);
        if (dist < bestDist) { bestDist = dist; bestIdx = k; }
    }

    // LCG pseudo-random for 60/40 chase/wander split
    static unsigned int seed = 12345u;
    seed = seed * 1664525u + 1013904223u;
    bool chase  = (seed & 0xFF) < 153;
    int chosen  = chase ? bestIdx : (static_cast<int>(seed >> 8) % nValid);

    g.dirX = validDX[chosen];
    g.dirY = validDY[chosen];
}

// ---------------------------------------------------------------------------
//  Collision: Pac-Man vs ghosts (distance-based)
// ---------------------------------------------------------------------------

void PacManScene::checkGhostCollisions() {
    for (int i = 0; i < kNumGhosts; ++i) {
        float dx    = m_pacWorldX - m_ghosts[i].worldX;
        float dy    = m_pacWorldY - m_ghosts[i].worldY;
        float dist2 = dx * dx + dy * dy;
        if (dist2 < 0.5f * 0.5f) {
            handleDeath();
            return;
        }
    }
}

void PacManScene::handleDeath() {
    --m_lives;
    m_state     = GameState::Dead;
    m_deadTimer = kDeadDelay;
    if (m_pacMan) m_pacMan->setVisible(false);
    updateHud();
}

// ---------------------------------------------------------------------------
//  HUD update
// ---------------------------------------------------------------------------

void PacManScene::updateHud() {
    if (m_scoreText) m_scoreText->setText("SCORE " + std::to_string(m_score));
    if (m_livesText) m_livesText->setText("LIVES " + std::to_string(std::max(m_lives, 0)));

    if (!m_statusText) return;
    switch (m_state) {
        case GameState::Playing:
            m_statusText->setText("Eat all pellets. Avoid the ghosts.");
            break;
        case GameState::Dead:
            m_statusText->setText(m_lives > 0 ? "Caught!  Get ready..."
                                              : "Game Over!  Press SPACE to restart.");
            break;
        case GameState::GameOver:
            m_statusText->setText("GAME OVER  |  Score: " + std::to_string(m_score) +
                                   "  |  SPACE to restart");
            break;
        case GameState::Win:
            m_statusText->setText("YOU WIN!  Score: " + std::to_string(m_score) +
                                   "  |  SPACE to play again");
            break;
    }
}

}  // namespace pacman
