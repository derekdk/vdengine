#include "../GameBase.h"
#include "PacManScene.h"
#include "Input.h"

class PacManGame : public vde::games::BaseGame<pacman::PacManInput, pacman::PacManScene> {
  public:
    PacManGame() = default;
};

int main(int argc, char** argv) {
    PacManGame game;
    return vde::games::runGame(game, "VDE Pac-Man", 1280, 720, argc, argv);
}
