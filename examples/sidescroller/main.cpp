/**
 * @file main.cpp
 * @brief 2D sidescroller example demonstrating AnimatedSpriteEntity state workflow.
 *
 * This example demonstrates:
 * - SpriteSheet-backed AnimatedSpriteEntity state playback
 * - Explicit idle / run / jump / attack state transitions
 * - Attack frame events used for a brief hit-flash effect
 * - Player movement and jumping with simple platformer physics
 * - Camera follow and patrol enemies
 */

#include <vde/Texture.h>
#include <vde/api/GameAPI.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <memory>
#include <vector>

#include "../ExampleBase.h"

/**
 * @brief Simple 2D physics for platformer movement.
 */
struct Physics2D {
    glm::vec2 velocity{0.0f, 0.0f};
    glm::vec2 acceleration{0.0f, 0.0f};
    float gravity = -15.0f;
    bool onGround = false;

    void update(float deltaTime) {
        velocity += acceleration * deltaTime;
        velocity.y += gravity * deltaTime;
        velocity.y = std::max(velocity.y, -30.0f);
        acceleration = glm::vec2(0.0f, 0.0f);
    }

    void applyForce(const glm::vec2& force) { acceleration += force; }

    void jump(float power) {
        if (onGround) {
            velocity.y = power;
            onGround = false;
        }
    }
};

namespace {

struct RGBA {
    uint8_t r, g, b, a;
};

struct HeroFrameSpec {
    int bobY = 0;
    int leftLegX = 6;
    int rightLegX = 9;
    int legTopY = 10;
    int legHeight = 4;
    int rightArmY = 6;
    int rightArmLength = 4;
    int swordStartX = 12;
    int swordY = 7;
    int swordLength = 0;
    bool leftArmRaised = false;
    bool rightArmRaised = false;
    bool crouch = false;
};

void putPixel(std::vector<uint8_t>& buffer, uint32_t stride, uint32_t x, uint32_t y, RGBA color) {
    size_t offset = (static_cast<size_t>(y) * stride + x) * 4;
    buffer.at(offset + 0) = color.r;
    buffer.at(offset + 1) = color.g;
    buffer.at(offset + 2) = color.b;
    buffer.at(offset + 3) = color.a;
}

void fillRect(std::vector<uint8_t>& buffer, uint32_t stride, uint32_t x0, uint32_t y0,
              uint32_t width, uint32_t height, RGBA color) {
    for (uint32_t y = y0; y < y0 + height; ++y) {
        for (uint32_t x = x0; x < x0 + width; ++x) {
            putPixel(buffer, stride, x, y, color);
        }
    }
}

void drawHeroFrame(std::vector<uint8_t>& buffer, uint32_t stride, uint32_t originX,
                   uint32_t originY, const HeroFrameSpec& spec, RGBA tunic, RGBA skin, RGBA blade,
                   RGBA shadow, RGBA background) {
    fillRect(buffer, stride, originX, originY, 16, 16, background);

    int bobY = spec.bobY;
    int headY = static_cast<int>(originY) + 1 + bobY;
    int torsoY = static_cast<int>(originY) + 5 + bobY + (spec.crouch ? 1 : 0);
    int torsoHeight = spec.crouch ? 4 : 5;
    int legY = static_cast<int>(originY) + spec.legTopY + bobY;

    fillRect(buffer, stride, originX + 6, static_cast<uint32_t>(headY), 4, 4, skin);
    putPixel(buffer, stride, originX + 8, static_cast<uint32_t>(headY + 1), shadow);

    fillRect(buffer, stride, originX + 5, static_cast<uint32_t>(torsoY), 6,
             static_cast<uint32_t>(torsoHeight), tunic);
    fillRect(buffer, stride, originX + 5, static_cast<uint32_t>(torsoY + torsoHeight - 1), 6, 1,
             shadow);

    int leftArmY = static_cast<int>(originY) + (spec.leftArmRaised ? 4 : 6) + bobY;
    int leftArmHeight = spec.leftArmRaised ? 3 : 4;
    fillRect(buffer, stride, originX + 4, static_cast<uint32_t>(leftArmY), 2,
             static_cast<uint32_t>(leftArmHeight), tunic);

    int rightArmY = static_cast<int>(originY) + (spec.rightArmRaised ? 4 : spec.rightArmY) + bobY;
    int rightArmHeight = spec.rightArmRaised ? 3 : spec.rightArmLength;
    fillRect(buffer, stride, originX + 10, static_cast<uint32_t>(rightArmY), 2,
             static_cast<uint32_t>(rightArmHeight), tunic);

    fillRect(buffer, stride, originX + static_cast<uint32_t>(spec.leftLegX),
             static_cast<uint32_t>(legY), 2, static_cast<uint32_t>(spec.legHeight), shadow);
    fillRect(buffer, stride, originX + static_cast<uint32_t>(spec.rightLegX),
             static_cast<uint32_t>(legY), 2, static_cast<uint32_t>(spec.legHeight), shadow);

    if (spec.swordLength > 0) {
        fillRect(buffer, stride, originX + static_cast<uint32_t>(spec.swordStartX),
                 originY + static_cast<uint32_t>(spec.swordY + bobY),
                 static_cast<uint32_t>(spec.swordLength), 1, blade);
        if (spec.swordLength > 2) {
            fillRect(buffer, stride, originX + static_cast<uint32_t>(spec.swordStartX + 1),
                     originY + static_cast<uint32_t>(spec.swordY + bobY + 1),
                     static_cast<uint32_t>(spec.swordLength - 2), 1, skin);
        }
    }
}

std::shared_ptr<vde::SpriteSheet> createCharacterSpriteSheet(vde::VulkanContext* context) {
    constexpr uint32_t kFrameSize = 16;
    constexpr uint32_t kColumns = 4;
    constexpr uint32_t kRows = 3;
    constexpr uint32_t kTextureWidth = kFrameSize * kColumns;
    constexpr uint32_t kTextureHeight = kFrameSize * kRows;

    constexpr RGBA kTransparent{.r = 0, .g = 0, .b = 0, .a = 0};
    constexpr RGBA kTunic{.r = 255, .g = 255, .b = 255, .a = 255};
    constexpr RGBA kSkin{.r = 215, .g = 215, .b = 215, .a = 255};
    constexpr RGBA kBlade{.r = 255, .g = 245, .b = 180, .a = 255};
    constexpr RGBA kShadow{.r = 80, .g = 80, .b = 80, .a = 255};

    std::vector<uint8_t> pixels(static_cast<size_t>(kTextureWidth) * kTextureHeight * 4, 0);

    std::vector<HeroFrameSpec> frames(10);
    frames.at(1).bobY = 1;

    frames.at(2).leftLegX = 5;
    frames.at(2).rightLegX = 9;

    frames.at(3).bobY = 1;
    frames.at(3).leftLegX = 6;
    frames.at(3).rightLegX = 8;
    frames.at(3).rightArmY = 5;

    frames.at(4).leftLegX = 9;
    frames.at(4).rightLegX = 5;

    frames.at(5).bobY = 1;
    frames.at(5).leftLegX = 8;
    frames.at(5).rightLegX = 6;
    frames.at(5).rightArmY = 5;

    frames.at(6).leftLegX = 6;
    frames.at(6).rightLegX = 8;
    frames.at(6).legTopY = 11;
    frames.at(6).legHeight = 3;
    frames.at(6).leftArmRaised = true;
    frames.at(6).rightArmRaised = true;
    frames.at(6).rightArmLength = 3;

    frames.at(7).rightArmY = 4;
    frames.at(7).rightArmLength = 3;
    frames.at(7).swordStartX = 11;
    frames.at(7).swordY = 6;
    frames.at(7).swordLength = 2;
    frames.at(7).crouch = true;

    frames.at(8).rightArmY = 5;
    frames.at(8).rightArmLength = 2;
    frames.at(8).swordStartX = 12;
    frames.at(8).swordY = 7;
    frames.at(8).swordLength = 4;

    frames.at(9).rightArmY = 6;
    frames.at(9).rightArmLength = 3;
    frames.at(9).swordStartX = 12;
    frames.at(9).swordY = 8;
    frames.at(9).swordLength = 3;

    for (size_t index = 0; index < frames.size(); ++index) {
        uint32_t originX = static_cast<uint32_t>(index % kColumns) * kFrameSize;
        uint32_t originY = static_cast<uint32_t>(index / kColumns) * kFrameSize;
        drawHeroFrame(pixels, kTextureWidth, originX, originY, frames.at(index), kTunic, kSkin,
                      kBlade, kShadow, kTransparent);
    }

    auto texture = std::make_shared<vde::Texture>();
    texture->loadFromData(pixels.data(), kTextureWidth, kTextureHeight);
    if (context != nullptr) {
        texture->uploadToGPU(context);
    }

    return vde::SpriteSheet::createGrid(texture, static_cast<int>(kColumns),
                                        static_cast<int>(kRows));
}

void addCharacterAnimations(vde::AnimatedSpriteEntity& entity) {
    vde::SpriteAnimation idle("idle");
    idle.addFrame(0, 0.24f);
    idle.addFrame(1, 0.24f);
    entity.addAnimation("idle", idle);

    vde::SpriteAnimation run("run");
    run.addFrame(2, 0.09f);
    run.addFrame(3, 0.09f);
    run.addFrame(4, 0.09f);
    run.addFrame(5, 0.09f);
    entity.addAnimation("run", run);

    vde::SpriteAnimation jump("jump");
    jump.addFrame(6, 0.12f);
    jump.setLooping(true);
    entity.addAnimation("jump", jump);

    vde::SpriteAnimation attack("attack");
    attack.addFrame(7, 0.08f);
    attack.addFrame(8, 0.08f);
    attack.addFrame(9, 0.08f);
    attack.setLooping(false);
    entity.addAnimation("attack", attack);
}

}  // namespace

/**
 * @brief Player character using AnimatedSpriteEntity state playback.
 */
class PlayerEntity : public vde::AnimatedSpriteEntity {
  public:
    explicit PlayerEntity(const std::shared_ptr<vde::SpriteSheet>& sheet)
        : m_baseTint(vde::Color::fromHex(0x00d2d3)) {
        setSpriteSheet(sheet);
        setScale(1.1f, 1.1f, 1.0f);
        setAnchor(0.5f, 0.0f);
        setColor(m_baseTint);

        addCharacterAnimations(*this);
        onFrameEvent("attack", 1, [this]() { m_attackFlashTime = 0.12f; });
        configureTransitions();
        play("idle");
    }

    void moveHorizontal(float direction, float speed) {
        m_movementInputThisFrame = true;
        m_physics.applyForce(glm::vec2(direction * speed, 0.0f));

        if (direction < 0.0f) {
            setFlipX(true);
        } else if (direction > 0.0f) {
            setFlipX(false);
        }
    }

    void jump(float power) { m_physics.jump(power); }

    void queueAttack() {
        if (getCurrentAnimation() == "attack" && !isAnimationFinished()) {
            return;
        }

        setSpeed(1.0f);
        play("attack");
    }

    void landOn(float surfaceY) {
        auto pos = getPosition();
        pos.y = surfaceY;
        setPosition(pos);
        m_physics.velocity.y = 0.0f;
        m_physics.onGround = true;
    }

    void update(float deltaTime) override {
        m_physics.update(deltaTime);

        auto pos = getPosition();
        pos.x += m_physics.velocity.x * deltaTime;
        pos.y += m_physics.velocity.y * deltaTime;

        if (pos.y <= 0.0f) {
            pos.y = 0.0f;
            m_physics.velocity.y = 0.0f;
            m_physics.onGround = true;
        }

        if (m_physics.onGround) {
            m_physics.velocity.x *= 0.85f;
        }

        setPosition(pos);

        if (getCurrentAnimation() != "attack") {
            setSpeed(shouldRunAnimation() ? getRunAnimationSpeed() : 1.0f);
        }

        AnimatedSpriteEntity::update(deltaTime);

        if (m_attackFlashTime > 0.0f) {
            m_attackFlashTime = std::max(0.0f, m_attackFlashTime - deltaTime);
            setColor(vde::Color::fromHex(0xfff1b8));
        } else {
            setColor(m_baseTint);
        }

        if (!hasActiveBlend()) {
            setScale(1.1f, 1.1f, 1.0f);
        }

        m_movementInputThisFrame = false;
    }

    [[nodiscard]] glm::vec2 getVelocity() const { return m_physics.velocity; }
    [[nodiscard]] bool isOnGround() const { return m_physics.onGround; }
    [[nodiscard]] bool isAttackFlashActive() const { return m_attackFlashTime > 0.0f; }

  private:
    void configureTransitions() {
        addConditionalTransition(
            "idle", "jump", [this](const AnimatedSpriteEntity&) { return shouldJumpAnimation(); });
        addConditionalTransition(
            "idle", "run", [this](const AnimatedSpriteEntity&) { return shouldRunAnimation(); },
            0.08f,
            [this](AnimatedSpriteEntity&, const std::string&, const std::string&, float progress) {
                applyBlendScale(progress);
            });

        addConditionalTransition(
            "run", "jump", [this](const AnimatedSpriteEntity&) { return shouldJumpAnimation(); });
        addConditionalTransition(
            "run", "idle", [this](const AnimatedSpriteEntity&) { return shouldIdleAnimation(); },
            0.08f,
            [this](AnimatedSpriteEntity&, const std::string&, const std::string&, float progress) {
                applyBlendScale(1.0f - progress);
            });

        addConditionalTransition(
            "jump", "run",
            [this](const AnimatedSpriteEntity&) {
                return m_physics.onGround && shouldRunAnimation();
            },
            0.10f,
            [this](AnimatedSpriteEntity&, const std::string&, const std::string&, float progress) {
                applyBlendScale(progress);
            });
        addConditionalTransition(
            "jump", "idle", [this](const AnimatedSpriteEntity&) { return shouldIdleAnimation(); },
            0.10f,
            [this](AnimatedSpriteEntity&, const std::string&, const std::string&, float progress) {
                applyBlendScale(1.0f - progress);
            });

        addConditionalTransition("attack", "jump", [this](const AnimatedSpriteEntity&) {
            return isAnimationFinished() && shouldJumpAnimation();
        });
        addConditionalTransition(
            "attack", "run",
            [this](const AnimatedSpriteEntity&) {
                return isAnimationFinished() && shouldRunAnimation();
            },
            0.06f,
            [this](AnimatedSpriteEntity&, const std::string&, const std::string&, float progress) {
                applyBlendScale(progress);
            });
        addConditionalTransition(
            "attack", "idle",
            [this](const AnimatedSpriteEntity&) {
                return isAnimationFinished() && shouldIdleAnimation();
            },
            0.06f,
            [this](AnimatedSpriteEntity&, const std::string&, const std::string&, float progress) {
                applyBlendScale(1.0f - progress);
            });
    }

    [[nodiscard]] bool shouldJumpAnimation() const { return !m_physics.onGround; }

    [[nodiscard]] bool shouldRunAnimation() const {
        return m_movementInputThisFrame || std::abs(m_physics.velocity.x) > 1.0f;
    }

    [[nodiscard]] bool shouldIdleAnimation() const {
        return m_physics.onGround && !shouldRunAnimation();
    }

    [[nodiscard]] float getRunAnimationSpeed() const {
        return std::clamp(std::abs(m_physics.velocity.x) / 8.0f, 0.9f, 1.8f);
    }

    void applyBlendScale(float progress) {
        float width = 1.1f + 0.08f * progress;
        float height = 1.1f - 0.04f * progress;
        setScale(width, height, 1.0f);
    }

    Physics2D m_physics;
    vde::Color m_baseTint;
    float m_attackFlashTime = 0.0f;
    bool m_movementInputThisFrame = false;
};

/**
 * @brief Platform entity.
 */
class PlatformEntity : public vde::SpriteEntity {
  public:
    PlatformEntity(float x, float y, float width, float height) {
        setPosition(x, y + height / 2.0f, -0.1f);
        setScale(width, height, 1.0f);
        setColor(vde::Color::fromHex(0x6c5ce7));

        m_bounds = {
            .minX = x - width / 2.0f, .maxX = x + width / 2.0f, .minY = y, .maxY = y + height};
    }

    struct Bounds {
        float minX;
        float maxX;
        float minY;
        float maxY;
    };

    [[nodiscard]] Bounds getBounds() const { return m_bounds; }

  private:
    Bounds m_bounds{};
};

/**
 * @brief Enemy entity that patrols using the run animation.
 */
class EnemyEntity : public vde::AnimatedSpriteEntity {
  public:
    EnemyEntity(const std::shared_ptr<vde::SpriteSheet>& sheet, float startX, float startY,
                float patrolDistance)
        : m_startX(startX), m_patrolDistance(patrolDistance) {
        setSpriteSheet(sheet);
        setPosition(startX, startY, 0.0f);
        setScale(0.9f, 0.9f, 1.0f);
        setAnchor(0.5f, 0.0f);
        setColor(vde::Color::fromHex(0xff6b6b));

        addCharacterAnimations(*this);
        setSpeed(1.0f);
        play("run");
    }

    void update(float deltaTime) override {
        AnimatedSpriteEntity::update(deltaTime);

        auto pos = getPosition();
        pos.x += m_direction * m_speed * deltaTime;

        if (std::abs(pos.x - m_startX) > m_patrolDistance) {
            m_direction *= -1.0f;
            setFlipX(m_direction < 0.0f);
        }

        setPosition(pos);
    }

  private:
    float m_startX = 0.0f;
    float m_patrolDistance = 0.0f;
    float m_direction = 1.0f;
    float m_speed = 2.0f;
};

/**
 * @brief Input handler for the sidescroller.
 */
class SidescrollerInputHandler : public vde::examples::BaseExampleInputHandler {
  public:
    void onKeyPress(int key) override {
        BaseExampleInputHandler::onKeyPress(key);

        if (key == vde::KEY_A || key == vde::KEY_LEFT) {
            m_moveLeft = true;
        }
        if (key == vde::KEY_D || key == vde::KEY_RIGHT) {
            m_moveRight = true;
        }
        if (key == vde::KEY_SPACE || key == vde::KEY_W || key == vde::KEY_UP) {
            m_jump = true;
        }
        if (key == vde::KEY_X) {
            m_attack = true;
        }
    }

    void onKeyRelease(int key) override {
        if (key == vde::KEY_A || key == vde::KEY_LEFT) {
            m_moveLeft = false;
        }
        if (key == vde::KEY_D || key == vde::KEY_RIGHT) {
            m_moveRight = false;
        }
    }

    [[nodiscard]] bool isMoveLeft() const { return m_moveLeft; }
    [[nodiscard]] bool isMoveRight() const { return m_moveRight; }

    bool isJump() {
        bool value = m_jump;
        m_jump = false;
        return value;
    }

    bool isAttack() {
        bool value = m_attack;
        m_attack = false;
        return value;
    }

  private:
    bool m_moveLeft = false;
    bool m_moveRight = false;
    bool m_jump = false;
    bool m_attack = false;
};

/**
 * @brief Main sidescroller game scene.
 */
class SidescrollerScene : public vde::examples::BaseExampleScene {
  public:
    void onEnter() override {
        printExampleHeader();

        m_sceneCamera2D = new vde::Camera2D(20.0f, 15.0f);
        m_sceneCamera2D->setPosition(0.0f, 5.0f);
        setCamera(m_sceneCamera2D);

        setBackgroundColor(vde::Color::fromHex(0x74b9ff));

        createBackground();
        createPlatforms();

        m_characterSheet = createCharacterSpriteSheet(getGame()->getVulkanContext());

        m_player = addEntity<PlayerEntity>(m_characterSheet);
        m_player->setName("Player");
        m_player->setPosition(0.0f, 5.0f, 0.0f);

        auto enemy1 = addEntity<EnemyEntity>(m_characterSheet, 8.0f, 0.0f, 3.0f);
        enemy1->setName("Enemy1");

        auto enemy2 = addEntity<EnemyEntity>(m_characterSheet, 15.0f, 3.0f, 2.0f);
        enemy2->setName("Enemy2");

        createHud();

        std::cout << "\n=== SIDESCROLLER GAME ===\n";
        std::cout << "AnimatedSpriteEntity state demo with idle/run/jump/attack\n";
    }

    void update(float deltaTime) override {
        BaseExampleScene::update(deltaTime);

        auto* input = dynamic_cast<SidescrollerInputHandler*>(getInputHandler());
        if (!input) {
            return;
        }

        constexpr float moveSpeed = 50.0f;
        if (input->isMoveLeft()) {
            m_player->moveHorizontal(-1.0f, moveSpeed);
        }
        if (input->isMoveRight()) {
            m_player->moveHorizontal(1.0f, moveSpeed);
        }
        if (input->isJump()) {
            m_player->jump(12.0f);
        }
        if (input->isAttack()) {
            m_player->queueAttack();
        }

        auto playerPos = m_player->getPosition();
        auto cameraPos = m_sceneCamera2D->getPosition();
        constexpr float cameraSpeed = 3.0f;

        cameraPos.x += (playerPos.x - cameraPos.x) * cameraSpeed * deltaTime;
        float targetY = std::max(5.0f, playerPos.y);
        cameraPos.y += (targetY - cameraPos.y) * cameraSpeed * deltaTime;
        m_sceneCamera2D->setPosition(cameraPos.x, cameraPos.y);

        for (auto& platform : m_platforms) {
            const auto bounds = platform->getBounds();
            const auto playerVelocity = m_player->getVelocity();
            const auto playerPosition = m_player->getPosition();

            if (playerVelocity.y < 0.0f && playerPosition.x > bounds.minX &&
                playerPosition.x < bounds.maxX && playerPosition.y > bounds.minY &&
                playerPosition.y < bounds.maxY + 1.0f) {
                m_player->landOn(bounds.maxY);
            }
        }

        if (m_stateText) {
            m_stateText->setText("State: " + m_player->getCurrentAnimation());
        }
        if (m_eventText) {
            m_eventText->setText(m_player->isAttackFlashActive() ? "Slash Event: ACTIVE"
                                                                 : "Slash Event: ready");
        }
    }

  protected:
    [[nodiscard]] std::string getExampleName() const override { return "2D Sidescroller"; }

    [[nodiscard]] std::vector<std::string> getFeatures() const override {
        return {"AnimatedSpriteEntity idle/run/jump/attack states",
                "SpriteSheet-backed frame playback with no manual UV math in gameplay code",
                "Attack frame event driving a hit-flash effect",
                "Simple platformer physics and patrol enemies", "Camera following"};
    }

    [[nodiscard]] std::vector<std::string> getExpectedVisuals() const override {
        return {"Cyan player sprite changes between idle, run, jump, and attack poses",
                "A HUD label reports the active animation state",
                "The player flashes briefly during the attack impact frame",
                "Red enemies patrol on platforms while the camera follows the player"};
    }

    [[nodiscard]] std::vector<std::string> getControls() const override {
        return {"A/D or Arrow Keys - Move left/right", "Space/W/Up Arrow - Jump", "X - Attack"};
    }

  private:
    void createBackground() {
        for (int i = -2; i < 10; ++i) {
            auto background = addEntity<vde::SpriteEntity>();
            background->setPosition(static_cast<float>(i) * 5.0f, 5.0f, -0.9f);
            background->setScale(5.0f, 10.0f, 1.0f);
            background->setColor(vde::Color(0.6f, 0.7f, 0.8f, 0.3f));
            background->setAnchor(0.0f, 0.0f);
        }

        for (int i = -5; i < 20; ++i) {
            auto ground = addEntity<vde::SpriteEntity>();
            ground->setPosition(static_cast<float>(i) * 2.0f, -1.0f, -0.2f);
            ground->setScale(2.0f, 1.0f, 1.0f);
            ground->setColor(vde::Color::fromHex(0x6c5ce7));
            ground->setAnchor(0.0f, 0.0f);
        }
    }

    void createPlatforms() {
        m_platforms.push_back(addEntity<PlatformEntity>(5.0f, 0.0f, 4.0f, 0.5f));
        m_platforms.push_back(addEntity<PlatformEntity>(10.0f, 2.0f, 3.0f, 0.5f));
        m_platforms.push_back(addEntity<PlatformEntity>(14.0f, 4.0f, 4.0f, 0.5f));
        m_platforms.push_back(addEntity<PlatformEntity>(18.0f, 1.0f, 3.0f, 0.5f));
        m_platforms.push_back(addEntity<PlatformEntity>(22.0f, 3.0f, 4.0f, 0.5f));
    }

    void createHud() {
        auto title = addEntity<vde::TextEntity>();
        title->setText("Animated Sprite States");
        title->setFont(vde::BitmapFont::small());
        title->setStyle({.color = vde::Color::white(), .pixelScale = 2});
        title->setPosition(-8.8f, 6.4f, 0.2f);
        title->setWorldHeight(0.35f);

        m_stateText = addEntity<vde::TextEntity>();
        m_stateText->setText("State: idle");
        m_stateText->setFont(vde::BitmapFont::small());
        m_stateText->setStyle({.color = vde::Color::white(), .pixelScale = 2});
        m_stateText->setPosition(-8.8f, 5.9f, 0.2f);
        m_stateText->setWorldHeight(0.30f);

        m_eventText = addEntity<vde::TextEntity>();
        m_eventText->setText("Slash Event: ready");
        m_eventText->setFont(vde::BitmapFont::small());
        m_eventText->setStyle({.color = vde::Color::fromHex(0xfff1b8), .pixelScale = 2});
        m_eventText->setPosition(-8.8f, 5.45f, 0.2f);
        m_eventText->setWorldHeight(0.30f);
    }

    std::shared_ptr<vde::SpriteSheet> m_characterSheet;
    std::shared_ptr<PlayerEntity> m_player;
    std::shared_ptr<vde::TextEntity> m_stateText;
    std::shared_ptr<vde::TextEntity> m_eventText;
    std::vector<std::shared_ptr<PlatformEntity>> m_platforms;
    vde::Camera2D* m_sceneCamera2D = nullptr;
};

/**
 * @brief Game class for the sidescroller.
 */
class SidescrollerGame
    : public vde::examples::BaseExampleGame<SidescrollerInputHandler, SidescrollerScene> {};

/**
 * @brief Main entry point.
 */
// NOLINTNEXTLINE(bugprone-exception-escape)
int main(int argc, char** argv) {
    try {
        SidescrollerGame game;
        return vde::examples::runExample(game, "VDE 2D Sidescroller", 1280, 720, argc, argv);
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << '\n';
        return 1;
    } catch (...) {
        std::cerr << "Fatal error: unknown exception\n";
        return 1;
    }
}