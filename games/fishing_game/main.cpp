#include "../GameBase.h"
#include "FishingGameScene.h"
#include "FishingInput.h"

class FishingGame : public vde::games::BaseGame<fishing::FishingInput, fishing::FishingGameScene> {
  public:
    FishingGame() = default;
};

int main(int argc, char** argv) {  // NOLINT(bugprone-exception-escape)
    FishingGame game;
    return vde::games::runGame(game, "VDE Pond Fisher", 1280, 720, argc, argv);
}