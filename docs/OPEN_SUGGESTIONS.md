# Open API Suggestions

> **Consolidated:** 2026-02-08
>
> This document rolls up all remaining (open or partial) suggestions from previous
> suggestion documents. Completed items have been archived to `docs/history/`.
>
> **Source documents (archived):**
> - `API_SUGGESTION_MULTI_SCENE.md` — items 1-2 done, 3-5 open
> - `API_SUGGESTION_SPLIT_SCREEN_VIEWPORTS.md` — items 1-4, 6 done, 5 open
> - `SIDESCROLLER_API_SUGGESTIONS.md` — items 5-6 partial, rest open

---

## Priority Summary

| # | Feature | Priority | Origin | Status |
|---|---------|----------|--------|--------|
| 1 | [SpriteSheet / Atlas Management](#1-spritesheet--atlas-management) | High | Sidescroller | Not Started |
| 2 | [Sprite Animation System](#2-sprite-animation-system) | High | Sidescroller | Not Started |
| 3 | [TileMap / Tiled Background](#3-tilemap--tiled-background) | High | Sidescroller | Not Started |
| 4 | [Sprite Flipping / Mirroring](#4-sprite-flipping--mirroring) | High | Sidescroller | Not Started |
| 5 | [Scene Transition Effects](#5-scene-transition-effects) | Medium | Multi-Scene | Not Started |
| 6 | [Scene Re-entry Optimization](#6-scene-re-entry-optimization) | Medium | Multi-Scene | Not Started |
| 7 | [Per-Scene Input Blocking](#7-per-scene-input-blocking) | Medium | Multi-Scene | Partial |
| 8 | [2D Circle Collider & Spatial Partitioning](#8-2d-circle-collider--spatial-partitioning) | Medium | Sidescroller | Partial |
| 9 | [Camera2D Deadzone & Shake](#9-camera2d-deadzone--shake) | Medium | Sidescroller | Partial |
| 10 | [Viewport Decorations](#10-viewport-decorations) | Low | Split-Screen | Not Started |
| 11 | [2D Particle System](#11-2d-particle-system) | Low | Sidescroller | Not Started |
| 12 | [CMake find_package Support](#12-cmake-find_package-support) | Low | API Review | Not Started |

---

## High Priority — Essential for 2D Games

### 1. SpriteSheet / Atlas Management

Loading a spritesheet requires manual UV subdivision. No helper exists to load and manage sprite atlases.

**Proposed API:**

```cpp
class SpriteSheet : public Resource {
public:
    using Ref = std::shared_ptr<SpriteSheet>;

    struct UVRect { float u, v, width, height; };

    /// Create from texture with uniform grid layout.
    static Ref createGrid(std::shared_ptr<Texture> texture,
                          int columns, int rows, int spacing = 0);

    /// Create from texture with manually defined regions.
    static Ref create(std::shared_ptr<Texture> texture);

    void addSprite(const std::string& name, int x, int y, int width, int height);
    UVRect getUVRect(int index) const;
    UVRect getUVRect(const std::string& name) const;
    std::shared_ptr<Texture> getTexture() const;
    int getSpriteCount() const;
};
```

**Usage:**
```cpp
auto sheet = vde::SpriteSheet::createGrid(playerTexture, 4, 2);
sprite->setTexture(sheet->getTexture());
auto uv = sheet->getUVRect(0);
sprite->setUVRect(uv.u, uv.v, uv.width, uv.height);
```

**Notes:** Should support texture atlasing and common formats (Aseprite, TexturePacker JSON/XML).

---

### 2. Sprite Animation System

Manual frame management in the update loop with manual timing. No animation state machine.

**Proposed API:**

```cpp
class SpriteAnimation {
public:
    struct Frame { int spriteIndex; float duration; };

    SpriteAnimation(const std::string& name);
    void addFrame(int spriteIndex, float duration = 0.1f);
    void setLooping(bool loop);
    int getFrameAtTime(float time) const;
    float getTotalDuration() const;
};

class AnimatedSprite : public SpriteEntity {
public:
    void setSpriteSheet(std::shared_ptr<SpriteSheet> sheet);
    void addAnimation(const std::string& name, SpriteAnimation animation);
    void play(const std::string& name, bool reset = true);
    void pause();
    void resume();
    void stop();
    void setSpeed(float speed);
    bool isPlaying() const;
    const std::string& getCurrentAnimation() const;
    void update(float deltaTime) override;
};
```

**Usage:**
```cpp
vde::SpriteAnimation walk("walk");
walk.addFrame(0, 0.1f);
walk.addFrame(1, 0.1f);
walk.addFrame(2, 0.1f);
walk.addFrame(3, 0.1f);
player->addAnimation("walk", walk);
player->play("walk");
```

**Notes:** Animated sprites should only update when visible.

---

### 3. TileMap / Tiled Background

No efficient tile-grid rendering. Creating tiled backgrounds requires manual sprite placement.

**Proposed API:**

```cpp
class TileMap : public Entity {
public:
    TileMap(float tileWidth, float tileHeight, int columns, int rows);
    void setTileSet(std::shared_ptr<SpriteSheet> tileSet);
    void setTile(int col, int row, int tileId);
    int getTile(int col, int row) const;
    void fillRegion(int startCol, int startRow, int endCol, int endRow, int tileId);
    void loadFromArray(const std::vector<int>& tiles);
    bool loadFromCSV(const std::string& filepath);
    void setCulling(bool enabled);
    void render() override;
};

class RepeatingBackground : public Entity {
public:
    RepeatingBackground(std::shared_ptr<Texture> texture,
                        float tileSize, int tilesX, int tilesY);
    void setParallaxFactor(float factorX, float factorY);
    void update(float deltaTime) override;
    void render() override;
};
```

**Notes:**
- Should use instanced rendering for efficiency.
- Support view frustum culling.
- Consider Tiled (.tmx) format support.

---

### 4. Sprite Flipping / Mirroring

No built-in way to flip sprites. Must manually adjust scale to negative values or manipulate UVs.

**Proposed API:**

```cpp
// In SpriteEntity:
void setFlipX(bool flip);
void setFlipY(bool flip);
bool isFlippedX() const;
bool isFlippedY() const;
```

**Implementation options:**
1. Adjust UV coordinates in the vertex shader
2. Negative scale with proper anchor handling
3. Additional uniform/push constant to sprite shader

---

## Medium Priority — Quality of Life

### 5. Scene Transition Effects

Scene transitions are instantaneous. No fade, crossfade, or slide effects.

**Proposed API:**

```cpp
enum class TransitionType {
    NONE,        // Instant switch (current behavior)
    FADE_BLACK,  // Fade to black, then fade in
    CROSSFADE,   // Blend old and new scenes
    SLIDE_LEFT,  // Slide new scene in from right
    SLIDE_RIGHT, // Slide new scene in from left
};

// Game.h — modified setActiveScene
void setActiveScene(const std::string& name,
                    TransitionType transition = TransitionType::NONE,
                    float duration = 0.5f);
```

**Notes:**
- Fade-to-black is simplest (darkening overlay on old scene, lightening on new).
- Crossfade requires offscreen render targets for both scenes.
- Leverages existing multi-viewport rendering infrastructure.

---

### 6. Scene Re-entry Optimization

`onExit()`/`onEnter()` are called every time a scene is switched to, causing redundant teardown/setup for scenes with heavy `onEnter()` logic.

**Proposed API:**

```cpp
class Scene {
public:
    virtual void onFirstEnter() {}  // Called only the first time
    virtual void onReEnter() {}     // Called on subsequent activations
    bool hasBeenEntered() const;
};
```

**Current workaround:** Scenes use an `m_initialized` flag to skip setup in subsequent `onEnter()` calls.

---

### 7. Per-Scene Input Blocking

When a HUD overlay is pushed, the underlying scene's input handler also receives events. No way to block input propagation to paused scenes.

**Current state:** Focus routing via `setFocusedScene()` exists. Missing: `setBlocksInput()` propagation control.

**Proposed API:**

```cpp
// Scene.h
void setBlocksInput(bool blocks);
bool getBlocksInput() const;

// Game.cpp — only deliver input to top scene if it blocks,
// otherwise deliver to all active stack scenes
```

---

### 8. 2D Circle Collider & Spatial Partitioning

**What exists:** `PhysicsScene` + `PhysicsEntity` provide 2D AABB collision detection and impulse-based resolution.

**What's missing:**
- Circle collider (`Circle2D`) and circle-vs-AABB tests
- `ColliderEntity` base class combining collider shape with entity
- Spatial partitioning (grid/quadtree) for large entity counts

**Proposed additions:**

```cpp
struct Circle2D {
    glm::vec2 center;
    float radius;
    bool intersects(const Circle2D& other) const;
    bool intersects(const AABB2D& box) const;
    bool contains(const glm::vec2& point) const;
};
```

---

### 9. Camera2D Deadzone & Shake

**What exists:** `Camera2D` has `setBounds()` and `setFollowTarget()` with smoothing.

**What's missing:**

```cpp
// In Camera2D:
void setDeadzone(float width, float height);   // Area where camera won't track
void shake(float intensity, float duration);    // Screenshake effect
void zoomTo(float targetZoom, float speed);     // Smooth zoom transition
```

---

## Low Priority — Nice to Have

### 10. Viewport Decorations

No mechanism to draw viewport borders or focus indicators between scene renders. Current workaround uses border sprites as entities, which don't work correctly with 3D cameras.

**Proposed API:**

```cpp
// Option A: Built-in viewport decoration
struct ViewportStyle {
    Color borderColor = Color::transparent();
    float borderWidth = 0.0f;  // pixels
};
scene->setViewportStyle(style);

// Option B: Post-render overlay callback
virtual void onPostSceneRender(VkCommandBuffer cmd, const std::string& sceneName);
```

---

### 11. 2D Particle System

No particle emitter. Visual effects require manual sprite create/destroy.

**Proposed API:**

```cpp
class ParticleEmitter2D : public Entity {
public:
    struct ParticleConfig {
        glm::vec2 velocityMin{-1.0f, 0.0f};
        glm::vec2 velocityMax{1.0f, 2.0f};
        float lifetimeMin = 0.5f;
        float lifetimeMax = 1.5f;
        float sizeStart = 0.2f;
        float sizeEnd = 0.05f;
        vde::Color colorStart = vde::Color::white();
        vde::Color colorEnd = vde::Color(1, 1, 1, 0);
        float emissionRate = 10.0f;
        int maxParticles = 100;
    };

    ParticleEmitter2D(const ParticleConfig& config);
    void start();
    void stop();
    void burst(int count);
    void setTexture(std::shared_ptr<Texture> texture);
    void update(float deltaTime) override;
    void render() override;
};
```

**Notes:** Should batch particles into a single draw call.

---

### 12. CMake find_package Support

`find_package(vde)` is advertised in the README but a `vdeConfig.cmake` is not generated or installed. Consumers must use `add_subdirectory()`.

**Required work:**
- Generate and install `vdeConfig.cmake` with exported targets
- Export transitive dependencies (GLFW, GLM, glslang) or wrap in exported targets
- Version the artifacts
- Document dependency linkage for consumers

---

## Implementation Notes

### Rendering Optimizations
- TileMap should use instanced rendering
- ParticleEmitter2D should batch particles into a single draw call
- SpriteSheet should support texture atlasing to reduce texture switches

### Performance Considerations
- Collision detection should support spatial partitioning (grid/quadtree)
- Animated sprites should only update when visible
- TileMap should support view frustum culling

### File Format Support
Consider supporting common formats:
- **Aseprite** (.ase/.aseprite) — pixel art animation tool
- **Tiled** (.tmx) — industry-standard tilemap editor
- **TexturePacker** (.xml/.json) — sprite atlas format
