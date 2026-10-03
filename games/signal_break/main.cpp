#include <cstdio>
#include <exception>

#include "../GameBase.h"
#include "Input.h"
#include "SignalBreakScene.h"

class SignalBreakGame
    : public vde::games::BaseGame<signalbreak::SignalBreakInput, signalbreak::SignalBreakScene> {
  public:
    SignalBreakGame() = default;
};

int main(int argc, char** argv) {
    try {
        SignalBreakGame game;
        return vde::games::runGame(game, "VDE Signal Break", 1280, 720, argc, argv);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "Signal Break: %s\n", error.what());
    } catch (...) {
        std::fputs("Signal Break: unexpected startup error\n", stderr);
    }
    return 1;
}
