# API Improvements for 2D Sidescroller Games

> **Implementation Status (Updated)**
>
> | # | Feature | Status | Notes |
> |---|---------|--------|-------|
> | 1 | SpriteSheet / Atlas Management | Not Done | Manual UV calculation still required |
> | 2 | Sprite Animation System | Not Done | Manual frame management in update loop |
> | 3 | TileMap / Tiled Background | Not Done | No tilemap support |
> | 4 | Sprite Flipping / Mirroring | Not Done | Must use negative scale workaround |
> | 5 | 2D Physics & Collision Helpers | **Partial** | `PhysicsScene` + `PhysicsEntity` provide 2D AABB collision & impulse resolution; no Circle2D or spatial partitioning yet |
> | 6 | Camera Extensions for 2D | **Partial** | `Camera2D` has `setBounds()`, `setFollowTarget()` with smoothing; no deadzone or camera shake yet |
> | 7 | Particle System | Not Done | No particle emitter |

This document outlines suggested API additions to make 2D sidescroller/platformer game development easier in VDE.

## Current Gaps Found While Building a Sidescroller

While building the sidescroller example (`examples/sidescroller/main.cpp`), several features were identified that would significantly improve the 2D game development experience.

---

## 1. Spritesheet/Atlas Management

### Current Situation
- `SpriteEntity::setUVRect()` exists but requires manual calculation of UV coordinates
- Loading a spritesheet requires manual subdivision into frames
- No helper to load and manage sprite atlases

### Suggested Addition: `SpriteSheet` Class

```cpp
namespace vde {

/**
 * @brief Manages a sprite sheet/texture atlas with automatic UV calculation.
 */
class SpriteSheet : public Resource {
public:
    using Ref = std::shared_ptr<SpriteSheet>;
    
    /**
     * @brief Create from texture with uniform grid layout.
     * @param texture The texture containing sprites
     * @param columns Number of sprite columns
     * @param rows Number of sprite rows
     * @param spacing Optional spacing between sprites in pixels
     */
    static Ref createGrid(std::shared_ptr<Texture> texture, 
                         int columns, int rows,
                         int spacing = 0);
    
    /**
     * @brief Create from texture with manually defined regions.
     * @param texture The texture containing sprites
     */
    static Ref create(std::shared_ptr<Texture> texture);
    
    /**
     * @brief Add a named sprite region.
     * @param name Name identifier for this sprite
     * @param x X position in pixels
     * @param y Y position in pixels  
     * @param width Width in pixels
     * @param height Height in pixels
     */
    void addSprite(const std::string& name, int x, int y, int width, int height);
    
    /**
     * @brief Get UV rectangle for a sprite by index (for grid layout).
     */
    struct UVRect {
        float u, v, width, height;
    };
    UVRect getUVRect(int index) const;
    
    /**
     * @brief Get UV rectangle for a sprite by name.
     */
    UVRect getUVRect(const std::string& name) const;
    
    /**
     * @brief Get the underlying texture.
     */
    std::shared_ptr<Texture> getTexture() const { return m_texture; }
    
    /**
     * @brief Get total number of sprites (for grid layout).
     */
    int getSpriteCount() const { return m_columns * m_rows; }
    
private:
    std::shared_ptr<Texture> m_texture;
    int m_columns = 1;
    int m_rows = 1;
    int m_spacing = 0;
    std::unordered_map<std::string, UVRect> m_namedSprites;
};

} // namespace vde
```

### Usage Example

```cpp
// Load spritesheet
auto sheet = vde::SpriteSheet::createGrid(playerTexture, 4, 2);  // 4x2 grid

// Apply to sprite entity
auto sprite = addEntity<vde::SpriteEntity>();
sprite->setTexture(sheet->getTexture());
auto uv = sheet->getUVRect(0);  // First frame
sprite->setUVRect(uv.u, uv.v, uv.width, uv.height);

// Named regions for complex atlases
auto atlas = vde::SpriteSheet::create(uiTexture);
atlas->addSprite("button", 0, 0, 64, 32);
atlas->addSprite("healthbar", 64, 0, 128, 16);
auto buttonUV = atlas->getUVRect("button");
```

---

## 2. Sprite Animation System

### Current Situation
- Manual frame management in update loop
- No built-in animation state machine
- Animation timing must be tracked manually

### Suggested Addition: `SpriteAnimation` and `AnimatedSprite`

```cpp
namespace vde {

/**
 * @brief Defines a sprite animation sequence.
 */
class SpriteAnimation {
public:
    struct Frame {
        int spriteIndex;      // Index in spritesheet
        float duration;       // Duration in seconds
    };
    
    SpriteAnimation(const std::string& name) : m_name(name) {}
    
    /**
     * @brief Add a frame to the animation.
     */
    void addFrame(int spriteIndex, float duration = 0.1f);
    
    /**
     * @brief Set if animation should loop.
     */
    void setLooping(bool loop) { m_looping = loop; }
    
    /**
     * @brief Get the frame at a specific time.
     */
    int getFrameAtTime(float time) const;
    
    float getTotalDuration() const;
    const std::string& getName() const { return m_name; }
    
private:
    std::string m_name;
    std::vector<Frame> m_frames;
    bool m_looping = true;
};

/**
 * @brief Animated sprite entity with state machine.
 */
class AnimatedSprite : public SpriteEntity {
public:
    /**
     * @brief Set the sprite sheet to use.
     */
    void setSpriteSheet(std::shared_ptr<SpriteSheet> sheet);
    
    /**
     * @brief Add an animation.
     */
    void addAnimation(const std::string& name, SpriteAnimation animation);
    
    /**
     * @brief Play an animation by name.
     */
    void play(const std::string& name, bool reset = true);
    
    /**
     * @brief Pause current animation.
     */
    void pause();
    
    /**
     * @brief Resume current animation.
     */
    void resume();
    
    /**
     * @brief Stop and reset to first frame.
     */
    void stop();
    
    /**
     * @brief Set animation speed multiplier.
     */
    void setSpeed(float speed) { m_speed = speed; }
    
    /**
     * @brief Check if currently playing.
     */
    bool isPlaying() const { return m_playing; }
    
    /**
     * @brief Get current animation name.
     */
    const std::string& getCurrentAnimation() const { return m_currentAnim; }
    
    void update(float deltaTime) override;
    
private:
    std::shared_ptr<SpriteSheet> m_spriteSheet;
    std::unordered_map<std::string, SpriteAnimation> m_animations;
    std::string m_currentAnim;
    float m_animTime = 0.0f;
    float m_speed = 1.0f;
    bool m_playing = false;
};

} // namespace vde
```

### Usage Example

```cpp
auto player = addEntity<vde::AnimatedSprite>();
player->setSpriteSheet(playerSheet);

// Define walk animation
vde::SpriteAnimation walk("walk");
walk.addFrame(0, 0.1f);
walk.addFrame(1, 0.1f);
walk.addFrame(2, 0.1f);
walk.addFrame(3, 0.1f);
walk.setLooping(true);
player->addAnimation("walk", walk);

// Define idle animation
vde::SpriteAnimation idle("idle");
idle.addFrame(4, 0.3f);
idle.addFrame(5, 0.3f);
idle.setLooping(true);
player->addAnimation("idle", idle);

// Play animation
player->play("walk");

// State-based animation switching
if (moving) {
    if (player->getCurrentAnimation() != "walk")
        player->play("walk");
} else {
    if (player->getCurrentAnimation() != "idle")
        player->play("idle");
}
```

---

## 3. Tilemap/Tiled Background System

### Current Situation
- Creating tiled backgrounds requires manual sprite placement
- No efficient rendering of tile grids
- No support for loading common tilemap formats

### Suggested Addition: `TileMap` Class

```cpp
namespace vde {

/**
 * @brief Efficient tile-based grid rendering.
 */
class TileMap : public Entity {
public:
    using Ref = std::shared_ptr<TileMap>;
    
    /**
     * @brief Create a tilemap.
     * @param tileWidth Tile width in world units
     * @param tileHeight Tile height in world units
     * @param columns Number of columns
     * @param rows Number of rows
     */
    TileMap(float tileWidth, float tileHeight, int columns, int rows);
    
    /**
     * @brief Set the tileset (spritesheet).
     */
    void setTileSet(std::shared_ptr<SpriteSheet> tileSet);
    
    /**
     * @brief Set a tile at grid position.
     * @param col Column index
     * @param row Row index
     * @param tileId Tile sprite index (-1 for empty)
     */
    void setTile(int col, int row, int tileId);
    
    /**
     * @brief Get tile at grid position.
     */
    int getTile(int col, int row) const;
    
    /**
     * @brief Fill a region with a tile.
     */
    void fillRegion(int startCol, int startRow, int endCol, int endRow, int tileId);
    
    /**
     * @brief Load tilemap from array (row-major order).
     */
    void loadFromArray(const std::vector<int>& tiles);
    
    /**
     * @brief Load from CSV file (common tilemap format).
     */
    bool loadFromCSV(const std::string& filepath);
    
    /**
     * @brief Enable/disable culling of off-screen tiles.
     */
    void setCulling(bool enabled) { m_culling = enabled; }
    
    void render() override;
    
private:
    float m_tileWidth, m_tileHeight;
    int m_columns, m_rows;
    std::vector<int> m_tiles;
    std::shared_ptr<SpriteSheet> m_tileSet;
    bool m_culling = true;
};

/**
 * @brief Helper to create repeating background patterns.
 */
class RepeatingBackground : public Entity {
public:
    /**
     * @brief Create repeating background.
     * @param texture Texture to repeat
     * @param tileSize Size of each tile in world units
     * @param tilesX Number of horizontal repetitions
     * @param tilesY Number of vertical repetitions
     */
    RepeatingBackground(std::shared_ptr<Texture> texture, 
                       float tileSize,
                       int tilesX, int tilesY);
    
    /**
     * @brief Set parallax scroll factor (1.0 = normal, 0.5 = half speed).
     */
    void setParallaxFactor(float factorX, float factorY);
    
    void update(float deltaTime) override;
    void render() override;
    
private:
    std::shared_ptr<Texture> m_texture;
    float m_tileSize;
    int m_tilesX, m_tilesY;
    float m_parallaxX = 1.0f, m_parallaxY = 1.0f;
};

} // namespace vde
```

### Usage Example

```cpp
// Create tilemap
auto tilemap = addEntity<vde::TileMap>(1.0f, 1.0f, 30, 20);  // 30x20 tiles
tilemap->setTileSet(groundTileset);

// Set tiles manually
tilemap->setTile(0, 0, 1);  // Grass
tilemap->setTile(1, 0, 2);  // Dirt

// Or load from array
std::vector<int> level = {
    1, 1, 1, 1, 1,
    0, 0, 0, 0, 0,
    2, 2, 2, 2, 2,
    // ...
};
tilemap->loadFromArray(level);

// Repeating background with parallax
auto bg = addEntity<vde::RepeatingBackground>(cloudTexture, 5.0f, 10, 5);
bg->setParallaxFactor(0.5f, 0.3f);  // Scroll slower than camera
```

---

## 4. Sprite Flipping/Mirroring

### Current Situation
- No built-in way to flip sprites horizontally or vertically
- Must manually adjust scale to negative values or use UV manipulation
- Complicates animations and character direction changes

### Suggested Addition: Flip Methods

```cpp
// In SpriteEntity class:
/**
 * @brief Flip sprite horizontally.
 */
void setFlipX(bool flip);

/**
 * @brief Flip sprite vertically.
 */
void setFlipY(bool flip);

/**
 * @brief Check if flipped horizontally.
 */
bool isFlippedX() const { return m_flipX; }

/**
 * @brief Check if flipped vertically.
 */
bool isFlippedY() const { return m_flipY; }
```

### Implementation Notes
Could be implemented either by:
1. Adjusting UV coordinates in the vertex shader
2. Negative scale with proper anchor handling
3. Additional uniform/push constant to sprite shader

### Usage Example
```cpp
// Character facing direction
if (movingLeft) {
    player->setFlipX(true);
} else if (movingRight) {
    player->setFlipX(false);
}
```

---

## 5. 2D Physics & Collision Helpers

### Current Situation
- No built-in 2D collision detection
- Must manually implement AABB, circle, or other collision types
- No physics integration

### Suggested Addition: Basic 2D Collision Utilities

```cpp
namespace vde {

/**
 * @brief Simple AABB (Axis-Aligned Bounding Box) for 2D.
 */
struct AABB2D {
    glm::vec2 min;
    glm::vec2 max;
    
    static AABB2D fromCenter(const glm::vec2& center, const glm::vec2& halfSize);
    static AABB2D fromEntity(const Entity* entity);
    
    bool intersects(const AABB2D& other) const;
    bool contains(const glm::vec2& point) const;
    glm::vec2 getCenter() const;
    glm::vec2 getSize() const;
};

/**
 * @brief Circle collider for 2D.
 */
struct Circle2D {
    glm::vec2 center;
    float radius;
    
    bool intersects(const Circle2D& other) const;
    bool intersects(const AABB2D& box) const;
    bool contains(const glm::vec2& point) const;
};

/**
 * @brief Entity with 2D collision component.
 */
class ColliderEntity : public Entity {
public:
    enum class ColliderType {
        Box,
        Circle
    };
    
    void setCollider(ColliderType type, const glm::vec2& size);
    void setColliderOffset(const glm::vec2& offset);
    
    AABB2D getAABB() const;
    Circle2D getCircle() const;
    
    /**
     * @brief Check collision with another collider entity.
     */
    bool checkCollision(const ColliderEntity* other) const;
    
private:
    ColliderType m_colliderType = ColliderType::Box;
    glm::vec2 m_colliderSize{1.0f, 1.0f};
    glm::vec2 m_colliderOffset{0.0f, 0.0f};
};

} // namespace vde
```

### Usage Example

```cpp
class Player : public vde::ColliderEntity {
public:
    Player() {
        setCollider(ColliderType::Box, glm::vec2(0.8f, 1.6f));
    }
};

class Platform : public vde::ColliderEntity {
public:
    Platform() {
        setCollider(ColliderType::Box, glm::vec2(4.0f, 0.5f));
    }
};

// In update loop
for (auto& platform : m_platforms) {
    if (m_player->checkCollision(platform.get())) {
        // Handle collision
    }
}
```

---

## 6. Camera Extensions for 2D Games

### Current Situation
- `Camera2D` exists but lacks common platformer features
- No camera bounds/constraints
- No smooth following with deadzone
- No camera shake effect

### Suggested Addition: Enhanced 2D Camera Features

```cpp
// In Camera2D class:

/**
 * @brief Set camera bounds (constrains camera movement).
 */
void setBounds(float minX, float minY, float maxX, float maxY);

/**
 * @brief Enable/disable bounds.
 */
void setBoundsEnabled(bool enabled);

/**
 * @brief Follow a target with optional smoothing.
 * @param target Target position
 * @param speed Interpolation speed (0 = instant, higher = smoother)
 */
void followTarget(const glm::vec2& target, float speed = 5.0f);

/**
 * @brief Set deadzone (area where camera won't move).
 * @param width Deadzone width in world units
 * @param height Deadzone height in world units
 */
void setDeadzone(float width, float height);

/**
 * @brief Apply camera shake effect.
 * @param intensity Shake intensity
 * @param duration Duration in seconds
 */
void shake(float intensity, float duration);

/**
 * @brief Smoothly zoom to target.
 */
void zoomTo(float targetZoom, float speed);
```

### Usage Example

```cpp
auto camera = new vde::Camera2D(20.0f, 15.0f);
camera->setBounds(0.0f, 0.0f, 100.0f, 20.0f);  // Constrain to level bounds
camera->setDeadzone(4.0f, 3.0f);  // Don't move camera in small area

// In update:
camera->followTarget(player->getPosition(), 3.0f);

// On player damage:
camera->shake(2.0f, 0.3f);
```

---

## 7. Particle System for 2D Effects

### Current Situation
- No particle system
- Must manually create/destroy sprites for effects

### Suggested Addition: `ParticleEmitter2D`

```cpp
namespace vde {

/**
 * @brief 2D particle emitter for visual effects.
 */
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
        vde::Color colorEnd = vde::Color(1, 1, 1, 0);  // Fade out
        float emissionRate = 10.0f;  // Particles per second
        int maxParticles = 100;
    };
    
    ParticleEmitter2D(const ParticleConfig& config);
    
    /**
     * @brief Start emitting particles.
     */
    void start();
    
    /**
     * @brief Stop emitting (existing particles continue).
     */
    void stop();
    
    /**
     * @brief Emit a burst of particles.
     */
    void burst(int count);
    
    /**
     * @brief Set the particle texture.
     */
    void setTexture(std::shared_ptr<Texture> texture);
    
    void update(float deltaTime) override;
    void render() override;
    
private:
    ParticleConfig m_config;
    // ... internal particle management
};

} // namespace vde
```

### Usage Example

```cpp
// Jump dust effect
vde::ParticleEmitter2D::ParticleConfig dustConfig;
dustConfig.velocityMin = glm::vec2(-2.0f, 0.5f);
dustConfig.velocityMax = glm::vec2(2.0f, 1.5f);
dustConfig.colorStart = vde::Color(0.8f, 0.7f, 0.6f);
dustConfig.lifetimeMax = 0.3f;

auto dustEmitter = addEntity<vde::ParticleEmitter2D>(dustConfig);
dustEmitter->setPosition(player->getPosition());
dustEmitter->burst(5);
```

---

## Summary of Priority Additions

Based on the sidescroller implementation, these are the priority features:

### High Priority (Essential for 2D Games)
1. **SpriteSheet Management** - Critical for any sprite-based game
2. **Sprite Animation System** - Nearly every 2D game needs this
3. **TileMap Support** - Essential for efficient level design
4. **Sprite Flipping** - Very common need for character facing direction

### Medium Priority (Quality of Life)
5. **2D Collision Helpers** - Improves development speed significantly
6. **Enhanced Camera2D** - Professional camera feel is important

### Lower Priority (Nice to Have)
7. **Particle System** - Can be worked around but adds polish

---

## Implementation Notes

### Rendering Optimizations
- TileMap should use instanced rendering for efficiency
- ParticleEmitter2D should batch particles into a single draw call
- SpriteSheet should support texture atlasing to reduce texture switches

### Performance Considerations
- Collision detection should support spatial partitioning (grid/quadtree) for large numbers of entities
- Animated sprites should only update when visible
- TileMap should support view frustum culling

### File Format Support
Consider supporting common formats:
- **Aseprite** (.ase/.aseprite) - Popular pixel art animation tool
- **Tiled** (.tmx) - Industry-standard tilemap editor
- **TexturePacker** (.xml/.json) - Common sprite atlas format
