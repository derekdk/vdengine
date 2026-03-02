#include <vde/api/GameAPI.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

#include "../ExampleBase.h"

using namespace vde;

class BreakoutInputHandler : public vde::examples::BaseExampleInputHandler {
  public:
    // Keyboard input
    void onKeyPress(int key) override {
        BaseExampleInputHandler::onKeyPress(key);
        if (key == vde::KEY_LEFT)
            m_left = true;
        if (key == vde::KEY_RIGHT)
            m_right = true;
        if (key == vde::KEY_SPACE)
            m_space = true;
    }

    void onKeyRelease(int key) override {
        if (key == vde::KEY_LEFT)
            m_left = false;
        if (key == vde::KEY_RIGHT)
            m_right = false;
    }

    // Gamepad input
    void onGamepadButtonPress(int /*gamepadId*/, int button) override {
        if (button == vde::GAMEPAD_BUTTON_DPAD_LEFT)
            m_left = true;
        if (button == vde::GAMEPAD_BUTTON_DPAD_RIGHT)
            m_right = true;
        if (button == vde::GAMEPAD_BUTTON_A)
            m_space = true;
    }

    void onGamepadButtonRelease(int /*gamepadId*/, int button) override {
        if (button == vde::GAMEPAD_BUTTON_DPAD_LEFT)
            m_left = false;
        if (button == vde::GAMEPAD_BUTTON_DPAD_RIGHT)
            m_right = false;
    }

    bool isLeft() const { return m_left; }
    bool isRight() const { return m_right; }
    bool isSpacePressed() {
        bool v = m_space;
        m_space = false;
        return v;
    }

    /**
     * @brief Get the left-stick X axis value for the first connected gamepad.
     * @return Axis value in [-1, 1], or 0 if no gamepad is connected.
     */
    float getLeftStickX() const {
        for (int i = 0; i < vde::MAX_GAMEPADS; ++i) {
            if (isGamepadConnected(i))
                return getGamepadAxis(i, vde::GAMEPAD_AXIS_LEFT_X);
        }
        return 0.0f;
    }

  private:
    bool m_left = false;
    bool m_right = false;
    bool m_space = false;
};

class BreakoutScene : public vde::examples::BaseExampleScene {
  public:
    BreakoutScene() : BaseExampleScene(30.0f) {}

    void onEnter() override {
        printExampleHeader();

        // Camera: 10x7.5 world units
        auto* camera = new Camera2D(10.0f, 7.5f);
        camera->setPosition(0.0f, 0.0f);
        camera->setZoom(1.0f);
        setCamera(camera);

        setBackgroundColor(Color::fromHex(0x2c3e50));

        // Paddle
        m_paddle = addEntity<SpriteEntity>();
        m_paddle->setName("Paddle");
        m_paddle->setScale(1.6f, 0.25f, 1.0f);
        m_paddle->setAnchor(0.5f, 0.5f);
        m_paddle->setPosition(0.0f, -3.0f, 0.0f);
        m_paddle->setColor(Color::fromHex(0x00b894));

        // Ball (starts sitting on paddle)
        m_ball = addEntity<SpriteEntity>();
        m_ball->setName("Ball");
        m_ball->setScale(0.18f, 0.18f, 1.0f);
        m_ball->setAnchor(0.5f, 0.5f);
        resetBallToPaddle();
        m_ball->setColor(Color::fromHex(0xffffff));

        // Create bricks
        createBricks();

        std::cout << "Enjoy! Use LEFT/RIGHT or gamepad left stick/D-pad to move paddle,"
                  << " SPACE or A button to launch the ball." << std::endl;
    }

    void update(float deltaTime) override {
        BaseExampleScene::update(deltaTime);

        auto* input = dynamic_cast<BreakoutInputHandler*>(getInputHandler());
        if (!input)
            return;

        // Move paddle (keyboard / D-pad)
        float paddleSpeed = 6.0f;
        auto ppos = m_paddle->getPosition();
        if (input->isLeft())
            ppos.x -= paddleSpeed * deltaTime;
        if (input->isRight())
            ppos.x += paddleSpeed * deltaTime;

        // Move paddle (left analog stick)
        float stickX = input->getLeftStickX();
        if (std::abs(stickX) > 0.0f)  // dead zone already handled by engine
            ppos.x += stickX * paddleSpeed * deltaTime;

        // Clamp paddle to world bounds (camera half width = 5.0)
        float halfWorldX = 5.0f;
        float paddleHalfW = m_paddle->getScale().x * 0.5f;
        ppos.x = std::clamp(ppos.x, -halfWorldX + paddleHalfW, halfWorldX - paddleHalfW);
        m_paddle->setPosition(ppos);

        // Launch ball
        if (!m_ballLaunched && input->isSpacePressed()) {
            m_ballLaunched = true;
            // initial upward velocity with small horizontal offset
            m_ballVX = 0.35f * m_ballSpeed;
            m_ballVY = 0.95f * m_ballSpeed;
        }

        // Update ball
        if (m_ballLaunched) {
            auto bpos = m_ball->getPosition();
            bpos.x += m_ballVX * deltaTime;
            bpos.y += m_ballVY * deltaTime;

            float halfW = m_ball->getScale().x * 0.5f;
            float halfH = m_ball->getScale().y * 0.5f;

            // Walls
            if (bpos.x - halfW <= -halfWorldX) {
                bpos.x = -halfWorldX + halfW;
                m_ballVX = -m_ballVX;
            }
            if (bpos.x + halfW >= halfWorldX) {
                bpos.x = halfWorldX - halfW;
                m_ballVX = -m_ballVX;
            }

            // Ceiling
            float halfWorldY = 7.5f / 2.0f;  // camera height 7.5
            if (bpos.y + halfH >= halfWorldY) {
                bpos.y = halfWorldY - halfH;
                m_ballVY = -std::abs(m_ballVY);
            }

            m_ball->setPosition(bpos);

            // Paddle collision
            if (aabbIntersect(bpos, m_ball->getScale().x, m_ball->getScale().y,
                              m_paddle->getPosition(), m_paddle->getScale().x,
                              m_paddle->getScale().y)) {
                // Only reflect if moving downwards
                if (m_ballVY < 0.0f) {
                    // Place ball above paddle
                    float paddleTop = m_paddle->getPosition().y + (m_paddle->getScale().y * 0.5f);
                    bpos.y = paddleTop + halfH + 0.001f;

                    // Adjust horizontal velocity based on hit location
                    float hitDelta =
                        (bpos.x - m_paddle->getPosition().x) / (m_paddle->getScale().x * 0.5f);
                    m_ballVX = hitDelta * m_ballSpeed * 0.9f;
                    m_ballVY = std::abs(m_ballVY);

                    // Normalize to constant speed
                    normalizeBallVelocity();
                    m_ball->setPosition(bpos);
                }
            }

            // Bricks
            for (int i = static_cast<int>(m_bricks.size()) - 1; i >= 0; --i) {
                EntityId id = m_bricks[i];
                Entity* e = getEntity(id);
                if (!e)
                    continue;
                auto* brick = dynamic_cast<SpriteEntity*>(e);
                if (!brick)
                    continue;

                if (aabbIntersect(bpos, m_ball->getScale().x, m_ball->getScale().y,
                                  brick->getPosition(), brick->getScale().x, brick->getScale().y)) {
                    // Remove brick
                    removeEntity(brick->getId());
                    m_bricks.erase(m_bricks.begin() + i);

                    // Bounce ball
                    m_ballVY = -m_ballVY;
                    normalizeBallVelocity();

                    // Win condition
                    if (m_bricks.empty()) {
                        std::cout << "All bricks cleared!" << std::endl;
                        handleTestSuccess();
                        return;
                    }

                    break;  // only one brick per frame
                }
            }

            // Missed paddle - reset ball
            if (bpos.y < -halfWorldY - 1.0f) {
                std::cout << "Ball missed the paddle - resetting." << std::endl;
                resetBallToPaddle();
            }
        } else {
            // Ball follows paddle when not launched
            auto bpos = m_ball->getPosition();
            bpos.x = m_paddle->getPosition().x;
            bpos.y = m_paddle->getPosition().y + (m_paddle->getScale().y * 0.5f) +
                     (m_ball->getScale().y * 0.5f) + 0.05f;
            m_ball->setPosition(bpos);
        }
    }

  protected:
    std::string getExampleName() const override { return "Breakout Clone"; }

    std::vector<std::string> getFeatures() const override {
        return {"Simple 2D gameplay (paddle, ball, bricks)", "SpriteEntity usage",
                "Basic collision and game logic"};
    }

    std::vector<std::string> getExpectedVisuals() const override {
        return {"Paddle at bottom (green)", "White ball bouncing",
                "Rows of colored bricks at top breaking on hit"};
    }

    std::vector<std::string> getControls() const override {
        return {"Left/Right or D-pad - Move paddle", "Left stick - Move paddle (analog)",
                "Space or A button - Launch ball", "F - Report failure, ESC - Exit"};
    }

  private:
    std::shared_ptr<SpriteEntity> m_paddle;
    std::shared_ptr<SpriteEntity> m_ball;
    std::vector<EntityId> m_bricks;

    bool m_ballLaunched = false;
    float m_ballSpeed = 6.0f;
    float m_ballVX = 0.0f;
    float m_ballVY = 0.0f;

    static bool aabbIntersect(const vde::Position& aPos, float aW, float aH,
                              const vde::Position& bPos, float bW, float bH) {
        float aHalfW = aW * 0.5f;
        float aHalfH = aH * 0.5f;
        float bHalfW = bW * 0.5f;
        float bHalfH = bH * 0.5f;

        return !(aPos.x + aHalfW < bPos.x - bHalfW || aPos.x - aHalfW > bPos.x + bHalfW ||
                 aPos.y + aHalfH < bPos.y - bHalfH || aPos.y - aHalfH > bPos.y + bHalfH);
    }

    void normalizeBallVelocity() {
        float len = std::sqrt(m_ballVX * m_ballVX + m_ballVY * m_ballVY);
        if (len > 0.0001f) {
            m_ballVX = (m_ballVX / len) * m_ballSpeed;
            m_ballVY = (m_ballVY / len) * m_ballSpeed;
        }
    }

    void resetBallToPaddle() {
        m_ballLaunched = false;
        m_ballVX = 0.0f;
        m_ballVY = 0.0f;
        if (m_paddle && m_ball) {
            auto ppos = m_paddle->getPosition();
            auto bpos = glm::vec3(ppos.x,
                                  ppos.y + (m_paddle->getScale().y * 0.5f) +
                                      (m_ball->getScale().y * 0.5f) + 0.05f,
                                  0.0f);
            m_ball->setPosition(bpos);
        }
    }

    void createBricks() {
        const int cols = 8;
        const int rows = 5;
        float brickW = 1.0f;
        float brickH = 0.4f;
        float spacingX = 0.12f;
        float spacingY = 0.1f;

        float totalWidth = cols * brickW + (cols - 1) * spacingX;
        float startX = -totalWidth * 0.5f + brickW * 0.5f;
        float startY = 2.5f;

        std::vector<uint32_t> colors = {0xe74c3c, 0xf39c12, 0xf1c40f, 0x2ecc71, 0x3498db};

        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                float x = startX + c * (brickW + spacingX);
                float y = startY - r * (brickH + spacingY);

                auto brick = addEntity<SpriteEntity>();
                brick->setName("Brick");
                brick->setScale(brickW, brickH, 1.0f);
                brick->setAnchor(0.5f, 0.5f);
                brick->setPosition(x, y, 0.0f);
                brick->setColor(Color::fromHex(colors[r % static_cast<int>(colors.size())]));

                m_bricks.push_back(brick->getId());
            }
        }
    }
};

class BreakoutGame : public vde::examples::BaseExampleGame<BreakoutInputHandler, BreakoutScene> {};

int main(int argc, char** argv) {
    BreakoutGame demo;
    return vde::examples::runExample(demo, "VDE Breakout Demo", 1024, 768, argc, argv);
}
