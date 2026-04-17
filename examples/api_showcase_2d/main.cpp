/**
 * @file main.cpp
 * @brief 2D API Showcase — demonstrates all major 2D features of the VDE Game API.
 *
 * Features demonstrated:
 * - Camera2D with orthographic projection
 * - SpriteEntity: colors, anchors, scaling, positioning
 * - TextEntity: BitmapFont small/large, dynamic text, styles, anchoring
 * - Scrolling: parallax-style background layer with wrapping sprites
 * - Gamepad/joystick input: left stick movement, button actions
 * - Keyboard input: arrow keys, WASD, SPACE
 * - Physics: bouncing sprites in a walled arena section
 */

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "../ExampleBase.h"

using namespace vde;

// ============================================================================
// Constants
// ============================================================================

static constexpr float VIEW_W = 28.0f;
static constexpr float VIEW_H = 16.0f;

// Player area (left half)
static constexpr float PLAYER_AREA_LEFT = -VIEW_W * 0.5f + 0.5f;
static constexpr float PLAYER_AREA_RIGHT = -1.0f;
static constexpr float PLAYER_AREA_BOTTOM = -VIEW_H * 0.5f + 0.5f;
static constexpr float PLAYER_AREA_TOP = VIEW_H * 0.5f - 2.5f;

// Physics arena (right side)
static constexpr float ARENA_CENTER_X = 8.5f;
static constexpr float ARENA_CENTER_Y = -2.0f;
static constexpr float ARENA_W = 10.0f;
static constexpr float ARENA_H = 8.0f;
static constexpr float WALL_THICKNESS = 0.3f;

// Scroll layer (top strip)
static constexpr float SCROLL_Y = VIEW_H * 0.5f - 1.0f;
static constexpr float SCROLL_SPRITE_SIZE = 1.2f;
static constexpr float SCROLL_SPEED = 4.0f;
static constexpr int SCROLL_COUNT = 28;

// Player
static constexpr float PLAYER_SIZE = 1.0f;
static constexpr float PLAYER_SPEED = 8.0f;

// ============================================================================
// Input handler — keyboard + gamepad
// ============================================================================

class ShowcaseInputHandler : public vde::examples::BaseExampleInputHandler {
  public:
    void onKeyPress(int key) override {
        BaseExampleInputHandler::onKeyPress(key);
        if (key == KEY_LEFT || key == KEY_A)
            m_left = true;
        if (key == KEY_RIGHT || key == KEY_D)
            m_right = true;
        if (key == KEY_UP || key == KEY_W)
            m_up = true;
        if (key == KEY_DOWN || key == KEY_S)
            m_down = true;
        if (key == KEY_SPACE)
            m_spawnPressed = true;
        if (key == KEY_R)
            m_resetPressed = true;
    }

    void onKeyRelease(int key) override {
        BaseExampleInputHandler::onKeyRelease(key);
        if (key == KEY_LEFT || key == KEY_A)
            m_left = false;
        if (key == KEY_RIGHT || key == KEY_D)
            m_right = false;
        if (key == KEY_UP || key == KEY_W)
            m_up = false;
        if (key == KEY_DOWN || key == KEY_S)
            m_down = false;
    }

    void onGamepadButtonPress(int /*gamepadId*/, int button) override {
        if (button == GAMEPAD_BUTTON_A)
            m_spawnPressed = true;
        if (button == GAMEPAD_BUTTON_B)
            m_resetPressed = true;
        if (button == GAMEPAD_BUTTON_DPAD_LEFT)
            m_left = true;
        if (button == GAMEPAD_BUTTON_DPAD_RIGHT)
            m_right = true;
        if (button == GAMEPAD_BUTTON_DPAD_UP)
            m_up = true;
        if (button == GAMEPAD_BUTTON_DPAD_DOWN)
            m_down = true;
    }

    void onGamepadButtonRelease(int gamepadId, int button) override {
        BaseExampleInputHandler::onGamepadButtonRelease(gamepadId, button);
        if (button == GAMEPAD_BUTTON_DPAD_LEFT)
            m_left = false;
        if (button == GAMEPAD_BUTTON_DPAD_RIGHT)
            m_right = false;
        if (button == GAMEPAD_BUTTON_DPAD_UP)
            m_up = false;
        if (button == GAMEPAD_BUTTON_DPAD_DOWN)
            m_down = false;
    }

    void onGamepadAxis(int /*gamepadId*/, int axis, float value) override {
        if (axis == GAMEPAD_AXIS_LEFT_X)
            m_stickX = value;
        if (axis == GAMEPAD_AXIS_LEFT_Y)
            m_stickY = value;
    }

    // Movement direction from keyboard + joystick, normalized
    glm::vec2 getMoveDirection() const {
        glm::vec2 dir(0.0f);
        if (m_left)
            dir.x -= 1.0f;
        if (m_right)
            dir.x += 1.0f;
        if (m_up)
            dir.y += 1.0f;
        if (m_down)
            dir.y -= 1.0f;

        // Blend in joystick (Y inverted on gamepads)
        dir.x += m_stickX;
        dir.y -= m_stickY;

        float len = glm::length(dir);
        if (len > 1.0f)
            dir /= len;
        return dir;
    }

    bool consumeSpawn() {
        bool v = m_spawnPressed;
        m_spawnPressed = false;
        return v;
    }

    bool consumeReset() {
        bool v = m_resetPressed;
        m_resetPressed = false;
        return v;
    }

  private:
    bool m_left = false;
    bool m_right = false;
    bool m_up = false;
    bool m_down = false;
    bool m_spawnPressed = false;
    bool m_resetPressed = false;
    float m_stickX = 0.0f;
    float m_stickY = 0.0f;
};

// ============================================================================
// Helper: auto-size text to world-space height
// ============================================================================

static void sizeToFit(TextEntity& te, float worldHeight) {
    auto tex = te.getTexture();
    if (!tex || tex->getWidth() < 2)
        return;
    float aspect = static_cast<float>(tex->getWidth()) / static_cast<float>(tex->getHeight());
    te.setScale(worldHeight * aspect, worldHeight, 1.0f);
}

// ============================================================================
// Scene
// ============================================================================

class ShowcaseScene : public vde::examples::BaseExampleScene {
  public:
    ShowcaseScene() : BaseExampleScene(60.0f) {}

    void onEnter() override {
        printExampleHeader();
        setup2D(VIEW_W, VIEW_H, Color(0.06f, 0.06f, 0.12f, 1.0f));

        createTitle();
        createSpriteGallery();
        createScrollLayer();
        createPlayer();
        createPhysicsArena();
        createTextLabels();
    }

    void update(float deltaTime) override {
        BaseExampleScene::update(deltaTime);
        m_time += deltaTime;

        auto* input = dynamic_cast<ShowcaseInputHandler*>(getInputHandler());
        if (!input)
            return;

        updatePlayer(input, deltaTime);
        updateScrollLayer(deltaTime);
        updateDynamicText();
        updateSpriteGalleryAnimations();

        if (input->consumeSpawn())
            spawnPhysicsBox();
        if (input->consumeReset())
            resetPhysicsArena();
    }

  protected:
    std::string getExampleName() const override { return "2D API Showcase"; }

    std::vector<std::string> getFeatures() const override {
        return {"Camera2D with orthographic projection",
                "SpriteEntity: colors, anchors, scaling, animated hue cycling",
                "TextEntity: BitmapFont small/large, dynamic text, styles",
                "Scrolling sprite layer with wrap-around",
                "Gamepad/joystick + keyboard input for player movement",
                "Physics arena with bouncing dynamic sprites"};
    }

    std::vector<std::string> getExpectedVisuals() const override {
        return {"Title text at top: '2D API SHOWCASE'",
                "Scrolling colored squares moving left across the top",
                "Grid of colored sprites in the left area with animated hue cycling",
                "Player sprite (white) controllable with arrows/WASD/joystick",
                "Physics arena on the right with bouncing colored boxes",
                "Dynamic text labels showing time, FPS, and entity count"};
    }

    std::vector<std::string> getControls() const override {
        return {"Arrow keys / WASD / Left stick - Move player",
                "SPACE / Gamepad A              - Spawn physics box",
                "R / Gamepad B                  - Reset physics arena",
                "D-Pad                          - Also moves player"};
    }

  private:
    // ---- Title ----
    void createTitle() {
        m_title = addEntity<TextEntity>();
        m_title->setText("2D API SHOWCASE");
        m_title->setFont(BitmapFont::large());
        m_title->setStyle(
            {.color = Color(0.0f, 0.898f, 1.0f), .pixelScale = 3, .letterSpacing = 2});
        m_title->setAnchor(0.5f, 0.5f);
        m_title->setPosition(0.0f, VIEW_H * 0.5f - 0.5f, 0.1f);
        m_title->update(0.0f);
        sizeToFit(*m_title, 0.8f);
    }

    // ---- Sprite Gallery (left side, below scroll strip) ----
    void createSpriteGallery() {
        // Grid of colored sprites demonstrating different colors and animation
        const Color colors[] = {
            Color::red(),
            Color::green(),
            Color::fromHex(0x3498db),  // blue
            Color::yellow(),
            Color::fromHex(0xe74c3c),  // crimson
            Color::fromHex(0x9b59b6),  // purple
            Color::fromHex(0x1abc9c),  // teal
            Color::fromHex(0xf39c12),  // orange
            Color::fromHex(0xe91e63),  // pink
        };
        constexpr int cols = 3;
        constexpr int rows = 3;
        constexpr float spacing = 2.0f;
        float startX = PLAYER_AREA_LEFT + 1.5f;
        float startY = PLAYER_AREA_TOP - 1.5f;

        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                int idx = r * cols + c;
                auto sprite = addEntity<SpriteEntity>();
                sprite->setColor(colors[idx]);
                sprite->setScale(1.0f, 1.0f, 1.0f);
                sprite->setAnchor(0.5f, 0.5f);
                sprite->setPosition(startX + c * spacing, startY - r * spacing, 0.0f);
                m_gallerySprites.push_back(sprite);
                m_galleryBaseColors.push_back(colors[idx]);
            }
        }

        // Label for the gallery
        m_galleryLabel = addEntity<TextEntity>();
        m_galleryLabel->setText("SPRITE GALLERY");
        m_galleryLabel->setFont(BitmapFont::small());
        m_galleryLabel->setStyle({.color = Color::cyan(), .pixelScale = 1, .letterSpacing = 1});
        m_galleryLabel->setAnchor(0.0f, 0.5f);
        m_galleryLabel->setPosition(PLAYER_AREA_LEFT, PLAYER_AREA_TOP + 0.3f, 0.0f);
        m_galleryLabel->update(0.0f);
        sizeToFit(*m_galleryLabel, 0.35f);
    }

    // ---- Scrolling Layer (top strip below title) ----
    void createScrollLayer() {
        for (int i = 0; i < SCROLL_COUNT; ++i) {
            auto sprite = addEntity<SpriteEntity>();
            // Cycle through rainbow colors
            float hue = static_cast<float>(i) / static_cast<float>(SCROLL_COUNT);
            sprite->setColor(hsvToRgb(hue, 0.8f, 0.9f));
            sprite->setScale(SCROLL_SPRITE_SIZE, SCROLL_SPRITE_SIZE, 1.0f);
            sprite->setAnchor(0.5f, 0.5f);

            float x = -VIEW_W * 0.5f + i * SCROLL_SPRITE_SIZE * 1.5f;
            sprite->setPosition(x, SCROLL_Y, 0.0f);
            m_scrollSprites.push_back(sprite);
            m_scrollBaseX.push_back(x);
        }
    }

    // ---- Player Sprite ----
    void createPlayer() {
        m_player = addEntity<SpriteEntity>();
        m_player->setColor(Color::white());
        m_player->setScale(PLAYER_SIZE, PLAYER_SIZE, 1.0f);
        m_player->setAnchor(0.5f, 0.5f);
        m_playerX = (PLAYER_AREA_LEFT + PLAYER_AREA_RIGHT) * 0.5f;
        m_playerY = (PLAYER_AREA_BOTTOM + PLAYER_AREA_TOP) * 0.5f - 2.0f;
        m_player->setPosition(m_playerX, m_playerY, 0.05f);

        // Player label
        m_playerLabel = addEntity<TextEntity>();
        m_playerLabel->setText("PLAYER");
        m_playerLabel->setFont(BitmapFont::small());
        m_playerLabel->setStyle({.color = Color::white(), .pixelScale = 1, .letterSpacing = 1});
        m_playerLabel->setAnchor(0.5f, 0.0f);
        m_playerLabel->update(0.0f);
        sizeToFit(*m_playerLabel, 0.25f);
    }

    // ---- Physics Arena (right side) ----
    void createPhysicsArena() {
        // Enable physics
        PhysicsConfig cfg;
        cfg.gravity = {0.0f, -9.81f};
        enablePhysics(cfg);

        // Create arena walls using individual static boxes
        float halfW = ARENA_W * 0.5f;
        float halfH = ARENA_H * 0.5f;
        float halfThick = WALL_THICKNESS * 0.5f;
        Color wallColor(0.4f, 0.4f, 0.5f, 1.0f);

        // Floor
        auto floor = addEntity<PhysicsSpriteEntity>();
        floor->setColor(wallColor);
        floor->setScale(ARENA_W, WALL_THICKNESS, 1.0f);
        floor->createPhysicsBody(PhysicsBodyDef::staticBox({ARENA_CENTER_X, ARENA_CENTER_Y - halfH},
                                                           {halfW, halfThick}));

        // Ceiling
        auto ceiling = addEntity<PhysicsSpriteEntity>();
        ceiling->setColor(wallColor);
        ceiling->setScale(ARENA_W, WALL_THICKNESS, 1.0f);
        ceiling->createPhysicsBody(PhysicsBodyDef::staticBox(
            {ARENA_CENTER_X, ARENA_CENTER_Y + halfH}, {halfW, halfThick}));

        // Left wall
        auto leftWall = addEntity<PhysicsSpriteEntity>();
        leftWall->setColor(wallColor);
        leftWall->setScale(WALL_THICKNESS, ARENA_H, 1.0f);
        leftWall->createPhysicsBody(PhysicsBodyDef::staticBox(
            {ARENA_CENTER_X - halfW, ARENA_CENTER_Y}, {halfThick, halfH}));

        // Right wall
        auto rightWall = addEntity<PhysicsSpriteEntity>();
        rightWall->setColor(wallColor);
        rightWall->setScale(WALL_THICKNESS, ARENA_H, 1.0f);
        rightWall->createPhysicsBody(PhysicsBodyDef::staticBox(
            {ARENA_CENTER_X + halfW, ARENA_CENTER_Y}, {halfThick, halfH}));

        // Arena label
        m_arenaLabel = addEntity<TextEntity>();
        m_arenaLabel->setText("PHYSICS ARENA");
        m_arenaLabel->setFont(BitmapFont::small());
        m_arenaLabel->setStyle({.color = Color::cyan(), .pixelScale = 1, .letterSpacing = 1});
        m_arenaLabel->setAnchor(0.5f, 0.5f);
        m_arenaLabel->setPosition(ARENA_CENTER_X, ARENA_CENTER_Y + halfH + 0.5f, 0.0f);
        m_arenaLabel->update(0.0f);
        sizeToFit(*m_arenaLabel, 0.35f);

        // Spawn a few initial boxes
        for (int i = 0; i < 5; ++i) {
            spawnPhysicsBox();
        }
    }

    // ---- Text Labels (info panel, bottom-right) ----
    void createTextLabels() {
        float infoX = ARENA_CENTER_X;
        float infoY = ARENA_CENTER_Y - ARENA_H * 0.5f - 1.0f;

        // Time counter
        m_timeText = addEntity<TextEntity>();
        m_timeText->setText("TIME: 0.00s");
        m_timeText->setFont(BitmapFont::small());
        m_timeText->setStyle({.color = Color::green(), .pixelScale = 1, .letterSpacing = 1});
        m_timeText->setAnchor(0.5f, 0.5f);
        m_timeText->setPosition(infoX, infoY, 0.0f);
        m_timeText->update(0.0f);
        sizeToFit(*m_timeText, 0.30f);

        // FPS
        m_fpsText = addEntity<TextEntity>();
        m_fpsText->setText("FPS: ---");
        m_fpsText->setFont(BitmapFont::small());
        m_fpsText->setStyle({.color = Color::yellow(), .pixelScale = 1, .letterSpacing = 1});
        m_fpsText->setAnchor(0.5f, 0.5f);
        m_fpsText->setPosition(infoX, infoY - 0.5f, 0.0f);
        m_fpsText->update(0.0f);
        sizeToFit(*m_fpsText, 0.30f);

        // Entity count
        m_entityCountText = addEntity<TextEntity>();
        m_entityCountText->setText("ENTITIES: 0");
        m_entityCountText->setFont(BitmapFont::small());
        m_entityCountText->setStyle(
            {.color = Color(0.8f, 0.6f, 1.0f), .pixelScale = 1, .letterSpacing = 1});
        m_entityCountText->setAnchor(0.5f, 0.5f);
        m_entityCountText->setPosition(infoX, infoY - 1.0f, 0.0f);
        m_entityCountText->update(0.0f);
        sizeToFit(*m_entityCountText, 0.30f);

        // Input hint (bottom center)
        m_inputHint = addEntity<TextEntity>();
        m_inputHint->setText("ARROWS/WASD/STICK: MOVE   SPACE/A: SPAWN   R/B: RESET");
        m_inputHint->setFont(BitmapFont::small());
        m_inputHint->setStyle(
            {.color = Color(0.5f, 0.5f, 0.6f), .pixelScale = 1, .letterSpacing = 1});
        m_inputHint->setAnchor(0.5f, 0.5f);
        m_inputHint->setPosition(0.0f, -VIEW_H * 0.5f + 0.3f, 0.0f);
        m_inputHint->update(0.0f);
        sizeToFit(*m_inputHint, 0.25f);
    }

    // ---- Update: Player Movement ----
    void updatePlayer(ShowcaseInputHandler* input, float deltaTime) {
        glm::vec2 dir = input->getMoveDirection();
        m_playerX += dir.x * PLAYER_SPEED * deltaTime;
        m_playerY += dir.y * PLAYER_SPEED * deltaTime;

        // Clamp to player area
        m_playerX = std::clamp(m_playerX, PLAYER_AREA_LEFT + PLAYER_SIZE * 0.5f,
                               PLAYER_AREA_RIGHT - PLAYER_SIZE * 0.5f);
        m_playerY = std::clamp(m_playerY, PLAYER_AREA_BOTTOM + PLAYER_SIZE * 0.5f,
                               PLAYER_AREA_TOP - PLAYER_SIZE * 0.5f);

        m_player->setPosition(m_playerX, m_playerY, 0.05f);

        // Player label follows above the sprite
        m_playerLabel->setPosition(m_playerX, m_playerY + PLAYER_SIZE * 0.8f, 0.05f);

        // Gentle pulse on player
        float pulse = 0.85f + 0.15f * std::sin(m_time * 3.0f);
        m_player->setScale(PLAYER_SIZE * pulse, PLAYER_SIZE * pulse, 1.0f);
    }

    // ---- Update: Scrolling Layer ----
    void updateScrollLayer(float deltaTime) {
        m_scrollOffset -= SCROLL_SPEED * deltaTime;

        // Total width of all scroll sprites
        float totalWidth = static_cast<float>(SCROLL_COUNT) * SCROLL_SPRITE_SIZE * 1.5f;

        // Wrap offset
        if (m_scrollOffset < -totalWidth) {
            m_scrollOffset += totalWidth;
        }

        for (size_t i = 0; i < m_scrollSprites.size(); ++i) {
            float x = m_scrollBaseX[i] + m_scrollOffset;

            // Wrap individual sprites
            while (x < -VIEW_W * 0.5f - SCROLL_SPRITE_SIZE) {
                x += totalWidth;
            }
            while (x > VIEW_W * 0.5f + totalWidth) {
                x -= totalWidth;
            }

            m_scrollSprites[i]->setPosition(x, SCROLL_Y, 0.0f);

            // Gentle vertical bob
            float bob = std::sin(m_time * 2.0f + static_cast<float>(i) * 0.5f) * 0.15f;
            m_scrollSprites[i]->setPosition(x, SCROLL_Y + bob, 0.0f);
        }
    }

    // ---- Update: Dynamic Text ----
    void updateDynamicText() {
        // Time
        char buf[64];
        std::snprintf(buf, sizeof(buf), "TIME: %.2fs", m_time);
        m_timeText->setText(buf);
        m_timeText->update(0.0f);
        sizeToFit(*m_timeText, 0.30f);

        // FPS
        if (getGame()) {
            std::snprintf(buf, sizeof(buf), "FPS: %.0f", getGame()->getFPS());
            m_fpsText->setText(buf);
            m_fpsText->update(0.0f);
            sizeToFit(*m_fpsText, 0.30f);
        }

        // Entity count
        std::snprintf(buf, sizeof(buf), "ENTITIES: %zu", getEntities().size());
        m_entityCountText->setText(buf);
        m_entityCountText->update(0.0f);
        sizeToFit(*m_entityCountText, 0.30f);
    }

    // ---- Update: Gallery Animations ----
    void updateSpriteGalleryAnimations() {
        for (size_t i = 0; i < m_gallerySprites.size(); ++i) {
            // Pulsing scale
            float phase = m_time * 1.5f + static_cast<float>(i) * 0.7f;
            float scale = 0.8f + 0.3f * std::sin(phase);
            m_gallerySprites[i]->setScale(scale, scale, 1.0f);

            // Gentle rotation (using Z rotation encoded in the roll)
            float rot = std::sin(m_time * 0.8f + static_cast<float>(i) * 1.1f) * 15.0f;
            m_gallerySprites[i]->setRotation(rot, 0.0f, 0.0f);
        }
    }

    // ---- Spawn Physics Box ----
    void spawnPhysicsBox() {
        if (m_physicsBoxIds.size() >= 30)
            return;  // Limit total physics objects

        float spawnX = ARENA_CENTER_X + (static_cast<float>(m_spawnCounter % 5) - 2.0f) * 0.8f;
        float spawnY = ARENA_CENTER_Y + ARENA_H * 0.3f;

        // Cycle colors
        Color boxColors[] = {Color::red(),    Color::green(),           Color::fromHex(0x3498db),
                             Color::yellow(), Color::fromHex(0xe74c3c), Color::fromHex(0x9b59b6)};
        Color boxColor = boxColors[m_spawnCounter % 6];

        float halfExt = 0.25f + (m_spawnCounter % 3) * 0.1f;

        auto box = addEntity<PhysicsSpriteEntity>();
        box->setColor(boxColor);
        box->setScale(halfExt * 2.0f, halfExt * 2.0f, 1.0f);

        auto def = PhysicsBodyDef::dynamicBox({spawnX, spawnY}, {halfExt, halfExt});
        def.restitution = 0.5f;
        box->createPhysicsBody(def);

        m_physicsBoxIds.push_back(box->getId());
        m_spawnCounter++;
    }

    // ---- Reset Physics Arena ----
    void resetPhysicsArena() {
        for (auto id : m_physicsBoxIds) {
            removeEntity(id);
        }
        m_physicsBoxIds.clear();

        // Re-spawn initial boxes
        for (int i = 0; i < 5; ++i) {
            spawnPhysicsBox();
        }
    }

    // ---- HSV to RGB ----
    static Color hsvToRgb(float h, float s, float v) {
        float c = v * s;
        float x = c * (1.0f - std::fabs(std::fmod(h * 6.0f, 2.0f) - 1.0f));
        float m = v - c;
        float r = 0.0f, g = 0.0f, b = 0.0f;
        int sector = static_cast<int>(h * 6.0f) % 6;
        switch (sector) {
        case 0:
            r = c;
            g = x;
            break;
        case 1:
            r = x;
            g = c;
            break;
        case 2:
            g = c;
            b = x;
            break;
        case 3:
            g = x;
            b = c;
            break;
        case 4:
            r = x;
            b = c;
            break;
        case 5:
            r = c;
            b = x;
            break;
        }
        return Color(r + m, g + m, b + m, 1.0f);
    }

    // ---- Members ----
    float m_time = 0.0f;

    // Title
    std::shared_ptr<TextEntity> m_title;

    // Sprite gallery
    std::vector<std::shared_ptr<SpriteEntity>> m_gallerySprites;
    std::vector<Color> m_galleryBaseColors;
    std::shared_ptr<TextEntity> m_galleryLabel;

    // Scroll layer
    std::vector<std::shared_ptr<SpriteEntity>> m_scrollSprites;
    std::vector<float> m_scrollBaseX;
    float m_scrollOffset = 0.0f;

    // Player
    std::shared_ptr<SpriteEntity> m_player;
    std::shared_ptr<TextEntity> m_playerLabel;
    float m_playerX = 0.0f;
    float m_playerY = 0.0f;

    // Physics arena
    std::vector<EntityId> m_physicsBoxIds;
    int m_spawnCounter = 0;
    std::shared_ptr<TextEntity> m_arenaLabel;

    // Dynamic text
    std::shared_ptr<TextEntity> m_timeText;
    std::shared_ptr<TextEntity> m_fpsText;
    std::shared_ptr<TextEntity> m_entityCountText;
    std::shared_ptr<TextEntity> m_inputHint;
};

// ============================================================================
// Game
// ============================================================================

class ShowcaseGame : public vde::examples::BaseExampleGame<ShowcaseInputHandler, ShowcaseScene> {};

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    ShowcaseGame game;
    return vde::examples::runExample(game, "VDE 2D API Showcase", 1280, 720, argc, argv);
}
