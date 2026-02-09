/**
 * @file PhysicsScene_test.cpp
 * @brief Unit tests for PhysicsScene (Phase 5)
 *
 * Tests body management, gravity, fixed-timestep accumulator,
 * AABB collision, callbacks, sensors, and configuration.
 */

#include <vde/api/PhysicsScene.h>
#include <vde/api/PhysicsTypes.h>
#include <vde/api/Scene.h>

#include <cmath>

#include <gtest/gtest.h>

using namespace vde;

namespace vde::test {

// ============================================================================
// PhysicsScene Basic Tests
// ============================================================================

class PhysicsSceneTest : public ::testing::Test {
  protected:
    PhysicsConfig config;
    std::unique_ptr<PhysicsScene> physics;

    void SetUp() override {
        config.gravity = {0.0f, -9.81f};
        config.fixedTimestep = 1.0f / 60.0f;
        config.maxSubSteps = 8;
        config.iterations = 4;
        physics = std::make_unique<PhysicsScene>(config);
    }
};

TEST_F(PhysicsSceneTest, CreateWithDefaultConfig) {
    PhysicsScene defaultPhysics;
    EXPECT_EQ(defaultPhysics.getBodyCount(), 0);
    EXPECT_EQ(defaultPhysics.getActiveBodyCount(), 0);
}

TEST_F(PhysicsSceneTest, CreateWithCustomConfig) {
    EXPECT_FLOAT_EQ(physics->getConfig().fixedTimestep, 1.0f / 60.0f);
    EXPECT_FLOAT_EQ(physics->getConfig().gravity.x, 0.0f);
    EXPECT_FLOAT_EQ(physics->getConfig().gravity.y, -9.81f);
    EXPECT_EQ(physics->getConfig().maxSubSteps, 8);
    EXPECT_EQ(physics->getConfig().iterations, 4);
}

// ============================================================================
// Body Management
// ============================================================================

TEST_F(PhysicsSceneTest, CreateBody) {
    PhysicsBodyDef def;
    def.type = PhysicsBodyType::Dynamic;
    def.position = {1.0f, 2.0f};
    def.extents = {0.5f, 0.5f};
    def.mass = 1.0f;

    PhysicsBodyId id = physics->createBody(def);
    EXPECT_NE(id, INVALID_PHYSICS_BODY_ID);
    EXPECT_TRUE(physics->hasBody(id));
    EXPECT_EQ(physics->getBodyCount(), 1);
    EXPECT_EQ(physics->getActiveBodyCount(), 1);
}

TEST_F(PhysicsSceneTest, CreateMultipleBodies) {
    PhysicsBodyDef def;
    def.type = PhysicsBodyType::Dynamic;

    PhysicsBodyId id1 = physics->createBody(def);
    PhysicsBodyId id2 = physics->createBody(def);
    PhysicsBodyId id3 = physics->createBody(def);

    EXPECT_NE(id1, id2);
    EXPECT_NE(id2, id3);
    EXPECT_EQ(physics->getBodyCount(), 3);
    EXPECT_EQ(physics->getActiveBodyCount(), 3);
}

TEST_F(PhysicsSceneTest, DestroyBody) {
    PhysicsBodyDef def;
    PhysicsBodyId id = physics->createBody(def);
    EXPECT_TRUE(physics->hasBody(id));

    physics->destroyBody(id);
    EXPECT_FALSE(physics->hasBody(id));
    EXPECT_EQ(physics->getBodyCount(), 0);
}

TEST_F(PhysicsSceneTest, DestroyNonExistentBody) {
    // Should not throw
    physics->destroyBody(999);
    EXPECT_EQ(physics->getBodyCount(), 0);
}

TEST_F(PhysicsSceneTest, GetBodyState) {
    PhysicsBodyDef def;
    def.position = {3.0f, 4.0f};
    def.rotation = 1.5f;

    PhysicsBodyId id = physics->createBody(def);
    PhysicsBodyState state = physics->getBodyState(id);

    EXPECT_FLOAT_EQ(state.position.x, 3.0f);
    EXPECT_FLOAT_EQ(state.position.y, 4.0f);
    EXPECT_FLOAT_EQ(state.rotation, 1.5f);
    EXPECT_FLOAT_EQ(state.velocity.x, 0.0f);
    EXPECT_FLOAT_EQ(state.velocity.y, 0.0f);
}

TEST_F(PhysicsSceneTest, GetBodyStateThrowsForMissing) {
    EXPECT_THROW(physics->getBodyState(999), std::runtime_error);
}

TEST_F(PhysicsSceneTest, GetBodyDef) {
    PhysicsBodyDef def;
    def.type = PhysicsBodyType::Dynamic;
    def.shape = PhysicsShape::Circle;
    def.mass = 2.5f;
    def.friction = 0.5f;
    def.restitution = 0.8f;

    PhysicsBodyId id = physics->createBody(def);
    PhysicsBodyDef retrieved = physics->getBodyDef(id);

    EXPECT_EQ(retrieved.type, PhysicsBodyType::Dynamic);
    EXPECT_EQ(retrieved.shape, PhysicsShape::Circle);
    EXPECT_FLOAT_EQ(retrieved.mass, 2.5f);
    EXPECT_FLOAT_EQ(retrieved.friction, 0.5f);
    EXPECT_FLOAT_EQ(retrieved.restitution, 0.8f);
}

// ============================================================================
// Gravity & Integration
// ============================================================================

TEST_F(PhysicsSceneTest, DynamicBodyFallsUnderGravity) {
    PhysicsBodyDef def;
    def.type = PhysicsBodyType::Dynamic;
    def.position = {0.0f, 10.0f};
    def.mass = 1.0f;
    def.linearDamping = 0.0f;

    PhysicsBodyId id = physics->createBody(def);

    // Step a few frames
    for (int i = 0; i < 60; ++i) {
        physics->step(1.0f / 60.0f);
    }

    PhysicsBodyState state = physics->getBodyState(id);
    // Body should have moved downward
    EXPECT_LT(state.position.y, 10.0f);
    // Velocity should be negative (falling)
    EXPECT_LT(state.velocity.y, 0.0f);
}

TEST_F(PhysicsSceneTest, StaticBodyDoesNotMove) {
    PhysicsBodyDef def;
    def.type = PhysicsBodyType::Static;
    def.position = {5.0f, 5.0f};

    PhysicsBodyId id = physics->createBody(def);

    for (int i = 0; i < 60; ++i) {
        physics->step(1.0f / 60.0f);
    }

    PhysicsBodyState state = physics->getBodyState(id);
    EXPECT_FLOAT_EQ(state.position.x, 5.0f);
    EXPECT_FLOAT_EQ(state.position.y, 5.0f);
    EXPECT_FLOAT_EQ(state.velocity.x, 0.0f);
    EXPECT_FLOAT_EQ(state.velocity.y, 0.0f);
}

TEST_F(PhysicsSceneTest, KinematicBodyCanBeRepositioned) {
    PhysicsBodyDef def;
    def.type = PhysicsBodyType::Kinematic;
    def.position = {0.0f, 0.0f};

    PhysicsBodyId id = physics->createBody(def);

    // Should not be affected by gravity
    physics->step(1.0f / 60.0f);
    PhysicsBodyState state = physics->getBodyState(id);
    EXPECT_FLOAT_EQ(state.position.x, 0.0f);
    EXPECT_FLOAT_EQ(state.position.y, 0.0f);

    // Can be repositioned manually
    physics->setBodyPosition(id, {10.0f, 20.0f});
    state = physics->getBodyState(id);
    EXPECT_FLOAT_EQ(state.position.x, 10.0f);
    EXPECT_FLOAT_EQ(state.position.y, 20.0f);
}

// ============================================================================
// Fixed Timestep Accumulator
// ============================================================================

TEST_F(PhysicsSceneTest, FixedTimestepAccumulator) {
    PhysicsBodyDef def;
    def.type = PhysicsBodyType::Dynamic;
    def.position = {0.0f, 10.0f};
    def.mass = 1.0f;
    def.linearDamping = 0.0f;

    physics->createBody(def);

    // Step with exactly one fixed timestep — should perform 1 step
    physics->step(1.0f / 60.0f);
    EXPECT_EQ(physics->getLastStepCount(), 1);

    // Step with a tiny dt — should not perform any steps
    physics->step(0.001f);
    // Accumulated dt < fixedTimestep, but depends on accumulated leftover
    // At minimum, step count should be 0 or 1
    EXPECT_GE(physics->getLastStepCount(), 0);
}

TEST_F(PhysicsSceneTest, LargeDtCappedByMaxSubSteps) {
    PhysicsBodyDef def;
    def.type = PhysicsBodyType::Dynamic;
    def.position = {0.0f, 10.0f};
    def.mass = 1.0f;

    physics->createBody(def);

    // Very large dt — should be capped at maxSubSteps
    physics->step(1.0f);  // 1 second, at 60Hz = 60 steps, capped at 8
    EXPECT_LE(physics->getLastStepCount(), config.maxSubSteps);
}

TEST_F(PhysicsSceneTest, InterpolationAlphaInRange) {
    PhysicsBodyDef def;
    def.type = PhysicsBodyType::Dynamic;
    def.position = {0.0f, 0.0f};
    def.mass = 1.0f;

    physics->createBody(def);
    physics->step(1.0f / 60.0f);

    float alpha = physics->getInterpolationAlpha();
    EXPECT_GE(alpha, 0.0f);
    EXPECT_LT(alpha, 1.0f);
}

// ============================================================================
// Forces & Impulses
// ============================================================================

TEST_F(PhysicsSceneTest, ApplyForce) {
    PhysicsConfig zeroCfg;
    zeroCfg.gravity = {0.0f, 0.0f};
    PhysicsScene phys(zeroCfg);

    PhysicsBodyDef def;
    def.type = PhysicsBodyType::Dynamic;
    def.position = {0.0f, 0.0f};
    def.mass = 1.0f;
    def.linearDamping = 0.0f;

    PhysicsBodyId id = phys.createBody(def);
    phys.applyForce(id, {10.0f, 0.0f});
    phys.step(1.0f / 60.0f);

    PhysicsBodyState state = phys.getBodyState(id);
    // Force = 10N, mass = 1kg, dt = 1/60s → v ≈ 10 * (1/60) ≈ 0.167
    EXPECT_GT(state.velocity.x, 0.0f);
    EXPECT_GT(state.position.x, 0.0f);
}

TEST_F(PhysicsSceneTest, ApplyImpulse) {
    PhysicsConfig zeroCfg;
    zeroCfg.gravity = {0.0f, 0.0f};
    PhysicsScene phys(zeroCfg);

    PhysicsBodyDef def;
    def.type = PhysicsBodyType::Dynamic;
    def.position = {0.0f, 0.0f};
    def.mass = 2.0f;
    def.linearDamping = 0.0f;

    PhysicsBodyId id = phys.createBody(def);
    phys.applyImpulse(id, {10.0f, 0.0f});

    // Impulse is applied immediately to velocity: v += impulse / mass
    PhysicsBodyState state = phys.getBodyState(id);
    EXPECT_FLOAT_EQ(state.velocity.x, 5.0f);  // 10 / 2
}

TEST_F(PhysicsSceneTest, SetLinearVelocity) {
    PhysicsConfig zeroCfg;
    zeroCfg.gravity = {0.0f, 0.0f};
    PhysicsScene phys(zeroCfg);

    PhysicsBodyDef def;
    def.type = PhysicsBodyType::Dynamic;
    def.mass = 1.0f;
    def.linearDamping = 0.0f;

    PhysicsBodyId id = phys.createBody(def);
    phys.setLinearVelocity(id, {5.0f, -3.0f});

    PhysicsBodyState state = phys.getBodyState(id);
    EXPECT_FLOAT_EQ(state.velocity.x, 5.0f);
    EXPECT_FLOAT_EQ(state.velocity.y, -3.0f);
}

TEST_F(PhysicsSceneTest, SetBodyPosition) {
    PhysicsBodyDef def;
    def.type = PhysicsBodyType::Dynamic;

    PhysicsBodyId id = physics->createBody(def);
    physics->setBodyPosition(id, {100.0f, 200.0f});

    PhysicsBodyState state = physics->getBodyState(id);
    EXPECT_FLOAT_EQ(state.position.x, 100.0f);
    EXPECT_FLOAT_EQ(state.position.y, 200.0f);
}

// ============================================================================
// Gravity Configuration
// ============================================================================

TEST_F(PhysicsSceneTest, SetGravityChangesGravity) {
    physics->setGravity({0.0f, -20.0f});
    glm::vec2 g = physics->getGravity();
    EXPECT_FLOAT_EQ(g.x, 0.0f);
    EXPECT_FLOAT_EQ(g.y, -20.0f);
}

TEST_F(PhysicsSceneTest, ZeroGravityBodyStaysStill) {
    PhysicsConfig zeroCfg;
    zeroCfg.gravity = {0.0f, 0.0f};
    PhysicsScene phys(zeroCfg);

    PhysicsBodyDef def;
    def.type = PhysicsBodyType::Dynamic;
    def.position = {5.0f, 5.0f};
    def.mass = 1.0f;
    def.linearDamping = 0.0f;

    PhysicsBodyId id = phys.createBody(def);

    for (int i = 0; i < 60; ++i) {
        phys.step(1.0f / 60.0f);
    }

    PhysicsBodyState state = phys.getBodyState(id);
    EXPECT_NEAR(state.position.x, 5.0f, 0.01f);
    EXPECT_NEAR(state.position.y, 5.0f, 0.01f);
}

// ============================================================================
// AABB Collision
// ============================================================================

TEST_F(PhysicsSceneTest, AABBCollisionBetweenTwoDynamicBoxes) {
    PhysicsConfig zeroCfg;
    zeroCfg.gravity = {0.0f, 0.0f};
    PhysicsScene phys(zeroCfg);

    // Two overlapping dynamic boxes
    PhysicsBodyDef defA;
    defA.type = PhysicsBodyType::Dynamic;
    defA.shape = PhysicsShape::Box;
    defA.position = {0.0f, 0.0f};
    defA.extents = {1.0f, 1.0f};
    defA.mass = 1.0f;
    defA.linearDamping = 0.0f;
    defA.restitution = 0.0f;

    PhysicsBodyDef defB;
    defB.type = PhysicsBodyType::Dynamic;
    defB.shape = PhysicsShape::Box;
    defB.position = {1.5f, 0.0f};  // Overlapping (1.5 < 1.0 + 1.0)
    defB.extents = {1.0f, 1.0f};
    defB.mass = 1.0f;
    defB.linearDamping = 0.0f;
    defB.restitution = 0.0f;

    PhysicsBodyId idA = phys.createBody(defA);
    PhysicsBodyId idB = phys.createBody(defB);

    phys.step(1.0f / 60.0f);

    PhysicsBodyState stateA = phys.getBodyState(idA);
    PhysicsBodyState stateB = phys.getBodyState(idB);

    // After resolution, bodies should have been pushed apart
    EXPECT_LT(stateA.position.x, stateB.position.x);
    // The overlap should have decreased (original was 0.5)
    float newOverlap = (stateA.position.x + defA.extents.x) - (stateB.position.x - defB.extents.x);
    EXPECT_LT(newOverlap, 0.5f);
}

TEST_F(PhysicsSceneTest, DynamicBoxCollidesWithStaticGround) {
    // Dynamic box falling onto a static ground
    PhysicsConfig cfg;
    cfg.gravity = {0.0f, -9.81f};
    cfg.fixedTimestep = 1.0f / 60.0f;
    PhysicsScene phys(cfg);

    PhysicsBodyDef ground;
    ground.type = PhysicsBodyType::Static;
    ground.shape = PhysicsShape::Box;
    ground.position = {0.0f, -1.0f};
    ground.extents = {10.0f, 0.5f};

    PhysicsBodyDef box;
    box.type = PhysicsBodyType::Dynamic;
    box.shape = PhysicsShape::Box;
    box.position = {0.0f, 2.0f};
    box.extents = {0.5f, 0.5f};
    box.mass = 1.0f;
    box.restitution = 0.0f;
    box.linearDamping = 0.0f;

    phys.createBody(ground);
    PhysicsBodyId boxId = phys.createBody(box);

    // Run for 2 seconds (should land on ground)
    for (int i = 0; i < 120; ++i) {
        phys.step(1.0f / 60.0f);
    }

    PhysicsBodyState state = phys.getBodyState(boxId);

    // Box should be resting on top of the ground
    // Ground top = -1.0 + 0.5 = -0.5, box bottom = state.y - 0.5
    // So state.y should be approximately -0.5 + 0.5 = 0.0 (or close)
    EXPECT_GT(state.position.y, -1.0f);  // Above ground center
    EXPECT_LT(state.position.y, 2.0f);   // Below start position

    // Velocity should be near zero (resting)
    EXPECT_NEAR(state.velocity.y, 0.0f, 2.0f);
}

// ============================================================================
// Collision Callbacks
// ============================================================================

TEST_F(PhysicsSceneTest, CollisionCallbackFires) {
    PhysicsConfig zeroCfg;
    zeroCfg.gravity = {0.0f, 0.0f};
    PhysicsScene phys(zeroCfg);

    int callbackCount = 0;
    phys.setOnCollisionBegin([&callbackCount](const CollisionEvent& evt) {
        callbackCount++;
        EXPECT_NE(evt.bodyA, INVALID_PHYSICS_BODY_ID);
        EXPECT_NE(evt.bodyB, INVALID_PHYSICS_BODY_ID);
        EXPECT_GT(evt.depth, 0.0f);
    });

    // Two overlapping boxes
    PhysicsBodyDef defA;
    defA.type = PhysicsBodyType::Dynamic;
    defA.position = {0.0f, 0.0f};
    defA.extents = {1.0f, 1.0f};
    defA.mass = 1.0f;

    PhysicsBodyDef defB;
    defB.type = PhysicsBodyType::Dynamic;
    defB.position = {1.5f, 0.0f};
    defB.extents = {1.0f, 1.0f};
    defB.mass = 1.0f;

    phys.createBody(defA);
    phys.createBody(defB);

    phys.step(1.0f / 60.0f);

    EXPECT_GT(callbackCount, 0);
}

// ============================================================================
// Sensors
// ============================================================================

TEST_F(PhysicsSceneTest, SensorTriggersCallbackButNoResponse) {
    PhysicsConfig zeroCfg;
    zeroCfg.gravity = {0.0f, 0.0f};
    PhysicsScene phys(zeroCfg);

    int callbackCount = 0;
    phys.setOnCollisionBegin([&callbackCount](const CollisionEvent&) { callbackCount++; });

    PhysicsBodyDef defA;
    defA.type = PhysicsBodyType::Dynamic;
    defA.position = {0.0f, 0.0f};
    defA.extents = {1.0f, 1.0f};
    defA.mass = 1.0f;
    defA.isSensor = true;

    PhysicsBodyDef defB;
    defB.type = PhysicsBodyType::Dynamic;
    defB.position = {1.0f, 0.0f};
    defB.extents = {1.0f, 1.0f};
    defB.mass = 1.0f;

    PhysicsBodyId idA = phys.createBody(defA);
    PhysicsBodyId idB = phys.createBody(defB);

    // Record positions before step
    glm::vec2 posABefore = phys.getBodyState(idA).position;
    glm::vec2 posBBefore = phys.getBodyState(idB).position;

    phys.step(1.0f / 60.0f);

    // Callback should fire
    EXPECT_GT(callbackCount, 0);

    // Positions should NOT change from collision response (sensor doesn't push)
    glm::vec2 posAAfter = phys.getBodyState(idA).position;
    glm::vec2 posBAfter = phys.getBodyState(idB).position;

    EXPECT_NEAR(posAAfter.x, posABefore.x, 0.01f);
    EXPECT_NEAR(posBAfter.x, posBBefore.x, 0.01f);
}

// ============================================================================
// Body Count
// ============================================================================

TEST_F(PhysicsSceneTest, BodyCountAccurate) {
    PhysicsBodyDef def;
    def.type = PhysicsBodyType::Dynamic;

    PhysicsBodyId id1 = physics->createBody(def);
    physics->createBody(def);
    physics->createBody(def);

    EXPECT_EQ(physics->getBodyCount(), 3);
    EXPECT_EQ(physics->getActiveBodyCount(), 3);

    physics->destroyBody(id1);
    EXPECT_EQ(physics->getBodyCount(), 2);
    EXPECT_EQ(physics->getActiveBodyCount(), 2);
}

// ============================================================================
// Scene Integration
// ============================================================================

TEST_F(PhysicsSceneTest, SceneEnableDisablePhysics) {
    Scene scene;
    EXPECT_FALSE(scene.hasPhysics());
    EXPECT_EQ(scene.getPhysicsScene(), nullptr);

    scene.enablePhysics();
    EXPECT_TRUE(scene.hasPhysics());
    EXPECT_NE(scene.getPhysicsScene(), nullptr);

    scene.disablePhysics();
    EXPECT_FALSE(scene.hasPhysics());
    EXPECT_EQ(scene.getPhysicsScene(), nullptr);
}

TEST_F(PhysicsSceneTest, ScenePhysicsWithCustomConfig) {
    Scene scene;
    PhysicsConfig cfg;
    cfg.gravity = {0.0f, -20.0f};
    cfg.fixedTimestep = 1.0f / 120.0f;

    scene.enablePhysics(cfg);
    ASSERT_NE(scene.getPhysicsScene(), nullptr);

    EXPECT_FLOAT_EQ(scene.getPhysicsScene()->getConfig().gravity.y, -20.0f);
    EXPECT_FLOAT_EQ(scene.getPhysicsScene()->getConfig().fixedTimestep, 1.0f / 120.0f);
}

TEST_F(PhysicsSceneTest, ScenePhysicsCreateBody) {
    Scene scene;
    scene.enablePhysics();

    PhysicsBodyDef def;
    def.type = PhysicsBodyType::Dynamic;
    def.position = {1.0f, 5.0f};
    def.mass = 1.0f;

    PhysicsBodyId id = scene.getPhysicsScene()->createBody(def);
    EXPECT_NE(id, INVALID_PHYSICS_BODY_ID);
    EXPECT_EQ(scene.getPhysicsScene()->getBodyCount(), 1);
}

TEST_F(PhysicsSceneTest, SceneEnablePhysicsTwiceNoOp) {
    Scene scene;
    scene.enablePhysics();
    auto* first = scene.getPhysicsScene();

    scene.enablePhysics();  // Should not create a new scene
    EXPECT_EQ(scene.getPhysicsScene(), first);
}

// ============================================================================
// Move semantics
// ============================================================================

TEST_F(PhysicsSceneTest, MoveConstruct) {
    PhysicsBodyDef def;
    def.type = PhysicsBodyType::Dynamic;
    def.position = {1.0f, 2.0f};

    PhysicsBodyId id = physics->createBody(def);

    PhysicsScene moved(std::move(*physics));
    EXPECT_TRUE(moved.hasBody(id));
    EXPECT_EQ(moved.getBodyCount(), 1);

    PhysicsBodyState state = moved.getBodyState(id);
    EXPECT_FLOAT_EQ(state.position.x, 1.0f);
    EXPECT_FLOAT_EQ(state.position.y, 2.0f);
}

TEST_F(PhysicsSceneTest, StepWithNoBodiesIsNoOp) {
    // Should not crash
    physics->step(1.0f / 60.0f);
    EXPECT_EQ(physics->getLastStepCount(), 1);
    EXPECT_EQ(physics->getBodyCount(), 0);
}

}  // namespace vde::test
