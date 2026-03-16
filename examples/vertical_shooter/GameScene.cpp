/**
 * @file GameScene.cpp
 * @brief Main game scene — ties together scrolling, player, enemies, projectiles, and audio.
 */

#include "GameScene.h"

#include <vde/api/AudioClip.h>
#include <vde/api/AudioManager.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>

using namespace vde;

namespace shooter {

// ============================================================================
// Construction
// ============================================================================

GameScene::GameScene() : BaseExampleScene(130.0f) {}

// ============================================================================
// Lifecycle
// ============================================================================

void GameScene::onEnter() {
    printExampleHeader();

    // Camera
    auto* cam = new Camera2D(VIEW_WIDTH, VIEW_HEIGHT);
    cam->setPosition(0.0f, VIEW_HEIGHT * 0.5f);
    cam->setZoom(1.0f);
    setCamera(cam);
    setBackgroundColor(Color(0.02f, 0.02f, 0.08f, 1.0f));

    // White ambient for 2D
    setLightBox(std::make_unique<SimpleColorLightBox>(Color::white()));

    // Generate procedural textures
    auto* ctx = getGame()->getVulkanContext();
    m_playerTex = createPlayerTexture(ctx);
    m_bulletTex[0] = createBulletTexture(ctx, WeaponType::Basic);
    m_bulletTex[1] = createBulletTexture(ctx, WeaponType::Spread);
    m_bulletTex[2] = createBulletTexture(ctx, WeaponType::Rapid);
    m_enemyTex[0] = createEnemyTexture(ctx, EnemyType::Turret);
    m_enemyTex[1] = createEnemyTexture(ctx, EnemyType::Drone);
    m_enemyTex[2] = createEnemyTexture(ctx, EnemyType::Chaser);
    m_enemyTex[3] = createEnemyTexture(ctx, EnemyType::Tank);
    m_enemyBulletTex = createEnemyBulletTexture(ctx);
    m_starTex = createStarTexture(ctx);

    // Generate procedural audio
    auto tempDir = (std::filesystem::temp_directory_path() / "vde_vertical_shooter").string();
    m_sounds = generateSoundBank(tempDir);

    // Generate map
    m_map = generateMap(42);

    enterTitle();
}

void GameScene::update(float deltaTime) {
    BaseExampleScene::update(deltaTime);

    switch (m_state) {
    case State::Title:
        updateTitle(deltaTime);
        break;
    case State::Playing:
        updatePlaying(deltaTime);
        break;
    case State::GameOver:
    case State::Victory:
        updateGameOver(deltaTime);
        break;
    }
}

// ============================================================================
// State transitions
// ============================================================================

void GameScene::enterTitle() {
    clearAll();
    m_state = State::Title;

    m_titleBanner = addEntity<TextEntity>();
    m_titleBanner->setText("VERTICAL SHOOTER");
    m_titleBanner->setFont(BitmapFont::small());
    m_titleBanner->setStyle({.color = Color(0.0f, 0.898f, 1.0f), .pixelScale = 3, .letterSpacing = 1});
    m_titleBanner->setAnchor(0.5f, 0.5f);
    m_titleBanner->setPosition(0.0f, VIEW_HEIGHT * 0.5f + 2.0f, 0.0f);
    // Force first update to build the texture so we can read its dimensions
    m_titleBanner->update(0.0f);
    {
        auto tex = m_titleBanner->getTexture();
        if (tex && tex->getWidth() > 1) {
            const float w = 4.5f;
            const float h = w * static_cast<float>(tex->getHeight()) /
                            static_cast<float>(tex->getWidth());
            m_titleBanner->setScale(w, h, 1.0f);
        }
    }

    m_promptSprite = addEntity<TextEntity>();
    m_promptSprite->setText("PRESS ENTER / START");
    m_promptSprite->setFont(BitmapFont::small());
    m_promptSprite->setStyle({.color = Color::white(), .pixelScale = 2, .letterSpacing = 1});
    m_promptSprite->setAnchor(0.5f, 0.5f);
    m_promptSprite->setPosition(0.0f, VIEW_HEIGHT * 0.5f - 1.5f, 0.0f);
    m_promptSprite->update(0.0f);
    {
        auto tex = m_promptSprite->getTexture();
        if (tex && tex->getWidth() > 1) {
            const float w = 4.0f;
            const float h = w * static_cast<float>(tex->getHeight()) /
                            static_cast<float>(tex->getWidth());
            m_promptSprite->setScale(w, h, 1.0f);
        }
    }

    // Camera at starting position
    if (auto* cam = dynamic_cast<Camera2D*>(getCamera()))
        cam->setPosition(0.0f, VIEW_HEIGHT * 0.5f);

    initStars();

    std::cout << "=== VERTICAL SHOOTER ===" << std::endl;
    std::cout << "Press ENTER / START to begin!" << std::endl;
}

void GameScene::enterPlaying() {
    clearAll();
    m_state = State::Playing;
    m_scrollY = 0.0f;
    m_score = 0;
    m_health = PLAYER_MAX_HEALTH;
    m_invulnTimer = 0.0f;
    m_weapon = WeaponType::Basic;
    m_fireCooldown = Cooldown(BASIC_COOLDOWN);
    m_fireCooldown.finish();
    m_nextSpawnIdx = 0;

    // Camera at bottom of map
    if (auto* cam = dynamic_cast<Camera2D*>(getCamera()))
        cam->setPosition(0.0f, VIEW_HEIGHT * 0.5f);

    // Create player
    m_player = addEntity<SpriteEntity>();
    m_player->setScale(0.7f, 0.9f, 1.0f);
    m_player->setPosition(0.0f, 2.0f, 0.0f);
    m_player->setAnchor(0.5f, 0.5f);
    m_player->setTexture(m_playerTex);

    initStars();

    std::cout << "Lives: " << m_health << "  Weapon: Basic  Score: 0" << std::endl;
}

void GameScene::enterGameOver() {
    m_state = State::GameOver;
    if (m_player)
        m_player->setColor(Color::red());
    std::cout << "GAME OVER!  Final Score: " << m_score << std::endl;
    std::cout << "Press R / BACK to restart." << std::endl;
}

void GameScene::enterVictory() {
    m_state = State::Victory;
    std::cout << "VICTORY!  Final Score: " << m_score << std::endl;
    std::cout << "Press R / BACK to restart." << std::endl;
}

// ============================================================================
// Title
// ============================================================================

void GameScene::updateTitle(float dt) {
    int ticks = m_blinkTimer.advance(dt);
    if (ticks > 0) {
        m_blinkOn = !m_blinkOn;
        if (m_promptSprite)
            m_promptSprite->setVisible(m_blinkOn);
    }

    auto* in = input();
    if (in && (in->consumeStart() || in->consumeFire() || in->isFireHeld())) {
        enterPlaying();
    }
}

// ============================================================================
// Playing
// ============================================================================

void GameScene::updatePlaying(float dt) {
    auto* in = input();
    if (!in)
        return;

    // Scroll
    m_scrollY += SCROLL_SPEED * dt;
    float camY = VIEW_HEIGHT * 0.5f + m_scrollY;
    if (auto* cam = dynamic_cast<Camera2D*>(getCamera()))
        cam->setPosition(0.0f, camY);

    // Check for map completion
    if (m_scrollY >= MAP_HEIGHT - VIEW_HEIGHT) {
        enterVictory();
        return;
    }

    // Weapon switching
    if (in->consumeNextWeapon())
        cycleWeapon(1);
    if (in->consumePrevWeapon())
        cycleWeapon(-1);

    // Fire
    m_fireCooldown.advance(dt);
    if ((in->isFireHeld() || in->consumeFire()) && m_fireCooldown.tryConsume()) {
        m_fireCooldown.start();
        fireWeapon();
    }

    // Move player
    movePlayer(dt);

    // Invulnerability
    if (m_invulnTimer > 0.0f) {
        m_invulnTimer -= dt;
        // Blink player
        if (m_player) {
            bool visible = static_cast<int>(m_invulnTimer * 10.0f) % 2 == 0;
            m_player->setVisible(visible);
        }
    } else if (m_player) {
        m_player->setVisible(true);
    }

    // Spawn enemies that enter the view
    spawnVisibleEnemies();

    // Update all
    updateProjectiles(dt);
    updateEnemies(dt);
    recycleStars();

    // Collisions
    checkCollisions();
}

void GameScene::updateGameOver(float /*dt*/) {
    auto* in = input();
    if (in && in->consumeRestart()) {
        enterPlaying();
    }
}

// ============================================================================
// Player
// ============================================================================

void GameScene::movePlayer(float dt) {
    auto* in = input();
    if (!m_player || !in)
        return;

    glm::vec2 dir = in->getMoveDirection();
    auto pos = m_player->getPosition();
    glm::vec2 pos2d(pos.x, pos.y);
    pos2d += dir * PLAYER_SPEED * dt;

    // Clamp to visible play area (1.5-unit bottom margin keeps player fully on-screen)
    float camY = VIEW_HEIGHT * 0.5f + m_scrollY;
    float minY = camY - HALF_VIEW_H + 1.5f;
    float maxY = camY + HALF_VIEW_H - 0.5f;
    pos2d.x = std::clamp(pos2d.x, -HALF_VIEW_W + 0.5f, HALF_VIEW_W - 0.5f);
    pos2d.y = std::clamp(pos2d.y, minY, maxY);

    m_player->setPosition(pos2d.x, pos2d.y, 0.0f);
}

void GameScene::fireWeapon() {
    if (!m_player)
        return;
    auto pos = m_player->getPosition();
    glm::vec2 origin(pos.x, pos.y + 0.5f);

    switch (m_weapon) {
    case WeaponType::Basic:
        spawnPlayerBullet(origin, {0.0f, BULLET_SPEED});
        if (m_sounds.shoot)
            AudioManager::getInstance().playSFX(m_sounds.shoot, 0.3f);
        break;

    case WeaponType::Spread:
        spawnPlayerBullet(origin, {0.0f, SPREAD_SPEED});
        spawnPlayerBullet(
            origin, {SPREAD_SPEED * std::sin(SPREAD_ANGLE), SPREAD_SPEED * std::cos(SPREAD_ANGLE)});
        spawnPlayerBullet(origin, {SPREAD_SPEED * std::sin(-SPREAD_ANGLE),
                                   SPREAD_SPEED * std::cos(-SPREAD_ANGLE)});
        if (m_sounds.spreadShoot)
            AudioManager::getInstance().playSFX(m_sounds.spreadShoot, 0.25f);
        break;

    case WeaponType::Rapid:
        spawnPlayerBullet(origin, {0.0f, RAPID_SPEED});
        if (m_sounds.rapidShoot)
            AudioManager::getInstance().playSFX(m_sounds.rapidShoot, 0.2f);
        break;

    default:
        break;
    }
}

void GameScene::cycleWeapon(int dir) {
    int w = static_cast<int>(m_weapon) + dir;
    int count = static_cast<int>(WeaponType::Count);
    w = ((w % count) + count) % count;
    m_weapon = static_cast<WeaponType>(w);

    // Update fire rate
    switch (m_weapon) {
    case WeaponType::Basic:
        m_fireCooldown = Cooldown(BASIC_COOLDOWN);
        break;
    case WeaponType::Spread:
        m_fireCooldown = Cooldown(SPREAD_COOLDOWN);
        break;
    case WeaponType::Rapid:
        m_fireCooldown = Cooldown(RAPID_COOLDOWN);
        break;
    default:
        break;
    }
    m_fireCooldown.finish();

    const char* names[] = {"Basic", "Spread", "Rapid"};
    std::cout << "Weapon: " << names[static_cast<int>(m_weapon)] << std::endl;

    if (m_sounds.weaponSwitch)
        AudioManager::getInstance().playSFX(m_sounds.weaponSwitch, 0.25f);
}

// ============================================================================
// Projectiles
// ============================================================================

void GameScene::spawnPlayerBullet(glm::vec2 pos, glm::vec2 vel) {
    auto sprite = addEntity<SpriteEntity>();
    sprite->setScale(0.15f, 0.35f, 1.0f);
    sprite->setPosition(pos.x, pos.y, -0.1f);
    sprite->setAnchor(0.5f, 0.5f);
    int idx = static_cast<int>(m_weapon);
    if (m_bulletTex[idx])
        sprite->setTexture(m_bulletTex[idx]);
    else
        sprite->setColor(Color::white());

    m_projectiles.push_back({sprite, {vel, true}, false});
}

void GameScene::spawnEnemyBullet(glm::vec2 pos, glm::vec2 vel) {
    auto sprite = addEntity<SpriteEntity>();
    sprite->setScale(0.2f, 0.2f, 1.0f);
    sprite->setPosition(pos.x, pos.y, -0.1f);
    sprite->setAnchor(0.5f, 0.5f);
    if (m_enemyBulletTex)
        sprite->setTexture(m_enemyBulletTex);
    else
        sprite->setColor(Color::red());

    m_projectiles.push_back({sprite, {vel, true}, true});
}

void GameScene::updateProjectiles(float dt) {
    float camY = VIEW_HEIGHT * 0.5f + m_scrollY;
    float margin = 2.0f;

    for (int i = static_cast<int>(m_projectiles.size()) - 1; i >= 0; --i) {
        auto& p = m_projectiles[i];
        if (!p.data.alive) {
            removeEntity(p.sprite->getId());
            m_projectiles.erase(m_projectiles.begin() + i);
            continue;
        }
        auto pos = p.sprite->getPosition();
        pos.x += p.data.velocity.x * dt;
        pos.y += p.data.velocity.y * dt;
        p.sprite->setPosition(pos);

        // Remove if off screen
        if (pos.y > camY + HALF_VIEW_H + margin || pos.y < camY - HALF_VIEW_H - margin ||
            pos.x < -HALF_VIEW_W - margin || pos.x > HALF_VIEW_W + margin) {
            removeEntity(p.sprite->getId());
            m_projectiles.erase(m_projectiles.begin() + i);
        }
    }
}

// ============================================================================
// Enemies
// ============================================================================

void GameScene::spawnVisibleEnemies() {
    float spawnLine = m_scrollY + VIEW_HEIGHT + 2.0f;  // slightly above top of screen

    while (m_nextSpawnIdx < m_map.enemies.size()) {
        const auto& spawn = m_map.enemies[m_nextSpawnIdx];
        if (spawn.y > spawnLine)
            break;

        auto sprite = addEntity<SpriteEntity>();
        glm::vec2 halfExt = enemyHalfExtents(spawn.type);
        sprite->setScale(halfExt.x * 2.0f, halfExt.y * 2.0f, 1.0f);
        sprite->setPosition(spawn.x, spawn.y, 0.0f);
        sprite->setAnchor(0.5f, 0.5f);

        int texIdx = static_cast<int>(spawn.type);
        if (m_enemyTex[texIdx])
            sprite->setTexture(m_enemyTex[texIdx]);

        EnemyData edata;
        edata.type = spawn.type;
        edata.baseX = spawn.x;
        edata.baseY = spawn.y;
        edata.alive = true;

        switch (spawn.type) {
        case EnemyType::Turret:
            edata.health = TURRET_HP;
            break;
        case EnemyType::Drone:
            edata.health = DRONE_HP;
            break;
        case EnemyType::Chaser:
            edata.health = CHASER_HP;
            break;
        case EnemyType::Tank:
            edata.health = TANK_HP;
            break;
        }

        m_enemies.push_back({sprite, edata});
        ++m_nextSpawnIdx;
    }
}

void GameScene::updateEnemies(float dt) {
    glm::vec2 playerPos(0.0f);
    if (m_player) {
        auto pp = m_player->getPosition();
        playerPos = {pp.x, pp.y};
    }

    float camY = VIEW_HEIGHT * 0.5f + m_scrollY;

    for (int i = static_cast<int>(m_enemies.size()) - 1; i >= 0; --i) {
        auto& e = m_enemies[i];
        if (!e.data.alive) {
            removeEntity(e.sprite->getId());
            m_enemies.erase(m_enemies.begin() + i);
            continue;
        }

        FireRequest fire;
        bool wantsFire = updateEnemy(e.data, e.sprite.get(), dt, playerPos, m_scrollY, fire);
        if (wantsFire)
            spawnEnemyBullet(fire.origin, fire.velocity);

        // Remove if scrolled well past the bottom of the view
        auto pos = e.sprite->getPosition();
        if (pos.y < camY - HALF_VIEW_H - 3.0f) {
            removeEntity(e.sprite->getId());
            m_enemies.erase(m_enemies.begin() + i);
        }
    }
}

// ============================================================================
// Stars
// ============================================================================

void GameScene::initStars() {
    constexpr int NUM_STARS = 40;
    m_stars.clear();

    RandomStream rng(99);
    float camY = VIEW_HEIGHT * 0.5f + m_scrollY;

    for (int i = 0; i < NUM_STARS; ++i) {
        auto sprite = addEntity<SpriteEntity>();
        float sz = rng.range(0.05f, 0.15f);
        sprite->setScale(sz, sz, 1.0f);
        float x = rng.range(-HALF_VIEW_W, HALF_VIEW_W);
        float y = rng.range(camY - HALF_VIEW_H - 2.0f, camY + HALF_VIEW_H + 2.0f);
        sprite->setPosition(x, y, 0.5f);  // behind everything
        sprite->setAnchor(0.5f, 0.5f);
        if (m_starTex)
            sprite->setTexture(m_starTex);
        float bright = rng.range(0.3f, 1.0f);
        sprite->setColor(Color(bright, bright, bright * 1.1f, 0.8f));
        m_stars.push_back({sprite});
    }
}

void GameScene::recycleStars() {
    float camY = VIEW_HEIGHT * 0.5f + m_scrollY;
    float bottom = camY - HALF_VIEW_H - 2.0f;
    float top = camY + HALF_VIEW_H + 2.0f;

    RandomStream rng(static_cast<uint32_t>(m_scrollY * 100.0f));

    for (auto& s : m_stars) {
        auto pos = s.sprite->getPosition();
        if (pos.y < bottom) {
            float x = rng.range(-HALF_VIEW_W, HALF_VIEW_W);
            s.sprite->setPosition(x, top + rng.range(0.0f, 2.0f), 0.5f);
        }
    }
}

// ============================================================================
// Collisions
// ============================================================================

void GameScene::checkCollisions() {
    if (!m_player)
        return;

    auto pp = m_player->getPosition();
    glm::vec2 playerPos(pp.x, pp.y);
    glm::vec2 playerHalf(PLAYER_HALF_W, PLAYER_HALF_H);

    // Player bullets vs enemies
    for (int p = static_cast<int>(m_projectiles.size()) - 1; p >= 0; --p) {
        auto& proj = m_projectiles[p];
        if (proj.isEnemy || !proj.data.alive)
            continue;
        auto bp = proj.sprite->getPosition();
        glm::vec2 bPos(bp.x, bp.y);
        glm::vec2 bHalf(0.1f, 0.2f);

        for (auto& e : m_enemies) {
            if (!e.data.alive)
                continue;
            auto ep = e.sprite->getPosition();
            glm::vec2 ePos(ep.x, ep.y);
            glm::vec2 eHalf = enemyHalfExtents(e.data.type);

            if (boxOverlap(bPos, bHalf, ePos, eHalf)) {
                proj.data.alive = false;
                e.data.health--;
                if (e.data.health <= 0) {
                    e.data.alive = false;
                    m_score += enemyScore(e.data.type);
                    std::cout << "Score: " << m_score << std::endl;
                    if (m_sounds.explosion)
                        AudioManager::getInstance().playSFX(m_sounds.explosion, 0.35f);
                } else {
                    if (m_sounds.hit)
                        AudioManager::getInstance().playSFX(m_sounds.hit, 0.25f);
                    // Flash enemy white briefly
                    e.sprite->setColor(Color::white());
                }
                break;
            }
        }
    }

    // Enemy bullets vs player
    if (m_invulnTimer <= 0.0f) {
        for (auto& proj : m_projectiles) {
            if (!proj.isEnemy || !proj.data.alive)
                continue;
            auto bp = proj.sprite->getPosition();
            glm::vec2 bPos(bp.x, bp.y);
            glm::vec2 bHalf(0.1f, 0.1f);

            if (boxOverlap(playerPos, playerHalf, bPos, bHalf)) {
                proj.data.alive = false;
                m_health--;
                m_invulnTimer = INVULN_TIME;
                std::cout << "Hit! Lives: " << m_health << std::endl;
                if (m_sounds.hit)
                    AudioManager::getInstance().playSFX(m_sounds.hit, 0.4f);
                if (m_health <= 0) {
                    enterGameOver();
                    return;
                }
                break;
            }
        }
    }

    // Enemies vs player (body collision)
    if (m_invulnTimer <= 0.0f) {
        for (auto& e : m_enemies) {
            if (!e.data.alive)
                continue;
            auto ep = e.sprite->getPosition();
            glm::vec2 ePos(ep.x, ep.y);
            glm::vec2 eHalf = enemyHalfExtents(e.data.type);

            if (boxOverlap(playerPos, playerHalf, ePos, eHalf)) {
                m_health--;
                m_invulnTimer = INVULN_TIME;
                e.data.alive = false;
                m_score += enemyScore(e.data.type);
                std::cout << "Collision! Lives: " << m_health << "  Score: " << m_score
                          << std::endl;
                if (m_sounds.explosion)
                    AudioManager::getInstance().playSFX(m_sounds.explosion, 0.4f);
                if (m_health <= 0) {
                    enterGameOver();
                    return;
                }
                break;
            }
        }
    }
}

// ============================================================================
// Helpers
// ============================================================================

void GameScene::clearAll() {
    if (m_player) {
        removeEntity(m_player->getId());
        m_player.reset();
    }
    for (auto& p : m_projectiles)
        removeEntity(p.sprite->getId());
    m_projectiles.clear();
    for (auto& e : m_enemies)
        removeEntity(e.sprite->getId());
    m_enemies.clear();
    for (auto& s : m_stars)
        removeEntity(s.sprite->getId());
    m_stars.clear();
    if (m_titleBanner) {
        removeEntity(m_titleBanner->getId());
        m_titleBanner.reset();
    }
    if (m_promptSprite) {
        removeEntity(m_promptSprite->getId());
        m_promptSprite.reset();
    }
}

ShooterInput* GameScene::input() {
    return dynamic_cast<ShooterInput*>(getInputHandler());
}

}  // namespace shooter
