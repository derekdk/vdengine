/**
 * @file main.cpp
 * @brief Entry point for the Vertical Shooter example.
 *
 * A top-down vertical shooter with:
 * - Gamepad and keyboard controls (left stick + face buttons)
 * - Three weapon types: Basic, Spread, Rapid
 * - 16-screen procedurally generated scrolling map
 * - Four enemy types: Turret, Drone, Chaser, Tank
 * - Procedurally generated sprites and sound effects
 */

#include "../ExampleBase.h"
#include "GameScene.h"
#include "Input.h"

class VerticalShooterGame
    : public vde::examples::BaseExampleGame<shooter::ShooterInput, shooter::GameScene> {
  public:
    VerticalShooterGame() = default;
};

int main(int argc, char** argv) {
    VerticalShooterGame game;
    return vde::examples::runExample(game, "VDE Vertical Shooter", 600, 840, argc, argv);
}
