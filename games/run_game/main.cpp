#include "../GameBase.h"
#include "Input.h"
#include "RunGameScene.h"

class RunGameGame : public vde::games::BaseGame<rungame::RunGameInput, rungame::RunGameScene> {
  public:
    RunGameGame() = default;
};

int main(int argc, char** argv) {
    RunGameGame game;
    return vde::games::runGame(game, "VDE RunGame", 1280, 720, argc, argv);
}
