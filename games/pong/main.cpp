#include "../GameBase.h"
#include "Input.h"
#include "PongScene.h"

class PongGame : public vde::games::BaseGame<pong::PongInput, pong::PongScene> {
  public:
    PongGame() = default;
};

int main(int argc, char** argv) {
    PongGame game;
    return vde::games::runGame(game, "VDE Pong", 1280, 720, argc, argv);
}
