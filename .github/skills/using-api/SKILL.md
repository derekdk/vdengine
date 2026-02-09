---
name: using-api
description: Guide for using the VDE Game API to create games, demos, applications, and examples. Use this when working with the Scene/Entity/Resource system or implementing gameplay features.
---

# Using the VDE API

This skill provides guidance on using the VDE Game API to create games, demos, applications, and examples. It focuses on practical implementation patterns and best practices.

## When to use this skill

- Creating a new game or application using the VDE API
- Building interactive demos that use the Game, Scene, and Entity system
- Implementing gameplay features with the high-level API
- Working with entities, resources, cameras, and lighting
- Setting up input handling (keyboard, mouse, gamepad)
- Implementing audio playback (music and sound effects)
- Working with 2D games and pixel-to-world coordinate conversion
- Setting up world boundaries and camera constraints
- Using materials for mesh rendering
- Need guidance on API patterns and common tasks

## Core Principles

When using the VDE API to build applications:

1. **Use the API as currently designed** - Work with existing classes and methods
2. **Follow established patterns** - Match the style of existing examples
3. **Suggest improvements separately** - If API limitations are found, document suggestions but complete the task with the current API
4. **Reference documentation** - Always check API-DOC.md and docs/API.md for current capabilities

## API Structure Overview

The VDE Game API has a hierarchical structure:

```
Game (manages the game loop and scenes)
├── Scene (represents a game state)
│   ├── Entities (game objects: MeshEntity, SpriteEntity)
│   ├── Resources (shared assets: Mesh, Texture, Material)
│   ├── Camera (view control: OrbitCamera, SimpleCamera, Camera2D)
│   └── LightBox (lighting: SimpleColorLightBox, etc.)
├── InputHandler (keyboard, mouse, gamepad input)
├── ResourceManager (global resource cache)
└── AudioManager (audio playback)
```

### Ownership Rules

- **Game** owns **Scenes**
- **Scene** owns **Entities** (shared_ptr) and **Resources** (shared_ptr)
- **Scene** owns **Camera** (unique_ptr) and **LightBox** (unique_ptr)
- **InputHandler** is NOT owned (raw pointer, usually stack-allocated)

## Standard Application Pattern

### 1. Basic Structure

Every application using the Game API should follow this pattern:

```cpp
#include <vde/api/GameAPI.h>

int main() {
    // 1. Create game instance
    vde::Game game;
    
    // 2. Configure settings
    vde::GameSettings settings;
    settings.gameName = "Your Game Name";
    settings.setWindowSize(1280, 720);
    
    // 3. Initialize game
    if (!game.initialize(settings)) {
        return 1;
    }
    
    // 4. Create and add scenes
    game.addScene("main", new MainScene());
    game.setActiveScene("main");
    
    // 5. Run the game loop
    game.run();
    
    return 0;
}
```

### 2. Scene Implementation

Scenes are where game logic lives:

```cpp
class MainScene : public vde::Scene {
public:
    void onEnter() override {
        // Called when scene becomes active
        setupCamera();
        setupLighting();
        loadResources();
        createEntities();
    }
    
    void onExit() override {
        // Called when leaving the scene
        // Cleanup if needed (usually automatic)
    }
    
    void onPause() override {
        // Called when another scene is pushed on top
    }
    
    void onResume() override {
        // Called when returning to top of stack
    }
    
    void update(float deltaTime) override {
        // Update game logic
        updateEntities(deltaTime);
        
        // IMPORTANT: Call base class
        vde::Scene::update(deltaTime);
    }
    
    void render() override {
        // Custom rendering if needed
        // Most cases just call base
        vde::Scene::render();
    }

private:
    void setupCamera() {
        // Create camera (Scene takes ownership)
        setCamera(new vde::OrbitCamera(
            vde::Position(0, 0, 0),  // target
            10.0f                     // distance
        ));
    }
    
    void setupLighting() {
        // Create light box (Scene takes ownership)
        setLightBox(new vde::SimpleColorLightBox(
            vde::Color::white()
        ));
    }
    
    void loadResources() {
        // Load shared resources via ResourceManager
        auto& resources = getGame()->getResourceManager();
        m_mesh = resources.load<vde::Mesh>("models/mymodel.obj");
        m_texture = resources.load<vde::Texture>("textures/mytexture.png");
        
        // Upload texture to GPU for sprite rendering
        if (auto* context = getGame()->getVulkanContext()) {
            m_texture->uploadToGPU(context);
        }
    }
    
    void createEntities() {
        // Create mesh entity
        auto entity = addEntity<vde::MeshEntity>();
        entity->setMesh(m_mesh);
        entity->setPosition(0, 0, 0);
        
        // Store entity ID for later access
        m_entityId = entity->getId();
    }
    
    void updateEntities(float deltaTime) {
        // Access and update entities
        if (auto entity = getEntity<vde::MeshEntity>(m_entityId)) {
            // Update entity
        }
    }

private:
    vde::ResourcePtr<vde::Mesh> m_mesh;
    vde::ResourcePtr<vde::Texture> m_texture;
    vde::EntityId m_entityId;
};
```

### 3. Input Handling

Input handlers process user input:

```cpp
class MyInputHandler : public vde::InputHandler {
public:
    // Keyboard
    void onKeyPress(int key) override {
        if (key == vde::KEY_SPACE) {
            m_spacePressed = true;
        }
        if (key == vde::KEY_ESCAPE) {
            // Request game exit
        }
    }
    
    void onKeyRelease(int key) override {
        // Handle key release
    }
    
    void onKeyRepeat(int key) override {
        // Handle key repeat
    }
    
    // Mouse
    void onMouseMove(double x, double y) override {
        m_mouseX = x;
        m_mouseY = y;
    }
    
    void onMouseButtonPress(int button, double x, double y) override {
        if (button == vde::MOUSE_BUTTON_LEFT) {
            // Handle left click
        }
    }
    
    void onMouseScroll(double xOffset, double yOffset) override {
        m_zoom += yOffset;
    }
    
    // Gamepad
    void onGamepadConnect(int gamepadId, const char* name) override {
        std::cout << "Gamepad connected: " << name << "\n";
    }
    
    void onGamepadDisconnect(int gamepadId) override {
        std::cout << "Gamepad disconnected\n";
    }
    
    void onGamepadButtonPress(int gamepadId, int button) override {
        if (button == vde::GAMEPAD_BUTTON_A) {
            m_jumpPressed = true;
        }
    }
    
    void onGamepadAxis(int gamepadId, int axis, float value) override {
        if (axis == vde::GAMEPAD_AXIS_LEFT_X) {
            m_moveX = value;
        }
    }
    
    // Query methods for game logic
    bool isSpacePressed() {
        bool val = m_spacePressed;
        m_spacePressed = false;
        return val;
    }
    
    bool isJumpPressed() {
        bool val = m_jumpPressed;
        m_jumpPressed = false;
        return val;
    }

private:
    bool m_spacePressed = false;
    bool m_jumpPressed = false;
    double m_mouseX = 0.0;
    double m_mouseY = 0.0;
    float m_zoom = 1.0f;
    float m_moveX = 0.0f;
};

// Usage in scene:
void update(float deltaTime) override {
    auto* input = dynamic_cast<MyInputHandler*>(getInputHandler());
    if (input && input->isSpacePressed()) {
        // Handle space press
    }
}
```

## Common Tasks

### Creating Entities

#### Mesh Entities (3D Objects)

```cpp
// In scene initialization:
auto& resources = getGame()->getResourceManager();
auto mesh = resources.load<vde::Mesh>("models/cube.obj");

// Create entity and set mesh
auto entity = addEntity<vde::MeshEntity>();
entity->setMesh(mesh);
entity->setPosition(0, 0, 0);
entity->setRotation(0, 45, 0);  // Euler angles in degrees
entity->setScale(1, 1, 1);

// Set material properties
auto material = vde::Material::createColored(vde::Color(0.8f, 0.2f, 0.1f));
material->setRoughness(0.4f);
material->setMetallic(0.1f);
entity->setMaterial(material);

// Store ID for later access
m_cubeId = entity->getId();
```

#### Sprite Entities (2D Objects)

```cpp
// Load sprite texture via ResourceManager
auto& resources = getGame()->getResourceManager();
auto spriteTexture = resources.load<vde::Texture>("sprites/player.png");

// IMPORTANT: Upload texture to GPU before use
if (auto* context = getGame()->getVulkanContext()) {
    spriteTexture->uploadToGPU(context);
}

// Create sprite entity
auto sprite = addEntity<vde::SpriteEntity>();
sprite->setTexture(spriteTexture);
sprite->setPosition(0, 0, 0);
sprite->setScale(1, 1);  // Width and height in world units
sprite->setColor(vde::Color::white());

// For sprite sheets, set UV rectangle:
sprite->setUVRect(0.0f, 0.0f, 0.25f, 0.25f);  // Top-left quarter

// Set anchor point (0,0 = top-left, 0.5,0.5 = center, 1,1 = bottom-right)
sprite->setAnchor(0.5f, 0.5f);  // Center the sprite
```

### Working with Cameras

#### OrbitCamera (Good for 3D scenes)

```cpp
auto camera = new vde::OrbitCamera(
    vde::Position(0, 0, 0),  // target
    15.0f,                    // distance
    45.0f,                    // pitch (degrees)
    0.0f                      // yaw (degrees)
);
camera->setZoomLimits(5.0f, 50.0f);
camera->setPitchLimits(10.0f, 80.0f);
setCamera(camera);

// Controls
camera->rotate(0, 5);   // rotate around target
camera->zoom(-1);       // zoom out
camera->pan(1, 0);      // pan right
```

#### SimpleCamera (First-person or free camera)

```cpp
auto camera = new vde::SimpleCamera(
    vde::Position(0, 5, 10),      // position
    vde::Direction(0, -0.3f, -1)  // look direction
);
camera->setFieldOfView(60.0f);
setCamera(camera);

// Controls
camera->move(vde::Direction(0, 0, -1));  // move forward
camera->rotate(0, 45);  // rotate yaw 45 degrees
```

#### Camera2D (For 2D games)

```cpp
auto camera = new vde::Camera2D(1920, 1080);  // viewport size
camera->setPosition(0, 0);
camera->setZoom(2.0f);      // 2x zoom
camera->setRotation(45.0f); // rotate 45 degrees
setCamera(camera);
```

### Lighting

#### Simple Color Lighting

```cpp
setLightBox(new vde::SimpleColorLightBox(vde::Color::white()));
```

#### With Custom Color

```cpp
setLightBox(new vde::SimpleColorLightBox(
    vde::Color(1.0f, 0.9f, 0.8f)  // Warm white
));
```

### Resources and ResourceManager

#### Using ResourceManager (Global Cache)

The `ResourceManager` provides a global cache for resources shared across scenes:

```cpp
// Access the global resource manager
auto& resources = getGame()->getResourceManager();

// Load (or get cached) resources
auto mesh = resources.load<vde::Mesh>("models/tree.obj");
auto texture = resources.load<vde::Texture>("textures/bark.png");

// Use the same resource again elsewhere (returns cached instance)
auto mesh2 = resources.load<vde::Mesh>("models/tree.obj");
// mesh and mesh2 point to the same resource

// Use it for multiple entities
for (int i = 0; i < 10; i++) {
    auto tree = addEntity<vde::MeshEntity>();
    tree->setMesh(mesh);
    tree->setPosition(i * 5.0f, 0, 0);
}
```

#### Creating Primitive Meshes

```cpp
auto cube = vde::Mesh::createCube(1.0f);
auto sphere = vde::Mesh::createSphere(0.5f, 32, 16);
auto plane = vde::Mesh::createPlane(10.0f, 10.0f);
auto cylinder = vde::Mesh::createCylinder(0.5f, 2.0f, 32);

auto entity = addEntity<vde::MeshEntity>();
entity->setMesh(cube);
```

#### Material Properties

```cpp
auto material = vde::Material::createColored(vde::Color(0.8f, 0.2f, 0.1f));
material->setRoughness(0.4f);
material->setMetallic(0.1f);

auto entity = addEntity<vde::MeshEntity>();
entity->setMesh(mesh);
entity->setMaterial(material);
```

### Entity Updates

```cpp
void update(float deltaTime) override {
    // Update specific entity
    if (auto entity = getEntity<vde::MeshEntity>(m_entityId)) {
        auto pos = entity->getPosition();
        pos.y += 1.0f * deltaTime;  // Move up
        entity->setPosition(pos);
        
        // Rotate
        auto rot = entity->getRotation();
        rot.y += 45.0f * deltaTime;  // Rotate 45 deg/sec
        entity->setRotation(rot);
    }
    
    vde::Scene::update(deltaTime);
}
```

### Audio

#### Loading and Playing Audio

```cpp
// Load audio clip via ResourceManager
auto& resources = getGame()->getResourceManager();
auto laserSound = resources.load<vde::AudioClip>("audio/laser.wav");
auto bgMusic = resources.load<vde::AudioClip>("audio/music.ogg");

// Play a one-shot sound effect
vde::AudioManager::getInstance().playSFX(laserSound, 0.8f);

// Play looping background music
vde::AudioManager::getInstance().playMusic(bgMusic, 0.6f, true, 1.0f);
```

#### 3D Audio Source

```cpp
vde::AudioSource source;
source.setClip(laserSound);
source.setSpatial(true);
source.setPosition(0.0f, 1.0f, 0.0f);
source.play(true);  // loop
```

### World Coordinates & Bounds

VDE provides type-safe world coordinate APIs for explicit units and preventing common bugs.

#### Coordinate System

VDE uses a right-handed Y-up coordinate system:

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

#### Type-Safe Units

```cpp
#include <vde/api/WorldUnits.h>

// Using user-defined literals (recommended)
vde::Meters distance = 100_m;
vde::Pixels screenX = 1920_px;

// Arithmetic
vde::Meters sum = 50_m + 30_m;     // 80 meters
vde::Meters scaled = 100_m * 2;    // 200 meters

// Comparisons
if (distance > 50_m) { /* ... */ }

// Implicit conversion to float for rendering
float raw = distance;  // 100.0f
```

#### WorldPoint - 3D Position with Units

```cpp
// Direct construction
vde::WorldPoint pt(10_m, 5_m, 20_m);  // x, y, z

// From cardinal directions (north, east, up)
vde::WorldPoint pt2 = vde::WorldPoint::fromDirections(100_m, 50_m, 20_m);
// Creates: x=50 (east), y=20 (up), z=100 (north)

// Convert to GLM for rendering
glm::vec3 v = pt.toVec3();
```

#### WorldBounds - 3D Scene Boundaries

```cpp
// Create 200m x 200m x 30m world bounds
auto bounds = vde::WorldBounds::fromDirectionalLimits(
    100_m,                          // north limit (+Z)
    vde::WorldBounds::south(100_m), // south limit (-Z)
    vde::WorldBounds::west(100_m),  // west limit (-X)
    100_m,                          // east limit (+X)
    20_m,                           // up limit (+Y)
    vde::WorldBounds::down(10_m)    // down limit (-Y)
);

// Query dimensions
vde::Meters width = bounds.width();   // 200m (east-west)
vde::Meters depth = bounds.depth();   // 200m (north-south)
vde::Meters height = bounds.height(); // 30m (up-down)

// Point containment
vde::WorldPoint pt(0_m, 0_m, 0_m);
bool inside = bounds.contains(pt);  // true

// Set scene bounds
setWorldBounds(bounds);
```

#### WorldBounds2D - 2D Scene Boundaries

```cpp
// Top-down game using cardinal directions
auto bounds = vde::WorldBounds2D::fromCardinal(
    100_m,   // north
    -100_m,  // south
    -100_m,  // west
    100_m    // east
);

// Side-scroller using left/right/top/bottom
auto level = vde::WorldBounds2D::fromLRTB(
    0_m,     // left
    1000_m,  // right
    100_m,   // top
    0_m      // bottom
);

// Convert to 3D bounds for unified APIs
vde::WorldBounds bounds3D = bounds.toWorldBounds(10_m, 0_m);
```

#### CameraBounds2D - Pixel-to-World Mapping

For 2D games, `CameraBounds2D` provides coordinate conversion:

```cpp
vde::CameraBounds2D camera;

// Configure screen size
camera.setScreenSize(1920_px, 1080_px);

// Set visible world width (height derived from aspect ratio)
camera.setWorldWidth(16_m);

// Position camera
camera.centerOn(0_m, 0_m);

// Zoom
camera.setZoom(2.0f);  // 2x zoom

// Constrain to world bounds
camera.setConstraintBounds(vde::WorldBounds2D::fromCardinal(
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
vde::WorldBounds2D visible = camera.getVisibleBounds();
```

#### 2D Game Setup Example

```cpp
class My2DScene : public vde::Scene {
    vde::CameraBounds2D m_camera;
    vde::ResourcePtr<vde::Texture> m_spriteTexture;
    
    void onEnter() override {
        // Define world bounds (100m x 100m)
        auto worldBounds = vde::WorldBounds2D::fromCardinal(
            50_m, -50_m, -50_m, 50_m
        );
        setWorldBounds(worldBounds.toWorldBounds(10_m, 0_m));
        
        // Configure camera
        m_camera.setScreenSize(1920_px, 1080_px);
        m_camera.setWorldWidth(20_m);  // Show 20m horizontally
        m_camera.setConstraintBounds(worldBounds);
        m_camera.centerOn(0_m, 0_m);
        
        // Load and upload sprite texture
        auto& resources = getGame()->getResourceManager();
        m_spriteTexture = resources.load<vde::Texture>("sprite.png");
        if (auto* context = getGame()->getVulkanContext()) {
            m_spriteTexture->uploadToGPU(context);
        }
        
        // Create sprite
        auto sprite = addEntity<vde::SpriteEntity>();
        sprite->setTexture(m_spriteTexture);
        sprite->setPosition(0, 0, 0);
    }
    
    void onMouseClick(double screenX, double screenY) {
        // Convert click to world coordinates
        glm::vec2 worldPos = m_camera.screenToWorld(
            vde::Pixels(screenX), 
            vde::Pixels(screenY)
        );
        
        // Spawn entity at click position
        auto entity = addEntity<vde::SpriteEntity>();
        entity->setTexture(m_spriteTexture);
        entity->setPosition(worldPos.x, worldPos.y, 0);
    }
    
    void update(float dt) override {
        // Pan camera with arrow keys
        auto* input = getInputHandler();
        vde::Meters panSpeed = 10_m;
        
        if (input && input->isKeyPressed(vde::KEY_LEFT)) {
            auto center = m_camera.getCenter();
            m_camera.centerOn(center.x - panSpeed * dt, center.y);
        }
        // ... similar for other directions
        
        vde::Scene::update(dt);
    }
};
```

### Scene Management

```cpp
// Switch to different scene
game.setActiveScene("gameplay");

// Push overlay scene (e.g., pause menu)
game.pushScene("pause");

// Pop back to previous scene
game.popScene();

// Check if we should quit
if (game.shouldQuit()) {
    // Cleanup if needed
}
```

## Best Practices

### 1. Always Call Base Class Methods

When overriding scene methods, always call the base class implementation:

```cpp
void update(float deltaTime) override {
    // Your logic
    
    // MUST call base class
    vde::Scene::update(deltaTime);
}
```

### 2. Store IDs for Entities, ResourcePtr for Resources

For entities, store IDs. For resources, use ResourcePtr:

```cpp
// Good: Store entity ID
auto sprite = addEntity<vde::SpriteEntity>();
m_playerId = sprite->getId();

// Later access:
if (auto player = getEntity<vde::SpriteEntity>(m_playerId)) {
    // Use player
}

// Good: Store resources as ResourcePtr
vde::ResourcePtr<vde::Mesh> m_mesh;
vde::ResourcePtr<vde::Texture> m_texture;

m_mesh = getGame()->getResourceManager().load<vde::Mesh>("mesh.obj");
```

### 3. Resource Sharing

Use ResourceManager to load resources once and share them:

```cpp
// Good: One resource, many entities
auto& resources = getGame()->getResourceManager();
auto mesh = resources.load<vde::Mesh>("tree.obj");

for (int i = 0; i < 100; i++) {
    auto tree = addEntity<vde::MeshEntity>();
    tree->setMesh(mesh);  // All share the same mesh
    tree->setPosition(randomPosition());
}

// Avoid: Loading same resource multiple times
// resources.load() in a loop is fine - it returns cached instance
// But don't create new Mesh objects in a loop!
```

### 4. Upload Textures to GPU

Textures must be uploaded to GPU before use in sprites:

```cpp
auto texture = getGame()->getResourceManager().load<vde::Texture>("sprite.png");

// REQUIRED: Upload to GPU
if (auto* context = getGame()->getVulkanContext()) {
    texture->uploadToGPU(context);
}

// Now safe to use in sprite
auto sprite = addEntity<vde::SpriteEntity>();
sprite->setTexture(texture);
```

### 5. Input Handler Lifecycle

Input handlers are typically stack-allocated in main():

```cpp
int main() {
    vde::Game game;
    MyInputHandler input;  // Stack-allocated
    
    game.initialize(settings);
    game.setInputHandler(&input);  // Game does NOT own this
    
    // ... setup scenes ...
    
    game.run();
    return 0;
}
```

### 6. Gamepad Input

Gamepads use polling and events:

```cpp
void update(float deltaTime) override {
    // Poll gamepad state
    if (getGame()->isGamepadConnected(vde::JOYSTICK_1)) {
        float moveX = getGame()->getGamepadAxis(vde::JOYSTICK_1, vde::GAMEPAD_AXIS_LEFT_X);
        float moveY = getGame()->getGamepadAxis(vde::JOYSTICK_1, vde::GAMEPAD_AXIS_LEFT_Y);
        
        if (getGame()->isGamepadButtonPressed(vde::JOYSTICK_1, vde::GAMEPAD_BUTTON_A)) {
            // Jump
        }
    }
    
    vde::Scene::update(deltaTime);
}
```

## Handling API Limitations

If you encounter a limitation in the current API while implementing a feature:

### 1. Document the Suggestion

Create a markdown file describing the improvement:

```markdown
# API Suggestion: [Feature Name]

## Problem
[Describe what you're trying to accomplish and why the current API makes it difficult]

## Current Workaround
[Show how you solved it with the current API]

## Proposed Improvement
[Describe the ideal API addition or change]

### Example Usage
[Show example code of how the improved API would be used]

## Implementation Notes
[Any relevant implementation details or considerations]
```

Save this file as: `API_SUGGESTION_[SHORT_NAME].md` in the project root.

### 2. Complete the Task with Current API

Use the existing API to complete the requested task, even if it requires a workaround:

```cpp
// Example: If there's no built-in collision detection
// Implement a simple solution using available API:

bool checkCollision(const vde::Position& pos1, float radius1,
                   const vde::Position& pos2, float radius2) {
    float dx = pos2.x - pos1.x;
    float dy = pos2.y - pos1.y;
    float dz = pos2.z - pos1.z;
    float distSq = dx*dx + dy*dy + dz*dz;
    float radiusSumSq = (radius1 + radius2) * (radius1 + radius2);
    return distSq < radiusSumSq;
}

// Use in scene update:
void update(float deltaTime) override {
    auto player = getEntity<vde::SpriteEntity>(m_playerId);
    auto enemy = getEntity<vde::SpriteEntity>(m_enemyId);
    
    if (player && enemy) {
        if (checkCollision(player->getPosition(), 0.5f,
                          enemy->getPosition(), 0.5f)) {
            // Handle collision
        }
    }
    
    vde::Scene::update(deltaTime);
}
```

### 3. Common Patterns for Workarounds

#### Custom Entity Behavior

If entities need custom behavior beyond position/rotation/scale:

```cpp
// Create wrapper class
class PlayerEntity {
public:
    PlayerEntity(vde::Scene* scene, vde::ResourcePtr<vde::Texture> texture) {
        m_sprite = scene->addEntity<vde::SpriteEntity>();
        m_sprite->setTexture(texture);
        m_entityId = m_sprite->getId();
    }
    
    void update(float deltaTime) {
        // Custom logic
        m_velocity.y -= 9.8f * deltaTime;  // Gravity
        
        auto pos = m_sprite->getPosition();
        pos.x += m_velocity.x * deltaTime;
        pos.y += m_velocity.y * deltaTime;
        m_sprite->setPosition(pos);
    }
    
    void jump() {
        m_velocity.y = 5.0f;
    }
    
    vde::EntityId getId() const { return m_entityId; }

private:
    std::shared_ptr<vde::SpriteEntity> m_sprite;
    vde::EntityId m_entityId;
    glm::vec2 m_velocity{0, 0};
};
```

#### Custom Scene Systems

If you need systems (physics, AI, etc.):

```cpp
class GameplayScene : public vde::Scene {
public:
    void update(float deltaTime) override {
        // Update custom systems
        m_physicsSystem.update(deltaTime);
        m_aiSystem.update(deltaTime);
        
        vde::Scene::update(deltaTime);
    }

private:
    PhysicsSystem m_physicsSystem;
    AISystem m_aiSystem;
};
```

## Quick Reference

### Include Headers

```cpp
#include <vde/api/GameAPI.h>  // Everything
// OR specific headers:
#include <vde/api/Game.h>
#include <vde/api/Scene.h>
#include <vde/api/Entity.h>
```

### Key Constants

```cpp
// From vde/api/KeyCodes.h

// Keyboard
vde::KEY_SPACE, vde::KEY_ESCAPE, vde::KEY_ENTER
vde::KEY_W, vde::KEY_A, vde::KEY_S, vde::KEY_D
vde::KEY_UP, vde::KEY_DOWN, vde::KEY_LEFT, vde::KEY_RIGHT
vde::KEY_LEFT_SHIFT, vde::KEY_LEFT_CONTROL
vde::KEY_F1 - vde::KEY_F12

// Mouse
vde::MOUSE_BUTTON_LEFT, vde::MOUSE_BUTTON_RIGHT, vde::MOUSE_BUTTON_MIDDLE

// Gamepad
vde::JOYSTICK_1 - vde::JOYSTICK_16
vde::GAMEPAD_BUTTON_A, vde::GAMEPAD_BUTTON_B, vde::GAMEPAD_BUTTON_X, vde::GAMEPAD_BUTTON_Y
vde::GAMEPAD_BUTTON_LEFT_BUMPER, vde::GAMEPAD_BUTTON_RIGHT_BUMPER
vde::GAMEPAD_BUTTON_START, vde::GAMEPAD_BUTTON_BACK
vde::GAMEPAD_BUTTON_DPAD_UP, vde::GAMEPAD_BUTTON_DPAD_DOWN
vde::GAMEPAD_BUTTON_DPAD_LEFT, vde::GAMEPAD_BUTTON_DPAD_RIGHT
vde::GAMEPAD_AXIS_LEFT_X, vde::GAMEPAD_AXIS_LEFT_Y
vde::GAMEPAD_AXIS_RIGHT_X, vde::GAMEPAD_AXIS_RIGHT_Y
vde::GAMEPAD_AXIS_LEFT_TRIGGER, vde::GAMEPAD_AXIS_RIGHT_TRIGGER
```

### Common Types

```cpp
// Basic types
vde::Position(x, y, z)           // 3D position (glm::vec3)
vde::Direction(x, y, z)          // Direction vector (glm::vec3)
vde::Rotation(pitch, yaw, roll)  // Euler angles in degrees
vde::Color(r, g, b, a)           // RGBA color (0-1 range)

// Type-safe units (with user-defined literals)
vde::Meters distance = 100_m;    // Meters
vde::Pixels screenX = 1920_px;   // Pixels

// World coordinates
vde::WorldPoint(10_m, 5_m, 20_m)              // 3D position with units
vde::WorldExtent(200_m, 30_m, 200_m)          // 3D size with units
vde::WorldBounds::fromDirectionalLimits(...)  // 3D boundaries
vde::WorldBounds2D::fromCardinal(...)         // 2D boundaries

// Screen/world mapping
vde::CameraBounds2D camera;                    // Pixel-to-world for 2D
vde::PixelToWorldMapping mapping;              // Conversion ratios
```

## Documentation References

Always check these files for current API capabilities:

- **API-DOC.md** - Complete Game API documentation
- **docs/API.md** - Core engine API reference
- **examples/** - Working example code
- **include/vde/api/** - Header files with inline documentation

## Summary

1. **Start with the pattern** - Use Game → Scene → Entity → Resource hierarchy
2. **Follow ownership rules** - Scene owns entities/resources, use IDs for entities and ResourcePtr for resources
3. **Use ResourceManager** - Load resources via `game.getResourceManager().load<T>()` for global caching
4. **Upload textures to GPU** - Call `texture->uploadToGPU(context)` before using textures in sprites
5. **Check documentation first** - API-DOC.md has comprehensive information
6. **Use existing examples** - Look at examples/ folder for patterns
7. **Suggest improvements but deliver** - Document API gaps but complete the task
8. **Call base class methods** - Always call Scene::update() and Scene::render()
9. **Use type-safe units** - Use `100_m` for meters, `1920_px` for pixels in 2D games
10. **Test thoroughly** - Run the application to verify behavior

**Key Systems:**
- **Input**: Keyboard, mouse, and gamepad support with events and polling
- **Audio**: Play sound effects and music via AudioManager
- **World Bounds**: Use WorldBounds/WorldBounds2D for scene boundaries
- **2D Mapping**: Use CameraBounds2D for pixel-to-world coordinate conversion
- **Materials**: Set surface properties with Material (color, roughness, metallic)
- **Cameras**: OrbitCamera (3D), SimpleCamera (first-person), Camera2D (2D games)

When in doubt, examine the existing examples in the `examples/` directory for proven patterns.
