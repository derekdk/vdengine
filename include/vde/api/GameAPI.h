#pragma once

/**
 * @file GameAPI.h
 * @brief Main include file for VDE Game API
 *
 * Include this single header to get access to all VDE Game API
 * classes and functionality.
 *
 * @example
 * @code
 * #include <vde/api/GameAPI.h>
 *
 * class MyGame : public vde::Game {
 *     // ...
 * };
 * @endcode
 */

// Core game classes
#include "Game.h"
#include "GameSettings.h"
#include "GameTypes.h"
#include "Scheduler.h"

// Scene and entity system
#include "AudioEvent.h"
#include "Entity.h"
#include "PhysicsEntity.h"
#include "PhysicsScene.h"
#include "PhysicsTypes.h"
#include "Resource.h"
#include "Scene.h"
#include "SceneGroup.h"
#include "ViewportRect.h"

// Input handling
#include "InputHandler.h"
#include "KeyCodes.h"

// Graphics
#include "GameCamera.h"
#include "LightBox.h"
#include "Material.h"
#include "Mesh.h"

// World coordinates and bounds
#include "CameraBounds.h"
#include "WorldBounds.h"
#include "WorldUnits.h"

/**
 * @namespace vde
 * @brief Vulkan Display Engine namespace
 *
 * The vde namespace contains all classes and functions for the
 * Vulkan Display Engine game API.
 *
 * ## Quick Start
 *
 * @code
 * #include <vde/api/GameAPI.h>
 *
 * class MyScene : public vde::Scene {
 * public:
 *     void onEnter() override {
 *         setCamera(new vde::OrbitCamera(vde::Position(0, 0, 0), 10.0f));
 *         setLightBox(new vde::SimpleColorLightBox(vde::Color::white()));
 *     }
 *
 *     void update(float dt) override {
 *         vde::Scene::update(dt);
 *     }
 * };
 *
 * int main() {
 *     vde::Game game;
 *     vde::GameSettings settings;
 *     settings.gameName = "My Game";
 *     settings.setWindowSize(1280, 720);
 *
 *     if (!game.initialize(settings)) {
 *         return 1;
 *     }
 *
 *     game.addScene("main", new MyScene());
 *     game.setActiveScene("main");
 *     game.run();
 *
 *     return 0;
 * }
 * @endcode
 *
 * ## Key Concepts
 *
 * ### Game
 * The vde::Game class is the main entry point. It manages the game loop,
 * window, and scene transitions.
 *
 * ### Scenes
 * vde::Scene represents a game state (menu, level, etc.). Each scene
 * manages its own entities, resources, camera, and lighting.
 *
 * ### Entities
 * vde::Entity is the base class for game objects. Specialized types
 * include vde::MeshEntity for 3D models and vde::SpriteEntity for 2D.
 *
 * ### Resources
 * Resources like vde::Mesh and textures are loaded through scenes
 * and referenced by ID to allow sharing.
 *
 * ### Input
 * Implement vde::InputHandler to receive keyboard, mouse, and gamepad
 * events. Set it on the Game or individual Scenes.
 *
 * ### Camera
 * Use vde::SimpleCamera, vde::OrbitCamera, or vde::Camera2D depending
 * on your game's perspective needs.
 *
 * ### Lighting
 * vde::LightBox and its subclasses define scene lighting. Use
 * vde::SimpleColorLightBox for basic lighting or vde::ThreePointLightBox
 * for professional-looking setups.
 */

// Version information
#define VDE_API_VERSION_MAJOR 0
#define VDE_API_VERSION_MINOR 1
#define VDE_API_VERSION_PATCH 0
#define VDE_API_VERSION_STRING "0.1.0"

namespace vde {

/**
 * @brief Get the VDE API version string.
 */
inline const char* getAPIVersion() {
    return VDE_API_VERSION_STRING;
}

/**
 * @brief Get the VDE API major version number.
 */
inline int getAPIMajorVersion() {
    return VDE_API_VERSION_MAJOR;
}

/**
 * @brief Get the VDE API minor version number.
 */
inline int getAPIMinorVersion() {
    return VDE_API_VERSION_MINOR;
}

/**
 * @brief Get the VDE API patch version number.
 */
inline int getAPIPatchVersion() {
    return VDE_API_VERSION_PATCH;
}

}  // namespace vde
