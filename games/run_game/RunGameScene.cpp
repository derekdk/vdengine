#include "RunGameScene.h"

#include "Input.h"

namespace rungame {

RunGameScene::RunGameScene() = default;

void RunGameScene::onEnter() {
    printGameHeader();

    // TODO: Set up camera, entities, lighting, etc.
    // Examples:
    //   auto* camera = new vde::OrbitCamera(vde::Position(0, 0, 0), 8.0f, 25.0f, 45.0f);
    //   setCamera(camera);
    //   setBackgroundColor(vde::Color::fromHex(0x1a1a2e));
}

void RunGameScene::update(float deltaTime) {
    BaseGameScene::update(deltaTime);  // handles ESC, F1, F11

    auto* input = dynamic_cast<RunGameInput*>(getInputHandler());
    if (!input) return;

    // TODO: Query input and update game state.
    // Example:
    //   if (input->keys.consume("action")) { /* ... */ }
}

std::string RunGameScene::getGameName() const { return "RunGame"; }

std::vector<std::string> RunGameScene::getGameplaySummary() const {
    return {
        "TODO: describe how the game is played",
    };
}

std::vector<std::string> RunGameScene::getGoals() const {
    return {
        "TODO: describe the win/loss condition",
    };
}

std::vector<std::string> RunGameScene::getControls() const {
    return {
        "ESC - Exit",
        "F1  - Toggle UI",
        // "SPACE - TODO: describe action",
    };
}

}  // namespace rungame
