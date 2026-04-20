---
name: 2d-games
description: Guide for creating 2D games, demos, and examples with the VDE API. Use this when building 2D physics, sprite-based, or side-view applications.
---

# Creating 2D Games and Demos

This skill covers the standard patterns, camera setup, lighting, physics, and best practices for building 2D games and demos with VDE. It ensures a flat, head-on orthographic view and correct physics integration.

## When to use this skill

- Creating any 2D game, demo, or example (sprite-based, physics-based, etc.)
- Setting up a 2D scene with orthographic camera
- Building physics arenas with walls/boundaries
- Working with `PhysicsSpriteEntity`, `SpriteEntity`, or `Camera2D`
- Choosing between manual collision detection and `PhysicsScene`
- Setting correct camera, lighting, and background for a 2D view

## Critical Rules

### ALWAYS use Camera2D for 2D scenes

**Never** use `OrbitCamera` or `SimpleCamera` for a 2D scene. They use perspective projection, which causes depth-dependent skew and parallax distortion.

```cpp
// CORRECT — flat orthographic view
auto* cam = new vde::Camera2D(20.0f, 15.0f);  // width x height in world units
cam->setPosition(0.0f, 0.0f);
setCamera(cam);

// WRONG — perspective, angled, 3D
auto* cam = new vde::OrbitCamera(vde::Position(0, 0, 0), 12.0f);  // DO NOT use for 2D
```

### ALWAYS use SimpleColorLightBox with white ambient

2D scenes do not need directional, point, or spot lights. White ambient ensures sprites render at their true color.

```cpp
setLightBox(std::make_unique<vde::SimpleColorLightBox>(vde::Color::white()));
```

### Camera2D viewport size = visible world area

The constructor arguments to `Camera2D(width, height)` define how many **world units** are visible on screen. Choose values that frame your game content:

| Game Type | Typical Viewport | Reasoning |
|-----------|-----------------|-----------|
| Breakout / Pong | `10 × 7.5` | Small arena, large objects |
| Platformer | `20 × 15` | Wider view for scrolling |
| Physics sandbox | `20 × 13` | Room for arena + margins |
| Sprite showcase | `8 × 6` | Close-up on small sprites |

## Quick Setup: `setup2D()`

Use the convenience method `Scene::setup2D()` for one-call initialization:

```cpp
void onEnter() override {
    // Sets Camera2D, white ambient lighting, and background color
    setup2D(20.0f, 15.0f, vde::Color(0.1f, 0.1f, 0.15f, 1.0f));

    // Camera defaults to centered at (0, 0).
    // Reposition if needed:
    if (auto* cam = dynamic_cast<vde::Camera2D*>(getCamera())) {
        cam->setPosition(0.0f, 5.0f);
    }
}
```

Parameters:
- `viewWidth`, `viewHeight` — world units visible on screen
- `bgColor` — background/clear color (optional, defaults to black)

## Physics Setup

### Enabling physics

```cpp
// Default config: gravity {0, -9.81}, 60 Hz, 4 iterations
enablePhysics();

// Or custom:
vde::PhysicsConfig cfg;
cfg.gravity = {0.0f, -9.81f};
cfg.fixedTimestep = 1.0f / 60.0f;
cfg.iterations = 6;  // More iterations = more stable stacking
enablePhysics(cfg);
```

### Creating physics walls with `createPhysicsWalls()`

Most 2D physics scenes need boundary walls. Use the convenience method:

```cpp
// Creates floor, ceiling, left wall, right wall as static PhysicsSpriteEntity boxes
createPhysicsWalls(
    15.0f,   // arena width (total)
    10.0f,   // arena height (total)
    0.5f,    // wall thickness
    vde::Color(0.3f, 0.3f, 0.3f, 1.0f)  // wall color
);
```

The arena is centered at `(0, 0)`. Walls are created as static `PhysicsSpriteEntity` objects that participate in collision detection.

**Options:**
- `createPhysicsWalls(width, height)` — uses default thickness (0.5) and gray color
- `createPhysicsWalls(width, height, thickness, color)` — custom appearance

### PhysicsBodyDef factory methods

Use named constructors to avoid 7-line boilerplate per body:

```cpp
// Dynamic box: position, half-extents, mass, restitution, friction
auto def = vde::PhysicsBodyDef::dynamicBox({0.0f, 5.0f}, {0.5f, 0.5f});

// Dynamic circle: position, radius, mass, restitution, friction
auto def = vde::PhysicsBodyDef::dynamicCircle({0.0f, 5.0f}, 0.4f);

// Static box: position, half-extents
auto def = vde::PhysicsBodyDef::staticBox({0.0f, -2.0f}, {5.0f, 0.25f});

// Kinematic box: position, half-extents
auto def = vde::PhysicsBodyDef::kinematicBox({0.0f, 0.0f}, {1.0f, 0.5f});
```

All factory methods set sensible defaults for mass (1.0), friction (0.3), restitution (0.2), and damping (0.01).

### Creating physics entities (full pattern)

```cpp
// Compact: factory + create in one shot
auto entity = addEntity<vde::PhysicsSpriteEntity>();
entity->setColor(vde::Color::red());
entity->setScale(vde::Scale(1.0f, 1.0f, 1.0f));
entity->createPhysicsBody(vde::PhysicsBodyDef::dynamicBox({0.0f, 5.0f}, {0.5f, 0.5f}));
```

### Scale-to-extents rule

**Physics extents are half-sizes; sprite scale is full size.** The visual scale must be **2× the physics extents** for the sprite to match the collision box:

```cpp
float halfW = 0.5f, halfH = 0.3f;

// Scale = extents × 2 (ALWAYS)
entity->setScale(vde::Scale(halfW * 2.0f, halfH * 2.0f, 1.0f));

vde::PhysicsBodyDef def;
def.extents = {halfW, halfH};
entity->createPhysicsBody(def);
```

Getting this wrong causes the visual sprite to be the wrong size relative to the collision shape.

### When to use PhysicsScene vs. manual collision

| Scenario | Recommendation |
|----------|---------------|
| Falling bodies, stacking, bouncing | **PhysicsScene** — handles gravity, collisions, resolution |
| Simple ball-paddle games (breakout) | **PhysicsScene** — cleaner than hand-rolled AABB |
| Platformer with jump/gravity | **PhysicsScene** — use dynamic player + static platforms |
| UI animations, parallax scrolling | **Manual** — no collisions needed |
| Particle effects | **Manual** — too many objects for physics engine |

Prefer `PhysicsScene` unless you have a specific reason not to. It handles timestep accumulation, collision resolution, and interpolation automatically.

## Complete 2D Scene Template

For the full app structure (input handler, game class, `main()`), see the `writing-examples` skill. The 2D-specific parts go in `onEnter()` and `update()`:

```cpp
class MyScene : public vde::examples::BaseExampleScene {
  public:
    MyScene() : BaseExampleScene(30.0f) {}

    void onEnter() override {
        printExampleHeader();

        // One-call 2D setup: Camera2D + white lighting + background
        setup2D(20.0f, 15.0f, vde::Color(0.1f, 0.1f, 0.15f, 1.0f));

        // Physics
        enablePhysics();
        createPhysicsWalls(18.0f, 13.0f, 0.5f, vde::Color(0.3f, 0.5f, 0.3f, 1.0f));

        // Create a dynamic entity
        auto box = addEntity<vde::PhysicsSpriteEntity>();
        box->setColor(vde::Color::red());
        box->setScale(vde::Scale(1.0f, 1.0f, 1.0f));
        box->createPhysicsBody(
            vde::PhysicsBodyDef::dynamicBox({0.0f, 5.0f}, {0.5f, 0.5f}));

        // Input — use KeyStateTracker for action bindings
        m_keys.bindOneShot("action", vde::KEY_SPACE);
        setInputHandler(this);
    }

    void onKeyPress(int key) override { m_keys.handlePress(key); }
    void onKeyRelease(int key) override { m_keys.handleRelease(key); }

    void update(float dt) override {
        BaseExampleScene::update(dt);
        if (m_keys.consume("action")) { /* ... */ }
    }

  protected:
    std::string getExampleName() const override { return "My 2D Demo"; }
    std::vector<std::string> getFeatures() const override { return {"2D physics"}; }
    std::vector<std::string> getExpectedVisuals() const override { return {"Boxes falling"}; }
    std::vector<std::string> getControls() const override { return {"SPACE - Action"}; }

  private:
    vde::KeyStateTracker m_keys;
};
```

## Common Patterns

### Camera following a player

```cpp
void update(float dt) override {
    BaseExampleScene::update(dt);
    if (auto* cam = dynamic_cast<vde::Camera2D*>(getCamera())) {
        auto state = m_player->getPhysicsState();
        cam->setPosition(state.position.x, state.position.y);
    }
}
```

### Changing gravity at runtime

```cpp
if (hasPhysics()) {
    getPhysicsScene()->setGravity({0.0f, 0.0f});      // Zero gravity
    getPhysicsScene()->setGravity({0.0f, 9.81f});      // Upward gravity
    getPhysicsScene()->setGravity({-9.81f, 0.0f});     // Leftward gravity
}
```

### Collision callbacks

```cpp
getPhysicsScene()->setOnCollisionBegin([this](const vde::CollisionEvent& evt) {
    auto* entityA = getEntityByPhysicsBody(evt.bodyA);
    auto* entityB = getEntityByPhysicsBody(evt.bodyB);
    // Handle collision...
});
```

### Spawning entities dynamically

```cpp
void spawnBox(float x, float y) {
    auto e = addEntity<vde::PhysicsSpriteEntity>();
    e->setColor(vde::Color(0.8f, 0.3f, 0.3f, 1.0f));
    e->setScale(vde::Scale(0.6f, 0.6f, 1.0f));
    e->createPhysicsBody(
        vde::PhysicsBodyDef::dynamicBox({x, y}, {0.3f, 0.3f}));
    m_entities.push_back(e);
}
```

### Kinematic movers (platforms, wrecking balls)

```cpp
auto platform = addEntity<vde::PhysicsSpriteEntity>();
platform->setColor(vde::Color::yellow());
platform->setScale(vde::Scale(3.0f, 0.5f, 1.0f));
platform->createPhysicsBody(
    vde::PhysicsBodyDef::kinematicBox({0.0f, 0.0f}, {1.5f, 0.25f}));

// In update: move via velocity, not position
platform->setLinearVelocity({2.0f, 0.0f});
```

## Checklist for 2D Scenes

- [ ] Camera is `Camera2D`, **never** `OrbitCamera` or `SimpleCamera`
- [ ] Lighting is `SimpleColorLightBox(Color::white())`
- [ ] `Camera2D` viewport size matches your arena/game world
- [ ] Physics extents are **half** the visual scale
- [ ] Background color is set (not left as default black unless intentional)
- [ ] Walls/boundaries exist if objects should be contained
- [ ] `BaseExampleScene::update(dt)` is called first in `update()`
- [ ] `printExampleHeader()` is called in `onEnter()`

## Reference Examples

| Example | Description | Key Patterns |
|---------|-------------|--------------|
| `physics_showcase_demo` | 7 physics tests with arena walls | `Camera2D`, `createPhysicsWalls`, dynamic spawning |
| `sprite_demo` | Sprite colors, anchors, animation | `Camera2D`, no physics |
| `sidescroller` | Side-scrolling platformer | `Camera2D` with scroll, player follow |
| `breakout_demo` | Breakout clone | `Camera2D`, ball/paddle mechanics |
| `physics_demo` | Falling boxes with player control | Physics entities, collision callbacks |
| `physics_audio_demo` | Physics + audio integration | Collision-triggered audio events |
