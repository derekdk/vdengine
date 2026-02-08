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
│   ├── Camera (view control: OrbitCamera, FixedCamera, etc.)
│   └── LightBox (lighting: SimpleColorLightBox, etc.)
└── InputHandler (keyboard, mouse input)
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
        // Load shared resources
        m_meshId = addResource<vde::Mesh>("models/mymodel.obj");
        m_textureId = addResource<vde::Texture>("textures/mytexture.png");
    }
    
    void createEntities() {
        // Create entities
        auto entity = addEntity<vde::MeshEntity>(m_meshId);
        entity->setTexture(m_textureId);
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
    vde::ResourceId m_meshId;
    vde::ResourceId m_textureId;
    vde::EntityId m_entityId;
};
```

### 3. Input Handling

Input handlers process user input:

```cpp
class MyInputHandler : public vde::InputHandler {
public:
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
    
    void onMouseMove(double x, double y) override {
        m_mouseX = x;
        m_mouseY = y;
    }
    
    void onMouseButton(int button, int action) override {
        // Handle mouse clicks
    }
    
    // Query methods for game logic
    bool isSpacePressed() {
        bool val = m_spacePressed;
        m_spacePressed = false;
        return val;
    }

private:
    bool m_spacePressed = false;
    double m_mouseX = 0.0;
    double m_mouseY = 0.0;
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
auto meshId = addResource<vde::Mesh>("models/cube.obj");
auto textureId = addResource<vde::Texture>("textures/cube.png");

auto entity = addEntity<vde::MeshEntity>(meshId);
entity->setTexture(textureId);
entity->setPosition(0, 0, 0);
entity->setRotation(0, 45, 0);  // Euler angles in degrees
entity->setScale(1, 1, 1);

// Store ID for later access
m_cubeId = entity->getId();
```

#### Sprite Entities (2D Objects)

```cpp
// Create sprite texture
auto spriteTexture = addResource<vde::Texture>("sprites/player.png");

// Create sprite entity
auto sprite = addEntity<vde::SpriteEntity>(spriteTexture);
sprite->setPosition(0, 0, 0);
sprite->setScale(1, 1);  // Width and height in world units
sprite->setColor(vde::Color::white());

// For sprite sheets, set UV rectangle:
sprite->setUVRect(0.0f, 0.0f, 0.25f, 0.25f);  // Top-left quarter

// Set anchor point (0,0 = top-left, 0.5,0.5 = center, 1,1 = bottom-right)
sprite->setAnchor(0.5f, 0.5f);  // Center the sprite
```

### Working with Cameras

#### Orbit Camera (Good for 3D scenes)

```cpp
auto camera = new vde::OrbitCamera(
    vde::Position(0, 0, 0),  // target
    15.0f,                    // distance
    45.0f,                    // pitch (degrees)
    0.0f                      // yaw (degrees)
);
setCamera(camera);
```

#### Fixed Camera (Good for controlled views)

```cpp
auto camera = new vde::FixedCamera(
    vde::Position(0, 5, 10),  // eye position
    vde::Position(0, 0, 0),   // look at
    vde::Direction(0, 1, 0)   // up vector
);
setCamera(camera);
```

#### Side Scroller Camera (2D side-view games)

```cpp
auto camera = new vde::SideScrollerCamera(
    vde::Meters(20.0f),  // view width in meters
    vde::Position(0, 0, 10)  // initial position
);
setCamera(camera);

// Update camera to follow player:
if (auto player = getEntity<vde::SpriteEntity>(m_playerId)) {
    auto pos = player->getPosition();
    camera->setPosition(vde::Position(pos.x, pos.y, 10));
}
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

### Resources

Resources are shared assets loaded once and used by multiple entities:

```cpp
// Load a resource
auto meshId = addResource<vde::Mesh>("models/tree.obj");

// Use it for multiple entities
for (int i = 0; i < 10; i++) {
    auto tree = addEntity<vde::MeshEntity>(meshId);
    tree->setPosition(i * 5.0f, 0, 0);
}
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

### 2. Store IDs, Not Pointers

Entities and resources return shared_ptr, but store IDs instead:

```cpp
// Good: Store ID
m_playerId = addEntity<vde::SpriteEntity>(...)->getId();

// Later access:
if (auto player = getEntity<vde::SpriteEntity>(m_playerId)) {
    // Use player
}

// Avoid: Don't store shared_ptr member variables
// std::shared_ptr<vde::SpriteEntity> m_player;  // NO
```

### 3. Resource Sharing

Load resources once and share them:

```cpp
// Good: One resource, many entities
auto meshId = addResource<vde::Mesh>("tree.obj");
for (int i = 0; i < 100; i++) {
    auto tree = addEntity<vde::MeshEntity>(meshId);
    tree->setPosition(randomPosition());
}

// Avoid: Loading same resource multiple times
// Don't do this in a loop!
```

### 4. Input Handler Lifecycle

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

### 5. Use Type-Safe Units

For 2D games with pixel/world conversions, use the type-safe units:

```cpp
#include <vde/api/WorldUnits.h>

vde::Meters worldWidth(20.0f);
vde::Pixels screenWidth(1280);

// Compile-time safety prevents mixing units
// worldWidth = screenWidth;  // Compilation error!
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
    PlayerEntity(vde::Scene* scene, vde::ResourceId textureId) {
        m_sprite = scene->addEntity<vde::SpriteEntity>(textureId);
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
vde::KEY_SPACE, vde::KEY_ESCAPE
vde::KEY_W, vde::KEY_A, vde::KEY_S, vde::KEY_D
vde::KEY_UP, vde::KEY_DOWN, vde::KEY_LEFT, vde::KEY_RIGHT
vde::MOUSE_BUTTON_LEFT, vde::MOUSE_BUTTON_RIGHT
```

### Common Types

```cpp
vde::Position(x, y, z)       // 3D position (glm::vec3)
vde::Direction(x, y, z)      // Direction vector (glm::vec3)
vde::Rotation(pitch, yaw, roll)  // Euler angles in degrees
vde::Color(r, g, b, a)       // RGBA color (0-1 range)
vde::Meters(value)           // Meters (type-safe)
vde::Pixels(value)           // Pixels (type-safe)
```

## Documentation References

Always check these files for current API capabilities:

- **API-DOC.md** - Complete Game API documentation
- **docs/API.md** - Core engine API reference
- **examples/** - Working example code
- **include/vde/api/** - Header files with inline documentation

## Summary

1. **Start with the pattern** - Use Game → Scene → Entity → Resource hierarchy
2. **Follow ownership rules** - Scene owns entities/resources, use IDs not pointers
3. **Check documentation first** - API-DOC.md has comprehensive information
4. **Use existing examples** - Look at examples/ folder for patterns
5. **Suggest improvements but deliver** - Document API gaps but complete the task
6. **Call base class methods** - Always call Scene::update() and Scene::render()
7. **Test thoroughly** - Run the application to verify behavior

When in doubt, examine the existing examples in the `examples/` directory for proven patterns.
