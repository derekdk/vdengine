/**
 * @file main.cpp
 * @brief Input Script Demo — validates all scripted input features.
 *
 * Eight colored panels display which scripted input feature has fired.
 * Three pip indicators below track SPACE presses from a loop command.
 * A cursor sprite follows mouse movement.
 *
 * The final rendered frame is a unique color pattern that is only correct
 * when all scripted inputs were executed in the right order, making it
 * suitable for golden-image render verification.
 *
 * Features covered by the companion vdescript files:
 *   wait startup, wait ms, wait Ns, wait_frames
 *   press, press shift+key, press ctrl+key
 *   keydown / keyup
 *   hold key duration
 *   click, scroll, mousedown, mouseup, mousemove
 *   print, label, loop
 *   set, assert scene entity_count
 *   screenshot, compare
 */

#include <vde/api/GameAPI.h>

#include <array>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

#include "../ExampleBase.h"

using namespace vde;

// ============================================================================
// Window and world constants
// ============================================================================

constexpr float WORLD_W = 16.0f;
constexpr float WORLD_H = 10.0f;
constexpr int WIN_W = 1280;
constexpr int WIN_H = 720;

// Pixels per world unit
constexpr float PX_PER_UNIT_X = WIN_W / WORLD_W;  // 80
constexpr float PX_PER_UNIT_Y = WIN_H / WORLD_H;  // 72

static float pixelToWorldX(double px) {
    return static_cast<float>((px - WIN_W * 0.5) / PX_PER_UNIT_X);
}

static float pixelToWorldY(double py) {
    return static_cast<float>((WIN_H * 0.5 - py) / PX_PER_UNIT_Y);
}

// ============================================================================
// Layout constants
// ============================================================================

constexpr float PANEL_W = 2.6f;
constexpr float PANEL_H = 2.4f;
constexpr float COL_STEP = 3.1f;  // horizontal distance between column centres
constexpr float ROW_STEP = 3.2f;  // vertical distance between row centres

// 4 columns, 2 rows; centred within the 16×10 world
static float panelCX(int col) {
    return (static_cast<float>(col) - 1.5f) * COL_STEP;
}

static float panelCY(int row) {
    return 1.6f - static_cast<float>(row) * ROW_STEP;
}

// Inactive colour (dark navy-grey) and per-panel active colours
static const Color INACTIVE_COLOR = Color::fromHex(0x2d2d3d);

// clang-format off
static const std::array<uint32_t, 8> PANEL_HEX = {
    0xe74c3c,  // 0 — Red      : press A         (bare key press)
    0x2ecc71,  // 1 — Green    : keydown/keyup W  (explicit pair)
    0x1abc9c,  // 2 — Teal     : hold RIGHT 400   (timed hold)
    0xe67e22,  // 3 — Orange   : press shift+B    (shift modifier)
    0xf1c40f,  // 4 — Yellow   : press ctrl+D     (ctrl modifier, no char)
    0x3498db,  // 5 — Blue     : click            (left mouse click)
    0x9b59b6,  // 6 — Purple   : scroll           (mouse scroll)
    0xecf0f1,  // 7 — White    : mousedown+mouseup (drag gesture)
};

static const std::array<uint32_t, 3> PIP_HEX = {
    0xfd79a8,  // pip 0 — Pink     : 1st SPACE press (loop iteration 1)
    0xa29bfe,  // pip 1 — Lavender : 2nd SPACE press (loop iteration 2)
    0xfdcb6e,  // pip 2 — Gold     : 3rd SPACE press (loop iteration 3)
};
// clang-format on

static const Color CURSOR_ACTIVE_COLOR = Color::fromHex(0xffffff);
static const Color CURSOR_IDLE_COLOR = Color::fromHex(0x636e72);

// ============================================================================
// Input handler
// ============================================================================

class InputScriptDemoHandler : public vde::examples::BaseExampleInputHandler {
  public:
    // One-shot flags — set by event callbacks, cleared by scene each frame

    bool pressA = false;         // KEY_A pressed (bare press feature)
    bool releasedW = false;      // KEY_W released after a keydown (keydown/keyup feature)
    bool releasedRight = false;  // KEY_RIGHT released after a hold (hold feature)
    bool shiftBPressed = false;  // KEY_B while shift held (shift modifier feature)
    bool ctrlDPressed = false;   // KEY_D while ctrl held (ctrl modifier feature)
    bool spacePressed = false;   // KEY_SPACE pressed (loop feature)

    bool leftClicked = false;   // click in panel-5 zone (click feature)
    bool scrolled = false;      // any scroll delta received (scroll feature)
    bool dragComplete = false;  // mousedown+mouseup in panel-7 zone (drag feature)

    bool mouseMoved = false;  // any mousemove received (mousemove feature)
    double mouseMoveX = 0.0;
    double mouseMoveY = 0.0;

    // ── Internal state for modifier / held-key tracking ──────────────────────

    bool shiftHeld = false;
    bool ctrlHeld = false;
    bool wHeld = false;
    bool rightHeld = false;
    bool mouseDownInDragZone = false;

    void onKeyPress(int key) override {
        BaseExampleInputHandler::onKeyPress(key);

        if (key == KEY_LEFT_SHIFT || key == KEY_RIGHT_SHIFT)
            shiftHeld = true;
        if (key == KEY_LEFT_CONTROL || key == KEY_RIGHT_CONTROL)
            ctrlHeld = true;

        if (key == KEY_A)
            pressA = true;
        if (key == KEY_W)
            wHeld = true;
        if (key == KEY_RIGHT)
            rightHeld = true;
        if (key == KEY_B && shiftHeld)
            shiftBPressed = true;
        if (key == KEY_D && ctrlHeld)
            ctrlDPressed = true;
        if (key == KEY_SPACE)
            spacePressed = true;
    }

    void onKeyRelease(int key) override {
        if (key == KEY_LEFT_SHIFT || key == KEY_RIGHT_SHIFT)
            shiftHeld = false;
        if (key == KEY_LEFT_CONTROL || key == KEY_RIGHT_CONTROL)
            ctrlHeld = false;
        if (key == KEY_W && wHeld) {
            wHeld = false;
            releasedW = true;
        }
        if (key == KEY_RIGHT && rightHeld) {
            rightHeld = false;
            releasedRight = true;
        }
    }

    void onMouseButtonPress(int button, double x, double y) override {
        if (button != MOUSE_BUTTON_LEFT)
            return;

        float wx = pixelToWorldX(x);
        float wy = pixelToWorldY(y);

        // Panel 5 click zone: col=1, row=1
        {
            float cx = panelCX(1);
            float cy = panelCY(1);
            if (std::abs(wx - cx) <= PANEL_W * 0.5f && std::abs(wy - cy) <= PANEL_H * 0.5f) {
                leftClicked = true;
            }
        }

        // Panel 7 drag zone: col=3, row=1
        {
            float cx = panelCX(3);
            float cy = panelCY(1);
            if (std::abs(wx - cx) <= PANEL_W * 0.5f && std::abs(wy - cy) <= PANEL_H * 0.5f) {
                mouseDownInDragZone = true;
            }
        }
    }

    void onMouseButtonRelease(int button, double /*x*/, double /*y*/) override {
        if (button == MOUSE_BUTTON_LEFT && mouseDownInDragZone) {
            mouseDownInDragZone = false;
            dragComplete = true;
        }
    }

    void onMouseScroll(double /*x*/, double dy) override {
        if (dy != 0.0)
            scrolled = true;
    }

    void onMouseMove(double x, double y) override {
        mouseMoveX = x;
        mouseMoveY = y;
        mouseMoved = true;
    }
};

// ============================================================================
// Scene
// ============================================================================

class InputScriptDemoScene : public vde::examples::BaseExampleScene {
  public:
    InputScriptDemoScene() : BaseExampleScene(30.0f) {}

    void onEnter() override {
        printExampleHeader();
        setup2D(WORLD_W, WORLD_H, Color::fromHex(0x1a1a2e));

        // 8 indicator panels in a 4×2 grid
        for (int row = 0; row < 2; ++row) {
            for (int col = 0; col < 4; ++col) {
                int idx = row * 4 + col;
                auto panel = addEntity<SpriteEntity>();
                panel->setName("panel_" + std::to_string(idx));
                panel->setScale(PANEL_W, PANEL_H, 1.0f);
                panel->setPosition(panelCX(col), panelCY(row), 0.0f);
                panel->setColor(INACTIVE_COLOR);
                m_panels[idx] = panel->getId();
            }
        }

        // 3 pip indicators: bottom-centre, equally spaced
        for (int i = 0; i < 3; ++i) {
            auto pip = addEntity<SpriteEntity>();
            pip->setName("pip_" + std::to_string(i));
            pip->setScale(0.5f, 0.5f, 1.0f);
            pip->setPosition((static_cast<float>(i) - 1.0f) * 0.75f, -4.2f, 0.0f);
            pip->setColor(INACTIVE_COLOR);
            m_pips[i] = pip->getId();
        }

        // Cursor sprite: starts at centre, idle colour
        auto cursor = addEntity<SpriteEntity>();
        cursor->setName("cursor");
        cursor->setScale(0.35f, 0.35f, 1.0f);
        cursor->setPosition(0.0f, 0.0f, 0.0f);
        cursor->setColor(CURSOR_IDLE_COLOR);
        m_cursor = cursor->getId();

        // Total: 8 panels + 3 pips + 1 cursor = 12 entities
    }

    void update(float deltaTime) override {
        BaseExampleScene::update(deltaTime);

        auto* input = dynamic_cast<InputScriptDemoHandler*>(getInputHandler());
        if (!input)
            return;

        // Panel 0 — bare press A
        if (input->pressA && !m_panelActive[0])
            activatePanel(0);
        input->pressA = false;

        // Panel 1 — keydown W then keyup W
        if (input->releasedW && !m_panelActive[1])
            activatePanel(1);
        input->releasedW = false;

        // Panel 2 — hold RIGHT (detected on key release)
        if (input->releasedRight && !m_panelActive[2])
            activatePanel(2);
        input->releasedRight = false;

        // Panel 3 — press shift+B
        if (input->shiftBPressed && !m_panelActive[3])
            activatePanel(3);
        input->shiftBPressed = false;

        // Panel 4 — press ctrl+D (no char emitted)
        if (input->ctrlDPressed && !m_panelActive[4])
            activatePanel(4);
        input->ctrlDPressed = false;

        // Panel 5 — mouse click in the panel-5 zone
        if (input->leftClicked && !m_panelActive[5])
            activatePanel(5);
        input->leftClicked = false;

        // Panel 6 — scroll event
        if (input->scrolled && !m_panelActive[6])
            activatePanel(6);
        input->scrolled = false;

        // Panel 7 — mousedown + mouseup (drag gesture in panel-7 zone)
        if (input->dragComplete && !m_panelActive[7])
            activatePanel(7);
        input->dragComplete = false;

        // Pips — each SPACE press from the loop
        if (input->spacePressed && m_spaceCount < 3) {
            activatePip(m_spaceCount);
            ++m_spaceCount;
        }
        input->spacePressed = false;

        // Cursor — follows mouse position
        if (input->mouseMoved) {
            auto* cursor = static_cast<SpriteEntity*>(getEntity(m_cursor));
            if (cursor) {
                float wx = pixelToWorldX(input->mouseMoveX);
                float wy = pixelToWorldY(input->mouseMoveY);
                cursor->setPosition(wx, wy, 0.0f);
                cursor->setColor(CURSOR_ACTIVE_COLOR);
            }
            input->mouseMoved = false;
        }
    }

  protected:
    std::string getExampleName() const override { return "Input Script Demo"; }

    std::vector<std::string> getFeatures() const override {
        return {
            "press (bare key)",
            "keydown / keyup",
            "hold (timed key hold)",
            "modifier keys: shift, ctrl",
            "mouse click, scroll, mousedown/mouseup",
            "mousemove (cursor tracking)",
            "label + loop (control flow)",
            "set + assert (variable + assertion)",
            "screenshot + compare (render verification)",
        };
    }

    std::vector<std::string> getExpectedVisuals() const override {
        return {
            "8 coloured panels in a 4×2 grid — dark until the matching input fires",
            "3 small pip indicators at bottom centre — lit by SPACE presses via a loop",
            "A small white cursor sprite that follows mousemove events",
        };
    }

  private:
    std::array<EntityId, 8> m_panels{};
    std::array<EntityId, 3> m_pips{};
    EntityId m_cursor{};

    std::array<bool, 8> m_panelActive{};
    int m_spaceCount = 0;

    void activatePanel(int idx) {
        auto* panel = static_cast<SpriteEntity*>(getEntity(m_panels[idx]));
        if (panel) {
            panel->setColor(Color::fromHex(PANEL_HEX[idx]));
            m_panelActive[idx] = true;
        }
    }

    void activatePip(int idx) {
        auto* pip = static_cast<SpriteEntity*>(getEntity(m_pips[idx]));
        if (pip)
            pip->setColor(Color::fromHex(PIP_HEX[idx]));
    }
};

// ============================================================================
// Game + main
// ============================================================================

class InputScriptDemoGame
    : public vde::examples::BaseExampleGame<InputScriptDemoHandler, InputScriptDemoScene> {};

int main(int argc, char** argv) {
    InputScriptDemoGame demo;
    return vde::examples::runExample(demo, "VDE Input Script Demo", WIN_W, WIN_H, argc, argv);
}
