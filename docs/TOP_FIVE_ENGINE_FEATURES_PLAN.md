# Top Five Engine Features Plan

> **Created:** 2026-04-16
>
> The five highest-impact features required to make VDE a competitive 2D game engine,
> ordered by dependency chain and value delivered. Each feature includes engine/API
> changes, a demonstration example, a smoke test, and a render verification test.

---

## Dependency Graph

```
1. SpriteSheet & Sprite Flipping
        │
        ▼
2. Sprite Animation System
        │
        ▼
3. 2D Particle System
        │
        ▼
4. Collision Layers & Filtering ─────► 5. Game UI System (HUD)
```

Features 1–2 are prerequisites: animation depends on spritesheets, and every feature
after that benefits from the first two being in place.

---

## Feature 1 — SpriteSheet / Atlas Management & Sprite Flipping

### Problem

Loading a spritesheet requires manual UV math. Every example that uses frame animation
repeats the same `col = frame % framesPerRow; row = frame / framesPerRow` arithmetic.
Sprites cannot be horizontally flipped without manually negating scale or recomputing UVs,
which interacts poorly with anchor points and physics extents.

### Acceptance Criteria

- [x] `SpriteSheet` resource class with grid and named-region creation
- [x] `SpriteEntity::setFlipX()` / `setFlipY()` with correct anchor-point behavior
- [ ] Existing sidescroller can be simplified to use the new API (not required to change, but should be possible)
- [x] Demo example exercises all SpriteSheet and flip APIs
- [x] Smoke test passes, render verification passes

### Engine / API Changes

#### New Files

| File | Purpose |
|------|---------|
| `include/vde/api/SpriteSheet.h` | Public header |
| `src/api/SpriteSheet.cpp` | Implementation |
| `tests/SpriteSheet_test.cpp` | Unit tests |

#### `SpriteSheet` API

```cpp
namespace vde {

class SpriteSheet : public Resource {
public:
    using Ref = std::shared_ptr<SpriteSheet>;

    struct UVRect { float u, v, width, height; };

    /// Uniform grid: columns × rows with optional pixel spacing.
    static Ref createGrid(std::shared_ptr<Texture> texture,
                          int columns, int rows, int spacingPx = 0);

    /// Manual regions: call addSprite() to define each frame.
    static Ref create(std::shared_ptr<Texture> texture);

    void addSprite(const std::string& name, int x, int y, int w, int h);

    UVRect getUVRect(int index) const;
    UVRect getUVRect(const std::string& name) const;

    std::shared_ptr<Texture> getTexture() const;
    int getSpriteCount() const;
};

} // namespace vde
```

#### `SpriteEntity` Flip Extension

```cpp
// In SpriteEntity.h — new public methods:
void setFlipX(bool flip);
void setFlipY(bool flip);
bool isFlippedX() const;
bool isFlippedY() const;
```

Implementation: flip by negating UV width/height in the vertex data or via a push-constant
flag in the sprite shader. Must not change the entity's world-space extents or anchor
behavior.

#### Unit Tests (`tests/test_SpriteSheet.cpp`)

- Grid creation with correct UV rects for first, last, and middle frames
- Named sprite lookup
- Out-of-bounds index returns sentinel / throws
- Flip flags toggle correctly and don't affect position

### Example: `spritesheet_demo`

**Directory:** `examples/spritesheet_demo/`

**What it shows:**

| Feature | Visual |
|---------|--------|
| Grid spritesheet | 4×2 character sheet displayed as 8 individual sprites in a row |
| Named regions | HUD icons extracted by name from a mixed atlas |
| Sprite flipping | Character sprite facing left, then flipped to face right |
| Integration with PhysicsSprite | Flipped sprite on a physics body bouncing on a platform |

**Controls:**

| Key | Action |
|-----|--------|
| LEFT/RIGHT | Move character (demonstrates flip on direction change) |
| ESC | Exit |

**Console output:**
```
SpriteSheet Demo
- LEFT/RIGHT arrows: move character (auto-flips sprite)
- ESC: exit
Expected: 8 sprites in a row (top), named icons (middle), flip demo (bottom)
```

### Smoke Test

**File:** `smoketests/scripts/smoke_spritesheet_demo.vdescript`

```vdescript
# Smoke test: spritesheet_demo
wait startup
wait 500
press right
wait 500
press left
wait 500
exit
```

**`examples/spritesheet_demo/vde.toml`:**

```toml
[smoke]
scripts = ["smoke_spritesheet_demo.vdescript"]
priority = 1
sections = ["entity", "resource"]

[render_verify]
scripts = ["verify_spritesheet_demo.vdescript"]
capture_script = "capture_spritesheet_demo.vdescript"
priority = 1
golden = "spritesheet_demo.png"
threshold = 0.05
```

### Render Verification

**Capture script:** `smoketests/scripts/capture_spritesheet_demo.vdescript`

```vdescript
# Golden image capture: spritesheet_demo
wait startup
wait_frames 30
screenshot render_verify_output/spritesheet_demo.png
wait 100
exit
```

**Verify script:** `smoketests/scripts/verify_spritesheet_demo.vdescript`

```vdescript
# Render verification: spritesheet_demo
wait startup
wait_frames 30
screenshot render_verify_output/spritesheet_demo.png
wait 100
compare render_verify_output/spritesheet_demo.png ../../smoketests/golden/spritesheet_demo.png 0.05
exit
```

### Registration

Add to `examples/CMakeLists.txt`:
```cmake
add_vde_example(vde_spritesheet_demo "spritesheet_demo/main.cpp")
```

---

## Feature 2 — Sprite Animation System

### Problem

Every VDE example that animates sprites reimplements the same frame-cycling logic: track
an accumulator, advance the frame index, compute the UV rect. There is no animation state
machine, no way to define named animations with per-frame durations, and no frame-event
callbacks for hitbox activation or sound triggers.

### Acceptance Criteria

- [ ] `SpriteAnimation` data class with named frame sequences and per-frame durations
- [ ] `AnimatedSpriteEntity` engine class that owns a `SpriteSheet` and a set of named animations
- [ ] `play()`, `pause()`, `stop()`, `setSpeed()` controls
- [ ] Frame-event callback: `onFrameEvent(animName, frameIndex, callback)`
- [ ] Looping and one-shot modes
- [ ] Demo example with multiple animation states and frame events
- [ ] Smoke test passes, render verification passes

### Engine / API Changes

#### New Files

| File | Purpose |
|------|---------|
| `include/vde/SpriteAnimation.h` | Animation data + AnimatedSpriteEntity |
| `src/SpriteAnimation.cpp` | Implementation |
| `tests/test_SpriteAnimation.cpp` | Unit tests |

#### `SpriteAnimation` API

```cpp
namespace vde {

class SpriteAnimation {
public:
    struct Frame {
        int spriteIndex;          // Index into SpriteSheet
        float duration = 0.1f;    // Seconds this frame is displayed
    };

    explicit SpriteAnimation(const std::string& name);

    void addFrame(int spriteIndex, float duration = 0.1f);
    void setLooping(bool loop);
    bool isLooping() const;

    const std::string& getName() const;
    int getFrameCount() const;
    float getTotalDuration() const;

    /// Returns the frame index for a given elapsed time.
    int getFrameAtTime(float time) const;
    const Frame& getFrame(int index) const;
};

} // namespace vde
```

#### `AnimatedSpriteEntity` API

```cpp
namespace vde {

class AnimatedSpriteEntity : public SpriteEntity {
public:
    void setSpriteSheet(SpriteSheet::Ref sheet);

    void addAnimation(const std::string& name, SpriteAnimation animation);

    void play(const std::string& name, bool reset = true);
    void pause();
    void resume();
    void stop();
    void setSpeed(float speed);    // 1.0 = normal, 2.0 = double speed

    bool isPlaying() const;
    const std::string& getCurrentAnimation() const;
    int getCurrentFrame() const;

    /// Register a callback invoked when a specific frame is reached.
    /// Use for hitbox activation, SFX triggers, VFX spawns.
    using FrameCallback = std::function<void()>;
    void onFrameEvent(const std::string& animName, int frameIndex,
                      FrameCallback callback);

    void update(float deltaTime) override;
};

} // namespace vde
```

#### Unit Tests (`tests/test_SpriteAnimation.cpp`)

- `SpriteAnimation::getFrameAtTime()` returns correct frame for mid-animation, end, and looping wrap
- One-shot animation clamps to last frame
- `AnimatedSpriteEntity::play()` resets time; `play(name, false)` preserves time
- Speed multiplier affects frame advancement
- Frame callbacks fire exactly once per pass through the target frame

### Example: `animation_demo`

**Directory:** `examples/animation_demo/`

**What it shows:**

| Feature | Visual |
|---------|--------|
| Named animations | Character with idle, walk, attack, hurt states |
| Directional flip | Walking left flips sprite (uses Feature 1) |
| Speed control | +/- keys speed up / slow down animation |
| Frame events | Attack animation plays a sound and spawns a flash sprite on the "hit" frame |
| One-shot vs loop | Attack is one-shot → returns to idle; walk and idle loop |

**Controls:**

| Key | Action |
|-----|--------|
| LEFT/RIGHT | Walk (plays walk animation, flips sprite) |
| SPACE | Attack (one-shot, returns to idle) |
| H | Hurt (one-shot, flashes red via frame event) |
| +/- | Speed up / slow down |
| ESC | Exit |

**Console output:**
```
Animation Demo
- LEFT/RIGHT: walk animation (auto-flip)
- SPACE: attack (one-shot with hit-frame SFX)
- H: hurt (one-shot with red flash)
- +/-: animation speed
- ESC: exit
Expected: character with idle/walk/attack/hurt animations and frame-event feedback
```

### Smoke Test

**File:** `smoketests/scripts/smoke_animation_demo.vdescript`

```vdescript
# Smoke test: animation_demo
wait startup
wait 500
press right
wait 1s
press left
wait 1s
press space
wait 1s
press h
wait 1s
exit
```

**`examples/animation_demo/vde.toml`:**

```toml
[smoke]
scripts = ["smoke_animation_demo.vdescript"]
priority = 1
sections = ["entity", "resource"]

[render_verify]
scripts = ["verify_animation_demo.vdescript"]
capture_script = "capture_animation_demo.vdescript"
priority = 1
golden = "animation_demo.png"
threshold = 0.05
```

### Render Verification

**Capture script:** `smoketests/scripts/capture_animation_demo.vdescript`

```vdescript
# Golden image capture: animation_demo (idle pose, deterministic frame)
wait startup
wait_frames 10
screenshot render_verify_output/animation_demo.png
wait 100
exit
```

**Verify script:** `smoketests/scripts/verify_animation_demo.vdescript`

```vdescript
# Render verification: animation_demo
wait startup
wait_frames 10
screenshot render_verify_output/animation_demo.png
wait 100
compare render_verify_output/animation_demo.png ../../smoketests/golden/animation_demo.png 0.05
exit
```

### Registration

Add to `examples/CMakeLists.txt`:
```cmake
add_vde_example(vde_animation_demo "animation_demo/main.cpp")
```

---

## Feature 3 — 2D Particle System

### Problem

VDE has no particle system. Visual effects like hit sparks, dust clouds, projectile trails,
and explosion debris require manually creating and destroying individual sprite entities,
which is tedious and inefficient. A 2D fighting game needs particles for nearly every
interaction: landing dust, hit impacts, special move effects, KO explosions.

### Acceptance Criteria

- [ ] `ParticleEmitter2D` entity class with configurable emission parameters
- [ ] Continuous emission (`start`/`stop`) and burst mode (`burst(count)`)
- [ ] Per-particle: position, velocity, lifetime, size interpolation, color interpolation, rotation
- [ ] Optional texture (falls back to colored quad)
- [ ] Batched rendering (single draw call per emitter)
- [ ] Demo example with multiple emitter types
- [ ] Smoke test passes, render verification passes

### Engine / API Changes

#### New Files

| File | Purpose |
|------|---------|
| `include/vde/ParticleEmitter2D.h` | Public header |
| `src/ParticleEmitter2D.cpp` | Emitter logic and batched rendering |
| `tests/test_ParticleEmitter2D.cpp` | Unit tests |

#### `ParticleEmitter2D` API

```cpp
namespace vde {

struct ParticleConfig {
    // Emission shape
    glm::vec2 emitOffset{0.0f};      // Offset from entity position
    float emitRadius = 0.0f;          // Random spawn radius (0 = point)
    float emitArc = 360.0f;           // Emission cone angle in degrees
    float emitDirection = 90.0f;      // Center of emission cone (degrees, 90 = up)

    // Velocity (randomized per particle between min/max)
    float speedMin = 1.0f;
    float speedMax = 3.0f;

    // Lifetime
    float lifetimeMin = 0.3f;
    float lifetimeMax = 1.0f;

    // Size interpolation over lifetime
    float sizeStart = 0.2f;
    float sizeEnd = 0.05f;

    // Color interpolation over lifetime
    Color colorStart = Color::white();
    Color colorEnd = Color(1.0f, 1.0f, 1.0f, 0.0f);  // Fade out

    // Rotation
    float rotationSpeedMin = 0.0f;    // Degrees/sec
    float rotationSpeedMax = 0.0f;

    // Physics
    glm::vec2 gravity{0.0f, 0.0f};   // Per-emitter gravity override
    float drag = 0.0f;                // Velocity damping

    // Emission rate
    float emissionRate = 20.0f;       // Particles per second
    int maxParticles = 200;           // Pool size
};

class ParticleEmitter2D : public Entity {
public:
    explicit ParticleEmitter2D(const ParticleConfig& config = {});

    void setConfig(const ParticleConfig& config);
    const ParticleConfig& getConfig() const;

    void setTexture(std::shared_ptr<Texture> texture);

    void start();                     // Begin continuous emission
    void stop();                      // Stop emitting (existing particles finish)
    void burst(int count);            // Emit N particles immediately
    void clear();                     // Kill all active particles

    bool isEmitting() const;
    int getActiveParticleCount() const;

    void update(float deltaTime) override;
    void render() override;
};

} // namespace vde
```

#### Unit Tests (`tests/test_ParticleEmitter2D.cpp`)

- `burst(10)` creates exactly 10 active particles
- Particles die after `lifetimeMax` seconds
- `stop()` ceases emission but existing particles survive
- `clear()` immediately kills all particles
- Particle count never exceeds `maxParticles`
- Config changes take effect on next emission

### Example: `particle_demo`

**Directory:** `examples/particle_demo/`

**What it shows:**

| Feature | Visual |
|---------|--------|
| Continuous emitter | Fountain of particles rising and falling with gravity |
| Burst emitter | Click to spawn explosion burst at mouse position |
| Textured particles | Fire particles with a soft-glow texture |
| Color gradient | Particles shift from yellow → orange → transparent |
| Multiple emitters | Fire, smoke, sparks running simultaneously |

**Controls:**

| Key | Action |
|-----|--------|
| 1–4 | Switch between emitter presets (fire, smoke, sparks, snow) |
| CLICK | Burst 50 particles at mouse position |
| SPACE | Toggle continuous emission on/off |
| ESC | Exit |

**Console output:**
```
Particle Demo
- 1-4: switch emitter preset (fire/smoke/sparks/snow)
- CLICK: burst particles at cursor
- SPACE: toggle continuous emission
- ESC: exit
Expected: colorful particle effects with gravity and fade-out
```

### Smoke Test

**File:** `smoketests/scripts/smoke_particle_demo.vdescript`

```vdescript
# Smoke test: particle_demo
wait startup
wait 500
press space
wait 1s
press 1
wait 500
press 2
wait 500
press 3
wait 500
press 4
wait 500
click 400 300
wait 500
exit
```

**`examples/particle_demo/vde.toml`:**

```toml
[smoke]
scripts = ["smoke_particle_demo.vdescript"]
priority = 1
sections = ["entity"]

[render_verify]
scripts = ["verify_particle_demo.vdescript"]
capture_script = "capture_particle_demo.vdescript"
priority = 2
golden = "particle_demo.png"
threshold = 0.06
```

> **Note:** Render verify is priority 2 with a higher threshold (0.06) because
> particles have inherent randomness. The capture/verify scripts use a fixed seed
> or deterministic burst for the golden image.

### Render Verification

**Capture script:** `smoketests/scripts/capture_particle_demo.vdescript`

```vdescript
# Golden image capture: particle_demo (deterministic burst at known position)
wait startup
wait_frames 5
click 400 300
wait_frames 10
screenshot render_verify_output/particle_demo.png
wait 100
exit
```

**Verify script:** `smoketests/scripts/verify_particle_demo.vdescript`

```vdescript
# Render verification: particle_demo
wait startup
wait_frames 5
click 400 300
wait_frames 10
screenshot render_verify_output/particle_demo.png
wait 100
compare render_verify_output/particle_demo.png ../../smoketests/golden/particle_demo.png 0.06
exit
```

### Registration

Add to `examples/CMakeLists.txt`:
```cmake
add_vde_example(vde_particle_demo "particle_demo/main.cpp")
```

---

## Feature 4 — Collision Layers & Filtering

### Problem

VDE's physics engine has no collision filtering. Every dynamic body collides with every
static and dynamic body. A fighting game needs:

- Player hitboxes that only interact with opponent hurtboxes
- Projectiles that pass through the caster but hit opponents
- Ground platforms that all characters collide with
- Sensor triggers (item pickups, stage boundaries) that detect overlap without physics response

The existing `isSensor` flag provides overlap-without-response for individual bodies, but
there is no way to say "body A should only collide with bodies on layer X."

### Acceptance Criteria

- [ ] `CollisionLayer` bitmask system (at least 16 layers)
- [ ] Per-body layer assignment and collision mask
- [ ] `PhysicsBodyDef` extended with `layer` and `collisionMask` fields
- [ ] Predefined layer constants for common cases
- [ ] Filtering applied in broadphase (before narrow-phase)
- [ ] Demo example showing selective collision
- [ ] Smoke test passes, render verification passes

### Engine / API Changes

#### Modified Files

| File | Change |
|------|--------|
| `include/vde/PhysicsBodyDef.h` | Add `layer` and `collisionMask` fields |
| `include/vde/CollisionLayers.h` | New: layer constants and helper functions |
| `src/PhysicsScene.cpp` | Filter pairs in broadphase using bitmask AND |
| `tests/test_CollisionLayers.cpp` | New: unit tests |

#### `CollisionLayers` API

```cpp
namespace vde {

/// Collision layers are a 16-bit bitmask.
using CollisionLayer = uint16_t;

namespace Layers {
    constexpr CollisionLayer Default    = 1 << 0;   // 0x0001
    constexpr CollisionLayer Player     = 1 << 1;   // 0x0002
    constexpr CollisionLayer Enemy      = 1 << 2;   // 0x0004
    constexpr CollisionLayer Projectile = 1 << 3;   // 0x0008
    constexpr CollisionLayer Trigger    = 1 << 4;   // 0x0010
    constexpr CollisionLayer Platform   = 1 << 5;   // 0x0020
    constexpr CollisionLayer Hitbox     = 1 << 6;   // 0x0040
    constexpr CollisionLayer Hurtbox    = 1 << 7;   // 0x0080
    // Layers 8–15 available for game-specific use
    constexpr CollisionLayer All        = 0xFFFF;
    constexpr CollisionLayer None       = 0x0000;
}

} // namespace vde
```

#### `PhysicsBodyDef` Extension

```cpp
// Added to PhysicsBodyDef:
CollisionLayer layer = Layers::Default;
CollisionLayer collisionMask = Layers::All;

// A collides with B when: (A.layer & B.collisionMask) && (B.layer & A.collisionMask)
```

#### Broadphase Filter

In `PhysicsScene::step()`, before narrow-phase collision testing:

```cpp
bool shouldCollide(const PhysicsBody& a, const PhysicsBody& b) {
    return (a.layer & b.collisionMask) != 0
        && (b.layer & a.collisionMask) != 0;
}
```

#### Unit Tests (`tests/test_CollisionLayers.cpp`)

- Two bodies on the same layer with mutual masks collide
- Two bodies on different layers with no overlap in masks do not collide
- `Layers::All` mask collides with everything
- `Layers::None` mask collides with nothing
- Sensor + layer filtering works together (overlap callback fires, no impulse)
- Default layer/mask behavior unchanged (backward compatible)

### Example: `collision_layers_demo`

**Directory:** `examples/collision_layers_demo/`

**What it shows:**

| Feature | Visual |
|---------|--------|
| Layer filtering | Red and blue boxes fall; red passes through blue platforms, blue passes through red platforms |
| Hitbox/hurtbox | A fighter sprite with a visible hitbox that only triggers on the opponent's hurtbox |
| Projectiles | Projectile passes through caster, hits opponent |
| Sensor trigger | Pickup item detected via sensor + layer, triggers visual feedback |

**Controls:**

| Key | Action |
|-----|--------|
| SPACE | Spawn red and blue falling objects |
| P | Fire projectile from player |
| ESC | Exit |

**Console output:**
```
Collision Layers Demo
- SPACE: spawn colored objects (red ignores blue platforms, blue ignores red)
- P: fire projectile (passes through player, hits enemy)
- ESC: exit
Expected: selective collision — objects pass through opposite-color platforms
```

### Smoke Test

**File:** `smoketests/scripts/smoke_collision_layers_demo.vdescript`

```vdescript
# Smoke test: collision_layers_demo
wait startup
wait 500
press space
wait 1s
press space
wait 500
press p
wait 1s
exit
```

**`examples/collision_layers_demo/vde.toml`:**

```toml
[smoke]
scripts = ["smoke_collision_layers_demo.vdescript"]
priority = 1
sections = ["physics", "entity"]

[render_verify]
scripts = ["verify_collision_layers_demo.vdescript"]
capture_script = "capture_collision_layers_demo.vdescript"
priority = 1
golden = "collision_layers_demo.png"
threshold = 0.05
```

### Render Verification

**Capture script:** `smoketests/scripts/capture_collision_layers_demo.vdescript`

```vdescript
# Golden image capture: collision_layers_demo (static initial state)
wait startup
wait_frames 10
screenshot render_verify_output/collision_layers_demo.png
wait 100
exit
```

**Verify script:** `smoketests/scripts/verify_collision_layers_demo.vdescript`

```vdescript
# Render verification: collision_layers_demo
wait startup
wait_frames 10
screenshot render_verify_output/collision_layers_demo.png
wait 100
compare render_verify_output/collision_layers_demo.png ../../smoketests/golden/collision_layers_demo.png 0.05
exit
```

### Registration

Add to `examples/CMakeLists.txt`:
```cmake
add_vde_example(vde_collision_layers_demo "collision_layers_demo/main.cpp")
```

---

## Feature 5 — Game UI System (HUD)

### Problem

VDE has no game-facing UI system. The only UI integration is Dear ImGui, which is
a debug/tool UI library — not designed for resolution-independent game HUDs, health bars,
or menus. Building a health bar today requires manually positioning `SpriteEntity` and
`TextEntity` objects in screen space, handling DPI scaling, and managing layout by hand.

A fighting game needs health bars, combo counters, round timers, character portraits,
and win indicators — all of which must be resolution-independent and render on top of
the game world.

### Acceptance Criteria

- [ ] `UICanvas` overlay entity that renders in screen space (pixel coordinates, on top of scene)
- [ ] `UIPanel` container with anchor-based layout (top-left, center, bottom-right, etc.)
- [ ] `UIProgressBar` widget (for health, meter, charge)
- [ ] `UILabel` widget (wraps TextEntity for screen-space text)
- [ ] `UIImage` widget (wraps SpriteEntity for screen-space icons/portraits)
- [ ] All widgets support DPI scaling via `game.getDPIScale()`
- [ ] Demo example showing a complete fighting game HUD
- [ ] Smoke test passes, render verification passes

### Engine / API Changes

#### New Files

| File | Purpose |
|------|---------|
| `include/vde/UICanvas.h` | Canvas and all widget types |
| `src/UICanvas.cpp` | Implementation |
| `tests/test_UICanvas.cpp` | Unit tests |

#### `UICanvas` API

```cpp
namespace vde {

/// Anchor point for positioning UI elements relative to screen edges.
enum class UIAnchor {
    TopLeft, TopCenter, TopRight,
    CenterLeft, Center, CenterRight,
    BottomLeft, BottomCenter, BottomRight
};

/// Base class for UI widgets. Positioned in screen-space pixels.
class UIWidget {
public:
    void setPosition(float x, float y);      // Offset from anchor
    void setSize(float width, float height);
    void setAnchor(UIAnchor anchor);
    void setVisible(bool visible);
    bool isVisible() const;

    virtual void update(float deltaTime) {}
    virtual void render(float dpiScale) = 0;
    virtual ~UIWidget() = default;
};

/// Filled progress bar (health, meter, stamina).
class UIProgressBar : public UIWidget {
public:
    void setValue(float value);               // 0.0–1.0
    float getValue() const;
    void setFillColor(const Color& color);
    void setBackgroundColor(const Color& color);
    void setBorderColor(const Color& color);
    void setDirection(bool leftToRight);      // false = right-to-left (P2 health)

    void render(float dpiScale) override;
};

/// Screen-space text label.
class UILabel : public UIWidget {
public:
    void setText(const std::string& text);
    void setFont(std::shared_ptr<Font> font);
    void setColor(const Color& color);
    void setFontSize(float size);

    void render(float dpiScale) override;
};

/// Screen-space image (portrait, icon).
class UIImage : public UIWidget {
public:
    void setTexture(std::shared_ptr<Texture> texture);
    void setUVRect(float u, float v, float w, float h);
    void setColor(const Color& tint);

    void render(float dpiScale) override;
};

/// Overlay canvas that renders UI widgets on top of the scene.
/// One UICanvas per scene; rendered after all scene entities.
class UICanvas : public Entity {
public:
    template<typename T, typename... Args>
    T* addWidget(Args&&... args);

    void removeWidget(UIWidget* widget);

    void setScreenSize(float width, float height);

    void update(float deltaTime) override;
    void render() override;
};

} // namespace vde
```

#### Unit Tests (`tests/test_UICanvas.cpp`)

- Widget anchor positioning computes correct screen-space coordinates
- Progress bar clamps value to [0, 1]
- Label text updates propagate
- Canvas renders widgets in add order (painter's algorithm)
- DPI scale multiplies positions and sizes correctly

### Example: `game_hud_demo`

**Directory:** `examples/game_hud_demo/`

**What it shows:**

| Feature | Visual |
|---------|--------|
| Health bars | Two health bars (P1 left-to-right, P2 right-to-left) at screen top |
| Combo counter | Center-screen text that appears on hit and fades out |
| Round timer | Countdown timer at top-center |
| Character portraits | Small character icons next to each health bar |
| Win indicator | "K.O." text with scale animation on defeat |

The demo has two simple physics-sprite characters that can punch each other.
Health bars decrease on hit, combo counter increments, timer counts down.

**Controls:**

| Key | Action |
|-----|--------|
| A/D | P1 move left/right |
| W | P1 jump |
| F | P1 punch |
| LEFT/RIGHT | P2 move |
| UP | P2 jump |
| RSHIFT | P2 punch |
| R | Reset round |
| ESC | Exit |

**Console output:**
```
Game HUD Demo
- A/D/W/F: P1 move/jump/punch
- LEFT/RIGHT/UP/RSHIFT: P2 move/jump/punch
- R: reset round
- ESC: exit
Expected: two fighters with health bars, combo counter, round timer, and KO text
```

### Smoke Test

**File:** `smoketests/scripts/smoke_game_hud_demo.vdescript`

```vdescript
# Smoke test: game_hud_demo
wait startup
wait 500
press d
wait 300
press f
wait 500
press right
wait 300
press rshift
wait 1s
press r
wait 500
exit
```

**`examples/game_hud_demo/vde.toml`:**

```toml
[smoke]
scripts = ["smoke_game_hud_demo.vdescript"]
priority = 1
sections = ["entity", "text", "physics"]

[render_verify]
scripts = ["verify_game_hud_demo.vdescript"]
capture_script = "capture_game_hud_demo.vdescript"
priority = 1
golden = "game_hud_demo.png"
threshold = 0.05
```

### Render Verification

**Capture script:** `smoketests/scripts/capture_game_hud_demo.vdescript`

```vdescript
# Golden image capture: game_hud_demo (initial state with full health bars)
wait startup
wait_frames 10
screenshot render_verify_output/game_hud_demo.png
wait 100
exit
```

**Verify script:** `smoketests/scripts/verify_game_hud_demo.vdescript`

```vdescript
# Render verification: game_hud_demo
wait startup
wait_frames 10
screenshot render_verify_output/game_hud_demo.png
wait 100
compare render_verify_output/game_hud_demo.png ../../smoketests/golden/game_hud_demo.png 0.05
exit
```

### Registration

Add to `examples/CMakeLists.txt`:
```cmake
add_vde_example(vde_game_hud_demo "game_hud_demo/main.cpp")
```

---

## Implementation Order & Estimates

| Phase | Feature | Depends On |
|-------|---------|------------|
| 1 | SpriteSheet & Sprite Flipping | — |
| 2 | Sprite Animation System | Phase 1 |
| 3 | 2D Particle System | Phase 1 (uses SpriteSheet for textured particles) |
| 4 | Collision Layers & Filtering | — (can parallel with Phase 2–3) |
| 5 | Game UI System (HUD) | Phase 1 (UIImage uses sprites), Phase 4 (demo needs collision) |

Phases 1 and 4 are independent and could be developed in parallel.

---

## Success Criteria (All Features Complete)

After all five features are implemented:

1. `.\scripts\build.ps1` completes with no errors
2. `.\scripts\test.ps1` passes all unit tests including new test files
3. `.\scripts\smoke-test.ps1` passes with all five new examples discovered and green
4. `.\scripts\render-verify.ps1` passes with all new golden images
5. `.\scripts\verify.ps1` passes all four stages
6. Existing examples and tests continue to pass (no regressions)
