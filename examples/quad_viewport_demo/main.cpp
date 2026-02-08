/**
 * @file main.cpp
 * @brief Quad Viewport Demo - Four simultaneous scenes in split-screen
 *
 * This example demonstrates rendering four independent "scenes" simultaneously
 * by dividing the screen into four equal quadrants, each with its own theme:
 *
 *   +------------------+------------------+
 *   |   Top-Left:      |   Top-Right:     |
 *   |   SPACE          |   FOREST         |
 *   |   (rotating      |   (swaying       |
 *   |    planets)       |    trees)        |
 *   +------------------+------------------+
 *   |   Bottom-Left:   |   Bottom-Right:  |
 *   |   CITY           |   OCEAN          |
 *   |   (pulsing       |   (animated      |
 *   |    buildings)     |    waves)        |
 *   +------------------+------------------+
 *
 * Each quadrant runs independently with its own animations and visual style.
 */

#include <vde/api/GameAPI.h>

#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "../ExampleBase.h"

using namespace vde;

// ============================================================================
// Helper: cast Entity* to SpriteEntity*
// ============================================================================

static SpriteEntity* asSprite(Entity* e) {
    return dynamic_cast<SpriteEntity*>(e);
}

// ============================================================================
// Constants for quadrant layout
// ============================================================================

// Camera views 32 x 18 world units (matching 16:9 aspect ratio)
static constexpr float kWorldWidth = 32.0f;
static constexpr float kWorldHeight = 18.0f;
static constexpr float kHalfW = kWorldWidth * 0.5f;   // 16.0
static constexpr float kHalfH = kWorldHeight * 0.5f;  // 9.0
static constexpr float kQuadW = kHalfW;               // 16.0 each quadrant
static constexpr float kQuadH = kHalfH;               // 9.0 each quadrant
static constexpr float kDividerThickness = 0.08f;

// Quadrant center positions
static constexpr float kTLx = -kHalfW * 0.5f;  // -8.0
static constexpr float kTLy = kHalfH * 0.5f;   //  4.5
static constexpr float kTRx = kHalfW * 0.5f;   //  8.0
static constexpr float kTRy = kHalfH * 0.5f;   //  4.5
static constexpr float kBLx = -kHalfW * 0.5f;  // -8.0
static constexpr float kBLy = -kHalfH * 0.5f;  // -4.5
static constexpr float kBRx = kHalfW * 0.5f;   //  8.0
static constexpr float kBRy = -kHalfH * 0.5f;  // -4.5

// ============================================================================
// Input Handler
// ============================================================================

class QuadViewportInputHandler : public vde::examples::BaseExampleInputHandler {
  public:
    void onKeyPress(int key) override {
        BaseExampleInputHandler::onKeyPress(key);

        if (key == KEY_SPACE)
            m_spacePressed = true;
        if (key == KEY_1)
            m_toggleQuadrant = 1;
        if (key == KEY_2)
            m_toggleQuadrant = 2;
        if (key == KEY_3)
            m_toggleQuadrant = 3;
        if (key == KEY_4)
            m_toggleQuadrant = 4;
        if (key == KEY_R)
            m_resetPressed = true;
    }

    bool consumeSpace() {
        bool v = m_spacePressed;
        m_spacePressed = false;
        return v;
    }

    int consumeToggle() {
        int v = m_toggleQuadrant;
        m_toggleQuadrant = 0;
        return v;
    }

    bool consumeReset() {
        bool v = m_resetPressed;
        m_resetPressed = false;
        return v;
    }

  private:
    bool m_spacePressed = false;
    int m_toggleQuadrant = 0;
    bool m_resetPressed = false;
};

// ============================================================================
// Helper: create a colored background panel for a quadrant
// ============================================================================

static void createBackgroundPanel(Scene* scene, float cx, float cy, const Color& color,
                                  const std::string& name) {
    auto bg = scene->addEntity<SpriteEntity>();
    bg->setPosition(cx, cy, -0.5f);  // Behind other entities
    bg->setColor(color);
    bg->setScale(kQuadW - kDividerThickness, kQuadH - kDividerThickness, 1.0f);
    bg->setName(name);
}

// ============================================================================
// Space Quadrant (Top-Left) — rotating planets around a sun
// ============================================================================

static void createSpaceEntities(Scene* scene) {
    float cx = kTLx;
    float cy = kTLy;

    // Sun at center
    auto sun = scene->addEntity<SpriteEntity>();
    sun->setPosition(cx, cy, 0.1f);
    sun->setColor(Color::fromHex(0xffcc00));
    sun->setScale(0.8f, 0.8f, 1.0f);
    sun->setName("Space_Sun");

    // Planets with orbital data encoded in name
    struct PlanetDef {
        const char* name;
        uint32_t color;
        float radius;
        float orbitSpeed;
        float size;
    };

    PlanetDef planets[] = {
        {"Space_Planet1", 0xff4444, 2.0f, 1.2f, 0.4f},
        {"Space_Planet2", 0x4488ff, 3.2f, 0.7f, 0.35f},
        {"Space_Planet3", 0x44ff88, 4.2f, 0.45f, 0.5f},
        {"Space_Planet4", 0xff88ff, 1.4f, 2.0f, 0.25f},
        {"Space_Planet5", 0xffaa44, 5.0f, 0.3f, 0.3f},
    };

    for (const auto& def : planets) {
        auto p = scene->addEntity<SpriteEntity>();
        p->setPosition(cx + def.radius, cy, 0.2f);
        p->setColor(Color::fromHex(def.color));
        p->setScale(def.size, def.size, 1.0f);
        p->setName(def.name);
    }

    // Stars (small white dots)
    for (int i = 0; i < 25; ++i) {
        auto star = scene->addEntity<SpriteEntity>();
        float sx = cx + (static_cast<float>((i * 37 + 13) % 140) / 10.0f - 7.0f);
        float sy = cy + (static_cast<float>((i * 53 + 7) % 80) / 10.0f - 4.0f);
        star->setPosition(sx, sy, 0.0f);
        float brightness = 0.5f + (i % 5) * 0.1f;
        star->setColor(Color(brightness, brightness, brightness + 0.1f));
        float sz = 0.06f + (i % 3) * 0.03f;
        star->setScale(sz, sz, 1.0f);
        star->setName("Space_Star_" + std::to_string(i));
    }
}

static void updateSpaceEntities(Scene* scene, float totalTime) {
    float cx = kTLx;
    float cy = kTLy;

    // Orbital parameters per planet
    struct OrbitParams {
        float radius;
        float speed;
    };
    OrbitParams orbits[] = {
        {2.0f, 1.2f}, {3.2f, 0.7f}, {4.2f, 0.45f}, {1.4f, 2.0f}, {5.0f, 0.3f},
    };

    for (int i = 0; i < 5; ++i) {
        std::string name = "Space_Planet" + std::to_string(i + 1);
        auto* planet = scene->getEntityByName(name);
        if (planet) {
            float angle = totalTime * orbits[i].speed + i * 1.2f;
            float px = cx + orbits[i].radius * std::cos(angle);
            float py = cy + orbits[i].radius * std::sin(angle);
            planet->setPosition(px, py, 0.2f);
        }
    }

    // Twinkle stars
    for (int i = 0; i < 25; ++i) {
        auto* star = asSprite(scene->getEntityByName("Space_Star_" + std::to_string(i)));
        if (star) {
            float twinkle = 0.4f + 0.3f * std::sin(totalTime * 3.0f + i * 0.7f);
            float sz = 0.06f + 0.04f * std::sin(totalTime * 2.0f + i * 1.1f);
            star->setColor(Color(twinkle, twinkle, twinkle + 0.15f));
            star->setScale(sz, sz, 1.0f);
        }
    }
}

// ============================================================================
// Forest Quadrant (Top-Right) — swaying trees with wind
// ============================================================================

static void createForestEntities(Scene* scene) {
    float cx = kTRx;
    float cy = kTRy;

    const Color treeColors[] = {
        Color::fromHex(0x228b22), Color::fromHex(0x2e8b57),
        Color::fromHex(0x006400), Color::fromHex(0x32cd32),
        Color::fromHex(0x3cb371),
    };

    // Ground
    auto ground = scene->addEntity<SpriteEntity>();
    ground->setPosition(cx, cy - 3.5f, 0.0f);
    ground->setColor(Color::fromHex(0x2d5a1e));
    ground->setScale(kQuadW - 0.3f, 1.5f, 1.0f);
    ground->setName("Forest_Ground");

    // Trees — various sizes and positions
    for (int i = 0; i < 18; ++i) {
        // Trunk
        auto trunk = scene->addEntity<SpriteEntity>();
        float tx = cx + (static_cast<float>((i * 41 + 5) % 130) / 10.0f - 6.5f);
        float ty = cy + (static_cast<float>((i * 29 + 11) % 60) / 10.0f - 3.0f);
        float trunkH = 0.6f + (i % 4) * 0.2f;
        trunk->setPosition(tx, ty, 0.05f);
        trunk->setColor(Color::fromHex(0x8b4513));
        trunk->setScale(0.15f, trunkH, 1.0f);
        trunk->setName("Forest_Trunk_" + std::to_string(i));

        // Canopy (top of tree)
        auto canopy = scene->addEntity<SpriteEntity>();
        float canopySize = 0.5f + (i % 5) * 0.15f;
        canopy->setPosition(tx, ty + trunkH * 0.5f + canopySize * 0.3f, 0.1f);
        canopy->setColor(treeColors[i % 5]);
        canopy->setScale(canopySize, canopySize * 1.2f, 1.0f);
        canopy->setName("Forest_Canopy_" + std::to_string(i));
    }

    // Flowers
    const Color flowerColors[] = {
        Color::fromHex(0xff69b4), Color::fromHex(0xff4500),
        Color::fromHex(0xffd700), Color::fromHex(0xda70d6),
    };

    for (int i = 0; i < 8; ++i) {
        auto flower = scene->addEntity<SpriteEntity>();
        float fx = cx + (static_cast<float>((i * 67 + 23) % 130) / 10.0f - 6.5f);
        float fy = cy - 2.0f + (static_cast<float>((i * 43 + 17) % 20) / 10.0f);
        flower->setPosition(fx, fy, 0.15f);
        flower->setColor(flowerColors[i % 4]);
        flower->setScale(0.15f, 0.15f, 1.0f);
        flower->setName("Forest_Flower_" + std::to_string(i));
    }
}

static void updateForestEntities(Scene* scene, float totalTime) {
    // Sway canopies
    for (int i = 0; i < 18; ++i) {
        auto* canopy = scene->getEntityByName("Forest_Canopy_" + std::to_string(i));
        if (canopy) {
            auto pos = canopy->getPosition();
            float sway = 0.08f * std::sin(totalTime * 1.5f + pos.x * 0.8f + i * 0.4f);
            // Sway the x position slightly
            float baseX = kTRx + (static_cast<float>((i * 41 + 5) % 130) / 10.0f - 6.5f);
            canopy->setPosition(baseX + sway, pos.y, pos.z);
        }
    }

    // Gently bob flowers
    for (int i = 0; i < 8; ++i) {
        auto* flower = scene->getEntityByName("Forest_Flower_" + std::to_string(i));
        if (flower) {
            float bob = 0.03f * std::sin(totalTime * 2.0f + i * 1.3f);
            float baseScale = 0.15f;
            flower->setScale(baseScale + bob, baseScale + bob, 1.0f);
        }
    }
}

// ============================================================================
// City Quadrant (Bottom-Left) — skyline with pulsing lights
// ============================================================================

static void createCityEntities(Scene* scene) {
    float cx = kBLx;
    float cy = kBLy;

    const Color buildingColors[] = {
        Color::fromHex(0x555566), Color::fromHex(0x666677), Color::fromHex(0x444455),
        Color::fromHex(0x777788), Color::fromHex(0x888899),
    };

    // Road
    auto road = scene->addEntity<SpriteEntity>();
    road->setPosition(cx, cy - 3.5f, 0.0f);
    road->setColor(Color::fromHex(0x333333));
    road->setScale(kQuadW - 0.3f, 1.5f, 1.0f);
    road->setName("City_Road");

    // Road markings
    for (int i = 0; i < 6; ++i) {
        auto mark = scene->addEntity<SpriteEntity>();
        mark->setPosition(cx - 5.5f + i * 2.2f, cy - 3.5f, 0.05f);
        mark->setColor(Color::fromHex(0xcccc44));
        mark->setScale(0.8f, 0.06f, 1.0f);
        mark->setName("City_RoadMark_" + std::to_string(i));
    }

    // Buildings — varying heights
    for (int i = 0; i < 12; ++i) {
        auto building = scene->addEntity<SpriteEntity>();
        float bx = cx - 6.5f + i * 1.1f;
        float bHeight = 1.5f + static_cast<float>((i * 7 + 3) % 5) * 0.8f;
        float by = cy - 2.8f + bHeight * 0.5f;
        building->setPosition(bx, by, 0.1f);
        building->setColor(buildingColors[i % 5]);
        building->setScale(0.9f, bHeight, 1.0f);
        building->setName("City_Building_" + std::to_string(i));

        // Windows (small bright dots on the building)
        int numWindows = static_cast<int>(bHeight / 0.5f);
        for (int w = 0; w < std::min(numWindows, 6); ++w) {
            auto window = scene->addEntity<SpriteEntity>();
            float wy = by - bHeight * 0.4f + w * 0.55f;
            window->setPosition(bx - 0.15f, wy, 0.15f);
            window->setColor(Color::fromHex(0xffee88));
            window->setScale(0.12f, 0.12f, 1.0f);
            window->setName("City_Window_" + std::to_string(i) + "_" + std::to_string(w));

            auto window2 = scene->addEntity<SpriteEntity>();
            window2->setPosition(bx + 0.15f, wy, 0.15f);
            window2->setColor(Color::fromHex(0xffee88));
            window2->setScale(0.12f, 0.12f, 1.0f);
            window2->setName("City_Window2_" + std::to_string(i) + "_" + std::to_string(w));
        }
    }

    // Moon
    auto moon = scene->addEntity<SpriteEntity>();
    moon->setPosition(cx + 5.5f, cy + 3.0f, 0.05f);
    moon->setColor(Color::fromHex(0xeeeedd));
    moon->setScale(0.7f, 0.7f, 1.0f);
    moon->setName("City_Moon");
}

static void updateCityEntities(Scene* scene, float totalTime) {
    // Flicker windows randomly
    for (int i = 0; i < 12; ++i) {
        float bHeight = 1.5f + static_cast<float>((i * 7 + 3) % 5) * 0.8f;
        int numWindows = static_cast<int>(bHeight / 0.5f);
        for (int w = 0; w < std::min(numWindows, 6); ++w) {
            auto* window = asSprite(
                scene->getEntityByName("City_Window_" + std::to_string(i) + "_" + std::to_string(w)));
            auto* window2 = asSprite(scene->getEntityByName("City_Window2_" + std::to_string(i) + "_" +
                                                    std::to_string(w)));
            if (window) {
                float flicker = 0.5f + 0.5f * std::sin(totalTime * 2.5f + i * 1.3f + w * 0.9f);
                float r = 0.6f + 0.4f * flicker;
                float g = 0.55f + 0.35f * flicker;
                float b = 0.3f + 0.2f * flicker;
                window->setColor(Color(r, g, b));
            }
            if (window2) {
                // Slightly different phase for variety
                float flicker = 0.5f + 0.5f * std::sin(totalTime * 2.1f + i * 0.8f + w * 1.4f);
                float r = 0.6f + 0.4f * flicker;
                float g = 0.55f + 0.35f * flicker;
                float b = 0.3f + 0.2f * flicker;
                window2->setColor(Color(r, g, b));
            }
        }
    }

    // Pulse buildings subtly
    for (int i = 0; i < 12; ++i) {
        auto* building = asSprite(scene->getEntityByName("City_Building_" + std::to_string(i)));
        if (building) {
            float pulse = 0.03f * std::sin(totalTime * 1.5f + i * 0.5f);
            uint32_t baseColors[] = {0x555566, 0x666677, 0x444455, 0x777788, 0x888899};
            Color base = Color::fromHex(baseColors[i % 5]);
            building->setColor(
                Color(std::clamp(base.r + pulse, 0.0f, 1.0f),
                      std::clamp(base.g + pulse, 0.0f, 1.0f),
                      std::clamp(base.b + pulse * 1.5f, 0.0f, 1.0f)));
        }
    }

    // Moon glow
    auto* moon = asSprite(scene->getEntityByName("City_Moon"));
    if (moon) {
        float glow = 0.93f + 0.07f * std::sin(totalTime * 0.5f);
        moon->setColor(Color(glow, glow, glow * 0.95f));
    }
}

// ============================================================================
// Ocean Quadrant (Bottom-Right) — animated waves and a boat
// ============================================================================

static void createOceanEntities(Scene* scene) {
    float cx = kBRx;
    float cy = kBRy;

    // Wave rows
    for (int row = -3; row <= 3; ++row) {
        for (int col = -5; col <= 5; ++col) {
            auto wave = scene->addEntity<SpriteEntity>();
            float wx = cx + col * 1.3f;
            float wy = cy + row * 1.2f;
            wave->setPosition(wx, wy, 0.0f);
            float t = (row + 3) / 6.0f;
            wave->setColor(Color(0.05f + t * 0.2f, 0.1f + t * 0.3f, 0.3f + t * 0.5f));
            wave->setScale(1.1f, 0.3f, 1.0f);
            wave->setName("Ocean_Wave_" + std::to_string(row + 3) + "_" + std::to_string(col + 5));
        }
    }

    // Boat
    auto hull = scene->addEntity<SpriteEntity>();
    hull->setPosition(cx, cy, 0.3f);
    hull->setColor(Color::fromHex(0x8b4513));
    hull->setScale(1.0f, 0.3f, 1.0f);
    hull->setName("Ocean_Boat_Hull");

    auto mast = scene->addEntity<SpriteEntity>();
    mast->setPosition(cx, cy + 0.4f, 0.35f);
    mast->setColor(Color::fromHex(0xdddddd));
    mast->setScale(0.06f, 0.7f, 1.0f);
    mast->setName("Ocean_Boat_Mast");

    auto sail = scene->addEntity<SpriteEntity>();
    sail->setPosition(cx + 0.2f, cy + 0.5f, 0.32f);
    sail->setColor(Color::fromHex(0xffffee));
    sail->setScale(0.5f, 0.5f, 1.0f);
    sail->setName("Ocean_Boat_Sail");

    // Seagulls (small V shapes approximated as tiny sprites)
    for (int i = 0; i < 4; ++i) {
        auto gull = scene->addEntity<SpriteEntity>();
        float gx = cx - 3.0f + i * 2.5f;
        float gy = cy + 2.5f + (i % 2) * 0.5f;
        gull->setPosition(gx, gy, 0.4f);
        gull->setColor(Color::fromHex(0xcccccc));
        gull->setScale(0.2f, 0.06f, 1.0f);
        gull->setName("Ocean_Gull_" + std::to_string(i));
    }

    // Sun (top-right corner of ocean quadrant)
    auto oceanSun = scene->addEntity<SpriteEntity>();
    oceanSun->setPosition(cx + 6.5f, cy + 3.0f, 0.05f);
    oceanSun->setColor(Color::fromHex(0xffdd44));
    oceanSun->setScale(0.9f, 0.9f, 1.0f);
    oceanSun->setName("Ocean_Sun");
}

static void updateOceanEntities(Scene* scene, float totalTime) {
    float cx = kBRx;
    float cy = kBRy;

    // Animate waves
    for (int row = 0; row < 7; ++row) {
        for (int col = 0; col < 11; ++col) {
            auto* wave =
                scene->getEntityByName("Ocean_Wave_" + std::to_string(row) + "_" + std::to_string(col));
            if (wave) {
                float baseX = cx + (col - 5) * 1.3f;
                float baseY = cy + (row - 3) * 1.2f;
                float waveOffset =
                    0.15f * std::sin(totalTime * 2.0f + baseX * 0.6f + baseY * 0.4f);
                float scaleY =
                    0.3f + 0.1f * std::sin(totalTime * 1.8f + baseX * 0.8f + baseY * 0.3f);
                wave->setPosition(baseX + waveOffset * 0.3f, baseY + waveOffset, wave->getPosition().z);
                wave->setScale(1.1f, scaleY, 1.0f);
            }
        }
    }

    // Bob boat
    auto* hull = scene->getEntityByName("Ocean_Boat_Hull");
    auto* mast = scene->getEntityByName("Ocean_Boat_Mast");
    auto* sail = scene->getEntityByName("Ocean_Boat_Sail");
    if (hull) {
        float boatBob = 0.08f * std::sin(totalTime * 2.5f);
        float boatDrift = 1.5f * std::sin(totalTime * 0.15f);
        hull->setPosition(cx + boatDrift, cy + boatBob, 0.3f);
        if (mast) {
            mast->setPosition(cx + boatDrift, cy + 0.4f + boatBob, 0.35f);
        }
        if (sail) {
            float sailFlutter = 0.03f * std::sin(totalTime * 4.0f);
            sail->setPosition(cx + 0.2f + boatDrift + sailFlutter, cy + 0.5f + boatBob, 0.32f);
        }
    }

    // Move seagulls
    for (int i = 0; i < 4; ++i) {
        auto* gull = scene->getEntityByName("Ocean_Gull_" + std::to_string(i));
        if (gull) {
            float baseX = cx - 3.0f + i * 2.5f;
            float baseY = cy + 2.5f + (i % 2) * 0.5f;
            float gx = baseX + 1.5f * std::sin(totalTime * 0.4f + i * 1.5f);
            float gy = baseY + 0.3f * std::sin(totalTime * 0.6f + i * 2.0f);
            gull->setPosition(gx, gy, 0.4f);
            // Wing flap
            float flapW = 0.2f + 0.05f * std::sin(totalTime * 6.0f + i * 3.0f);
            gull->setScale(flapW, 0.06f, 1.0f);
        }
    }
}

// ============================================================================
// Quad Viewport Scene — single scene containing all 4 quadrants
// ============================================================================

class QuadViewportScene : public vde::examples::BaseExampleScene {
  public:
    QuadViewportScene() : BaseExampleScene(30.0f) {}

    void onEnter() override {
        printExampleHeader();

        // 2D camera covering the full viewport
        auto* cam = new Camera2D(kWorldWidth, kWorldHeight);
        cam->setPosition(0.0f, 0.0f);
        setCamera(cam);

        setLightBox(new SimpleColorLightBox(Color::white()));
        setBackgroundColor(Color::fromHex(0x111111));

        // Background panels for each quadrant
        createBackgroundPanel(this, kTLx, kTLy, Color::fromHex(0x050510), "BG_Space");
        createBackgroundPanel(this, kTRx, kTRy, Color::fromHex(0x1a3a1a), "BG_Forest");
        createBackgroundPanel(this, kBLx, kBLy, Color::fromHex(0x252530), "BG_City");
        createBackgroundPanel(this, kBRx, kBRy, Color::fromHex(0x0a1628), "BG_Ocean");

        // Dividing lines
        // Horizontal divider
        auto hDiv = addEntity<SpriteEntity>();
        hDiv->setPosition(0.0f, 0.0f, 0.9f);
        hDiv->setColor(Color::fromHex(0x888888));
        hDiv->setScale(kWorldWidth, kDividerThickness, 1.0f);
        hDiv->setName("Divider_H");

        // Vertical divider
        auto vDiv = addEntity<SpriteEntity>();
        vDiv->setPosition(0.0f, 0.0f, 0.9f);
        vDiv->setColor(Color::fromHex(0x888888));
        vDiv->setScale(kDividerThickness, kWorldHeight, 1.0f);
        vDiv->setName("Divider_V");

        // Quadrant labels (small colored markers in corners)
        createLabel(kTLx - 5.5f, kTLy + 3.2f, Color::fromHex(0xffcc00), "LBL_Space");
        createLabel(kTRx - 5.5f, kTRy + 3.2f, Color::fromHex(0x44ff44), "LBL_Forest");
        createLabel(kBLx - 5.5f, kBLy + 3.2f, Color::fromHex(0x8888aa), "LBL_City");
        createLabel(kBRx - 5.5f, kBRy + 3.2f, Color::fromHex(0x4488ff), "LBL_Ocean");

        // Create entities for each quadrant
        createSpaceEntities(this);
        createForestEntities(this);
        createCityEntities(this);
        createOceanEntities(this);

        m_totalTime = 0.0f;
    }

    void update(float deltaTime) override {
        m_totalTime += deltaTime;

        // Handle input
        auto* input = dynamic_cast<QuadViewportInputHandler*>(getInputHandler());
        if (input) {
            int toggle = input->consumeToggle();
            if (toggle >= 1 && toggle <= 4) {
                m_quadrantActive[toggle - 1] = !m_quadrantActive[toggle - 1];
                const char* names[] = {"Space", "Forest", "City", "Ocean"};
                std::cout << names[toggle - 1] << " quadrant: "
                          << (m_quadrantActive[toggle - 1] ? "RUNNING" : "PAUSED") << std::endl;
            }

            if (input->consumeReset()) {
                m_totalTime = 0.0f;
                std::cout << "Animations reset!" << std::endl;
            }

            if (input->consumeSpace()) {
                printStatus();
            }
        }

        // Update each active quadrant
        if (m_quadrantActive[0])
            updateSpaceEntities(this, m_totalTime);
        if (m_quadrantActive[1])
            updateForestEntities(this, m_totalTime);
        if (m_quadrantActive[2])
            updateCityEntities(this, m_totalTime);
        if (m_quadrantActive[3])
            updateOceanEntities(this, m_totalTime);

        BaseExampleScene::update(deltaTime);
    }

  protected:
    std::string getExampleName() const override { return "Quad Viewport (4 Scenes)"; }

    std::vector<std::string> getFeatures() const override {
        return {
            "Screen split into 4 equal quadrants",
            "Each quadrant runs an independent animated scene",
            "Space scene with orbiting planets",
            "Forest scene with swaying trees",
            "City scene with twinkling window lights",
            "Ocean scene with rolling waves and a boat",
            "Per-quadrant pause/resume controls",
        };
    }

    std::vector<std::string> getExpectedVisuals() const override {
        return {
            "Four distinct colored quadrants divided by gray lines",
            "Top-left (Space): Dark background, yellow sun, orbiting colored planets, twinkling stars",
            "Top-right (Forest): Green background, brown trunks with green canopies, colored flowers",
            "Bottom-left (City): Dark gray background, building skyline with flickering windows, moon",
            "Bottom-right (Ocean): Deep blue background, undulating blue waves, brown boat, seagulls",
        };
    }

    std::vector<std::string> getControls() const override {
        return {
            "1     - Toggle Space quadrant (top-left)",
            "2     - Toggle Forest quadrant (top-right)",
            "3     - Toggle City quadrant (bottom-left)",
            "4     - Toggle Ocean quadrant (bottom-right)",
            "R     - Reset animations",
            "SPACE - Print status",
        };
    }

  private:
    float m_totalTime = 0.0f;
    bool m_quadrantActive[4] = {true, true, true, true};

    void createLabel(float x, float y, const Color& color, const std::string& name) {
        auto label = addEntity<SpriteEntity>();
        label->setPosition(x, y, 0.8f);
        label->setColor(color);
        label->setScale(0.3f, 0.3f, 1.0f);
        label->setName(name);
    }

    void printStatus() {
        const char* names[] = {"Space (TL)", "Forest (TR)", "City (BL)", "Ocean (BR)"};
        std::cout << "\n--- Quad Viewport Status ---" << std::endl;
        for (int i = 0; i < 4; ++i) {
            std::cout << "  " << (i + 1) << ") " << names[i] << ": "
                      << (m_quadrantActive[i] ? "RUNNING" : "PAUSED") << std::endl;
        }
        std::cout << "  Time: " << std::fixed << std::setprecision(1) << m_totalTime << "s"
                  << std::endl;
        std::cout << "----------------------------\n" << std::endl;
    }
};

// ============================================================================
// Game Class
// ============================================================================

class QuadViewportDemo
    : public vde::examples::BaseExampleGame<QuadViewportInputHandler, QuadViewportScene> {
  public:
    QuadViewportDemo() = default;
};

// ============================================================================
// Main
// ============================================================================

int main() {
    QuadViewportDemo demo;
    return vde::examples::runExample(demo, "VDE Quad Viewport Demo", 1280, 720);
}
