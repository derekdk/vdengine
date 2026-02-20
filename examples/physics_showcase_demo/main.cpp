/**
 * @file main.cpp
 * @brief Physics Showcase Demo - multiple physics tests to rotate through
 *
 * This example demonstrates seven distinct physics scenarios:
 *   1. Gravity Rain       - Boxes spawn from above and pile up under gravity
 *   2. Bouncy Chamber     - High-restitution balls ricocheting off walls
 *   3. Domino Chain       - Standing boxes topple in sequence
 *   4. Wrecking Ball      - Kinematic sweeper demolishes box towers
 *   5. Zero Gravity       - No gravity; balls drift and collide freely
 *   6. Gravity Flip       - Gravity direction reverses every 3 seconds
 *   7. Explosion Burst    - Packed grid launches outward on impulse
 *
 * Controls:
 *   LEFT / RIGHT  - Previous / Next test
 *   SPACE         - Test-specific action (spawn, trigger, etc.)
 *   R             - Reset current test
 *   ESC           - Exit
 *   F             - Mark failure
 */

#include <vde/api/GameAPI.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include "../ExampleBase.h"

// ============================================================================
// Constants
// ============================================================================

// Arena bounds (world units)
static constexpr float ARENA_HALF_W = 7.5f;    // ±7.5 in X
static constexpr float ARENA_FLOOR_Y = -4.5f;  // floor position
static constexpr float ARENA_CEIL_Y = 5.5f;    // ceiling position
static constexpr float ARENA_MID_Y = (ARENA_FLOOR_Y + ARENA_CEIL_Y) * 0.5f;
static constexpr float ARENA_HEIGHT = ARENA_CEIL_Y - ARENA_FLOOR_Y;  // 10 units
static constexpr float WALL_HALF = 0.25f;                            // half-thickness of walls
static constexpr float WALL_HALF_H = ARENA_HEIGHT * 0.5f + WALL_HALF;

// ============================================================================
// Input Handler
// ============================================================================

class ShowcaseInputHandler : public vde::examples::BaseExampleInputHandler {
  public:
    void onKeyPress(int key) override {
        BaseExampleInputHandler::onKeyPress(key);
        if (key == vde::KEY_SPACE)
            m_spacePressed = true;
        if (key == vde::KEY_R)
            m_resetPressed = true;
        if (key == vde::KEY_LEFT)
            m_prevPressed = true;
        if (key == vde::KEY_RIGHT)
            m_nextPressed = true;
    }

    bool consumeSpace() { return consume(m_spacePressed); }
    bool consumeReset() { return consume(m_resetPressed); }
    bool consumePrev() { return consume(m_prevPressed); }
    bool consumeNext() { return consume(m_nextPressed); }

  private:
    bool m_spacePressed = false;
    bool m_resetPressed = false;
    bool m_prevPressed = false;
    bool m_nextPressed = false;

    static bool consume(bool& flag) {
        bool v = flag;
        flag = false;
        return v;
    }
};

// ============================================================================
// Test descriptions (parallel arrays indexed by test number)
// ============================================================================

enum TestId {
    TEST_GRAVITY_RAIN = 0,
    TEST_BOUNCY_CHAMBER,
    TEST_DOMINO_CHAIN,
    TEST_WRECKING_BALL,
    TEST_ZERO_GRAVITY,
    TEST_GRAVITY_FLIP,
    TEST_EXPLOSION_BURST,
    TEST_COUNT
};

static const char* TEST_NAMES[TEST_COUNT] = {
    "Gravity Rain", "Bouncy Chamber", "Domino Chain",    "Wrecking Ball",
    "Zero Gravity", "Gravity Flip",   "Explosion Burst",
};

static const char* TEST_DESCRIPTIONS[TEST_COUNT] = {
    "Boxes rain from above and pile up under gravity",
    "High-restitution circles bounce around the arena",
    "SPACE: tip the first domino and watch the chain reaction",
    "A kinematic wrecking ball sweeps across, demolishing towers",
    "No gravity - circles drift and collide freely",
    "Gravity reverses direction every 3 seconds",
    "SPACE: launch packed boxes outward in an explosion",
};

// ============================================================================
// Scene
// ============================================================================

class PhysicsShowcaseScene : public vde::examples::BaseExampleScene {
  public:
    PhysicsShowcaseScene() : BaseExampleScene(120.0f) {}

    // -----------------------------------------------------------------------
    void onEnter() override {
        printExampleHeader();

        // Camera - orthographic 2D view framing the full arena (arena is 15.5 wide x 10.5 tall)
        auto* cam = new vde::Camera2D(20.0f, 13.0f);  // 20x13 world units with margin
        cam->setPosition(0.0f, ARENA_MID_Y);
        setCamera(cam);

        setLightBox(std::make_unique<vde::SimpleColorLightBox>(vde::Color::white()));
        setBackgroundColor(vde::Color(0.07f, 0.07f, 0.12f, 1.0f));

        // Physics is created once; gravity is adjusted per-test via setGravity()
        vde::PhysicsConfig cfg;
        cfg.gravity = {0.0f, -9.81f};
        cfg.fixedTimestep = 1.0f / 60.0f;
        cfg.iterations = 6;
        enablePhysics(cfg);

        // Build permanent arena walls (re-used by every test)
        buildArena();

        // Load the first test
        loadTest(m_currentTest);
    }

    // -----------------------------------------------------------------------
    void update(float deltaTime) override {
        BaseExampleScene::update(deltaTime);  // handles ESC, F, auto-terminate

        auto* input = dynamic_cast<ShowcaseInputHandler*>(getInputHandler());
        if (input) {
            if (input->consumeNext())
                cycleTest(+1);
            if (input->consumePrev())
                cycleTest(-1);
            if (input->consumeReset())
                loadTest(m_currentTest);
            if (input->consumeSpace())
                handleSpaceAction();
        }

        // Per-test per-frame logic
        switch (m_currentTest) {
        case TEST_GRAVITY_RAIN:
            updateGravityRain(deltaTime);
            break;
        case TEST_WRECKING_BALL:
            updateWreckingBall(deltaTime);
            break;
        case TEST_GRAVITY_FLIP:
            updateGravityFlip(deltaTime);
            break;
        default:
            break;
        }
    }

  protected:
    std::string getExampleName() const override { return "Physics Showcase"; }

    std::vector<std::string> getFeatures() const override {
        return {
            "7 distinct physics tests",  "Gravity: falling bodies and stacking",
            "High-restitution bouncing", "Domino chain reaction",
            "Kinematic wrecking ball",   "Zero-gravity floating bodies",
            "Runtime gravity flip",      "Impulse-based explosion burst",
        };
    }

    std::vector<std::string> getExpectedVisuals() const override {
        return {
            "Boxed arena with colored walls",
            "Test-specific physics behaviors",
            "Smooth collision resolution and stacking",
        };
    }

    std::vector<std::string> getControls() const override {
        return {
            "LEFT/RIGHT  - Previous/Next test", "SPACE       - Test-specific action",
            "R           - Reset current test", "ESC         - Exit",
            "F           - Mark failure",
        };
    }

  private:
    // -----------------------------------------------------------------------
    // State
    // -----------------------------------------------------------------------

    int m_currentTest = TEST_GRAVITY_RAIN;

    // Entities created for the current test (cleared on each test switch)
    std::vector<std::shared_ptr<vde::PhysicsSpriteEntity>> m_testEntities;

    // Wrecking ball (test 4)
    std::shared_ptr<vde::PhysicsSpriteEntity> m_wreckingBall;
    float m_wreckingBallTime = 0.0f;

    // Gravity rain (test 1)
    float m_rainTimer = 0.0f;
    int m_rainCount = 0;
    static constexpr int RAIN_MAX = 35;

    // Gravity flip (test 6)
    float m_flipTimer = 0.0f;
    bool m_gravityDown = true;
    static constexpr float FLIP_INTERVAL = 3.0f;

    // First domino entity (test 3) - stored to apply initial impulse
    std::shared_ptr<vde::PhysicsSpriteEntity> m_firstDomino;

    // RNG
    std::mt19937 m_rng{42};

    // -----------------------------------------------------------------------
    // Helpers
    // -----------------------------------------------------------------------

    float randRange(float lo, float hi) {
        std::uniform_real_distribution<float> dist(lo, hi);
        return dist(m_rng);
    }

    static vde::Color hsv(float h, float s, float v) {
        // Simple HSV -> RGB conversion
        float r, g, b;
        float i = std::floor(h * 6.0f);
        float f = h * 6.0f - i;
        float p = v * (1.0f - s);
        float q = v * (1.0f - f * s);
        float t = v * (1.0f - (1.0f - f) * s);
        switch (static_cast<int>(i) % 6) {
        case 0:
            r = v;
            g = t;
            b = p;
            break;
        case 1:
            r = q;
            g = v;
            b = p;
            break;
        case 2:
            r = p;
            g = v;
            b = t;
            break;
        case 3:
            r = p;
            g = q;
            b = v;
            break;
        case 4:
            r = t;
            g = p;
            b = v;
            break;
        default:
            r = v;
            g = p;
            b = q;
            break;
        }
        return {r, g, b, 1.0f};
    }

    // -----------------------------------------------------------------------
    // Arena construction
    // -----------------------------------------------------------------------

    void buildArena() {
        // Floor
        addWall({0.0f, ARENA_FLOOR_Y - WALL_HALF}, {ARENA_HALF_W + WALL_HALF, WALL_HALF},
                vde::Color(0.25f, 0.55f, 0.25f, 1.0f));
        // Ceiling
        addWall({0.0f, ARENA_CEIL_Y + WALL_HALF}, {ARENA_HALF_W + WALL_HALF, WALL_HALF},
                vde::Color(0.25f, 0.25f, 0.55f, 1.0f));
        // Left wall
        addWall({-ARENA_HALF_W - WALL_HALF, ARENA_MID_Y}, {WALL_HALF, WALL_HALF_H},
                vde::Color(0.45f, 0.35f, 0.25f, 1.0f));
        // Right wall
        addWall({ARENA_HALF_W + WALL_HALF, ARENA_MID_Y}, {WALL_HALF, WALL_HALF_H},
                vde::Color(0.45f, 0.35f, 0.25f, 1.0f));
    }

    void addWall(glm::vec2 pos, glm::vec2 extents, vde::Color color) {
        auto e = addEntity<vde::PhysicsSpriteEntity>();
        e->setColor(color);
        e->setScale(vde::Scale(extents.x * 2.0f, extents.y * 2.0f, 1.0f));

        vde::PhysicsBodyDef def;
        def.type = vde::PhysicsBodyType::Static;
        def.shape = vde::PhysicsShape::Box;
        def.position = pos;
        def.extents = extents;
        e->createPhysicsBody(def);
    }

    // -----------------------------------------------------------------------
    // Test management
    // -----------------------------------------------------------------------

    void cycleTest(int dir) {
        m_currentTest = (m_currentTest + dir + TEST_COUNT) % TEST_COUNT;
        loadTest(m_currentTest);
    }

    void clearTestEntities() {
        if (m_wreckingBall) {
            removeEntity(m_wreckingBall->getId());
            m_wreckingBall.reset();
        }
        m_firstDomino.reset();
        for (auto& e : m_testEntities) {
            if (e)
                removeEntity(e->getId());
        }
        m_testEntities.clear();
    }

    void loadTest(int id) {
        clearTestEntities();

        // Reset per-test state
        m_rainTimer = 0.0f;
        m_rainCount = 0;
        m_wreckingBallTime = 0.0f;
        m_flipTimer = 0.0f;
        m_gravityDown = true;

        // Restore default gravity (tests that need different gravity override below)
        if (hasPhysics()) {
            getPhysicsScene()->setGravity({0.0f, -9.81f});
        }

        std::cout << "\n=== Test " << (id + 1) << "/" << TEST_COUNT << ": " << TEST_NAMES[id]
                  << " ===" << std::endl;
        std::cout << "    " << TEST_DESCRIPTIONS[id] << std::endl;

        switch (id) {
        case TEST_GRAVITY_RAIN:
            setupGravityRain();
            break;
        case TEST_BOUNCY_CHAMBER:
            setupBouncyChamber();
            break;
        case TEST_DOMINO_CHAIN:
            setupDominoChain();
            break;
        case TEST_WRECKING_BALL:
            setupWreckingBall();
            break;
        case TEST_ZERO_GRAVITY:
            setupZeroGravity();
            break;
        case TEST_GRAVITY_FLIP:
            setupGravityFlip();
            break;
        case TEST_EXPLOSION_BURST:
            setupExplosionBurst();
            break;
        }
    }

    void handleSpaceAction() {
        switch (m_currentTest) {
        case TEST_GRAVITY_RAIN:
            spawnRainBox();
            break;
        case TEST_BOUNCY_CHAMBER:
            spawnBouncyBall();
            break;
        case TEST_DOMINO_CHAIN:
            tipFirstDomino();
            break;
        case TEST_EXPLOSION_BURST:
            triggerExplosion();
            break;
        default:
            break;
        }
    }

    // -----------------------------------------------------------------------
    // Helper: add a dynamic box entity
    // -----------------------------------------------------------------------

    std::shared_ptr<vde::PhysicsSpriteEntity>
    addDynamicBox(float x, float y, float halfW, float halfH, vde::Color color,
                  float restitution = 0.25f, float friction = 0.4f, float damping = 0.01f) {
        auto e = addEntity<vde::PhysicsSpriteEntity>();
        e->setColor(color);
        e->setScale(vde::Scale(halfW * 2.0f, halfH * 2.0f, 1.0f));

        vde::PhysicsBodyDef def;
        def.type = vde::PhysicsBodyType::Dynamic;
        def.shape = vde::PhysicsShape::Box;
        def.position = {x, y};
        def.extents = {halfW, halfH};
        def.mass = 1.0f;
        def.restitution = restitution;
        def.friction = friction;
        def.linearDamping = damping;
        e->createPhysicsBody(def);

        m_testEntities.push_back(e);
        return e;
    }

    std::shared_ptr<vde::PhysicsSpriteEntity>
    addDynamicCircle(float x, float y, float radius, vde::Color color, float restitution = 0.7f,
                     float friction = 0.1f, float damping = 0.005f) {
        auto e = addEntity<vde::PhysicsSpriteEntity>();
        e->setColor(color);
        e->setScale(vde::Scale(radius * 2.0f, radius * 2.0f, 1.0f));

        vde::PhysicsBodyDef def;
        def.type = vde::PhysicsBodyType::Dynamic;
        def.shape = vde::PhysicsShape::Circle;
        def.position = {x, y};
        def.extents = {radius, 0.0f};
        def.mass = 1.0f;
        def.restitution = restitution;
        def.friction = friction;
        def.linearDamping = damping;
        e->createPhysicsBody(def);

        m_testEntities.push_back(e);
        return e;
    }

    // -----------------------------------------------------------------------
    // Test 1: Gravity Rain
    // -----------------------------------------------------------------------

    void setupGravityRain() {
        std::cout << "  Boxes will rain from above. SPACE to spawn extras." << std::endl;
        // Spawn a few starter boxes immediately
        for (int i = 0; i < 5; ++i) {
            spawnRainBox();
        }
    }

    void spawnRainBox() {
        if (m_rainCount >= RAIN_MAX)
            return;
        float x = randRange(-ARENA_HALF_W + 0.5f, ARENA_HALF_W - 0.5f);
        float y = ARENA_CEIL_Y - 0.5f;
        float h = 0.3f + randRange(0.0f, 0.2f);
        float w = 0.3f + randRange(0.0f, 0.2f);
        vde::Color col = hsv(randRange(0.0f, 1.0f), 0.8f, 0.9f);
        addDynamicBox(x, y, w, h, col, 0.2f, 0.5f, 0.02f);
        ++m_rainCount;
    }

    void updateGravityRain(float dt) {
        if (m_rainCount < RAIN_MAX) {
            m_rainTimer += dt;
            if (m_rainTimer >= 0.4f) {
                m_rainTimer = 0.0f;
                spawnRainBox();
                spawnRainBox();  // two at a time for visual density
            }
        }
    }

    // -----------------------------------------------------------------------
    // Test 2: Bouncy Chamber
    // -----------------------------------------------------------------------

    void setupBouncyChamber() {
        std::cout << "  High-restitution circles bouncing off walls. SPACE to add more."
                  << std::endl;
        for (int i = 0; i < 12; ++i) {
            spawnBouncyBall();
        }
    }

    void spawnBouncyBall() {
        float x = randRange(-ARENA_HALF_W + 0.5f, ARENA_HALF_W - 0.5f);
        float y = randRange(ARENA_FLOOR_Y + 0.5f, ARENA_CEIL_Y - 0.5f);
        float r = 0.3f + randRange(0.0f, 0.2f);
        vde::Color col = hsv(randRange(0.0f, 1.0f), 0.85f, 1.0f);
        auto e = addDynamicCircle(x, y, r, col, 0.95f, 0.05f, 0.0f);
        // Give each ball a random initial velocity
        float vx = randRange(-6.0f, 6.0f);
        float vy = randRange(-5.0f, 5.0f);
        e->setLinearVelocity({vx, vy});
    }

    // -----------------------------------------------------------------------
    // Test 3: Domino Chain
    // -----------------------------------------------------------------------

    static constexpr int DOMINO_COUNT = 18;
    static constexpr float DOMINO_HALF_W = 0.10f;
    static constexpr float DOMINO_HALF_H = 0.55f;
    static constexpr float DOMINO_SPACING = 0.55f;

    void setupDominoChain() {
        std::cout << "  Row of standing boxes. SPACE to tip the first domino." << std::endl;

        float startX = -(DOMINO_COUNT - 1) * DOMINO_SPACING * 0.5f;
        float groundY = ARENA_FLOOR_Y + DOMINO_HALF_H;

        for (int i = 0; i < DOMINO_COUNT; ++i) {
            float x = startX + i * DOMINO_SPACING;
            float hue = static_cast<float>(i) / DOMINO_COUNT;
            vde::Color col = hsv(hue, 0.75f, 0.95f);
            auto e = addDynamicBox(x, groundY, DOMINO_HALF_W, DOMINO_HALF_H, col, 0.1f, 0.6f, 0.0f);
            if (i == 0)
                m_firstDomino = e;
        }
    }

    void tipFirstDomino() {
        if (m_firstDomino) {
            m_firstDomino->applyImpulse({1.8f, 0.2f});
            std::cout << "  [Domino] First domino tipped!" << std::endl;
        }
    }

    // -----------------------------------------------------------------------
    // Test 4: Wrecking Ball
    // -----------------------------------------------------------------------

    void setupWreckingBall() {
        std::cout << "  A kinematic wrecking ball sweeps across demolishing towers." << std::endl;

        float groundY = ARENA_FLOOR_Y;

        // Left tower: 4 columns of 5 boxes each
        for (int col = 0; col < 4; ++col) {
            float cx = -6.0f + col * 0.65f;
            for (int row = 0; row < 5; ++row) {
                float cy = groundY + 0.3f + row * 0.62f;
                vde::Color c = hsv(0.05f + col * 0.04f, 0.7f, 0.85f);
                addDynamicBox(cx, cy, 0.3f, 0.3f, c, 0.2f, 0.5f, 0.01f);
            }
        }

        // Right tower: 4 columns of 5 boxes each
        for (int col = 0; col < 4; ++col) {
            float cx = 3.5f + col * 0.65f;
            for (int row = 0; row < 5; ++row) {
                float cy = groundY + 0.3f + row * 0.62f;
                vde::Color c = hsv(0.55f + col * 0.04f, 0.7f, 0.85f);
                addDynamicBox(cx, cy, 0.3f, 0.3f, c, 0.2f, 0.5f, 0.01f);
            }
        }

        // The kinematic wrecking ball (a large box that sweeps left->right->left)
        m_wreckingBall = addEntity<vde::PhysicsSpriteEntity>();
        m_wreckingBall->setColor(vde::Color(0.9f, 0.85f, 0.1f, 1.0f));
        m_wreckingBall->setScale(vde::Scale(1.0f, 1.0f, 1.0f));

        vde::PhysicsBodyDef bdef;
        bdef.type = vde::PhysicsBodyType::Kinematic;
        bdef.shape = vde::PhysicsShape::Box;
        bdef.position = {-ARENA_HALF_W + 1.0f, ARENA_FLOOR_Y + 1.5f};
        bdef.extents = {0.5f, 0.5f};
        m_wreckingBall->createPhysicsBody(bdef);
    }

    void updateWreckingBall(float dt) {
        if (!m_wreckingBall)
            return;
        m_wreckingBallTime += dt;

        // Sweep speed: 6 seconds for full left-right-left cycle
        const float amplitude = ARENA_HALF_W - 1.2f;
        const float omega = 2.0f * 3.14159f / 6.0f;  // one full oscillation every 6s
        float vx = amplitude * omega * std::cos(omega * m_wreckingBallTime);
        m_wreckingBall->setLinearVelocity({vx, 0.0f});
    }

    // -----------------------------------------------------------------------
    // Test 5: Zero Gravity
    // -----------------------------------------------------------------------

    void setupZeroGravity() {
        std::cout << "  No gravity. Circles drift and collide freely." << std::endl;

        if (hasPhysics())
            getPhysicsScene()->setGravity({0.0f, 0.0f});

        for (int i = 0; i < 16; ++i) {
            float x = randRange(-ARENA_HALF_W + 0.5f, ARENA_HALF_W - 0.5f);
            float y = randRange(ARENA_FLOOR_Y + 0.5f, ARENA_CEIL_Y - 0.5f);
            float r = 0.25f + randRange(0.0f, 0.25f);
            vde::Color col = hsv(static_cast<float>(i) / 16.0f, 0.8f, 1.0f);
            auto e = addDynamicCircle(x, y, r, col, 0.98f, 0.02f, 0.0f);
            // Random drift velocity
            float vx = randRange(-4.0f, 4.0f);
            float vy = randRange(-4.0f, 4.0f);
            e->setLinearVelocity({vx, vy});
        }
    }

    // -----------------------------------------------------------------------
    // Test 6: Gravity Flip
    // -----------------------------------------------------------------------

    void setupGravityFlip() {
        std::cout << "  Gravity reverses every " << FLIP_INTERVAL << " seconds." << std::endl;

        // Scatter boxes throughout the arena
        for (int i = 0; i < 20; ++i) {
            float x = randRange(-ARENA_HALF_W + 0.5f, ARENA_HALF_W - 0.5f);
            float y = randRange(ARENA_FLOOR_Y + 0.5f, ARENA_CEIL_Y - 0.5f);
            float hw = 0.2f + randRange(0.0f, 0.2f);
            float hh = 0.2f + randRange(0.0f, 0.2f);
            vde::Color col = hsv(static_cast<float>(i) / 20.0f, 0.7f, 0.9f);
            addDynamicBox(x, y, hw, hh, col, 0.3f, 0.4f, 0.005f);
        }
    }

    void updateGravityFlip(float dt) {
        m_flipTimer += dt;
        if (m_flipTimer >= FLIP_INTERVAL) {
            m_flipTimer = 0.0f;
            m_gravityDown = !m_gravityDown;
            float gy = m_gravityDown ? -9.81f : 9.81f;
            if (hasPhysics())
                getPhysicsScene()->setGravity({0.0f, gy});
            std::cout << "  [Gravity Flip] Gravity now " << (m_gravityDown ? "DOWN" : "UP")
                      << std::endl;
        }
    }

    // -----------------------------------------------------------------------
    // Test 7: Explosion Burst
    // -----------------------------------------------------------------------

    static constexpr int GRID_COLS = 7;
    static constexpr int GRID_ROWS = 7;

    void setupExplosionBurst() {
        std::cout << "  Boxes packed in a grid. SPACE to explode them outward." << std::endl;

        const float spacing = 0.62f;
        const float startX = -(GRID_COLS - 1) * spacing * 0.5f;
        const float startY = ARENA_MID_Y - (GRID_ROWS - 1) * spacing * 0.5f;

        for (int r = 0; r < GRID_ROWS; ++r) {
            for (int c = 0; c < GRID_COLS; ++c) {
                float x = startX + c * spacing;
                float y = startY + r * spacing;
                float hue = static_cast<float>(r * GRID_COLS + c) / (GRID_ROWS * GRID_COLS);
                vde::Color col = hsv(hue, 0.8f, 1.0f);
                addDynamicBox(x, y, 0.28f, 0.28f, col, 0.4f, 0.3f, 0.01f);
            }
        }
    }

    void triggerExplosion() {
        std::cout << "  [Explosion] Triggering burst!" << std::endl;

        // Find the center of the grid
        const float cx = 0.0f;
        const float cy = ARENA_MID_Y;
        const float impulseStrength = 14.0f;

        for (auto& e : m_testEntities) {
            if (!e)
                continue;
            auto state = e->getPhysicsState();
            glm::vec2 dir = state.position - glm::vec2(cx, cy);
            float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
            if (len < 0.001f) {
                // Dead-center box: random direction
                dir = {randRange(-1.0f, 1.0f), randRange(-1.0f, 1.0f)};
                len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
            }
            glm::vec2 impulse = (dir / len) * impulseStrength;
            e->applyImpulse(impulse);
        }
    }
};

// ============================================================================
// Game
// ============================================================================

class PhysicsShowcaseGame
    : public vde::examples::BaseExampleGame<ShowcaseInputHandler, PhysicsShowcaseScene> {
  public:
    PhysicsShowcaseGame() = default;
};

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    PhysicsShowcaseGame game;
    return vde::examples::runExample(game, "VDE Physics Showcase", 1280, 720, argc, argv);
}
