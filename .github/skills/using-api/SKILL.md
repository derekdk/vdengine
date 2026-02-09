---
name: using-api
description: Guide for using the VDE Game API to create games, demos, applications, and examples. Use this when working with the Scene/Entity/Resource system or implementing gameplay features.
---

# Using the VDE API

This skill provides high-level guidance on using the VDE Game API to create games, demos, and examples. It focuses on workflow patterns, decision-making, and best practices. **For detailed API reference and code examples, always refer to [API-DOC.md](../../../API-DOC.md).**

## When to use this skill

- Creating a new game or application using the VDE API
- Need guidance on API workflow and architecture decisions
- Understanding the Game/Scene/Entity hierarchy
- Best practices for resource management, input handling, and physics
- Choosing the right camera or lighting setup
- Implementing multi-scene or split-screen features
- Understanding ownership and lifecycle patterns

## Core Principles

1. **Work with the API as designed** - Use existing classes and methods
2. **Follow established patterns** - Match the style of [examples/](../../../examples/)
3. **Reference documentation first** - Check [API-DOC.md](../../../API-DOC.md) for detailed API reference
4. **Suggest improvements separately** - If API limitations are found, document the workaround but complete the task

## API Architecture

```
Game (game loop, scene management)
 SceneGroup (multiple active scenes with viewports)
    Scene (game state: menu, level, etc.)
        Entities (MeshEntity, SpriteEntity, PhysicsEntity)
        Resources (Mesh, Texture, Material, AudioClip)
        Camera (OrbitCamera, SimpleCamera, Camera2D)
        LightBox (lighting system)
        PhysicsScene (2D physics)
 ResourceManager (global resource cache)
 AudioManager (global audio playback)
```

**Key Ownership Rules:**
- Game owns Scenes and SceneGroups
- Scene owns Entities (shared_ptr), Resources (shared_ptr), Camera (unique_ptr), LightBox (unique_ptr)
- Store entity IDs, not pointers; use `ResourcePtr<T>` for resources
- InputHandler is not owned (raw pointer, typically stack-allocated)

## Standard Workflow

### Application Structure

Every VDE application follows this pattern:

1. **Create Game instance**
2. **Configure GameSettings** (window size, graphics, audio)
3. **Initialize Game**
4. **Create and register Scenes**
5. **Set active scene or scene group**
6. **Run the game loop**

See [API-DOC.md#getting-started](../../../API-DOC.md#getting-started) for minimal example.

### Scene Lifecycle

Implement scenes by inheriting from `vde::Scene` and overriding lifecycle methods:

| Method | When Called | Typical Use |
|--------|-------------|-------------|
| `onEnter()` | Scene becomes active | Setup camera, lighting, load resources, create entities |
| `update(dt)` | Every frame | Update game logic; **must call `Scene::update(dt)`** |
| `render()` | Every frame after update | Custom rendering (usually just call `Scene::render()`) |
| `onPause()` | Another scene pushed on top | Pause timers, animations |
| `onResume()` | Returned to after pop | Resume timers |
| `onExit()` | Scene deactivated | Cleanup (usually automatic) |

**Critical:** Always call base class `Scene::update(deltaTime)` and `Scene::render()` in overrides.

### InputHandling

Implement `vde::InputHandler` interface for event-driven input (keyboard, mouse, gamepad). Set handler globally via `game.setInputHandler()` or per-scene via `scene->setInputHandler()`.

**Gamepad support:**
- Automatic detection and hot-plug
- Standard button/axis mapping (Xbox/PlayStation layout)
- Event callbacks (`onGamepadButtonPress`, `onGamepadAxis`, etc.)
- Dead zone filtering (default 0.1)

See [API-DOC.md#input-system](../../../API-DOC.md#input-system) for complete interface and key codes.

## Decision Guide: Choosing the Right Components

### Cameras

| Camera Type | Best For | Key Features |
|-------------|----------|--------------|
| `OrbitCamera` | 3D games, RTS, demos | Orbits around target, zoom, pitch/yaw limits |
| `SimpleCamera` | First-person, free camera | Direct position/direction control |
| `Camera2D` | 2D games | Position, zoom, rotation in 2D plane |

See [API-DOC.md#camera-system](../../../API-DOC.md#camera-system) for detailed usage.

### Lighting

| Light Setup | Best For | Description |
|-------------|----------|-------------|
| `SimpleColorLightBox` | Simple scenes, 2D games | Ambient light only, or ambient + one directional light |
| `ThreePointLightBox` | Character showcases, demos | Professional key/fill/rim lighting |
| `LightBox` | Complex 3D scenes | Full control, multiple lights (directional, point, spot) |

See [API-DOC.md#lighting-system](../../../API-DOC.md#lighting-system) for light types and configuration.

### Entities

| Entity Type | Use Case | Physics |
|-------------|----------|---------|
| `MeshEntity` | 3D objects | No physics |
| `SpriteEntity` | 2D sprites | No physics |
| `PhysicsMeshEntity` | 3D objects with 2D physics | Auto-synced with physics body |
| `PhysicsSpriteEntity` | 2D sprites with physics | Auto-synced with physics body |

**Key Operations:**
- Create: `scene->addEntity<T>()`
- Access by ID: `scene->getEntity<T>(entityId)`
- Find by name: `scene->getEntityByName("player")`
- Transform: `setPosition()`, `setRotation()`, `setScale()`
- Rendering: `setMesh()`, `setTexture()`, `setMaterial()`, `setColor()`

See [API-DOC.md#entity](../../../API-DOC.md#entity) for complete API.

### Resources

All resources should be loaded via `ResourceManager` for automatic caching:

```cpp
auto& resources = getGame()->getResourceManager();
auto mesh = resources.load<vde::Mesh>("models/cube.obj");
auto texture = resources.load<vde::Texture>("textures/brick.png");
auto audio = resources.load<vde::AudioClip>("audio/jump.wav");
```

**Important:** Textures must be uploaded to GPU before use in sprites:
```cpp
if (auto* context = getGame()->getVulkanContext()) {
    texture->uploadToGPU(context);
}
```

**Mesh Primitives:** `createCube()`, `createSphere()`, `createPlane()`, `createCylinder()`

See [API-DOC.md#resource](../../../API-DOC.md#resource) and [API-DOC.md#resourcemanager](../../../API-DOC.md#resourcemanager).

## Feature Workflows

### Physics Setup

1. **Enable physics in scene:** `enablePhysics()` or `enablePhysics(customConfig)`
2. **Create physics entities:** Use `PhysicsSpriteEntity` or `PhysicsMeshEntity`
3. **Define body:** Set `PhysicsBodyDef` (type, shape, mass, friction, restitution)
4. **Create body:** `entity->createPhysicsBody(def)`
5. **Apply forces:** `entity->applyForce()` or `applyImpulse()`
6. **Handle collisions:** Set callbacks via `getPhysicsScene()->setOnCollisionBegin()`

**Body Types:** Static (immovable), Dynamic (physics-driven), Kinematic (code-driven)  
**Shapes:** Box (AABB), Circle  

**Important:** Apply forces, not direct position changes (except for initialization or teleportation).

See [API-DOC.md#physics-system](../../../API-DOC.md#physics-system) for comprehensive guide.

### Multi-Scene & Split-Screen

1. **Create individual scenes as usual**
2. **Create SceneGroup:**
   ```cpp
   auto group = vde::SceneGroup::createWithViewports("name", {
       {"scene1", vde::ViewportRect::leftHalf()},
       {"scene2", vde::ViewportRect::rightHalf()}
   });
   ```
3. **Activate group:** `game.setActiveSceneGroup(group)`

**Viewport Presets:** `fullWindow()`, `leftHalf()`, `rightHalf()`, `topLeft()`, `bottomRight()`, etc.

**Use Cases:**
- Split-screen multiplayer (2 or 4 players)
- Picture-in-picture minimap
- HUD overlays on gameplay

See [API-DOC.md#multi-scene--split-screen](../../../API-DOC.md#multi-scene--split-screen) for detailed examples.

### Audio Playback

```cpp
// Load
auto clip = getGame()->getResourceManager().load<vde::AudioClip>("audio/sound.wav");

// Play one-shot sound effect
vde::AudioManager::getInstance().playSFX(clip, volume);

// Play looping music
vde::AudioManager::getInstance().playMusic(clip, volume, loop, pitch);
```

For 3D spatial audio, use `AudioSource`. See [API-DOC.md#audio-system](../../../API-DOC.md#audio-system).

### World Coordinates & Bounds (2D Games)

For 2D games that need pixel-to-world conversion (e.g., platformers, sidescrollers):

**Type-safe units:**
```cpp
vde::Meters distance = 100_m;
vde::Pixels screenWidth = 1920_px;
```

**World boundaries:**
```cpp
auto bounds = vde::WorldBounds2D::fromCardinal(100_m, 100_m, 100_m, 100_m);
```

**Camera with pixel-to-world mapping:**
```cpp
vde::CameraBounds2D camera = vde::CameraBounds2D::fromPixelViewport(
    1920_px, 1080_px,
    100.0_px,  // pixels per meter
    bounds
);
```

This enables proper scaling and conversion between screen pixels and game world meters.

See [API-DOC.md#world-coordinates--bounds](../../../API-DOC.md#world-coordinates--bounds) for complete guide.

## Best Practices

### 1. Always Call Base Class Methods

```cpp
void update(float deltaTime) override {
    // Your logic here
    
    // MUST call base class to update entities, physics, etc.
    vde::Scene::update(deltaTime);
}

void render() override {
    // Custom rendering if needed
    
    // Call base to render entities
    vde::Scene::render();
}
```

### 2. Store EntityIds, Use ResourcePtr for Resources

```cpp
class MyScene : public vde::Scene {
    vde::EntityId m_playerId;                  // Store ID, not pointer
    vde::ResourcePtr<vde::Mesh> m_playerMesh;  // Store ResourcePtr
    
    void onEnter() override {
        auto player = addEntity<vde::MeshEntity>();
        m_playerId = player->getId();  // Store ID
        
        auto& rm = getGame()->getResourceManager();
        m_playerMesh = rm.load<vde::Mesh>("models/player.obj");  // ResourcePtr
    }
    
    void update(float dt) override {
        // Get entity by ID when needed
        if (auto player = getEntity<vde::MeshEntity>(m_playerId)) {
            player->setPosition(/* ... */);
        }
        vde::Scene::update(dt);
    }
};
```

### 3. Use ResourceManager for Shared Assets

```cpp
// Load via ResourceManager for automatic caching
auto& resources = getGame()->getResourceManager();
auto texture = resources.load<vde::Texture>("textures/ground.png");

// Multiple entities can share the same resource
for (int i = 0; i < 100; i++) {
    auto entity = addEntity<vde::SpriteEntity>();
    entity->setTexture(texture);  // All share the same texture
}
```

### 4. Upload Textures to GPU

```cpp
auto texture = resources.load<vde::Texture>("textures/sprite.png");

// MUST upload before using in SpriteEntity
if (auto* context = getGame()->getVulkanContext()) {
    texture->uploadToGPU(context);
}

auto sprite = addEntity<vde::SpriteEntity>();
sprite->setTexture(texture);  // Now safe to use
```

### 5. Input Handler Lifecycle

```cpp
class GameScene : public vde::Scene {
    MyInputHandler m_inputHandler;  // Stack-allocated, scene lifetime
    
    void onEnter() override {
        setInputHandler(&m_inputHandler);  // Set raw pointer
    }
    
    void onExit() override {
        setInputHandler(nullptr);  // Clear handler
    }
};
```

### 6. Physics Best Practices

**DO:**
- Apply forces or impulses: `entity->applyForce()`, `applyImpulse()`
- Use `PhysicsSpriteEntity` or `PhysicsMeshEntity` for auto-sync
- Set callbacks for collision handling
- Use Static bodies for walls/platforms

**DON'T:**
- Directly set position on physics entities during simulation
- Mix physics and non-physics movement on the same entity
- Create too many physics bodies (performance)

### 7. Multi-Scene Best Practices

**DO:**
- Use SceneGroup for split-screen and multi-viewport
- Give each scene its own camera
- Use normalized viewport coordinates (0-1 range)

**DON'T:**
- Share entities between scenes
- Assume scenes update in a specific order

## Handling API Limitations

When you encounter an API limitation:

### 1. Document the Limitation

Create a brief note describing:
- What you needed to do
- Why the current API doesn't support it well
- The workaround you used

### 2. Complete the Task with Current API

Use available workarounds to complete the user's request:
- Direct access to lower-level systems
- Custom entity subclasses
- Manual management where auto-management is missing

### 3. Common Workaround Patterns

**Pattern: Direct PhysicsScene Access**
```cpp
// If entities don't provide needed physics control
auto* physics = getPhysicsScene();
physics->applyForce(bodyId, force);
```

**Pattern: Custom Entity Subclass**
```cpp
// If built-in entities lack needed functionality
class CustomEntity : public vde::MeshEntity {
public:
    void update(float dt) override {
        // Custom behavior
        vde::MeshEntity::update(dt);
    }
};
```

**Pattern: Manual Resource Management**
```cpp
// If resource IDs aren't wired up yet
auto mesh = vde::Mesh::createCube(1.0f);
entity->setMesh(mesh);  // Direct assignment instead of ID binding
```

## Quick Reference

### Include Headers

```cpp
#include <vde/api/GameAPI.h>  // Everything

// OR specific headers:
#include <vde/api/Game.h>
#include <vde/api/Scene.h>
#include <vde/api/Entity.h>
#include <vde/api/PhysicsEntity.h>
#include <vde/api/SceneGroup.h>
```

### Common Types

```cpp
// Spatial types
vde::Position(x, y, z)
vde::Direction(x, y, z)
vde::Rotation(pitch, yaw, roll)
vde::Color(r, g, b, a)

// Type-safe units
vde::Meters distance = 100_m;
vde::Pixels screenWidth = 1920_px;

// Physics types
vde::PhysicsBodyType::Static / Dynamic / Kinematic
vde::PhysicsShape::Box / Circle
vde::PhysicsBodyDef
vde::CollisionEvent

// Multi-scene types
vde::SceneGroup
vde::ViewportRect
```

### Key Input Constants

See [API-DOC.md#key-codes](../../../API-DOC.md#key-codes) for complete list.

**Common keys:** `KEY_SPACE`, `KEY_ESCAPE`, `KEY_W/A/S/D`, `KEY_ARROW`  
**Mouse:** `MOUSE_BUTTON_LEFT/RIGHT/MIDDLE`  
**Gamepad:** `GAMEPAD_BUTTON_A/B/X/Y`, `GAMEPAD_AXIS_LEFT_X/Y`

## Documentation References

Always check these files for current API capabilities:

- **[API-DOC.md](../../../API-DOC.md)** - Complete Game API documentation with examples
- **[docs/API.md](../../../docs/API.md)** - Core engine API reference
- **[examples/](../../../examples/)** - Working example code
- **[include/vde/api/](../../../include/vde/api/)** - Header files with inline documentation

## Summary

1. **Start with the pattern** - Use Game  Scene  Entity  Resource hierarchy
2. **Follow ownership rules** - Scene owns entities/resources, use IDs for entities and ResourcePtr for resources
3. **Use ResourceManager** - Load resources via `game.getResourceManager().load<T>()` for global caching
4. **Upload textures to GPU** - Call `texture->uploadToGPU(context)` before using in sprites
5. **Check documentation first** - [API-DOC.md](../../../API-DOC.md) has comprehensive information
6. **Use existing examples** - Look at [examples/](../../../examples/) folder for patterns
7. **Suggest improvements but deliver** - Document API gaps but complete the task
8. **Call base class methods** - Always call `Scene::update()` and `Scene::render()`
9. **Test thoroughly** - Run the application to verify behavior

**Key Systems:**
- **Input**: Keyboard, mouse, gamepad with events and polling
- **Audio**: Sound effects and music via AudioManager
- **Cameras**: OrbitCamera (3D), SimpleCamera (first-person), Camera2D (2D)
- **Physics**: 2D AABB physics with collision, bodies, forces
- **Multi-Scene**: Split-screen, viewports, multiple active scenes
- **Lighting**: Ambient, directional, point, spot lights

**When in doubt,** examine the existing [examples/](../../../examples/) directory for proven patterns.
