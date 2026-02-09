# VDE Game API Documentation

This document describes the VDE (Vulkan Display Engine) Game API, a high-level interface for building games with the VDE engine.

## Table of Contents

- [Overview](#overview)
- [Getting Started](#getting-started)
- [API Structure](#api-structure)
- [Core Classes](#core-classes)
  - [Game](#game)
  - [GameSettings](#gamesettings)
  - [Scene](#scene)
  - [Entity](#entity)
  - [Resource](#resource)
    - [ResourceManager](#resourcemanager)
    - [Material](#material)
- [Input System](#input-system)
- [Camera System](#camera-system)
- [Lighting System](#lighting-system)
- [Audio System](#audio-system)
- [Common Types](#common-types)
- [World Coordinates & Bounds](#world-coordinates--bounds)
- [Examples](#examples)

---

## Overview

The VDE Game API provides a clean, object-oriented interface for game development built on top of the Vulkan-based rendering engine. It abstracts away low-level Vulkan details while providing full control over game logic, scenes, entities, and rendering.

### Key Features

- **Scene Management**: Organize your game into discrete scenes (menus, levels, etc.)
- **Entity System**: Manage game objects with transforms, meshes, and sprites
- **Resource Management**: Load and share textures, meshes, and other assets
- **Input Handling**: Unified keyboard, mouse, and gamepad/joystick input with event callbacks and polling
- **Camera System**: Multiple camera types for different game styles
- **Lighting**: Flexible lighting with ambient, directional, point, and spot lights
- **Audio**: Basic playback for music and sound effects

### Header Structure

```
include/vde/api/
├── GameAPI.h        # Main include (includes everything)
├── Game.h           # Game class and main loop
├── GameSettings.h   # Configuration structures
├── GameTypes.h      # Common types (Color, Position, etc.)
├── Scene.h          # Scene management
├── Entity.h         # Entity classes (MeshEntity, SpriteEntity)
├── Resource.h       # Resource base class
├── ResourceManager.h # Resource caching and sharing
├── Mesh.h           # 3D mesh resource
├── Material.h       # Material properties for meshes
├── InputHandler.h   # Input handling interface
├── KeyCodes.h       # Key, mouse, and gamepad constants
├── GameCamera.h     # Camera classes
├── LightBox.h       # Lighting system
├── AudioClip.h      # Audio resource (sounds/music)
├── AudioSource.h    # Audio emitter component
├── AudioManager.h   # Audio playback system
├── WorldUnits.h     # Type-safe units (Meters, Pixels)
├── WorldBounds.h    # 3D/2D world boundary definitions
└── CameraBounds.h   # 2D camera with pixel-to-world mapping
```

---

## Getting Started

### Minimal Example

```cpp
#include <vde/api/GameAPI.h>

int main() {
    vde::Game game;
    vde::GameSettings settings;
    settings.gameName = "My First VDE Game";
    settings.setWindowSize(1280, 720);
    
    if (!game.initialize(settings)) {
        return 1;
    }
    
    // Create and add a scene
    class MyScene : public vde::Scene {
    public:
        void onEnter() override {
            setCamera(new vde::OrbitCamera(vde::Position(0, 0, 0), 10.0f));
            setLightBox(new vde::SimpleColorLightBox(vde::Color::white()));
        }
    };
    
    game.addScene("main", new MyScene());
    game.setActiveScene("main");
    game.run();
    
    return 0;
}
```

### Including the API

You can include everything with a single header:

```cpp
#include <vde/api/GameAPI.h>
```

Or include only what you need:

```cpp
#include <vde/api/Game.h>
#include <vde/api/Scene.h>
#include <vde/api/Entity.h>
```

---

## API Structure

The API is organized into several interconnected systems:

```
┌─────────────────────────────────────────────────────────┐
│                         Game                             │
│  ┌─────────────────────────────────────────────────────┐│
│  │                    Scene Stack                       ││
│  │  ┌─────────────────────────────────────────────────┐││
│  │  │                 Active Scene                     │││
│  │  │  ┌──────────┐  ┌──────────┐  ┌───────────────┐ │││
│  │  │  │ Entities │  │Resources │  │ Camera/Lights │ │││
│  │  │  └──────────┘  └──────────┘  └───────────────┘ │││
│  │  └─────────────────────────────────────────────────┘││
│  └─────────────────────────────────────────────────────┘│
│  ┌──────────────┐  ┌──────────────┐                     │
│  │ InputHandler │  │ GameSettings │                     │
│  └──────────────┘  └──────────────┘                     │
└─────────────────────────────────────────────────────────┘
```

### Ownership Model

| Object | Ownership |
|--------|-----------|
| `Scene` | Owned by `Game` |
| `Entity` | Owned by `Scene` (shared_ptr) |
| `Resource` | Owned by `Scene` (shared_ptr) |
| `Camera` | Owned by `Scene` (unique_ptr) |
| `LightBox` | Owned by `Scene` (unique_ptr) |
| `InputHandler` | NOT owned (raw pointer) |

---

## Core Classes

### Game

The `Game` class is the central manager for your game. It handles:
- Engine initialization and shutdown
- The main game loop
- Scene management
- Global input handling
- Frame timing

#### Lifecycle

```cpp
vde::Game game;

// 1. Initialize
game.initialize(settings);

// 2. Setup scenes
game.addScene("menu", new MenuScene());
game.addScene("gameplay", new GameplayScene());
game.setActiveScene("menu");

// 3. Run (blocks until quit)
game.run();

// 4. Cleanup (automatic on destruction)
```

#### Scene Stack

The game supports a scene stack for overlays:

```cpp
// Push pause menu over gameplay
game.pushScene("pause");

// Return to gameplay
game.popScene();
```

#### Key Methods

| Method | Description |
|--------|-------------|
| `initialize(settings)` | Initialize engine with settings |
| `run()` | Start the game loop |
| `quit()` | Request game exit |
| `addScene(name, scene)` | Register a scene |
| `setActiveScene(name)` | Switch to a scene |
| `pushScene(name)` | Push scene onto stack |
| `popScene()` | Pop current scene |
| `getDeltaTime()` | Get frame delta time |
| `getFPS()` | Get current FPS |
| `getResourceManager()` | Access the global resource cache |

---

### GameSettings

Configuration for game initialization:

```cpp
vde::GameSettings settings;

// Basic settings
settings.gameName = "My Game";
settings.gameVersion = "1.0.0";

// Display settings
settings.display.windowWidth = 1920;
settings.display.windowHeight = 1080;
settings.display.fullscreen = true;
settings.display.vsync = vde::VSyncMode::On;

// Graphics settings
settings.graphics.quality = vde::GraphicsQuality::High;
settings.graphics.antiAliasing = vde::AntiAliasing::MSAA4x;
settings.graphics.shadows = true;

// Audio settings
settings.audio.masterVolume = 0.8f;
settings.audio.musicVolume = 0.5f;

// Debug settings
settings.debug.enableValidation = true;
settings.debug.showFPS = true;
```

#### Fluent API

```cpp
vde::GameSettings settings;
settings.setWindowSize(1280, 720)
        .setFullscreen(false)
        .setQuality(vde::GraphicsQuality::High)
        .enableDebug(true, true);
```

---

### Scene

A `Scene` represents a game state (main menu, level, pause screen, etc.).

#### Creating a Scene

```cpp
class GameplayScene : public vde::Scene {
public:
    void onEnter() override {
        // Called when scene becomes active
        setupCamera();
        loadResources();
        createEntities();
    }
    
    void onExit() override {
        // Called when leaving scene
    }
    
    void update(float deltaTime) override {
        // Game logic here
        
        // Don't forget to call base!
        vde::Scene::update(deltaTime);
    }
    
    void render() override {
        // Custom rendering if needed
        vde::Scene::render();
    }
    
private:
    void setupCamera() {
        setCamera(new vde::OrbitCamera(
            vde::Position(0, 0, 0),  // target
            15.0f,                    // distance
            45.0f,                    // pitch
            0.0f                      // yaw
        ));
    }
    
    void loadResources() {
        auto& resources = getGame()->getResourceManager();
        m_mesh = resources.load<vde::Mesh>("models/character.obj");
    }
    
    void createEntities() {
        auto entity = addEntity<vde::MeshEntity>();
        entity->setMesh(m_mesh);
        entity->setPosition(0, 0, 0);
    }
    
    vde::ResourcePtr<vde::Mesh> m_mesh;
};
```

#### Scene Lifecycle

| Method | When Called |
|--------|-------------|
| `onEnter()` | Scene becomes active |
| `onExit()` | Scene is deactivated |
| `onPause()` | Another scene pushed on top |
| `onResume()` | Returned to top of stack |
| `update(dt)` | Every frame |
| `render()` | Every frame after update |

#### Resource Management

```cpp
// Load resource from file
ResourceId meshId = scene->addResource<vde::Mesh>("path/to/mesh.obj");

// Add pre-created resource
auto mesh = vde::Mesh::createCube(1.0f);
ResourceId cubeId = scene->addResource(mesh);

// Get resource
vde::Mesh* mesh = scene->getResource<vde::Mesh>(meshId);

// Remove resource
scene->removeResource(meshId);
```

> Note: Scene resource IDs are stored, but `MeshEntity` and `SpriteEntity` rendering
> currently uses direct `shared_ptr` assignments (`setMesh`, `setTexture`).
> Resource ID binding is planned but not wired yet.

#### Entity Management

```cpp
// Create entity with template
auto mesh = vde::Mesh::createCube(1.0f);
auto entity = scene->addEntity<vde::MeshEntity>();
entity->setMesh(mesh);

// Add existing entity
vde::EntityId id = scene->addEntity(existingEntity);

// Find entities
vde::Entity* e = scene->getEntity(entityId);
vde::Entity* e = scene->getEntityByName("player");
auto meshEntities = scene->getEntitiesOfType<vde::MeshEntity>();

// Remove entity
scene->removeEntity(entityId);
scene->clearEntities();
```

---

### Entity

Entities are game objects with transforms (position, rotation, scale).

#### Base Entity

```cpp
vde::Entity entity;

// Transform
entity.setPosition(10.0f, 0.0f, 5.0f);
entity.setRotation(0.0f, 45.0f, 0.0f);  // pitch, yaw, roll in degrees
entity.setScale(2.0f);                   // uniform scale
entity.setScale(1.0f, 2.0f, 1.0f);      // non-uniform scale

// Visibility
entity.setVisible(false);

// Identity
entity.setName("player");
vde::EntityId id = entity.getId();
```

#### MeshEntity

For 3D models:

```cpp
auto mesh = vde::Mesh::createCube(1.0f);
auto meshEntity = scene->addEntity<vde::MeshEntity>();
meshEntity->setMesh(mesh);
meshEntity->setColor(vde::Color(1.0f, 0.5f, 0.5f));  // tint
meshEntity->setPosition(0, 1, 0);
```

> Note: Mesh texturing is not wired in the current render path. Use `Material`
> and `setColor()` for now.

#### SpriteEntity

For 2D sprites:

```cpp
auto spriteTexture = getGame()->getResourceManager().load<vde::Texture>("sprites/player.png");
if (auto* context = getGame()->getVulkanContext()) {
    spriteTexture->uploadToGPU(context);
}

auto sprite = scene->addEntity<vde::SpriteEntity>();
sprite->setTexture(spriteTexture);
sprite->setColor(vde::Color::white());
sprite->setAnchor(0.5f, 0.5f);  // center
sprite->setUVRect(0.0f, 0.0f, 0.25f, 0.25f);  // sprite sheet
```

#### Custom Entities

```cpp
class PlayerEntity : public vde::MeshEntity {
public:
    void update(float deltaTime) override {
        // Custom update logic
        m_velocity.y -= 9.8f * deltaTime;  // gravity
        
        auto pos = getPosition();
        pos.y += m_velocity.y * deltaTime;
        setPosition(pos);
        
        vde::MeshEntity::update(deltaTime);
    }
    
private:
    glm::vec3 m_velocity{0.0f};
};
```

---

### Resource

Base class for loadable assets.

#### Built-in Resource Types

| Type | Description |
|------|-------------|
| `Mesh` | 3D geometry |
| `Texture` | 2D images (via engine) |
| `AudioClip` | Sound effects and music clips |

#### Mesh Primitives

```cpp
auto cube = vde::Mesh::createCube(1.0f);
auto sphere = vde::Mesh::createSphere(0.5f, 32, 16);
auto plane = vde::Mesh::createPlane(10.0f, 10.0f);
auto cylinder = vde::Mesh::createCylinder(0.5f, 2.0f, 32);
```

---

### ResourceManager

`ResourceManager` provides a global cache for resources that are shared across scenes.
You can access it from `Game` and use it to deduplicate loads by path.

```cpp
auto& resources = game.getResourceManager();

// Load (or get cached) resources
auto mesh = resources.load<vde::Mesh>("models/ship.obj");
auto texture = resources.load<vde::Texture>("textures/ship.png");

// Use the same texture again elsewhere (returns cached instance)
auto texture2 = resources.load<vde::Texture>("textures/ship.png");
```

> Note: `ResourceManager::load()` performs CPU-side loading. Meshes are uploaded
> on first render, but textures must be uploaded explicitly with
> `Texture::uploadToGPU()` before use in sprites.

---

### Material

`Material` defines surface properties for `MeshEntity` rendering.

```cpp
auto material = vde::Material::createColored(vde::Color(0.8f, 0.2f, 0.1f));
material->setRoughness(0.4f);
material->setMetallic(0.1f);

auto mesh = vde::Mesh::createCube(1.0f);
auto entity = scene->addEntity<vde::MeshEntity>();
entity->setMesh(mesh);
entity->setMaterial(material);
```

---

## Input System

### InputHandler

Implement the `InputHandler` interface to receive input events:

```cpp
class MyInputHandler : public vde::InputHandler {
public:
    // Keyboard
    void onKeyPress(int key) override {
        if (key == vde::KEY_ESCAPE) {
            // Handle escape
        }
    }
    
    void onKeyRelease(int key) override {}
    void onKeyRepeat(int key) override {}
    
    // Mouse
    void onMouseButtonPress(int button, double x, double y) override {
        if (button == vde::MOUSE_BUTTON_LEFT) {
            // Handle left click at (x, y)
        }
    }
    
    void onMouseMove(double x, double y) override {
        m_mouseX = x;
        m_mouseY = y;
    }
    
    void onMouseScroll(double xOffset, double yOffset) override {
        m_zoom += yOffset;
    }
    
    // Gamepad (events fired automatically by engine)
    void onGamepadConnect(int gamepadId, const char* name) override {
        std::cout << "Gamepad " << gamepadId << " connected: " << name << "\n";
    }
    
    void onGamepadButtonPress(int gamepadId, int button) override {
        if (button == vde::GAMEPAD_BUTTON_A) {
            jump();
        }
    }
    
    void onGamepadAxis(int gamepadId, int axis, float value) override {
        if (axis == vde::GAMEPAD_AXIS_LEFT_X) {
            m_moveX = value;
        }
        if (axis == vde::GAMEPAD_AXIS_LEFT_Y) {
            m_moveY = value;
        }
    }
    
private:
    double m_mouseX = 0, m_mouseY = 0;
    float m_zoom = 1.0f;
    float m_moveX = 0, m_moveY = 0;
};
```

#### Gamepad Input

Gamepad input uses GLFW's standardized gamepad mapping (Xbox/PlayStation layout). The engine automatically:
- Detects connected gamepads at startup and during runtime (hot-plug)
- Polls gamepad state each frame and dispatches press/release/axis events
- Applies dead zone filtering to analog sticks (default 0.1, customizable)
- Stores state for polling via `isGamepadButtonPressed()` and `getGamepadAxis()`

**Polling Example:**

```cpp
void update(float deltaTime) {
    if (isGamepadConnected(vde::JOYSTICK_1)) {
        float x = getGamepadAxis(vde::JOYSTICK_1, vde::GAMEPAD_AXIS_LEFT_X);
        float y = getGamepadAxis(vde::JOYSTICK_1, vde::GAMEPAD_AXIS_LEFT_Y);
        movePlayer(x * speed * deltaTime, y * speed * deltaTime);
        
        if (isGamepadButtonPressed(vde::JOYSTICK_1, vde::GAMEPAD_BUTTON_A)) {
            jump();
        }
    }
}
```

### Setting Input Handler

```cpp
MyInputHandler inputHandler;

// Global handler
game.setInputHandler(&inputHandler);

// Per-scene handler
scene->setInputHandler(&inputHandler);
```

### Key Codes

Common key codes from `KeyCodes.h`:

#### Keyboard

| Constant | Description |
|----------|-------------|
| `KEY_A` - `KEY_Z` | Letter keys |
| `KEY_0` - `KEY_9` | Number keys |
| `KEY_F1` - `KEY_F12` | Function keys |
| `KEY_ESCAPE` | Escape key |
| `KEY_ENTER` | Enter key |
| `KEY_SPACE` | Spacebar |
| `KEY_LEFT/RIGHT/UP/DOWN` | Arrow keys |
| `KEY_LEFT_SHIFT` | Left shift |
| `KEY_LEFT_CONTROL` | Left control |

#### Mouse

| Constant | Description |
|----------|-------------|
| `MOUSE_BUTTON_LEFT` | Left mouse button |
| `MOUSE_BUTTON_RIGHT` | Right mouse button |
| `MOUSE_BUTTON_MIDDLE` | Middle mouse button |

#### Gamepad

| Constant | Description |
|----------|-------------|
| `JOYSTICK_1` - `JOYSTICK_16` | Gamepad slots (0-15) |
| `GAMEPAD_BUTTON_A` | A button (Cross on PlayStation) |
| `GAMEPAD_BUTTON_B` | B button (Circle on PlayStation) |
| `GAMEPAD_BUTTON_X` | X button (Square on PlayStation) |
| `GAMEPAD_BUTTON_Y` | Y button (Triangle on PlayStation) |
| `GAMEPAD_BUTTON_LEFT_BUMPER` | Left bumper/shoulder |
| `GAMEPAD_BUTTON_RIGHT_BUMPER` | Right bumper/shoulder |
| `GAMEPAD_BUTTON_BACK` | Back/Select button |
| `GAMEPAD_BUTTON_START` | Start button |
| `GAMEPAD_BUTTON_GUIDE` | Guide/Home button |
| `GAMEPAD_BUTTON_LEFT_THUMB` | Left stick click |
| `GAMEPAD_BUTTON_RIGHT_THUMB` | Right stick click |
| `GAMEPAD_BUTTON_DPAD_UP/DOWN/LEFT/RIGHT` | D-pad directions |
| `GAMEPAD_AXIS_LEFT_X/Y` | Left analog stick axes |
| `GAMEPAD_AXIS_RIGHT_X/Y` | Right analog stick axes |
| `GAMEPAD_AXIS_LEFT_TRIGGER` | Left trigger (0.0 to 1.0) |
| `GAMEPAD_AXIS_RIGHT_TRIGGER` | Right trigger (0.0 to 1.0) |

**PlayStation Aliases:** `GAMEPAD_BUTTON_CROSS`, `CIRCLE`, `SQUARE`, `TRIANGLE`  
**Limits:** `MAX_GAMEPADS` (16), `MAX_GAMEPAD_BUTTONS` (15), `MAX_GAMEPAD_AXES` (6)  
**Dead Zone:** `GAMEPAD_AXIS_DEADZONE` (0.1)

---

## Camera System

### SimpleCamera

First-person or free camera:

```cpp
vde::SimpleCamera camera(
    vde::Position(0, 5, 10),      // position
    vde::Direction(0, -0.3f, -1)  // look direction
);

camera.setFieldOfView(60.0f);
camera.move(vde::Direction(0, 0, -1));  // move forward
camera.rotate(0, 45);  // rotate yaw 45 degrees

scene->setCamera(new vde::SimpleCamera(camera));
```

### OrbitCamera

Third-person or RTS camera:

```cpp
vde::OrbitCamera camera(
    vde::Position(0, 0, 0),  // target to orbit
    15.0f,                    // distance
    45.0f,                    // pitch (degrees)
    0.0f                      // yaw (degrees)
);

camera.setZoomLimits(5.0f, 50.0f);
camera.setPitchLimits(10.0f, 80.0f);

// Controls
camera.rotate(0, 5);   // rotate around target
camera.zoom(-1);       // zoom out
camera.pan(1, 0);      // pan right

scene->setCamera(new vde::OrbitCamera(camera));
```

### Camera2D

For 2D games:

```cpp
vde::Camera2D camera(1920, 1080);  // viewport size

camera.setPosition(0, 0);
camera.setZoom(2.0f);      // 2x zoom
camera.setRotation(45.0f); // rotate 45 degrees

scene->setCamera(new vde::Camera2D(camera));
```

---

## Lighting System

### LightBox

Container for scene lighting:

```cpp
auto lightBox = new vde::LightBox();
lightBox->setAmbientColor(vde::Color(0.2f, 0.2f, 0.3f));
lightBox->setAmbientIntensity(1.0f);

// Add lights
lightBox->addLight(vde::Light::directional(
    vde::Direction(-1, -1, -1),
    vde::Color::white(),
    1.0f
));

lightBox->addLight(vde::Light::point(
    vde::Position(5, 3, 0),
    vde::Color::red(),
    1.0f,
    10.0f  // range
));

scene->setLightBox(lightBox);
```

### SimpleColorLightBox

For simple ambient-only lighting:

```cpp
scene->setLightBox(new vde::SimpleColorLightBox(vde::Color::white()));

// With a main directional light
scene->setLightBox(new vde::SimpleColorLightBox(
    vde::Color(0.2f, 0.2f, 0.2f),  // ambient
    vde::Light::directional(vde::Direction(-1, -1, -1))
));
```

### ThreePointLightBox

Professional three-point lighting:

```cpp
auto lighting = new vde::ThreePointLightBox(
    vde::Color::white(),  // key light color
    1.0f                  // key light intensity
);

// Adjust individual lights
lighting->getKeyLight().intensity = 1.2f;
lighting->getFillLight().color = vde::Color(0.8f, 0.8f, 1.0f);

scene->setLightBox(lighting);
```

### Light Types

```cpp
// Directional (sun-like)
auto sun = vde::Light::directional(
    vde::Direction(-0.5f, -1.0f, -0.3f),
    vde::Color(1.0f, 0.95f, 0.8f),
    1.0f
);

// Point (light bulb)
auto bulb = vde::Light::point(
    vde::Position(0, 3, 0),
    vde::Color::yellow(),
    1.0f,
    8.0f  // range
);

// Spot (flashlight)
auto spot = vde::Light::spot(
    vde::Position(0, 5, 0),
    vde::Direction(0, -1, 0),
    30.0f,  // cone angle
    vde::Color::white(),
    2.0f
);
```

---

## Audio System

VDE includes basic audio playback via `AudioManager`, `AudioClip`, and `AudioSource`.
The audio system is initialized as part of `Game::initialize()` using `GameSettings::audio`.

### AudioClip and AudioManager

```cpp
// Load audio clip via ResourceManager
auto clip = game.getResourceManager().load<vde::AudioClip>("audio/laser.wav");

// Play a one-shot sound effect
vde::AudioManager::getInstance().playSFX(clip, 0.8f);

// Play looping music
vde::AudioManager::getInstance().playMusic(clip, 0.6f, true, 1.0f);
```

### AudioSource (3D emitter)

```cpp
vde::AudioSource source;
source.setClip(clip);
source.setSpatial(true);
source.setPosition(0.0f, 1.0f, 0.0f);
source.play(true);  // loop
```

---

## Common Types

### Color

```cpp
vde::Color color(1.0f, 0.5f, 0.0f);        // RGB float
vde::Color color(1.0f, 0.5f, 0.0f, 0.5f);  // RGBA float

// Factory methods
auto c1 = vde::Color::fromRGB8(255, 128, 0);
auto c2 = vde::Color::fromHex(0xFF8000);
auto c3 = vde::Color::fromHex(0xFF8000FF);  // with alpha

// Predefined colors
vde::Color::white();
vde::Color::black();
vde::Color::red();
vde::Color::green();
vde::Color::blue();
```

### Position

```cpp
vde::Position pos(10.0f, 5.0f, -3.0f);
glm::vec3 v = pos.toVec3();

// Arithmetic
auto newPos = pos + vde::Position(1, 0, 0);
auto delta = pos - otherPos;
auto scaled = pos * 2.0f;
```

### Direction

```cpp
vde::Direction dir(0.0f, 0.0f, -1.0f);
auto normalized = dir.normalized();

// Predefined directions
vde::Direction::forward();   // (0, 0, -1)
vde::Direction::back();      // (0, 0, 1)
vde::Direction::up();        // (0, 1, 0)
vde::Direction::down();      // (0, -1, 0)
vde::Direction::left();      // (-1, 0, 0)
vde::Direction::right();     // (1, 0, 0)
```

### Rotation

```cpp
vde::Rotation rot(45.0f, 90.0f, 0.0f);  // pitch, yaw, roll (degrees)
```

### Scale

```cpp
vde::Scale uniform(2.0f);           // 2x in all axes
vde::Scale nonUniform(1, 2, 1);     // stretched in Y
```

### Transform

```cpp
vde::Transform transform;
transform.position = vde::Position(0, 1, 0);
transform.rotation = vde::Rotation(0, 45, 0);
transform.scale = vde::Scale(1.0f);

glm::mat4 matrix = transform.getMatrix();
```

---

## World Coordinates & Bounds

VDE provides type-safe world coordinate APIs that make units and directions explicit, preventing common bugs and improving code readability.

### Coordinate System

VDE uses a right-handed Y-up coordinate system by default:

```
        +Y (Up)
         |
         |
         |_______ +X (East)
        /
       /
      +Z (North)
```

| Direction | Axis | Sign |
|-----------|------|------|
| North | Z | + |
| South | Z | - |
| East | X | + |
| West | X | - |
| Up | Y | + |
| Down | Y | - |

### Meters - Type-Safe Distance

The `Meters` type provides explicit unit documentation:

```cpp
using namespace vde;

// Using user-defined literals (recommended)
Meters distance = 100_m;
Meters halfDist = distance / 2;  // 50 meters

// Explicit construction
Meters other = Meters(50.0f);

// Arithmetic
Meters sum = distance + other;     // 150 meters
Meters diff = distance - other;    // 50 meters
Meters scaled = distance * 2;      // 200 meters

// Comparisons
if (distance > 50_m) { /* ... */ }

// Implicit conversion to float for glm/rendering
float raw = distance;  // 100.0f
```

### Pixels - Type-Safe Screen Units

```cpp
Pixels screenX = 1920_px;
Pixels screenY = 1080_px;

ScreenSize size(1920_px, 1080_px);
float aspect = size.aspectRatio();  // 1.777...
```

### WorldPoint - 3D Position with Units

```cpp
// Direct construction
WorldPoint pt(10_m, 5_m, 20_m);  // x, y, z

// From cardinal directions (north, east, up)
WorldPoint pt2 = WorldPoint::fromDirections(100_m, 50_m, 20_m);
// Creates: x=50 (east), y=20 (up), z=100 (north)

// Convert to GLM for rendering
glm::vec3 v = pt.toVec3();
```

### WorldExtent - 3D Size with Units

```cpp
WorldExtent size(200_m, 30_m, 200_m);  // width, height, depth

// 2D extent (no height)
WorldExtent flat = WorldExtent::flat(200_m, 200_m);  // width, depth

bool is2D = flat.is2D();  // true
```

### WorldBounds - 3D Scene Boundaries

Define scene bounds with explicit directional limits:

```cpp
// Create 200m x 200m x 30m world bounds
// From 100m north to 100m south, 100m west to 100m east
// From 20m up to 10m down
auto bounds = WorldBounds::fromDirectionalLimits(
    100_m,                     // north limit (+Z)
    WorldBounds::south(100_m), // south limit (-Z)
    WorldBounds::west(100_m),  // west limit (-X)
    100_m,                     // east limit (+X)
    20_m,                      // up limit (+Y)
    WorldBounds::down(10_m)    // down limit (-Y)
);

// Query dimensions
Meters width = bounds.width();   // 200m (east-west)
Meters depth = bounds.depth();   // 200m (north-south)
Meters height = bounds.height(); // 30m (up-down)

// Query limits
Meters north = bounds.northLimit();  // 100m
Meters south = bounds.southLimit();  // -100m

// Point containment
WorldPoint pt(0_m, 0_m, 0_m);
bool inside = bounds.contains(pt);  // true

// Intersection
bool overlaps = bounds.intersects(otherBounds);

// Center-based construction
auto centered = WorldBounds::fromCenterAndExtent(
    WorldPoint(0_m, 0_m, 0_m),
    WorldExtent(100_m, 50_m, 100_m)
);
```

### WorldBounds2D - 2D Scene Boundaries

For top-down or side-scrolling games:

```cpp
// Top-down game using cardinal directions
auto bounds = WorldBounds2D::fromCardinal(
    100_m,   // north
    -100_m,  // south
    -100_m,  // west
    100_m    // east
);

// Side-scroller using left/right/top/bottom
auto level = WorldBounds2D::fromLRTB(
    0_m,     // left
    1000_m,  // right
    100_m,   // top
    0_m      // bottom
);

// Convert to 3D bounds for unified APIs
WorldBounds bounds3D = bounds.toWorldBounds(10_m, 0_m);
```

### Setting Scene Bounds

```cpp
class MyScene : public vde::Scene {
    void onEnter() override {
        // Set world bounds for the scene
        setWorldBounds(WorldBounds::fromDirectionalLimits(
            100_m, WorldBounds::south(100_m),
            WorldBounds::west(100_m), 100_m,
            20_m, WorldBounds::down(10_m)
        ));
        
        // Check if 2D
        if (is2D()) {
            // Configure for 2D rendering
        }
    }
};
```

### CameraBounds2D - Pixel-to-World Mapping

For 2D games, `CameraBounds2D` provides coordinate conversion between screen pixels and world units:

```cpp
CameraBounds2D camera;

// Configure screen size (from window)
camera.setScreenSize(1920_px, 1080_px);

// Set visible world width (height derived from aspect ratio)
camera.setWorldWidth(16_m);  // Show 16 meters horizontally

// Position camera
camera.centerOn(0_m, 0_m);

// Zoom
camera.setZoom(2.0f);  // 2x zoom (shows 8m width instead of 16m)

// Constrain camera to world bounds
camera.setConstraintBounds(WorldBounds2D::fromCardinal(
    50_m, -50_m, -50_m, 50_m
));

// Coordinate conversion
glm::vec2 worldPos = camera.screenToWorld(mouseX_px, mouseY_px);
glm::vec2 screenPos = camera.worldToScreen(entityX, entityY);

// Visibility testing
if (camera.isVisible(entityX, entityY)) {
    // Entity is on screen
}

// Get what's visible
WorldBounds2D visible = camera.getVisibleBounds();
```

### PixelToWorldMapping - Conversion Ratios

For custom coordinate mapping:

```cpp
// 100 pixels = 1 meter
auto mapping = PixelToWorldMapping::fromPixelsPerMeter(100.0f);

// Fit 20 meters across a 1920 pixel screen
auto fit = PixelToWorldMapping::fitWidth(20_m, 1920_px);

// Convert
Meters worldDist = mapping.toWorld(500_px);   // 5 meters
Pixels screenDist = mapping.toPixels(10_m);   // 1000 pixels

// Get ratio
float ppm = mapping.getPixelsPerMeter();  // 100.0
```

### 2D Game Setup Example

```cpp
class My2DScene : public vde::Scene {
    CameraBounds2D m_camera;
    vde::ResourcePtr<vde::Texture> m_spriteTexture;
    
    void onEnter() override {
        // Define world bounds
        setWorldBounds(WorldBounds::flat(
            50_m, WorldBounds::south(50_m),
            WorldBounds::west(50_m), 50_m
        ));
        
        // Setup 2D camera
        m_camera.setScreenSize(1920_px, 1080_px);
        m_camera.setWorldWidth(32_m);
        m_camera.centerOn(0_m, 0_m);
        m_camera.setConstraintBounds(WorldBounds2D::fromCardinal(
            50_m, -50_m, -50_m, 50_m
        ));
        
        setCameraBounds2D(m_camera);
        
        // Create 2D camera for rendering
        setCamera(new vde::Camera2D(
            m_camera.getVisibleWidth(),
            m_camera.getVisibleHeight()
        ));

        // Load sprite texture and upload to GPU
        m_spriteTexture = getGame()->getResourceManager().load<vde::Texture>("sprites/player.png");
        if (auto* context = getGame()->getVulkanContext()) {
            m_spriteTexture->uploadToGPU(context);
        }
    }
    
    void onMouseClick(double screenX, double screenY) {
        // Convert click to world coordinates
        glm::vec2 worldPos = m_camera.screenToWorld(
            Pixels(screenX), Pixels(screenY)
        );
        
        // Spawn entity at click location
        auto entity = addEntity<vde::SpriteEntity>();
        entity->setTexture(m_spriteTexture);
        entity->setPosition(worldPos.x, worldPos.y, 0);
    }
    
    void update(float dt) override {
        // Pan camera with arrow keys
        auto* input = getInputHandler();
        if (input) {
            if (input->isKeyPressed(KEY_LEFT)) 
                m_camera.move(-10_m * dt, 0_m);
            if (input->isKeyPressed(KEY_RIGHT)) 
                m_camera.move(10_m * dt, 0_m);
        }
        
        // Update scene camera bounds
        setCameraBounds2D(m_camera);
        
        vde::Scene::update(dt);
    }
};
```

---

## Examples

### Complete Game Example

```cpp
#include <vde/api/GameAPI.h>

class MyInputHandler : public vde::InputHandler {
public:
    float rotationY = 0.0f;
    
    void onKeyPress(int key) override {
        if (key == vde::KEY_LEFT) rotationY -= 5.0f;
        if (key == vde::KEY_RIGHT) rotationY += 5.0f;
    }
};

class GameScene : public vde::Scene {
public:
    void onEnter() override {
        // Camera
        setCamera(new vde::OrbitCamera(
            vde::Position(0, 0, 0), 10.0f, 30.0f, 45.0f
        ));
        
        // Lighting
        setLightBox(new vde::SimpleColorLightBox(
            vde::Color(0.3f, 0.3f, 0.4f),
            vde::Light::directional(vde::Direction(-1, -1, -1))
        ));
        
        // Resources
        m_mesh = getGame()->getResourceManager().load<vde::Mesh>("models/cube.obj");
        
        // Entity
        m_cube = addEntity<vde::MeshEntity>();
        m_cube->setMesh(m_mesh);
        m_cube->setPosition(0, 0, 0);
    }
    
    void update(float dt) override {
        auto* input = static_cast<MyInputHandler*>(getInputHandler());
        if (input) {
            m_cube->setRotation(0, input->rotationY, 0);
        }
        
        vde::Scene::update(dt);
    }
    
private:
    vde::ResourcePtr<vde::Mesh> m_mesh;
    vde::MeshEntity::Ref m_cube;
};

int main() {
    vde::Game game;
    MyInputHandler input;
    
    vde::GameSettings settings;
    settings.gameName = "Rotating Cube";
    settings.setWindowSize(1280, 720);
    
    if (!game.initialize(settings)) return 1;
    
    game.setInputHandler(&input);
    
    auto* scene = new GameScene();
    scene->setInputHandler(&input);
    game.addScene("game", scene);
    game.setActiveScene("game");
    
    game.run();
    return 0;
}
```

### Menu and Gameplay Scenes

```cpp
class MenuScene : public vde::Scene {
public:
    void onEnter() override {
        setCamera(new vde::Camera2D(1920, 1080));
        setBackgroundColor(vde::Color(0.1f, 0.1f, 0.2f));
        // Setup menu UI...
    }
    
    void update(float dt) override {
        auto* input = getInputHandler();
        if (input && input->isKeyPressed(vde::KEY_ENTER)) {
            getGame()->setActiveScene("gameplay");
        }
        vde::Scene::update(dt);
    }
};

class GameplayScene : public vde::Scene {
public:
    void onEnter() override {
        setCamera(new vde::OrbitCamera(vde::Position(0,0,0), 20.0f));
        // Setup gameplay...
    }
    
    void update(float dt) override {
        auto* input = getInputHandler();
        if (input && input->isKeyPressed(vde::KEY_ESCAPE)) {
            getGame()->pushScene("pause");
        }
        vde::Scene::update(dt);
    }
};

class PauseScene : public vde::Scene {
public:
    void update(float dt) override {
        auto* input = getInputHandler();
        if (input && input->isKeyPressed(vde::KEY_ESCAPE)) {
            getGame()->popScene();  // Return to gameplay
        }
        vde::Scene::update(dt);
    }
};
```

---

## Version History

| Version | Date | Notes |
|---------|------|-------|
| 0.1.0 | 2026-01-17 | Initial API design |

---

## See Also

- [API-DESIGN.md](API-DESIGN.md) - Original design document
- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) - Engine architecture
- [docs/GETTING_STARTED.md](docs/GETTING_STARTED.md) - Engine setup guide
