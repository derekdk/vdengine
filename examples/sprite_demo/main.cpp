/**
 * @file main.cpp
 * @brief Sprite example demonstrating the VDE SpriteEntity functionality.
 *
 * This example demonstrates:
 * - Creating SpriteEntity objects
 * - Loading textures and assigning to sprites
 * - Using UV rectangles for sprite sheets
 * - Setting sprite colors/tints
 * - Using anchor points for sprite origins
 * - Combining 2D sprites with 3D meshes
 */

#include <vde/Texture.h>
#include <vde/api/GameAPI.h>

#include <cmath>
#include <iostream>

#include "../ExampleBase.h"

/**
 * @brief Simple input handler for the sprite demo.
 */
class SpriteInputHandler : public vde::examples::BaseExampleInputHandler {
  public:
    void onKeyPress(int key) override {
        // Call base class first for ESC and F keys
        BaseExampleInputHandler::onKeyPress(key);

        if (key == vde::KEY_SPACE)
            m_spacePressed = true;
        if (key == vde::KEY_LEFT)
            m_left = true;
        if (key == vde::KEY_RIGHT)
            m_right = true;
        if (key == vde::KEY_UP)
            m_up = true;
        if (key == vde::KEY_DOWN)
            m_down = true;
        if (key == vde::KEY_1)
            m_key1 = true;
        if (key == vde::KEY_2)
            m_key2 = true;
        if (key == vde::KEY_3)
            m_key3 = true;
    }

    void onKeyRelease(int key) override {
        if (key == vde::KEY_LEFT)
            m_left = false;
        if (key == vde::KEY_RIGHT)
            m_right = false;
        if (key == vde::KEY_UP)
            m_up = false;
        if (key == vde::KEY_DOWN)
            m_down = false;
    }

    bool isSpacePressed() {
        bool v = m_spacePressed;
        m_spacePressed = false;
        return v;
    }
    bool isKey1Pressed() {
        bool v = m_key1;
        m_key1 = false;
        return v;
    }
    bool isKey2Pressed() {
        bool v = m_key2;
        m_key2 = false;
        return v;
    }
    bool isKey3Pressed() {
        bool v = m_key3;
        m_key3 = false;
        return v;
    }

    bool isLeft() const { return m_left; }
    bool isRight() const { return m_right; }
    bool isUp() const { return m_up; }
    bool isDown() const { return m_down; }

  private:
    bool m_spacePressed = false;
    bool m_left = false, m_right = false, m_up = false, m_down = false;
    bool m_key1 = false, m_key2 = false, m_key3 = false;
};

/**
 * @brief An animated sprite that cycles through colors.
 */
class AnimatedSprite : public vde::SpriteEntity {
  public:
    AnimatedSprite() = default;

    void update(float deltaTime) override {
        m_time += deltaTime;

        // Cycle hue over time for rainbow effect
        float hue = std::fmod(m_time * 0.5f, 1.0f);

        // Convert HSV to RGB (simplified, assuming S=1, V=1)
        float r, g, b;
        int i = static_cast<int>(hue * 6.0f);
        float f = hue * 6.0f - i;
        switch (i % 6) {
        case 0:
            r = 1;
            g = f;
            b = 0;
            break;
        case 1:
            r = 1 - f;
            g = 1;
            b = 0;
            break;
        case 2:
            r = 0;
            g = 1;
            b = f;
            break;
        case 3:
            r = 0;
            g = 1 - f;
            b = 1;
            break;
        case 4:
            r = f;
            g = 0;
            b = 1;
            break;
        default:
            r = 1;
            g = 0;
            b = 1 - f;
            break;
        }

        setColor(vde::Color(r, g, b, 1.0f));

        // Gentle rotation
        auto rot = getRotation();
        rot.roll = std::sin(m_time * 2.0f) * 15.0f;
        setRotation(rot);

        // Pulse scale
        float scale = 1.0f + std::sin(m_time * 3.0f) * 0.1f;
        setScale(scale, scale, 1.0f);
    }

  private:
    float m_time = 0.0f;
};

/**
 * @brief Scene demonstrating sprite functionality.
 */
class SpriteScene : public vde::examples::BaseExampleScene {
  public:
    SpriteScene() : BaseExampleScene(15.0f) {}

    void onEnter() override {
        // Print standard header
        printExampleHeader();

        // Set up a 2D camera with viewport in world units (not pixels)
        // Use a viewport size that makes our sprites (which are ~1 unit in size) visible
        auto* camera = new vde::Camera2D(8.0f, 6.0f);  // 8x6 world units visible
        camera->setPosition(0.0f, 0.0f);
        camera->setZoom(1.0f);
        setCamera(camera);

        // Set background color
        setBackgroundColor(vde::Color::fromHex(0x2d3436));

        // Create player sprite (no texture for now - will render as colored quad)
        m_player = addEntity<vde::SpriteEntity>();
        m_player->setName("Player");
        m_player->setPosition(0.0f, 0.0f, 0.0f);
        m_player->setColor(vde::Color::fromHex(0x00b894));  // Green
        m_player->setScale(0.5f, 0.5f, 1.0f);
        m_player->setAnchor(0.5f, 0.5f);  // Center anchor

        // Create an animated rainbow sprite
        m_animated = addEntity<AnimatedSprite>();
        m_animated->setName("RainbowSprite");
        m_animated->setPosition(-1.5f, 1.0f, 0.0f);
        m_animated->setScale(0.4f, 0.4f, 1.0f);

        // Create corner sprites to show anchor point behavior
        createCornerSprites();

        // Create a background sprite
        m_background = addEntity<vde::SpriteEntity>();
        m_background->setName("Background");
        m_background->setPosition(0.0f, 0.0f, -0.1f);                 // Behind other sprites
        m_background->setColor(vde::Color(0.1f, 0.1f, 0.15f, 0.5f));  // Semi-transparent dark
        m_background->setScale(4.0f, 3.0f, 1.0f);
    }

    void createCornerSprites() {
        // Create sprites in corners to demonstrate anchor points
        float offset = 1.5f;

        // Top-left corner - anchor at top-left (0, 1)
        auto tl = addEntity<vde::SpriteEntity>();
        tl->setName("TopLeft");
        tl->setPosition(-offset, offset, 0.0f);
        tl->setColor(vde::Color::fromHex(0xe74c3c));  // Red
        tl->setScale(0.3f, 0.3f, 1.0f);
        tl->setAnchor(0.0f, 1.0f);

        // Top-right corner - anchor at top-right (1, 1)
        auto tr = addEntity<vde::SpriteEntity>();
        tr->setName("TopRight");
        tr->setPosition(offset, offset, 0.0f);
        tr->setColor(vde::Color::fromHex(0x3498db));  // Blue
        tr->setScale(0.3f, 0.3f, 1.0f);
        tr->setAnchor(1.0f, 1.0f);

        // Bottom-left corner - anchor at bottom-left (0, 0)
        auto bl = addEntity<vde::SpriteEntity>();
        bl->setName("BottomLeft");
        bl->setPosition(-offset, -offset, 0.0f);
        bl->setColor(vde::Color::fromHex(0xf39c12));  // Orange
        bl->setScale(0.3f, 0.3f, 1.0f);
        bl->setAnchor(0.0f, 0.0f);

        // Bottom-right corner - anchor at bottom-right (1, 0)
        auto br = addEntity<vde::SpriteEntity>();
        br->setName("BottomRight");
        br->setPosition(offset, -offset, 0.0f);
        br->setColor(vde::Color::fromHex(0x9b59b6));  // Purple
        br->setScale(0.3f, 0.3f, 1.0f);
        br->setAnchor(1.0f, 0.0f);
    }

    void update(float deltaTime) override {
        // Call base class first (handles ESC, F, auto-terminate)
        BaseExampleScene::update(deltaTime);

        auto* input = dynamic_cast<SpriteInputHandler*>(getInputHandler());
        if (!input)
            return;

        // Toggle player visibility
        if (input->isSpacePressed()) {
            m_player->setVisible(!m_player->isVisible());
            std::cout << "Player visibility: " << (m_player->isVisible() ? "ON" : "OFF")
                      << std::endl;
        }

        // Change anchor point with number keys
        if (input->isKey1Pressed()) {
            m_player->setAnchor(0.5f, 0.5f);
            std::cout << "Anchor: Center (0.5, 0.5)" << std::endl;
        }
        if (input->isKey2Pressed()) {
            m_player->setAnchor(0.0f, 0.0f);
            std::cout << "Anchor: Bottom-Left (0, 0)" << std::endl;
        }
        if (input->isKey3Pressed()) {
            m_player->setAnchor(1.0f, 0.5f);
            std::cout << "Anchor: Right-Center (1, 0.5)" << std::endl;
        }

        // Move player with arrow keys
        float speed = 2.0f;
        auto pos = m_player->getPosition();

        if (input->isLeft())
            pos.x -= speed * deltaTime;
        if (input->isRight())
            pos.x += speed * deltaTime;
        if (input->isUp())
            pos.y += speed * deltaTime;
        if (input->isDown())
            pos.y -= speed * deltaTime;

        m_player->setPosition(pos);
    }

  protected:
    std::string getExampleName() const override { return "Sprite System"; }

    std::vector<std::string> getFeatures() const override {
        return {"SpriteEntity creation and rendering", "Sprite colors and tinting",
                "Anchor point positioning", "Animated sprites"};
    }

    std::vector<std::string> getExpectedVisuals() const override {
        return {"Green player sprite at center (moveable)",
                "Rainbow animated sprite (top-left area)",
                "Colored corner sprites (red, blue, orange, purple)",
                "Dark semi-transparent background"};
    }

    std::vector<std::string> getControls() const override {
        return {"Arrow keys - Move player sprite", "1/2/3      - Change anchor point",
                "Space      - Toggle player visibility"};
    }

  private:
    std::shared_ptr<vde::SpriteEntity> m_player;
    std::shared_ptr<AnimatedSprite> m_animated;
    std::shared_ptr<vde::SpriteEntity> m_background;
};

/**
 * @brief Game class for the sprite demo.
 */
class SpriteDemo : public vde::examples::BaseExampleGame<SpriteInputHandler, SpriteScene> {};

/**
 * @brief Main entry point.
 */
int main(int argc, char** argv) {
    SpriteDemo demo;
    return vde::examples::runExample(demo, "VDE Sprite Demo", 1024, 768, argc, argv);
}
