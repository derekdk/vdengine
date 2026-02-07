# API Suggestion Template

When you encounter a limitation in the VDE API while implementing a feature, use this template to document your suggestion.

**Filename format:** `API_SUGGESTION_[SHORT_DESCRIPTIVE_NAME].md`  
**Location:** Project root directory (c:\Users\derek\source\repos\vdengine\)

---

# API Suggestion: [Feature Name]

## Problem

[Clearly describe what you're trying to accomplish and why the current API makes it difficult or impossible. Include specific use cases.]

**Example:**
> When creating a 2D platformer game, I need to detect collisions between the player sprite and enemy sprites. The current API provides Entity positions but no built-in collision detection, requiring manual distance calculations in every scene.

## Current Workaround

[Show the code you used to solve the problem with the existing API. This demonstrates that the task was completed despite the limitation.]

```cpp
bool checkCollision(const vde::Position& pos1, float radius1,
                   const vde::Position& pos2, float radius2) {
    float dx = pos2.x - pos1.x;
    float dy = pos2.y - pos1.y;
    float dz = pos2.z - pos1.z;
    float distSq = dx*dx + dy*dy + dz*dz;
    float radiusSumSq = (radius1 + radius2) * (radius1 + radius2);
    return distSq < radiusSumSq;
}

// Used in scene:
void update(float deltaTime) override {
    auto player = getEntity<vde::SpriteEntity>(m_playerId);
    auto enemy = getEntity<vde::SpriteEntity>(m_enemyId);
    
    if (player && enemy) {
        if (checkCollision(player->getPosition(), 0.5f,
                          enemy->getPosition(), 0.5f)) {
            // Handle collision
        }
    }
}
```

**Limitations of workaround:**
- Manual implementation in every scene
- Only supports sphere collisions
- No spatial partitioning for performance
- Doesn't handle rotated rectangles

## Proposed Improvement

[Describe the ideal API addition or change. Be specific about class names, method signatures, and behavior.]

### Option 1: Add Collision Methods to Entity

Add collision checking directly to entities:

```cpp
class Entity {
public:
    // Set collision bounds
    void setCollisionRadius(float radius);
    void setCollisionBox(const glm::vec3& halfExtents);
    
    // Check collision with another entity
    bool checkCollision(const Entity& other) const;
    
    // Check collision with point
    bool containsPoint(const vde::Position& point) const;
};
```

### Option 2: Scene-Level Collision System

Add a collision manager to scenes:

```cpp
class Scene {
public:
    // Register entities for collision checking
    void enableCollision(EntityId entityId, CollisionShape shape);
    
    // Query collisions
    std::vector<EntityId> getCollisions(EntityId entityId) const;
    
    // Collision callback
    void setCollisionCallback(CollisionCallback callback);
};
```

### Recommended Approach

[Explain which option you prefer and why]

I recommend **Option 2** because:
- Centralizes collision logic for better performance
- Enables spatial partitioning optimization
- Follows the Scene ownership model
- Optional feature (scenes can ignore if not needed)

## Example Usage

[Show example code demonstrating how the improved API would be used]

```cpp
class GameplayScene : public vde::Scene {
public:
    void onEnter() override {
        // Create entities
        auto player = addEntity<vde::SpriteEntity>(playerTexture);
        m_playerId = player->getId();
        
        auto enemy = addEntity<vde::SpriteEntity>(enemyTexture);
        m_enemyId = enemy->getId();
        
        // Enable collision detection
        enableCollision(m_playerId, vde::CollisionCircle(0.5f));
        enableCollision(m_enemyId, vde::CollisionCircle(0.5f));
        
        // Set callback
        setCollisionCallback([this](EntityId a, EntityId b) {
            if (a == m_playerId && b == m_enemyId) {
                handlePlayerEnemyCollision();
            }
        });
    }
    
    void handlePlayerEnemyCollision() {
        // Game logic for collision
    }
    
private:
    EntityId m_playerId;
    EntityId m_enemyId;
};
```

## Implementation Notes

[Any relevant implementation details, potential challenges, or alternative considerations]

### Performance Considerations
- Use spatial hashing or quad-tree for large numbers of entities
- Only check collisions for entities with collision enabled
- Consider broad-phase/narrow-phase separation

### API Compatibility
- All collision features should be optional
- Existing code continues to work without changes
- Default behavior is no collision checking

### Alternative Approaches
- Could use ECS (Entity Component System) pattern
- Could integrate physics library (Box2D, Bullet)
- Could provide callback-based or polling-based API

### Related Features
- This would work well with a future physics system
- Consider adding trigger volumes (non-blocking collisions)
- May want to differentiate collision layers/groups

---

## Metadata

**Date:** [YYYY-MM-DD]  
**Task Context:** [Brief description of what you were building when you discovered this need]  
**Priority:** [Low/Medium/High - your assessment of how commonly needed this is]
