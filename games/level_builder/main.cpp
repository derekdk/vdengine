#include "../GameBase.h"
#include "Input.h"
#include "LevelBuilderScene.h"

class LevelBuilderGame : public vde::games::BaseGame<levelbuilder::LevelBuilderInput,
                                                     levelbuilder::LevelBuilderScene> {
  public:
    LevelBuilderGame() = default;
};

int main(int argc, char** argv) {  // NOLINT(bugprone-exception-escape)
    LevelBuilderGame game;
    return vde::games::runGame(game, "VDE Level Builder", 1280, 720, argc, argv);
}