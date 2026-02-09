#pragma once

/**
 * @file PhysicsTypes.h
 * @brief Physics type definitions for VDE
 *
 * Provides core physics types used by PhysicsScene and PhysicsEntity:
 * body definitions, state, shapes, collision events, and configuration.
 */

#include <glm/glm.hpp>

#include <cstdint>
#include <functional>
#include <limits>

namespace vde {

// ============================================================================
// Identifiers
// ============================================================================

/**
 * @brief Unique identifier for a physics body.
 */
using PhysicsBodyId = uint32_t;

/**
 * @brief Sentinel value indicating an invalid physics body ID.
 */
constexpr PhysicsBodyId INVALID_PHYSICS_BODY_ID = 0;

// ============================================================================
// Enumerations
// ============================================================================

/**
 * @brief Shape type for physics collision detection.
 */
enum class PhysicsShape : uint8_t {
    Box,     ///< Axis-aligned box (2D: rectangle, 3D: cuboid)
    Circle,  ///< 2D circle
    Sphere,  ///< 3D sphere
    Capsule  ///< Capsule shape (reserved for future use)
};

/**
 * @brief Physics body simulation type.
 */
enum class PhysicsBodyType : uint8_t {
    Static,    ///< Does not move; infinite mass; participates in collision
    Dynamic,   ///< Moves under forces and gravity; full simulation
    Kinematic  ///< Moves via user code; not affected by forces/gravity; collides with dynamic
};

// ============================================================================
// Configuration
// ============================================================================

/**
 * @brief Configuration for a PhysicsScene.
 */
struct PhysicsConfig {
    float fixedTimestep = 1.0f / 60.0f;  ///< Fixed physics step (seconds)
    glm::vec2 gravity = {0.0f, -9.81f};  ///< Gravity vector (2D, Y-down is negative)
    int maxSubSteps = 8;                 ///< Max sub-steps per frame (spiral-of-death cap)
    int iterations = 4;                  ///< Solver iterations per step
};

// ============================================================================
// Body Definition
// ============================================================================

/**
 * @brief Describes how to create a physics body.
 *
 * @example
 * @code
 * PhysicsBodyDef def;
 * def.type = PhysicsBodyType::Dynamic;
 * def.shape = PhysicsShape::Box;
 * def.position = {0.0f, 10.0f};
 * def.extents = {0.5f, 0.5f};
 * def.mass = 1.0f;
 * auto id = physicsScene->createBody(def);
 * @endcode
 */
struct PhysicsBodyDef {
    PhysicsBodyType type = PhysicsBodyType::Dynamic;  ///< Body type
    PhysicsShape shape = PhysicsShape::Box;           ///< Collision shape
    glm::vec2 position = {0.0f, 0.0f};                ///< Initial position
    float rotation = 0.0f;                            ///< Initial rotation (radians)
    glm::vec2 extents = {0.5f, 0.5f};  ///< Half-extents (box) or {radius, 0} (circle)
    float mass = 1.0f;                 ///< Mass (kg); ignored for Static/Kinematic
    float friction = 0.3f;             ///< Surface friction coefficient
    float restitution = 0.2f;          ///< Bounciness (0 = no bounce, 1 = perfect)
    float linearDamping = 0.01f;       ///< Linear velocity damping
    bool isSensor = false;             ///< If true, triggers callbacks but no response
};

// ============================================================================
// Body State
// ============================================================================

/**
 * @brief Runtime state of a physics body.
 */
struct PhysicsBodyState {
    glm::vec2 position = {0.0f, 0.0f};  ///< Current position
    float rotation = 0.0f;              ///< Current rotation (radians)
    glm::vec2 velocity = {0.0f, 0.0f};  ///< Current linear velocity
    bool isAwake = true;                ///< Whether the body is awake (simulated)
};

// ============================================================================
// Collision Events
// ============================================================================

/**
 * @brief Describes a collision between two physics bodies.
 */
struct CollisionEvent {
    PhysicsBodyId bodyA = INVALID_PHYSICS_BODY_ID;  ///< First body in collision
    PhysicsBodyId bodyB = INVALID_PHYSICS_BODY_ID;  ///< Second body in collision
    glm::vec2 contactPoint = {0.0f, 0.0f};          ///< Approximate contact point
    glm::vec2 normal = {0.0f, 0.0f};                ///< Collision normal (from A to B)
    float depth = 0.0f;                             ///< Penetration depth
};

/**
 * @brief Callback type for collision events.
 */
using CollisionCallback = std::function<void(const CollisionEvent&)>;

// ============================================================================
// Raycast
// ============================================================================

/**
 * @brief Result of a raycast query.
 */
struct RaycastHit {
    PhysicsBodyId bodyId = INVALID_PHYSICS_BODY_ID;  ///< Body that was hit
    glm::vec2 point = {0.0f, 0.0f};                  ///< World-space hit point
    glm::vec2 normal = {0.0f, 0.0f};                 ///< Surface normal at hit point
    float distance = 0.0f;                           ///< Distance from ray origin to hit
};

}  // namespace vde
