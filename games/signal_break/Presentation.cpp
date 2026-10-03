#include <algorithm>
#include <cmath>

#include "SignalBreakScene.h"

namespace signalbreak {

vde::SpriteEntity* SignalBreakScene::sprite(vde::EntityId id) {
    return dynamic_cast<vde::SpriteEntity*>(getEntity(id));
}

vde::EntityId SignalBreakScene::rectangle(float horizontal, float vertical, float width,
                                          float height, uint32_t color, float depth) {
    auto entity = addEntity<vde::SpriteEntity>();
    entity->setPosition(horizontal, vertical, depth);
    entity->setScale(width, height, 1.0f);
    entity->setColor(vde::Color::fromHex(color));
    return entity->getId();
}

vde::EntityId SignalBreakScene::label(const std::string& text, float height, uint32_t color) {
    auto entity = addEntity<vde::TextEntity>();
    entity->setText(text);
    entity->setFont(vde::BitmapFont::small());
    entity->setStyle({.color = vde::Color::fromHex(color), .pixelScale = 2});
    entity->setWorldHeight(height);
    return entity->getId();
}

void SignalBreakScene::createWorldVisuals() {
    m_sky = rectangle(0, 0, 30, 14, 0xffffff, -9);
    sprite(m_sky)->setTexture(m_art.sky);
    for (int block = 0; block < 6; ++block) {
        auto city = rectangle(static_cast<float>(block) * 32.0f, 3.0f, 32, 12, 0xffffff, -7);
        sprite(city)->setTexture(m_art.skyline);
        m_city.push_back(city);
    }
    for (int tower = 0; tower < 7; ++tower) {
        const float horizontal = 8.0f + static_cast<float>(tower) * 18.0f;
        rectangle(horizontal, 3.8f, 0.3f, 11.0f, 0x5c7a74, -5);
        rectangle(horizontal + 2.5f, 8.5f, 6.0f, 0.22f, 0x5c7a74, -5);
        rectangle(horizontal + 5.0f, 7.1f, 0.09f, 2.8f, 0x5c7a74, -5);
        rectangle(horizontal + 5.0f, 5.7f, 0.5f, 0.3f, 0x5c7a74, -5);
    }
    for (const auto& platform : platforms()) {
        if (platform.halfSize.y > 2.0f) {
            continue;
        }
        const float width = platform.halfSize.x * 2.0f;
        auto deck = rectangle(platform.center.x, platform.center.y, width,
                              platform.halfSize.y * 2.0f, 0xffffff, 0);
        sprite(deck)->setTexture(m_art.metal);
        rectangle(platform.center.x, platform.center.y + platform.halfSize.y - 0.04f, width, 0.08f,
                  0xf0c75a, 0.1f);
        if (platform.center.y > 0) {
            const float supportHeight = platform.center.y;
            for (float side : {-1.0f, 1.0f}) {
                rectangle(platform.center.x + side * (platform.halfSize.x - 0.25f),
                          supportHeight * 0.5f, 0.18f, supportHeight, 0x496562, -0.5f);
            }
        }
    }
    for (float checkpoint : {44.0f, 84.0f}) {
        rectangle(checkpoint, 1.3f, 0.13f, 2.6f, 0x25383b, 0.2f);
        rectangle(checkpoint + 0.48f, 2.25f, 0.85f, 0.5f, 0x50dfc2, 0.3f);
        auto marker = label("RELAY", 0.2f, 0x25383b);
        sprite(marker)->setPosition(checkpoint + 0.5f, 2.9f, 0.3f);
    }
    rectangle(118.0f, 1.8f, 2.4f, 3.6f, 0x21373b, -0.1f);
    m_gate = rectangle(118.0f, 1.75f, 1.6f, 3.2f, 0xb34e50, 0);
    auto extraction = label("EXTRACTION", 0.32f, 0x25383b);
    sprite(extraction)->setPosition(117.8f, 4.1f, 0.3f);
    m_player = rectangle(3.0f, 0.7f, 1.45f, 1.8f, 0xffffff, 1);
    sprite(m_player)->setTexture(m_art.player[0]);
    for (const auto& enemy : m_gameplay.enemies()) {
        auto entity = rectangle(enemy.position.x, enemy.position.y, 1.45f, 1.8f,
                                enemy.commander ? 0xffcf75 : 0xffffff, 1);
        sprite(entity)->setTexture(m_art.enemy[0]);
        m_enemies.push_back(entity);
    }
    for (int projectile = 0; projectile < 96; ++projectile) {
        auto entity = rectangle(0, 0, 0.32f, 0.12f, 0xffe99a, 2);
        sprite(entity)->setVisible(false);
        m_projectiles.push_back(entity);
    }
    for (int impact = 0; impact < 32; ++impact) {
        auto entity = rectangle(0, 0, 0.25f, 0.25f, 0xfff0b0, 2.1f);
        sprite(entity)->setVisible(false);
        m_impacts.push_back(entity);
    }
}

void SignalBreakScene::createHud() {
    m_hudBand = rectangle(0, 10.55f, 24, 1.5f, 0x182e34, 4);
    m_title = label("SIGNAL BREAK", 0.42f, 0xf8e7b1);
    sprite(m_title)->setAnchor(0, 0.5f);
    m_status = label("OUTPOST 07 / APPROACH", 0.22f, 0x8dbbb5);
    sprite(m_status)->setAnchor(0, 0.5f);
    m_score = label("0000", 0.27f, 0xf8e7b1);
    sprite(m_score)->setAnchor(1, 0.5f);
    for (auto& segment : m_health) {
        segment = rectangle(0, 10.3f, 0.32f, 0.25f, 0x43d7c6, 4.2f);
    }
    m_progress = rectangle(0, 9.82f, 0.1f, 0.055f, 0xf0c75a, 4.2f);
    sprite(m_progress)->setAnchor(0, 0.5f);
    m_message = label(" ", 0.65f, 0x142c33);
}

void SignalBreakScene::updatePresentation() {
    auto* window = getGame()->getWindow();
    const float aspect = static_cast<float>(std::max(1u, window->getWidth())) /
                         static_cast<float>(std::max(1u, window->getHeight()));
    m_viewWidth = 14.0f * aspect;
    const auto player = m_gameplay.playerPosition();
    const float halfView = std::min(m_viewWidth * 0.5f, LEVEL_LENGTH * 0.5f);
    m_cameraX = std::clamp(player.x + 4.0f, halfView, LEVEL_LENGTH - halfView);
    auto* camera = dynamic_cast<vde::Camera2D*>(getCamera());
    camera->setViewportSize(m_viewWidth, 14.0f);
    camera->setPosition(m_cameraX, 4.3f);
    sprite(m_sky)->setPosition(m_cameraX, 4.3f, -9);
    sprite(m_sky)->setScale(m_viewWidth, 14.0f, 1.0f);
    for (size_t block = 0; block < m_city.size(); ++block) {
        sprite(m_city[block])
            ->setPosition(static_cast<float>(block) * 32.0f - 32.0f + m_cameraX * 0.7f, 3.0f, -7);
    }
    auto* playerSprite = sprite(m_player);
    playerSprite->setPosition(player.x, player.y + 0.08f, 1);
    playerSprite->setFlipX(m_gameplay.facing() < 0);
    const bool running = std::abs(m_gameplay.playerVelocity().x) > 0.2f;
    const int animation = running ? static_cast<int>(m_gameplay.time() * 10.0f) % 2 : 0;
    if (playerSprite->getTexture() != m_art.player[animation]) {
        playerSprite->setTexture(m_art.player[animation]);
    }
    const bool blink =
        m_gameplay.invulnerability() > 0 && static_cast<int>(m_gameplay.time() * 16.0f) % 2 == 0;
    playerSprite->setColor(vde::Color::fromHex(blink ? 0xffc99e : 0xffffff));
    playerSprite->setVisible(m_gameplay.phase() != RunPhase::Down);
    for (size_t index = 0; index < m_enemies.size(); ++index) {
        const auto& enemy = m_gameplay.enemies()[index];
        auto* entity = sprite(m_enemies[index]);
        entity->setVisible(enemy.health > 0);
        entity->setPosition(enemy.position.x, enemy.position.y + 0.08f, 1);
        entity->setFlipX(enemy.facing < 0);
    }
    for (size_t index = 0; index < m_projectiles.size(); ++index) {
        auto* entity = sprite(m_projectiles[index]);
        const bool active = index < m_gameplay.projectiles().size();
        entity->setVisible(active);
        if (active) {
            const auto& projectile = m_gameplay.projectiles()[index];
            entity->setPosition(projectile.position.x, projectile.position.y, 2);
            entity->setScale(projectile.hostile ? 0.22f : 0.38f, 0.14f, 1);
            entity->setColor(vde::Color::fromHex(projectile.hostile ? 0xff514a : 0xfff0ad));
        }
    }
    for (size_t index = 0; index < m_impacts.size(); ++index) {
        auto* entity = sprite(m_impacts[index]);
        const bool active = index < m_gameplay.impacts().size();
        entity->setVisible(active);
        if (active) {
            const auto& impact = m_gameplay.impacts()[index];
            const float size = 0.1f + impact.life * 2.0f;
            entity->setPosition(impact.position.x, impact.position.y, 2.1f);
            entity->setScale(size, size, 1);
            entity->setColor(vde::Color::fromHex(impact.hostile ? 0xfa7e62 : 0xfff0ad));
        }
    }
    sprite(m_gate)->setColor(
        vde::Color::fromHex(m_gameplay.enemies().back().health > 0 ? 0xb34e50 : 0x43d7c6));
    const float left = m_cameraX - m_viewWidth * 0.5f;
    const float right = m_cameraX + m_viewWidth * 0.5f;
    sprite(m_hudBand)->setPosition(m_cameraX, 10.55f, 4);
    sprite(m_hudBand)->setScale(m_viewWidth, 1.5f, 1);
    sprite(m_title)->setPosition(left + 0.5f, 10.78f, 4.2f);
    dynamic_cast<vde::TextEntity*>(getEntity(m_title))->setMaxWidth(m_viewWidth * 0.52f);
    auto* status = dynamic_cast<vde::TextEntity*>(getEntity(m_status));
    status->setPosition(left + 0.5f, 10.22f, 4.2f);
    status->setMaxWidth(m_viewWidth * 0.52f);
    status->setText(
        m_gameplay.checkpoint() >= 84
            ? "OUTPOST 07 / UPLINK"
            : (m_gameplay.checkpoint() >= 44 ? "OUTPOST 07 / FOUNDRY" : "OUTPOST 07 / APPROACH"));
    auto* score = dynamic_cast<vde::TextEntity*>(getEntity(m_score));
    score->setPosition(right - 0.5f, 10.83f, 4.2f);
    score->setMaxWidth(m_viewWidth * 0.3f);
    score->setText("SCORE " + std::to_string(m_gameplay.kills() * 100));
    for (size_t index = 0; index < m_health.size(); ++index) {
        const float spacing = std::min(0.45f, m_viewWidth * 0.045f);
        sprite(m_health[index])
            ->setPosition(right - 0.65f - static_cast<float>(index) * spacing, 10.25f, 4.2f);
        sprite(m_health[index])->setScale(spacing * 0.7f, 0.25f, 1);
        sprite(m_health[index])
            ->setColor(vde::Color::fromHex(
                static_cast<int>(index) < m_gameplay.health() ? 0x43d7c6 : 0x3e5156));
    }
    sprite(m_progress)->setPosition(left, 9.82f, 4.2f);
    sprite(m_progress)
        ->setScale(m_viewWidth * std::clamp(player.x / LEVEL_LENGTH, 0.001f, 1.0f), 0.055f, 1);
    auto* message = dynamic_cast<vde::TextEntity*>(getEntity(m_message));
    message->setPosition(m_cameraX, 7.5f, 4.3f);
    message->setMaxWidth(std::max(1.0f, m_viewWidth - 1.0f));
    message->setText(
        m_gameplay.paused()
            ? "PAUSED"
            : (m_gameplay.phase() == RunPhase::Down
                   ? "SIGNAL LOST"
                   : (m_gameplay.phase() == RunPhase::Complete ? "UPLINK SILENCED" : " ")));
}

}  // namespace signalbreak