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
- [Physics System](#physics-system)
- [Audio System](#audio-system)
- [Multi-Scene & Split-Screen](#multi-scene--split-screen)
- [Deferred Commands & Thread Safety](#deferred-commands--thread-safety)
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
- **Physics**: 2D AABB physics with collision detection, rigid body dynamics, and impulse-based resolution
- **Audio**: Basic playback for music and sound effects
- **Multi-Scene Support**: Run multiple scenes simultaneously with split-screen viewports

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
├── PhysicsScene.h   # 2D physics simulation
├── PhysicsEntity.h  # Physics-driven entities
├── PhysicsTypes.h   # Physics type definitions
├── AudioClip.h      # Audio resource (sounds/music)
├── AudioSource.h    # Audio emitter component
├── AudioManager.h   # Audio playback system
├── AudioEvent.h     # Audio event queuing
├── SceneGroup.h     # Multi-scene rendering
├── ViewportRect.h   # Split-screen viewports
├── Scheduler.h      # Task scheduling (advanced)
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

#### Deferred Commands (Safe Entity Mutation)

The game loop executes `update()` and `render()` in separate scheduler phases.
Entity mutations (`addEntity`, `removeEntity`, `setMesh`) are **only safe during
the update phase** (`onEnter()`, `update()`, `updateGameLogic()`).

Code that runs during the **render phase** — such as ImGui callbacks in
`onRender()` or `drawDebugUI()` — must **never** call these methods directly,
because the Vulkan command buffer may still reference the GPU buffers being freed.

Use `deferCommand()` to schedule entity mutations for the next update:

```cpp
// Safe: runs in the next update() / updateGameLogic() call
deferCommand([this]() {
    auto entity = addEntity<vde::MeshEntity>();
    entity->setMesh(myMesh);
    entity->setName("spawned");
});
```

When removing or replacing entities/meshes, use `retireResource()` to keep the
GPU buffers alive until the in-flight command buffer has finished:

```cpp
// Inside a deferred command — remove entity safely
deferCommand([this, entityId]() {
    removeEntity(entityId);
});
// Keep the entity's shared_ptr alive for one more frame
retireResource(std::move(entitySharedPtr));
```

Replacing a mesh on a visible entity:

```cpp
deferCommand([this, entityId, newMesh]() {
    auto* entity = getEntity<vde::MeshEntity>(entityId);
    if (entity) {
        auto oldMesh = entity->getMesh();
        if (oldMesh) {
            retireResource(std::move(oldMesh));
        }
        entity->setMesh(newMesh);
    }
});
```

**Key rules:**

| Method | Description |
|--------|-------------|
| `deferCommand(fn)` | Queue a `void()` callable for execution at the start of the next `update()` or `updateGameLogic()` |
| `retireResource(ptr)` | Extend a `shared_ptr` lifetime until the deferred flush (prevents GPU use-after-free) |
| `getDeferredCommandCount()` | Number of pending commands (useful for diagnostics) |

**When to use:**
- Adding/removing entities from ImGui UI callbacks (`onRender()`, `drawDebugUI()`)
- Swapping meshes or textures in response to render-phase user interaction
- Any entity mutation triggered outside of `update()` / `updateGameLogic()` / `onEnter()`

**When it's NOT needed:**
- Entity operations in `onEnter()`, `update()`, `updateGameLogic()` — these run in the safe GameLogic phase
- Reading entity state or calling `getEntity()` (read-only access is always safe)

> Commands are flushed in FIFO order. A deferred command may call `deferCommand()`
> again; the re-entrant command will execute on the *next* update cycle.

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

## Physics System

VDE provides a 2D physics engine with AABB (Axis-Aligned Bounding Box) collision detection, rigid body dynamics, and impulse-based collision resolution. Physics is opt-in per scene and runs on a fixed timestep with interpolation for smooth rendering.

### Enabling Physics

Enable physics in a scene with `enablePhysics()`:

```cpp
class GameScene : public vde::Scene {
    void onEnter() override {
        // Enable physics with default configuration
        enablePhysics();
        
        // Or with custom configuration
        vde::PhysicsConfig config;
        config.gravity = {0.0f, -9.81f};  // 9.8 m/s² downward
        config.fixedTimestep = 1.0f / 60.0f;  // 60 Hz
        config.maxSubSteps = 8;
        config.iterations = 4;
        enablePhysics(config);
    }
};
```

### PhysicsConfig

| Property | Default | Description |
|----------|---------|-------------|
| `gravity` | `{0, -9.81}` | Gravity vector (2D, Y-down is negative) |
| `fixedTimestep` | `1/60` | Physics simulation timestep in seconds |
| `maxSubSteps` | `8` | Maximum sub-steps per frame (spiral-of-death prevention) |
| `iterations` | `4` | Solver iterations per step for stability |

### Physics Body Types

| Type | Description | Use Case |
|------|-------------|----------|
| `Static` | Does not move; infinite mass | Walls, platforms, terrain |
| `Dynamic` | Moves under forces and gravity | Player, enemies, boxes, projectiles |
| `Kinematic` | Moves via user code; not affected by forces | Moving platforms, elevators |

### Physics Shapes

| Shape | Description | Extents |
|-------|-------------|---------|
| `Box` | Axis-aligned rectangle | `{halfWidth, halfHeight}` |
| `Circle` | 2D circle | `{radius, 0}` |

### PhysicsEntity - Physics-Driven Entities

Use `PhysicsEntity` subclasses to create entities that are automatically synchronized with physics bodies:

```cpp
// PhysicsSpriteEntity - 2D sprite with physics
auto entity = scene->addEntity<vde::PhysicsSpriteEntity>();
entity->setTexture(spriteTexture);
entity->setColor(vde::Color::red());

// Create the physics body
vde::PhysicsBodyDef def;
def.type = vde::PhysicsBodyType::Dynamic;
def.shape = vde::PhysicsShape::Box;
def.position = {0.0f, 5.0f};  // Starting position
def.extents = {0.5f, 0.5f};   // Half-width, half-height
def.mass = 1.0f;              // 1 kg
def.friction = 0.3f;
def.restitution = 0.5f;       // Bounciness (0 = no bounce, 1 = perfect)
entity->createPhysicsBody(def);

// PhysicsMeshEntity - 3D mesh with 2D physics
auto meshEntity = scene->addEntity<vde::PhysicsMeshEntity>();
meshEntity->setMesh(cubeMesh);
meshEntity->createPhysicsBody(def);
```

### Applying Forces and Impulses

```cpp
// Apply continuous force (accumulated over step)
entity->applyForce({100.0f, 0.0f});  // 100N to the right

// Apply instantaneous impulse (immediate velocity change)
entity->applyImpulse({0.0f, 500.0f});  // Upward impulse

// Set velocity directly
entity->setLinearVelocity({5.0f, 0.0f});

// Get current physics state
vde::PhysicsBodyState state = entity->getPhysicsState();
glm::vec2 pos = state.position;
glm::vec2 vel = state.velocity;
float rot = state.rotation;  // radians
```

### Creating Static Bodies (Platforms, Walls)

```cpp
// Create a static platform entity
auto platform = scene->addEntity<vde::PhysicsSpriteEntity>();
platform->setTexture(platformTexture);
platform->setPosition(0.0f, -2.0f, 0.0f);

vde::PhysicsBodyDef platformDef;
platformDef.type = vde::PhysicsBodyType::Static;
platformDef.shape = vde::PhysicsShape::Box;
platformDef.position = {0.0f, -2.0f};
platformDef.extents = {5.0f, 0.5f};  // 10m wide, 1m tall
platform->createPhysicsBody(platformDef);
```

### PhysicsBodyDef Properties

```cpp
vde::PhysicsBodyDef def;
def.type = vde::PhysicsBodyType::Dynamic;  // Body type
def.shape = vde::PhysicsShape::Box;        // Collision shape
def.position = {0.0f, 5.0f};               // Initial position
def.rotation = 0.0f;                       // Initial rotation (radians)
def.extents = {0.5f, 0.5f};                // Half-extents (box) or {radius, 0} (circle)
def.mass = 1.0f;                           // Mass in kg (ignored for Static/Kinematic)
def.friction = 0.3f;                       // Surface friction coefficient
def.restitution = 0.2f;                    // Bounciness (0 = inelastic, 1 = perfectly elastic)
def.linearDamping = 0.01f;                 // Linear velocity damping
def.isSensor = false;                      // If true, triggers callbacks but no collision response
```

### Collision Detection & Callbacks

Set up collision callbacks in your scene:

```cpp
class GameScene : public vde::Scene {
    void onEnter() override {
        enablePhysics();
        
        // Get the physics scene and set callbacks
        if (auto* physics = getPhysicsScene()) {
            physics->setOnCollisionBegin([this](const vde::CollisionEvent& evt) {
                onCollision(evt);
            });
        }
    }
    
    void onCollision(const vde::CollisionEvent& evt) {
        // Find entities involved in the collision
        auto* entityA = getEntityByPhysicsBody(evt.bodyA);
        auto* entityB = getEntityByPhysicsBody(evt.bodyB);
        
        if (entityA && entityB) {
            // Handle collision (play sound, apply damage, etc.)
            glm::vec2 contactPoint = evt.contactPoint;
            glm::vec2 normal = evt.normal;  // From A to B
            float depth = evt.depth;
        }
    }
};
```

### CollisionEvent

```cpp
struct CollisionEvent {
    PhysicsBodyId bodyA;           // First body
    PhysicsBodyId bodyB;           // Second body
    glm::vec2 contactPoint;        // Approximate contact point
    glm::vec2 normal;              // Collision normal (from A to B)
    float depth;                   // Penetration depth
};
```

### Direct PhysicsScene Access (Advanced)

For advanced use cases, you can access the `PhysicsScene` directly:

```cpp
auto* physics = scene->getPhysicsScene();

// Create bodies without entities
vde::PhysicsBodyDef def;
def.type = vde::PhysicsBodyType::Dynamic;
def.shape = vde::PhysicsShape::Circle;
def.position = {0.0f, 10.0f};
def.extents = {0.5f, 0.0f};  // radius = 0.5
def.mass = 2.0f;

vde::PhysicsBodyId bodyId = physics->createBody(def);

// Query body state
vde::PhysicsBodyState state = physics->getBodyState(bodyId);

// Apply forces
physics->applyForce(bodyId, {100.0f, 0.0f});
physics->applyImpulse(bodyId, {0.0f, 200.0f});
physics->setLinearVelocity(bodyId, {5.0f, 0.0f});
physics->setBodyPosition(bodyId, {10.0f, 5.0f});

// Destroy body
physics->destroyBody(bodyId);
```

### Automatic Synchronization

Physics entities automatically synchronize their visual transform with the physics body:

- **After Physics Step**: The entity's position/rotation is updated from the physics body with interpolation
- **Auto-Sync**: Enabled by default; disable with `setAutoSync(false)` if you want manual control
- **Manual Sync**: Call `syncFromPhysics(alpha)` or `syncToPhysics()` manually

```cpp
// Disable auto-sync for custom control
entity->setAutoSync(false);

// Manually sync from physics (in render or late update)
entity->syncFromPhysics(interpolationAlpha);

// Or sync entity position to physics (teleport)
entity->setPosition(10.0f, 5.0f, 0.0f);
entity->syncToPhysics();
```

### Advanced Physics Features

#### Raycasting

Perform raycasts to detect bodies along a line:

```cpp
auto* physics = scene->getPhysicsScene();

glm::vec2 origin = {0.0f, 10.0f};
glm::vec2 direction = {1.0f, -1.0f};  // Will be normalized
float maxDistance = 20.0f;

vde::RaycastHit hit;
if (physics->raycast(origin, direction, maxDistance, hit)) {
    // Hit something
    auto* entity = getEntityByPhysicsBody(hit.bodyId);
    glm::vec2 hitPoint = hit.point;
    glm::vec2 hitNormal = hit.normal;
    float distance = hit.distance;
}
```

#### Spatial Queries (AABB)

Query all bodies within a rectangular region:

```cpp
auto* physics = scene->getPhysicsScene();

glm::vec2 queryMin = {-5.0f, -5.0f};
glm::vec2 queryMax = {5.0f, 5.0f};

auto bodies = physics->queryAABB(queryMin, queryMax);
for (auto bodyId : bodies) {
    auto* entity = getEntityByPhysicsBody(bodyId);
    // Do something with entity
}
```

#### Per-Body Collision Callbacks

Set callbacks for specific bodies:

```cpp
auto* physics = scene->getPhysicsScene();

// Per-body collision begin
physics->setBodyOnCollisionBegin(playerBodyId, [this](const vde::CollisionEvent& evt) {
    // Only called when playerBodyId collides
    auto* other = getEntityByPhysicsBody(evt.bodyB);
    if (other->getName() == "enemy") {
        takeDamage();
    }
});

// Per-body collision end
physics->setBodyOnCollisionEnd(playerBodyId, [this](const vde::CollisionEvent& evt) {
    // Called when playerBodyId stops colliding
});
```

#### Global Collision Callbacks

```cpp
auto* physics = scene->getPhysicsScene();

// All collision begin events
physics->setOnCollisionBegin([this](const vde::CollisionEvent& evt) {
    // Called for all collisions
});

// All collision end events
physics->setOnCollisionEnd([this](const vde::CollisionEvent& evt) {
    // Called when bodies stop colliding
});
```

#### Physics Queries

```cpp
auto* physics = scene->getPhysicsScene();

// Get body counts
size_t totalBodies = physics->getBodyCount();       // Including destroyed slots
size_t activeBodies = physics->getActiveBodyCount(); // Only active bodies

// Per-frame stats
int steps = physics->getLastStepCount();  // Sub-steps taken last frame
float alpha = physics->getInterpolationAlpha();  // For interpolation

// Check if body exists
bool exists = physics->hasBody(bodyId);

// Get body definition
vde::PhysicsBodyDef def = physics->getBodyDef(bodyId);
```

### Complete Physics Example

```cpp
class PhysicsGameScene : public vde::Scene {
public:
    void onEnter() override {
        // Enable physics
        vde::PhysicsConfig config;
        config.gravity = {0.0f, -20.0f};  // Stronger gravity
        enablePhysics(config);
        
        // Setup camera and lighting
        setCamera(new vde::Camera2D(1920, 1080));
        setLightBox(new vde::SimpleColorLightBox(vde::Color::white()));
        
        // Load textures
        auto boxTexture = getGame()->getResourceManager().load<vde::Texture>("box.png");
        auto groundTexture = getGame()->getResourceManager().load<vde::Texture>("ground.png");
        
        // Create ground (static)
        auto ground = addEntity<vde::PhysicsSpriteEntity>();
        ground->setTexture(groundTexture);
        ground->setPosition(0.0f, -5.0f, 0.0f);
        
        vde::PhysicsBodyDef groundDef;
        groundDef.type = vde::PhysicsBodyType::Static;
        groundDef.shape = vde::PhysicsShape::Box;
        groundDef.position = {0.0f, -5.0f};
        groundDef.extents = {10.0f, 1.0f};
        ground->createPhysicsBody(groundDef);
        
        // Create falling boxes (dynamic)
        for (int i = 0; i < 5; i++) {
            auto box = addEntity<vde::PhysicsSpriteEntity>();
            box->setTexture(boxTexture);
            box->setColor(vde::Color::fromHex(0xFF0000 + i * 0x003300));
            
            vde::PhysicsBodyDef boxDef;
            boxDef.type = vde::PhysicsBodyType::Dynamic;
            boxDef.shape = vde::PhysicsShape::Box;
            boxDef.position = {i * 2.0f - 4.0f, 10.0f + i * 2.0f};
            boxDef.extents = {0.5f, 0.5f};
            boxDef.mass = 1.0f;
            boxDef.restitution = 0.6f;  // Bouncy
            box->createPhysicsBody(boxDef);
        }
        
        // Set collision callback
        if (auto* physics = getPhysicsScene()) {
            physics->setOnCollisionBegin([](const vde::CollisionEvent& evt) {
                // Play collision sound, spawn particles, etc.
            });
        }
    }
};
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

## Multi-Scene & Split-Screen

VDE supports running multiple scenes simultaneously with independent viewports, enabling split-screen multiplayer, picture-in-picture minimaps, HUDs overlaid on gameplay, and other multi-scene rendering scenarios.

### SceneGroup - Multiple Active Scenes

A `SceneGroup` describes a set of scenes that should be active simultaneously. Each scene can have its own camera, viewport, and update logic.

#### Basic Scene Group

```cpp
// Create a group with multiple scenes (all use full window)
auto group = vde::SceneGroup::create("gameplay_with_hud", {
    "world",    // Primary scene (rendered first)
    "hud",      // UI overlay
    "minimap"   // Picture-in-picture map
});

game.setActiveSceneGroup(group);
```

#### Scene Group with Split-Screen Viewports

```cpp
// Two-player split-screen (left/right)
auto splitscreen = vde::SceneGroup::createWithViewports("splitscreen", {
    {"player1", vde::ViewportRect::leftHalf()},
    {"player2", vde::ViewportRect::rightHalf()}
});

game.setActiveSceneGroup(splitscreen);

// Four-player split-screen (quadrants)
auto fourPlayer = vde::SceneGroup::createWithViewports("four_player", {
    {"player1", vde::ViewportRect::topLeft()},
    {"player2", vde::ViewportRect::topRight()},
    {"player3", vde::ViewportRect::bottomLeft()},
    {"player4", vde::ViewportRect::bottomRight()}
});

game.setActiveSceneGroup(fourPlayer);
```

#### Custom Viewport Layouts

```cpp
// Picture-in-picture minimap (top-right corner)
vde::SceneGroupEntry minimapEntry;
minimapEntry.sceneName = "minimap";
minimapEntry.viewport = vde::ViewportRect{0.75f, 0.0f, 0.25f, 0.25f};  // x, y, width, height

auto group = vde::SceneGroup::createWithViewports("gameplay_minimap", {
    {"world", vde::ViewportRect::fullWindow()},
    minimapEntry
});

game.setActiveSceneGroup(group);
```

### ViewportRect - Normalized Viewport Coordinates

`ViewportRect` defines a sub-region of the window using normalized [0, 1] coordinates, where (0, 0) is the top-left corner and (1, 1) is the bottom-right.

```cpp
vde::ViewportRect viewport;
viewport.x = 0.0f;       // Left edge (0 = left, 1 = right)
viewport.y = 0.0f;       // Top edge (0 = top, 1 = bottom)
viewport.width = 0.5f;   // Width (0 to 1)
viewport.height = 0.5f;  // Height (0 to 1)
```

#### Predefined Viewport Layouts

| Factory Method | Description | Coordinates |
|----------------|-------------|-------------|
| `fullWindow()` | Entire window (default) | `{0, 0, 1, 1}` |
| `leftHalf()` | Left half | `{0, 0, 0.5, 1}` |
| `rightHalf()` | Right half | `{0.5, 0, 0.5, 1}` |
| `topHalf()` | Top half | `{0, 0, 1, 0.5}` |
| `bottomHalf()` | Bottom half | `{0, 0.5, 1, 0.5}` |
| `topLeft()` | Top-left quadrant | `{0, 0, 0.5, 0.5}` |
| `topRight()` | Top-right quadrant | `{0.5, 0, 0.5, 0.5}` |
| `bottomLeft()` | Bottom-left quadrant | `{0, 0.5, 0.5, 0.5}` |
| `bottomRight()` | Bottom-right quadrant | `{0.5, 0.5, 0.5, 0.5}` |

#### Viewport Hit Testing

```cpp
vde::ViewportRect viewport = vde::ViewportRect::topRight();

// Check if normalized coordinates are inside viewport
bool hit = viewport.containsNormalized(0.6f, 0.2f);

// Check if pixel coordinates are inside viewport (given window size)
bool hitPixel = viewport.containsPixel(1200, 100, 1920, 1080);
```

### Scene Lifecycle in Groups

When a `SceneGroup` is active:

1. All scenes in the group receive `onEnter()` when the group becomes active
2. Each scene's `update(deltaTime)` is called every frame
3. Each scene can have its own camera and viewport
4. The first scene in the group is the "primary" scene (controls background color)
5. All scenes receive `onExit()` when the group is deactivated

### Setting Viewports Per Scene

You can also set viewports directly on scenes:

```cpp
class MinimapScene : public vde::Scene {
    void onEnter() override {
        // Set viewport for this scene (top-right corner)
        setViewport(vde::ViewportRect{0.75f, 0.0f, 0.25f, 0.25f});
        
        // Setup camera for minimap view
        setCamera(new vde::OrbitCamera(vde::Position(0, 100, 0), 150.0f, 89.0f, 0.0f));
    }
};
```

### Complete Split-Screen Example

```cpp
// Player scene with independent camera
class PlayerScene : public vde::Scene {
    int m_playerId;
    
public:
    PlayerScene(int playerId) : m_playerId(playerId) {}
    
    void onEnter() override {
        // Each player gets their own camera
        setCamera(new vde::OrbitCamera(
            vde::Position(0, 5, 0),
            15.0f, 45.0f, m_playerId * 45.0f  // Different starting yaw
        ));
        
        setLightBox(new vde::SimpleColorLightBox(vde::Color::white()));
        
        // Create player entity
        auto player = addEntity<vde::MeshEntity>();
        player->setMesh(getGame()->getResourceManager().load<vde::Mesh>("player.obj"));
        player->setPosition(m_playerId * 5.0f, 0, 0);
    }
    
    void update(float dt) override {
        // Each player's scene updates independently
        // Handle input, move camera, etc.
        vde::Scene::update(dt);
    }
};

// Main setup
int main() {
    vde::Game game;
    vde::GameSettings settings;
    settings.gameName = "Split-Screen Demo";
    settings.setWindowSize(1920, 1080);
    
    if (!game.initialize(settings)) return 1;
    
    // Create player scenes
    game.addScene("player1", new PlayerScene(0));
    game.addScene("player2", new PlayerScene(1));
    
    // Create split-screen group
    auto splitscreen = vde::SceneGroup::createWithViewports("splitscreen", {
        {"player1", vde::ViewportRect::leftHalf()},
        {"player2", vde::ViewportRect::rightHalf()}
    });
    
    game.setActiveSceneGroup(splitscreen);
    game.run();
    
    return 0;
}
```

### Switching Between Single Scene and Groups

```cpp
// Switch to single scene
game.setActiveScene("menu");

// Switch to scene group
auto gameplayGroup = vde::SceneGroup::create("gameplay", {"world", "hud"});
game.setActiveSceneGroup(gameplayGroup);

// Return to single scene
game.setActiveScene("pause_menu");
```

### Input Handling with Multiple Scenes

Each scene can have its own input handler. When multiple scenes are active, input events are dispatched to all active scenes in order:

```cpp
class Player1Scene : public vde::Scene {
    MyInputHandler m_input;
    
    void onEnter() override {
        setInputHandler(&m_input);
        
        // Player 1 uses WASD and Space
        m_input.setKeybindings(vde::KEY_W, vde::KEY_S, vde::KEY_A, vde::KEY_D, vde::KEY_SPACE);
    }
};

class Player2Scene : public vde::Scene {
    MyInputHandler m_input;
    
    void onEnter() override {
        setInputHandler(&m_input);
        
        // Player 2 uses arrow keys and Enter
        m_input.setKeybindings(vde::KEY_UP, vde::KEY_DOWN, vde::KEY_LEFT, vde::KEY_RIGHT, vde::KEY_ENTER);
    }
};
```

---

## Deferred Commands & Thread Safety

The VDE game loop uses a **scheduler** that executes work in ordered phases:

```
Input → GameLogic → Audio → Physics → PostPhysics → PreRender → Render
```

**Entity mutations** (`addEntity`, `removeEntity`, `setMesh`, `clearEntities`) modify
GPU-visible resources and are **only safe during the GameLogic phase** — that is, inside
`onEnter()`, `update()`, or `updateGameLogic()`.

The **Render phase** records a Vulkan command buffer that references entity meshes and
buffers. Mutating entities during this phase (e.g., from an `onRender()` or
`drawDebugUI()` callback) can free GPU memory while the command buffer still uses it,
causing crashes or validation errors.

### deferCommand()

Queue a callable that will execute at the very start of the next `update()` or
`updateGameLogic()`:

```cpp
void deferCommand(std::function<void()> command);
```

```cpp
// Example: spawn entity from an ImGui button (render-phase code)
if (ImGui::Button("Add Cube")) {
    deferCommand([this]() {
        auto cube = addEntity<vde::MeshEntity>();
        cube->setMesh(vde::Mesh::createCube(1.0f));
    });
}
```

Commands execute in FIFO order. A command may call `deferCommand()` again; the
re-entrant command runs on the *next* update cycle.

### retireResource()

Extend a `shared_ptr` lifetime until the deferred flush so that GPU buffers
remain valid through the current frame's command buffer submission:

```cpp
void retireResource(std::shared_ptr<void> resource);
```

```cpp
// Remove an entity from render-phase code
deferCommand([this, eid]() { removeEntity(eid); });
retireResource(std::move(entityPtr));  // GPU buffers stay alive one more frame

// Replace a mesh
deferCommand([this, entity, newMesh]() {
    auto old = entity->getMesh();
    if (old) retireResource(std::move(old));
    entity->setMesh(newMesh);
});
```

### getDeferredCommandCount()

```cpp
size_t getDeferredCommandCount() const;
```

Returns the number of pending commands. Useful for diagnostics or tests.

### Summary Table

| Context | Safe to mutate entities? | What to use |
|---------|:------------------------:|-------------|
| `onEnter()` | Yes | Direct calls |
| `update()` / `updateGameLogic()` | Yes | Direct calls |
| `onRender()` / `drawDebugUI()` | **No** | `deferCommand()` + `retireResource()` |
| Input callbacks | Yes (dispatched during GameLogic) | Direct calls |
| Physics callbacks | Yes (dispatched during Physics) | Direct calls |

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
| 0.2.0 | 2026-02-08 | Added Physics System and Multi-Scene/Split-Screen documentation |

---

## See Also

- [API-DESIGN.md](API-DESIGN.md) - Original design document
- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) - Engine architecture
- [docs/GETTING_STARTED.md](docs/GETTING_STARTED.md) - Engine setup guide
