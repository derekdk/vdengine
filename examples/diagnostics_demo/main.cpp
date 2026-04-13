/**
 * @file main.cpp
 * @brief Diagnostics Demo - showcases SceneDiagnostics and smoke test assertions.
 *
 * This example demonstrates:
 * - SceneDiagnostics entity type classification (Mesh, Sprite, Text, Physics)
 * - Lifecycle counters (onEnter, onExit, onPause, onResume)
 * - Dynamic entity add/remove with real-time counter tracking
 * - Multi-scene setup for verifying isFocused and scene-level counters
 * - Variable references in smoke test assertions ($VAR_NAME)
 * - Global assert fields (scenes_created, scenes_removed)
 */

#include <vde/api/GameAPI.h>
#include <vde/api/TextEntity.h>

#include <cmath>
#include <iostream>

#include "../ExampleBase.h"

using namespace vde;

// ============================================================================
// Input Handler
// ============================================================================

class DiagnosticsInputHandler : public vde::examples::BaseExampleInputHandler {
  public:
    void onKeyPress(int key) override {
        BaseExampleInputHandler::onKeyPress(key);

        if (key == KEY_1)
            m_addMesh = true;
        if (key == KEY_2)
            m_addSprite = true;
        if (key == KEY_3)
            m_addText = true;
        if (key == KEY_4)
            m_addPhysicsSprite = true;
        if (key == KEY_R)
            m_removeLastPressed = true;
        if (key == KEY_C)
            m_clearPressed = true;
        if (key == KEY_TAB)
            m_switchScene = true;
    }

    bool consumeAddMesh() {
        bool v = m_addMesh;
        m_addMesh = false;
        return v;
    }
    bool consumeAddSprite() {
        bool v = m_addSprite;
        m_addSprite = false;
        return v;
    }
    bool consumeAddText() {
        bool v = m_addText;
        m_addText = false;
        return v;
    }
    bool consumeAddPhysicsSprite() {
        bool v = m_addPhysicsSprite;
        m_addPhysicsSprite = false;
        return v;
    }
    bool consumeRemoveLast() {
        bool v = m_removeLastPressed;
        m_removeLastPressed = false;
        return v;
    }
    bool consumeClear() {
        bool v = m_clearPressed;
        m_clearPressed = false;
        return v;
    }
    bool consumeSwitchScene() {
        bool v = m_switchScene;
        m_switchScene = false;
        return v;
    }

  private:
    bool m_addMesh = false;
    bool m_addSprite = false;
    bool m_addText = false;
    bool m_addPhysicsSprite = false;
    bool m_removeLastPressed = false;
    bool m_clearPressed = false;
    bool m_switchScene = false;
};

// ============================================================================
// Primary Scene - entity creation and management
// ============================================================================

class PrimaryScene : public vde::examples::BaseExampleScene {
  public:
    PrimaryScene() : BaseExampleScene(20.0f) {}

    void onEnter() override {
        printExampleHeader();

        setup2D(20.0f, 15.0f);
        setBackgroundColor(Color(0.05f, 0.05f, 0.15f));

        // Seed with initial entities to demonstrate type classification
        addEntity<MeshEntity>();    // mesh +1
        addEntity<SpriteEntity>();  // sprite +1
        addEntity<TextEntity>();    // text +1, sprite +1
    }

    void update(float deltaTime) override {
        BaseExampleScene::update(deltaTime);

        auto* input = dynamic_cast<DiagnosticsInputHandler*>(getInputHandler());
        if (!input)
            return;

        if (input->consumeAddMesh()) {
            addEntity<MeshEntity>();
            std::cout << "[diag] Added MeshEntity\n";
        }
        if (input->consumeAddSprite()) {
            addEntity<SpriteEntity>();
            std::cout << "[diag] Added SpriteEntity\n";
        }
        if (input->consumeAddText()) {
            addEntity<TextEntity>();
            std::cout << "[diag] Added TextEntity\n";
        }
        if (input->consumeAddPhysicsSprite()) {
            enablePhysics();
            addEntity<PhysicsSpriteEntity>();
            std::cout << "[diag] Added PhysicsSpriteEntity\n";
        }
        if (input->consumeRemoveLast()) {
            auto& entities = getEntities();
            if (!entities.empty()) {
                removeEntity(entities.back()->getId());
                std::cout << "[diag] Removed last entity\n";
            }
        }
        if (input->consumeClear()) {
            clearEntities();
            std::cout << "[diag] Cleared all entities\n";
        }

        // Print diagnostics every few seconds
        m_printTimer += deltaTime;
        if (m_printTimer >= 3.0f) {
            m_printTimer = 0.0f;
            const auto& d = getDiagnostics();
            std::cout << "[diag] total=" << d.totalEntityCount << " mesh=" << d.meshEntityCount
                      << " sprite=" << d.spriteEntityCount << " text=" << d.textEntityCount
                      << " physics=" << d.physicsEntityCount << " created=" << d.entitiesCreated
                      << " removed=" << d.entitiesRemoved << " enter=" << d.enterCount
                      << " focused=" << d.isFocused << "\n";
        }
    }

  protected:
    std::string getExampleName() const override { return "Diagnostics Demo"; }

    std::vector<std::string> getFeatures() const override {
        return {"SceneDiagnostics entity type classification",
                "Lifecycle counters (enter/exit/pause/resume)",
                "Dynamic entity add/remove tracking", "Multi-scene isFocused tracking"};
    }

    std::vector<std::string> getExpectedVisuals() const override {
        return {"Dark blue background with colored entities",
                "Console shows real-time diagnostics counters"};
    }

    std::vector<std::string> getControls() const override {
        return {"1 - Add MeshEntity",          "2 - Add SpriteEntity",   "3 - Add TextEntity",
                "4 - Add PhysicsSpriteEntity", "R - Remove last entity", "C - Clear all entities",
                "TAB - Switch scene"};
    }

  private:
    float m_printTimer = 0.0f;
};

// ============================================================================
// Secondary Scene - verifies lifecycle counters and isFocused
// ============================================================================

class SecondaryScene : public Scene {
  public:
    void onEnter() override {
        setup2D(20.0f, 15.0f);
        setBackgroundColor(Color(0.15f, 0.05f, 0.05f));

        // Add a few entities so we have something to assert on
        addEntity<MeshEntity>();
        addEntity<SpriteEntity>();

        std::cout << "[diag] SecondaryScene entered\n";
    }

    void update(float /*deltaTime*/) override {
        auto* input = dynamic_cast<DiagnosticsInputHandler*>(getInputHandler());
        if (input && input->consumeSwitchScene()) {
            getGame()->setActiveScene("main");
        }
    }
};

// ============================================================================
// Game
// ============================================================================

class DiagnosticsGame
    : public vde::examples::BaseExampleGame<DiagnosticsInputHandler, PrimaryScene> {
  public:
    void onStart() override {
        BaseExampleGame::onStart();

        // Add a secondary scene to test lifecycle counters
        auto* secondary = new SecondaryScene();
        addScene("secondary", secondary);
    }

    void onUpdate(float /*deltaTime*/) override {
        // Check for scene switch from primary
        auto* input = dynamic_cast<DiagnosticsInputHandler*>(getInputHandler());
        if (input && input->consumeSwitchScene()) {
            auto* active = getActiveScene();
            if (active && active->getName() == "main") {
                setActiveScene("secondary");
            } else {
                setActiveScene("main");
            }
        }
    }
};

// ============================================================================
// Entry point
// ============================================================================

int main(int argc, char** argv) {
    DiagnosticsGame demo;
    return vde::examples::runExample(demo, "VDE Diagnostics Demo", 1280, 720, argc, argv);
}
