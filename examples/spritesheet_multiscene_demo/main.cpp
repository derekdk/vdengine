/**
 * @file main.cpp
 * @brief SpriteSheet Multi-Scene Demo — cross-scene resource sharing.
 *
 * Demonstrates:
 * - SpriteSheet as a shared Resource via ResourceManager
 * - Two scenes sharing the same SpriteSheet and atlas texture
 * - Scene 1: CPU-controlled 2D entities moving around a playfield
 * - Scene 2: Character detail screen cycling through characters with stats
 * - Scene transitions via setActiveScene()
 */

#include <vde/Texture.h>
#include <vde/api/GameAPI.h>
#include <vde/api/SpriteSheet.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "../ExampleBase.h"

using namespace vde;

// ============================================================================
// Character data shared between scenes
// ============================================================================

struct CharacterInfo {
    std::string name;
    std::string spriteName;  // Key into the SpriteSheet
    int hp = 10;
    int attack = 3;
    int defense = 2;
    int speed = 4;
    Color tint = Color::white();
};

// ============================================================================
// Pixel-art helpers (reused from spritesheet_demo, simplified)
// ============================================================================

struct RGBA {
    uint8_t r, g, b, a;
};

static void putPixel(std::vector<uint8_t>& buf, uint32_t stride, uint32_t x, uint32_t y, RGBA c) {
    size_t off = (static_cast<size_t>(y) * stride + x) * 4;
    buf[off + 0] = c.r;
    buf[off + 1] = c.g;
    buf[off + 2] = c.b;
    buf[off + 3] = c.a;
}

static void fillRect(std::vector<uint8_t>& buf, uint32_t stride, uint32_t x0, uint32_t y0,
                     uint32_t w, uint32_t h, RGBA c) {
    for (uint32_t y = y0; y < y0 + h; ++y)
        for (uint32_t x = x0; x < x0 + w; ++x)
            putPixel(buf, stride, x, y, c);
}

/// Draw a 16x16 character facing right — asymmetric body with eye, nose, feet, tail.
static void drawCharacter(std::vector<uint8_t>& buf, uint32_t stride, uint32_t ox, uint32_t oy,
                          RGBA body, RGBA eye, RGBA bg) {
    fillRect(buf, stride, ox, oy, 16, 16, bg);
    // Body block
    fillRect(buf, stride, ox + 1, oy + 3, 10, 10, body);
    // Pointed nose
    for (int r = 0; r < 6; ++r) {
        int extra = (r < 3) ? r + 1 : (5 - r) + 1;
        for (int e = 0; e < extra; ++e)
            putPixel(buf, stride, ox + 11 + static_cast<uint32_t>(e),
                     oy + 5 + static_cast<uint32_t>(r), body);
    }
    // Eye
    fillRect(buf, stride, ox + 3, oy + 5, 2, 2, eye);
    // Feet
    fillRect(buf, stride, ox + 2, oy + 13, 2, 2, body);
    fillRect(buf, stride, ox + 6, oy + 13, 2, 2, body);
    // Tail
    fillRect(buf, stride, ox + 0, oy + 7, 1, 3, body);
}

/// Draw a 16x16 star icon.
static void drawStar(std::vector<uint8_t>& buf, uint32_t stride, uint32_t ox, uint32_t oy, RGBA fg,
                     RGBA bg) {
    fillRect(buf, stride, ox, oy, 16, 16, bg);
    // clang-format off
    static const uint8_t star[16][16] = {
        {0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,1,1,1,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,1,1,1,0,0,0,0,0,0,0},
        {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0},
        {0,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0},
        {0,0,1,1,1,1,1,1,1,1,1,1,1,0,0,0},
        {0,0,0,1,1,1,1,1,1,1,1,1,0,0,0,0},
        {0,0,0,1,1,1,1,1,1,1,1,1,0,0,0,0},
        {0,0,1,1,1,1,0,0,0,1,1,1,1,0,0,0},
        {0,1,1,1,1,0,0,0,0,0,1,1,1,1,0,0},
        {1,1,1,1,0,0,0,0,0,0,0,1,1,1,0,0},
        {1,1,1,0,0,0,0,0,0,0,0,0,1,1,0,0},
        {1,1,0,0,0,0,0,0,0,0,0,0,0,1,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    };
    // clang-format on
    for (int r = 0; r < 16; ++r)
        for (int c = 0; c < 16; ++c)
            if (star[r][c] == 1)
                putPixel(buf, stride, ox + static_cast<uint32_t>(c), oy + static_cast<uint32_t>(r),
                         fg);
}

// ============================================================================
// Text sizing helper — call after update(0) to match world height
// ============================================================================

static void sizeToFit(TextEntity& te, float worldHeight) {
    auto tex = te.getTexture();
    if (!tex || tex->getWidth() < 2)
        return;
    float aspect = static_cast<float>(tex->getWidth()) / static_cast<float>(tex->getHeight());
    te.setScale(worldHeight * aspect, worldHeight, 1.0f);
}

// ============================================================================
// Input handler
// ============================================================================

class MultiSheetInputHandler : public vde::examples::BaseExampleInputHandler {
  public:
    void onKeyPress(int key) override {
        BaseExampleInputHandler::onKeyPress(key);
        if (key == KEY_1)
            m_sceneSwitch = 1;
        if (key == KEY_2)
            m_sceneSwitch = 2;
        if (key == KEY_LEFT)
            m_leftPressed = true;
        if (key == KEY_RIGHT)
            m_rightPressed = true;
    }

    int consumeSceneSwitch() {
        int v = m_sceneSwitch;
        m_sceneSwitch = 0;
        return v;
    }
    bool consumeLeft() {
        bool v = m_leftPressed;
        m_leftPressed = false;
        return v;
    }
    bool consumeRight() {
        bool v = m_rightPressed;
        m_rightPressed = false;
        return v;
    }

  private:
    int m_sceneSwitch = 0;
    bool m_leftPressed = false;
    bool m_rightPressed = false;
};

// ============================================================================
// Shared character roster (populated by the Game, read by both scenes)
// ============================================================================

static const std::vector<CharacterInfo>& getCharacterRoster() {
    static const std::vector<CharacterInfo> roster = {
        {"Red", "char_red", 12, 5, 3, 4, Color::fromHex(0xFF4444)},
        {"Blue", "char_blue", 10, 3, 6, 3, Color::fromHex(0x4488FF)},
        {"Green", "char_green", 14, 4, 4, 5, Color::fromHex(0x44CC44)},
        {"Gold", "char_gold", 8, 7, 2, 6, Color::fromHex(0xFFCC22)},
        {"Purple", "char_purple", 11, 4, 5, 3, Color::fromHex(0xBB66FF)},
    };
    return roster;
}

// ============================================================================
// Scene 1: Playfield — CPU-controlled characters moving around
// ============================================================================

class PlayfieldScene : public vde::examples::BaseExampleScene {
  public:
    PlayfieldScene() : BaseExampleScene() {}

    void onEnter() override {
        printExampleHeader();
        setup2D(16.0f, 10.0f, Color(0.08f, 0.08f, 0.12f, 1.0f));

        // Retrieve the shared SpriteSheet from ResourceManager
        auto& rm = getGame()->getResourceManager();
        auto sheet = rm.get<SpriteSheet>("shared_spritesheet");
        if (!sheet) {
            std::cerr << "ERROR: shared_spritesheet not found in ResourceManager!\n";
            return;
        }
        addResource<SpriteSheet>(sheet);

        auto texture = sheet->getTexture();
        const auto& roster = getCharacterRoster();

        // Playfield bounds (inner area where characters move)
        constexpr float kFieldHalfW = 7.0f;
        constexpr float kFieldHalfH = 4.0f;

        // Draw a border rectangle using sprites
        {
            auto border = addEntity<SpriteEntity>();
            border->setColor(Color(0.2f, 0.2f, 0.3f, 1.0f));
            border->setPosition(0.0f, 0.0f, -0.1f);
            border->setScale(kFieldHalfW * 2.0f + 0.4f, kFieldHalfH * 2.0f + 0.4f, 1.0f);
            border->setAnchor(0.5f, 0.5f);
        }
        {
            auto inner = addEntity<SpriteEntity>();
            inner->setColor(Color(0.1f, 0.1f, 0.16f, 1.0f));
            inner->setPosition(0.0f, 0.0f, -0.05f);
            inner->setScale(kFieldHalfW * 2.0f, kFieldHalfH * 2.0f, 1.0f);
            inner->setAnchor(0.5f, 0.5f);
        }

        // Create character sprites
        for (size_t i = 0; i < roster.size(); ++i) {
            const auto& info = roster[i];
            auto uv = sheet->getUVRect(info.spriteName);

            auto sprite = addEntity<SpriteEntity>();
            sprite->setTexture(texture);
            sprite->setUVRect(uv.u, uv.v, uv.width, uv.height);
            sprite->setScale(1.2f, 1.2f, 1.0f);
            sprite->setAnchor(0.5f, 0.5f);

            // Spread initial positions
            float angle = static_cast<float>(i) * 1.2566f;  // 2PI/5
            float startX = std::cos(angle) * 3.0f;
            float startY = std::sin(angle) * 2.0f;
            sprite->setPosition(startX, startY, 0.0f);

            CharacterState state;
            state.entity = sprite;
            state.x = startX;
            state.y = startY;
            // Give each character a different initial direction
            float dir = angle + 0.5f;
            float spd = 1.0f + static_cast<float>(info.speed) * 0.3f;
            state.vx = std::cos(dir) * spd;
            state.vy = std::sin(dir) * spd;
            state.facingRight = state.vx >= 0.0f;
            m_characters.push_back(state);
        }

        // Create name labels above characters
        for (size_t i = 0; i < roster.size(); ++i) {
            auto label = addEntity<TextEntity>();
            label->setText(roster[i].name);
            label->setFont(BitmapFont::small());
            label->setStyle({.color = roster[i].tint, .pixelScale = 1});
            label->setAnchor(0.5f, 0.0f);
            label->update(0.0f);
            sizeToFit(*label, 0.30f);
            m_nameLabels.push_back(label);
        }

        // Title text
        auto title = addEntity<TextEntity>();
        title->setText("PLAYFIELD  [1] Here  [2] Details");
        title->setFont(BitmapFont::small());
        title->setStyle({.color = Color::white(), .pixelScale = 1});
        title->setPosition(0.0f, 4.5f, 0.0f);
        title->setAnchor(0.5f, 0.5f);
        title->update(0.0f);
        sizeToFit(*title, 0.30f);
    }

    void update(float dt) override {
        BaseExampleScene::update(dt);

        constexpr float kFieldHalfW = 7.0f;
        constexpr float kFieldHalfH = 4.0f;
        constexpr float kCharHalf = 0.6f;

        for (size_t i = 0; i < m_characters.size(); ++i) {
            auto& ch = m_characters[i];

            // Move
            ch.x += ch.vx * dt;
            ch.y += ch.vy * dt;

            // Bounce off walls
            float minX = -kFieldHalfW + kCharHalf;
            float maxX = kFieldHalfW - kCharHalf;
            float minY = -kFieldHalfH + kCharHalf;
            float maxY = kFieldHalfH - kCharHalf;

            if (ch.x < minX) {
                ch.x = minX;
                ch.vx = std::abs(ch.vx);
            }
            if (ch.x > maxX) {
                ch.x = maxX;
                ch.vx = -std::abs(ch.vx);
            }
            if (ch.y < minY) {
                ch.y = minY;
                ch.vy = std::abs(ch.vy);
            }
            if (ch.y > maxY) {
                ch.y = maxY;
                ch.vy = -std::abs(ch.vy);
            }

            // Flip sprite based on horizontal direction
            bool nowRight = ch.vx >= 0.0f;
            if (nowRight != ch.facingRight) {
                ch.entity->setFlipX(!nowRight);
                ch.facingRight = nowRight;
            }

            ch.entity->setPosition(ch.x, ch.y, 0.0f);

            // Update name label position (above character)
            if (i < m_nameLabels.size()) {
                m_nameLabels[i]->setPosition(ch.x, ch.y + 0.85f, 0.0f);
            }
        }
    }

  protected:
    std::string getExampleName() const override { return "SpriteSheet Multi-Scene"; }

    std::vector<std::string> getFeatures() const override {
        return {
            "Shared SpriteSheet resource across two scenes via ResourceManager",
            "CPU-controlled characters bouncing around a playfield",
            "Character sprites auto-flip based on movement direction",
            "Press 1/2 to switch between playfield and detail scenes",
        };
    }

    std::vector<std::string> getExpectedVisuals() const override {
        return {
            "Dark playfield with bordered arena",
            "Five colored characters moving and bouncing off walls",
            "Name labels floating above each character",
        };
    }

    std::vector<std::string> getControls() const override {
        return {
            "1     - Switch to playfield scene (this scene)",
            "2     - Switch to character detail scene",
        };
    }

  private:
    struct CharacterState {
        std::shared_ptr<SpriteEntity> entity;
        float x = 0.0f;
        float y = 0.0f;
        float vx = 1.0f;
        float vy = 0.5f;
        bool facingRight = true;
    };

    std::vector<CharacterState> m_characters;
    std::vector<std::shared_ptr<TextEntity>> m_nameLabels;
};

// ============================================================================
// Scene 2: Character Detail — cycles through characters showing stats
// ============================================================================

class DetailScene : public vde::examples::BaseExampleScene {
  public:
    DetailScene() : BaseExampleScene() {}

    void onEnter() override {
        setup2D(16.0f, 10.0f, Color(0.05f, 0.06f, 0.1f, 1.0f));

        // Retrieve the shared SpriteSheet from ResourceManager
        auto& rm = getGame()->getResourceManager();
        m_sheet = rm.get<SpriteSheet>("shared_spritesheet");
        if (!m_sheet) {
            std::cerr << "ERROR: shared_spritesheet not found in ResourceManager!\n";
            return;
        }
        addResource<SpriteSheet>(m_sheet);

        auto texture = m_sheet->getTexture();
        const auto& roster = getCharacterRoster();

        // Background panels FIRST (draw-order: back to front)
        auto charPanel = addEntity<SpriteEntity>();
        charPanel->setColor(Color(0.12f, 0.12f, 0.2f, 1.0f));
        charPanel->setPosition(-4.0f, 0.0f, -0.1f);
        charPanel->setScale(5.5f, 5.5f, 1.0f);
        charPanel->setAnchor(0.5f, 0.5f);

        auto statsPanel = addEntity<SpriteEntity>();
        statsPanel->setColor(Color(0.12f, 0.12f, 0.2f, 1.0f));
        statsPanel->setPosition(3.5f, 0.0f, -0.1f);
        statsPanel->setScale(7.0f, 5.5f, 1.0f);
        statsPanel->setAnchor(0.5f, 0.5f);

        // Character sprite (after panels so it draws on top)
        auto uv = m_sheet->getUVRect(roster[0].spriteName);
        m_characterSprite = addEntity<SpriteEntity>();
        m_characterSprite->setTexture(texture);
        m_characterSprite->setUVRect(uv.u, uv.v, uv.width, uv.height);
        m_characterSprite->setScale(4.0f, 4.0f, 1.0f);
        m_characterSprite->setPosition(-4.0f, 0.0f, 0.0f);
        m_characterSprite->setAnchor(0.5f, 0.5f);

        // Title — character name
        m_titleLabel = addEntity<TextEntity>();
        m_titleLabel->setFont(BitmapFont::large());
        m_titleLabel->setStyle({.color = Color::white(), .pixelScale = 2});
        m_titleLabel->setPosition(0.0f, 4.2f, 0.0f);
        m_titleLabel->setAnchor(0.5f, 0.5f);

        // Stat layout
        constexpr float kStatX = 1.5f;
        constexpr float kStatStartY = 1.8f;
        constexpr float kStatSpacing = 1.2f;

        // Star icons for each stat row
        auto starUV = m_sheet->getUVRect("star");
        for (int i = 0; i < 4; ++i) {
            auto star = addEntity<SpriteEntity>();
            star->setTexture(texture);
            star->setUVRect(starUV.u, starUV.v, starUV.width, starUV.height);
            star->setScale(0.6f, 0.6f, 1.0f);
            star->setPosition(kStatX - 0.3f, kStatStartY - static_cast<float>(i) * kStatSpacing,
                              0.0f);
            star->setAnchor(0.5f, 0.5f);
        }

        // Stat name labels
        const char* statNames[] = {"HP", "ATK", "DEF", "SPD"};
        for (int i = 0; i < 4; ++i) {
            auto nameLabel = addEntity<TextEntity>();
            nameLabel->setText(statNames[i]);
            nameLabel->setFont(BitmapFont::small());
            nameLabel->setStyle({.color = Color(0.7f, 0.7f, 0.8f, 1.0f), .pixelScale = 1});
            nameLabel->setPosition(kStatX + 0.2f,
                                   kStatStartY - static_cast<float>(i) * kStatSpacing, 0.0f);
            nameLabel->setAnchor(0.0f, 0.5f);
            nameLabel->update(0.0f);
            sizeToFit(*nameLabel, 0.35f);
        }

        // Stat value labels
        for (int i = 0; i < 4; ++i) {
            auto valLabel = addEntity<TextEntity>();
            valLabel->setFont(BitmapFont::small());
            valLabel->setStyle({.color = Color::white(), .pixelScale = 1});
            valLabel->setPosition(kStatX + 2.8f, kStatStartY - static_cast<float>(i) * kStatSpacing,
                                  0.0f);
            valLabel->setAnchor(0.0f, 0.5f);
            m_statValues.push_back(valLabel);
        }

        // Navigation hint at bottom
        auto nav = addEntity<TextEntity>();
        nav->setText("< LEFT  RIGHT >  [1] Playfield  [2] Here");
        nav->setFont(BitmapFont::small());
        nav->setStyle({.color = Color(0.5f, 0.5f, 0.6f, 1.0f), .pixelScale = 1});
        nav->setPosition(0.0f, -4.2f, 0.0f);
        nav->setAnchor(0.5f, 0.5f);
        nav->update(0.0f);
        sizeToFit(*nav, 0.25f);

        // Set initial character
        showCharacter(0);
    }

    void update(float dt) override {
        BaseExampleScene::update(dt);

        auto* input = dynamic_cast<MultiSheetInputHandler*>(getInputHandler());
        if (!input)
            return;

        const auto& roster = getCharacterRoster();

        if (input->consumeLeft()) {
            int idx = (m_currentIndex - 1 + static_cast<int>(roster.size())) %
                      static_cast<int>(roster.size());
            showCharacter(idx);
            m_autoCycleTimer = 0.0f;
        }
        if (input->consumeRight()) {
            int idx = (m_currentIndex + 1) % static_cast<int>(roster.size());
            showCharacter(idx);
            m_autoCycleTimer = 0.0f;
        }

        // Auto-cycle every few seconds
        m_autoCycleTimer += dt;
        if (m_autoCycleTimer >= 3.0f) {
            m_autoCycleTimer = 0.0f;
            int idx = (m_currentIndex + 1) % static_cast<int>(roster.size());
            showCharacter(idx);
        }
    }

  protected:
    std::string getExampleName() const override { return "Character Details"; }

    std::vector<std::string> getFeatures() const override {
        return {
            "Same SpriteSheet shared from Scene 1 via ResourceManager",
            "Large character sprite display with stats panel",
            "Auto-cycles through characters every 3 seconds",
            "LEFT/RIGHT arrows to manually navigate characters",
        };
    }

    std::vector<std::string> getExpectedVisuals() const override {
        return {
            "Left: large character sprite on dark panel",
            "Right: stat labels (HP, ATK, DEF, SPD) with values",
            "Star icons next to each stat row",
            "Character name in title, auto-cycling every 3s",
        };
    }

    std::vector<std::string> getControls() const override {
        return {
            "LEFT  - Previous character",
            "RIGHT - Next character",
            "1     - Switch to playfield scene",
            "2     - Switch to detail scene (this scene)",
        };
    }

  private:
    void showCharacter(int index) {
        const auto& roster = getCharacterRoster();
        if (index < 0 || index >= static_cast<int>(roster.size()))
            return;

        m_currentIndex = index;
        const auto& info = roster[static_cast<size_t>(index)];

        // Update sprite
        if (m_sheet) {
            auto uv = m_sheet->getUVRect(info.spriteName);
            m_characterSprite->setUVRect(uv.u, uv.v, uv.width, uv.height);
        }

        // Update title
        m_titleLabel->setText(info.name);
        m_titleLabel->setStyle({.color = info.tint, .pixelScale = 2});
        m_titleLabel->update(0.0f);
        sizeToFit(*m_titleLabel, 0.55f);

        // Update stat values
        if (m_statValues.size() >= 4) {
            m_statValues[0]->setText(std::to_string(info.hp));
            m_statValues[1]->setText(std::to_string(info.attack));
            m_statValues[2]->setText(std::to_string(info.defense));
            m_statValues[3]->setText(std::to_string(info.speed));
            for (auto& sv : m_statValues) {
                sv->update(0.0f);
                sizeToFit(*sv, 0.35f);
            }
        }
    }

    SpriteSheet::Ref m_sheet;
    std::shared_ptr<SpriteEntity> m_characterSprite;
    std::shared_ptr<TextEntity> m_titleLabel;
    std::vector<std::shared_ptr<TextEntity>> m_statValues;
    int m_currentIndex = 0;
    float m_autoCycleTimer = 0.0f;
};

// ============================================================================
// Game — creates shared SpriteSheet, manages both scenes
// ============================================================================

class SpriteSheetMultiSceneDemo : public vde::Game {
  public:
    void onStart() override {
        m_inputHandler = std::make_unique<MultiSheetInputHandler>();
        setInputHandler(m_inputHandler.get());

        // Build atlas FIRST so both scenes find it in ResourceManager.
        buildSharedAtlas();

        // Create scenes — setActiveScene calls onEnter() which needs the sheet.
        m_playfield = new PlayfieldScene();
        m_detail = new DetailScene();
        addScene("main", m_playfield);
        addScene("detail", m_detail);
        setActiveScene("main");
    }

    void onUpdate(float /*deltaTime*/) override {
        auto* input = m_inputHandler.get();
        if (!input)
            return;

        int sw = input->consumeSceneSwitch();
        if (sw == 1)
            setActiveScene("main");
        if (sw == 2)
            setActiveScene("detail");
    }

    void onShutdown() override {
        if ((m_playfield && m_playfield->didTestFail()) || (m_detail && m_detail->didTestFail()))
            m_exitCode = 1;
    }

    int getExitCode() const {
        if (m_exitCode != 0)
            return m_exitCode;
        return Game::getExitCode();
    }

  private:
    void buildSharedAtlas() {
        // Atlas layout: 5 character sprites (16x16 each) in a row, + 1 star icon
        // Total: 96 x 16 (6 cells of 16x16)
        constexpr int kCellSize = 16;
        constexpr int kCols = 6;
        constexpr uint32_t kTexW = kCellSize * kCols;
        constexpr uint32_t kTexH = kCellSize;
        constexpr RGBA kBg{0, 0, 0, 0};
        constexpr RGBA kEye{255, 255, 255, 255};

        std::vector<uint8_t> pixels(kTexW * kTexH * 4, 0);
        fillRect(pixels, kTexW, 0, 0, kTexW, kTexH, kBg);

        // Character colors matching the roster
        struct CharColor {
            RGBA body;
        };
        const CharColor colors[] = {
            {{230, 57, 70, 255}},    // Red
            {{68, 136, 255, 255}},   // Blue
            {{68, 204, 68, 255}},    // Green
            {{255, 204, 34, 255}},   // Gold
            {{187, 102, 255, 255}},  // Purple
        };

        for (int i = 0; i < 5; ++i) {
            drawCharacter(pixels, kTexW, static_cast<uint32_t>(i) * kCellSize, 0, colors[i].body,
                          kEye, kBg);
        }

        // Star icon at column 5
        drawStar(pixels, kTexW, 5 * kCellSize, 0, {255, 220, 50, 255}, kBg);

        // Create texture and upload to GPU
        auto atlasTex = std::make_shared<Texture>();
        atlasTex->loadFromData(pixels.data(), kTexW, kTexH);
        if (auto* ctx = getVulkanContext()) {
            atlasTex->uploadToGPU(ctx);
        }

        // Create the SpriteSheet with named regions
        auto sheet = SpriteSheet::create(atlasTex);
        const auto& roster = getCharacterRoster();
        for (size_t i = 0; i < roster.size(); ++i) {
            sheet->addSprite(roster[i].spriteName, static_cast<int>(i) * kCellSize, 0, kCellSize,
                             kCellSize);
        }
        sheet->addSprite("star", 5 * kCellSize, 0, kCellSize, kCellSize);

        // Cache in ResourceManager for cross-scene sharing.
        // Keep a strong reference so the weak_ptr cache doesn't expire.
        m_sharedSheet = sheet;
        getResourceManager().add<SpriteSheet>("shared_spritesheet", sheet);
    }

    SpriteSheet::Ref m_sharedSheet;
    std::unique_ptr<MultiSheetInputHandler> m_inputHandler;
    PlayfieldScene* m_playfield = nullptr;
    DetailScene* m_detail = nullptr;
    int m_exitCode = 0;
};

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    SpriteSheetMultiSceneDemo demo;
    return vde::examples::runExample(demo, "VDE SpriteSheet Multi-Scene Demo", 1024, 768, argc,
                                     argv);
}
