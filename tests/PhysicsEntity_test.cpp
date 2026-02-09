/**
 * @file PhysicsEntity_test.cpp
 * @brief Unit tests for PhysicsEntity, PhysicsSpriteEntity, PhysicsMeshEntity (Phase 6)
 *
 * Tests body creation, sync from/to physics, interpolation, auto-sync,
 * force/impulse helpers, and lifecycle (onAttach / onDetach).
 */

#include <vde/api/PhysicsEntity.h>
#include <vde/api/PhysicsScene.h>
#include <vde/api/PhysicsTypes.h>
#include <vde/api/Scene.h>

#include <cmath>
#include <memory>

#include <gtest/gtest.h>

using namespace vde;

namespace vde::test {

// ============================================================================
// Helpers
// ============================================================================

/**
 * @brief Create a simple Scene with physics enabled and return it.
 */
static std::unique_ptr<Scene> makePhysicsScene() {
    auto scene = std::make_unique<Scene>();
    PhysicsConfig cfg;
    cfg.gravity = {0.0f, -9.81f};
    cfg.fixedTimestep = 1.0f / 60.0f;
    scene->enablePhysics(cfg);
    return scene;
}

/**
 * @brief Create a default dynamic box body definition.
 */
static PhysicsBodyDef makeDynamicBoxDef(float x = 0.0f, float y = 5.0f) {
    PhysicsBodyDef def;
    def.type = PhysicsBodyType::Dynamic;
    def.shape = PhysicsShape::Box;
    def.position = {x, y};
    def.extents = {0.5f, 0.5f};
    def.mass = 1.0f;
    def.restitution = 0.0f;
    return def;
}

// ============================================================================
// PhysicsEntity base — body creation
// ============================================================================

class PhysicsEntityTest : public ::testing::Test {
  protected:
    std::unique_ptr<Scene> scene;

    void SetUp() override { scene = makePhysicsScene(); }
};

TEST_F(PhysicsEntityTest, CreatePhysicsBodySucceeds) {
    auto entity = scene->addEntity<PhysicsSpriteEntity>();

    PhysicsBodyDef def = makeDynamicBoxDef(0.0f, 5.0f);
    PhysicsBodyId id = entity->createPhysicsBody(def);

    EXPECT_NE(id, INVALID_PHYSICS_BODY_ID);
    EXPECT_EQ(entity->getPhysicsBodyId(), id);
    EXPECT_TRUE(scene->getPhysicsScene()->hasBody(id));
}

TEST_F(PhysicsEntityTest, CreateBodySetsEntityPosition) {
    auto entity = scene->addEntity<PhysicsSpriteEntity>();

    PhysicsBodyDef def = makeDynamicBoxDef(3.0f, 7.0f);
    entity->createPhysicsBody(def);

    const auto& pos = entity->getPosition();
    EXPECT_FLOAT_EQ(pos.x, 3.0f);
    EXPECT_FLOAT_EQ(pos.y, 7.0f);
}

TEST_F(PhysicsEntityTest, GetPhysicsStateReturnsBodyState) {
    auto entity = scene->addEntity<PhysicsSpriteEntity>();

    PhysicsBodyDef def = makeDynamicBoxDef(1.0f, 2.0f);
    entity->createPhysicsBody(def);

    auto state = entity->getPhysicsState();
    EXPECT_FLOAT_EQ(state.position.x, 1.0f);
    EXPECT_FLOAT_EQ(state.position.y, 2.0f);
}

TEST_F(PhysicsEntityTest, GetPhysicsStateThrowsWithoutBody) {
    auto entity = scene->addEntity<PhysicsSpriteEntity>();
    EXPECT_THROW(entity->getPhysicsState(), std::runtime_error);
}

TEST_F(PhysicsEntityTest, CreatePhysicsBodyWithoutPhysicsSceneThrows) {
    // Scene without physics
    auto plainScene = std::make_unique<Scene>();
    auto entity = plainScene->addEntity<PhysicsSpriteEntity>();

    PhysicsBodyDef def = makeDynamicBoxDef();
    EXPECT_THROW(entity->createPhysicsBody(def), std::runtime_error);
}

// ============================================================================
// syncFromPhysics
// ============================================================================

TEST_F(PhysicsEntityTest, SyncFromPhysicsUpdatesPosition) {
    auto entity = scene->addEntity<PhysicsSpriteEntity>();

    PhysicsBodyDef def = makeDynamicBoxDef(0.0f, 5.0f);
    entity->createPhysicsBody(def);

    // Step physics so the body falls
    scene->getPhysicsScene()->step(1.0f / 60.0f);

    auto bodyState = scene->getPhysicsScene()->getBodyState(entity->getPhysicsBodyId());

    // Sync with alpha = 1.0 (use current position exactly)
    entity->syncFromPhysics(1.0f);

    const auto& pos = entity->getPosition();
    EXPECT_FLOAT_EQ(pos.x, bodyState.position.x);
    EXPECT_FLOAT_EQ(pos.y, bodyState.position.y);
}

TEST_F(PhysicsEntityTest, SyncFromPhysicsInterpolates) {
    auto entity = scene->addEntity<PhysicsSpriteEntity>();

    PhysicsBodyDef def = makeDynamicBoxDef(0.0f, 10.0f);
    entity->createPhysicsBody(def);

    // Step physics so position changes
    scene->getPhysicsScene()->step(1.0f / 60.0f);

    auto bodyState = scene->getPhysicsScene()->getBodyState(entity->getPhysicsBodyId());

    // The previous position was set during createPhysicsBody as (0, 10)
    // Sync with alpha = 0.5 → halfway between previous (0,10) and current
    entity->syncFromPhysics(0.5f);

    const auto& pos = entity->getPosition();
    float expectedY = 10.0f * 0.5f + bodyState.position.y * 0.5f;
    EXPECT_NEAR(pos.y, expectedY, 0.01f);
}

TEST_F(PhysicsEntityTest, SyncFromPhysicsWithAlphaZeroUsesPrevious) {
    auto entity = scene->addEntity<PhysicsSpriteEntity>();

    PhysicsBodyDef def = makeDynamicBoxDef(0.0f, 10.0f);
    entity->createPhysicsBody(def);

    // Step to change position
    scene->getPhysicsScene()->step(1.0f / 60.0f);

    // Sync with alpha = 0 → previous position
    entity->syncFromPhysics(0.0f);

    const auto& pos = entity->getPosition();
    EXPECT_FLOAT_EQ(pos.x, 0.0f);
    EXPECT_FLOAT_EQ(pos.y, 10.0f);  // previous position
}

// ============================================================================
// syncToPhysics
// ============================================================================

TEST_F(PhysicsEntityTest, SyncToPhysicsCopiesEntityPosition) {
    auto entity = scene->addEntity<PhysicsSpriteEntity>();

    PhysicsBodyDef def;
    def.type = PhysicsBodyType::Kinematic;
    def.shape = PhysicsShape::Box;
    def.position = {0.0f, 0.0f};
    def.extents = {0.5f, 0.5f};
    entity->createPhysicsBody(def);

    // Move entity manually
    entity->setPosition(Position(5.0f, 10.0f, 0.0f));
    entity->syncToPhysics();

    auto state = scene->getPhysicsScene()->getBodyState(entity->getPhysicsBodyId());
    EXPECT_FLOAT_EQ(state.position.x, 5.0f);
    EXPECT_FLOAT_EQ(state.position.y, 10.0f);
}

// ============================================================================
// Auto-sync
// ============================================================================

TEST_F(PhysicsEntityTest, AutoSyncDefaultIsTrue) {
    auto entity = scene->addEntity<PhysicsSpriteEntity>();
    EXPECT_TRUE(entity->getAutoSync());
}

TEST_F(PhysicsEntityTest, SetAutoSyncFalsePreventsSync) {
    auto entity = scene->addEntity<PhysicsSpriteEntity>();
    entity->setAutoSync(false);
    EXPECT_FALSE(entity->getAutoSync());
}

// ============================================================================
// Force / impulse helpers
// ============================================================================

TEST_F(PhysicsEntityTest, ApplyForceDelegate) {
    auto entity = scene->addEntity<PhysicsSpriteEntity>();

    PhysicsBodyDef def = makeDynamicBoxDef(0.0f, 0.0f);
    entity->createPhysicsBody(def);

    // Apply a rightward force and step
    entity->applyForce(glm::vec2(100.0f, 0.0f));
    scene->getPhysicsScene()->step(1.0f / 60.0f);

    auto state = entity->getPhysicsState();
    EXPECT_GT(state.velocity.x, 0.0f);
}

TEST_F(PhysicsEntityTest, ApplyImpulseDelegate) {
    auto entity = scene->addEntity<PhysicsSpriteEntity>();

    PhysicsBodyDef def = makeDynamicBoxDef(0.0f, 0.0f);
    // Zero gravity so impulse effect is clear
    scene->getPhysicsScene()->setGravity({0.0f, 0.0f});
    entity->createPhysicsBody(def);

    entity->applyImpulse(glm::vec2(0.0f, 5.0f));
    scene->getPhysicsScene()->step(1.0f / 60.0f);

    auto state = entity->getPhysicsState();
    EXPECT_GT(state.velocity.y, 0.0f);
}

TEST_F(PhysicsEntityTest, SetLinearVelocityDelegate) {
    auto entity = scene->addEntity<PhysicsSpriteEntity>();
    scene->getPhysicsScene()->setGravity({0.0f, 0.0f});

    PhysicsBodyDef def = makeDynamicBoxDef(0.0f, 0.0f);
    entity->createPhysicsBody(def);

    entity->setLinearVelocity(glm::vec2(3.0f, 4.0f));

    auto state = entity->getPhysicsState();
    EXPECT_FLOAT_EQ(state.velocity.x, 3.0f);
    EXPECT_FLOAT_EQ(state.velocity.y, 4.0f);
}

// ============================================================================
// Lifecycle — onDetach cleans up physics body
// ============================================================================

TEST_F(PhysicsEntityTest, OnDetachDestroysPhysicsBody) {
    auto entity = scene->addEntity<PhysicsSpriteEntity>();

    PhysicsBodyDef def = makeDynamicBoxDef();
    PhysicsBodyId id = entity->createPhysicsBody(def);

    EXPECT_TRUE(scene->getPhysicsScene()->hasBody(id));

    // Remove the entity from the scene (triggers onDetach)
    scene->removeEntity(entity->getId());

    EXPECT_FALSE(scene->getPhysicsScene()->hasBody(id));
}

TEST_F(PhysicsEntityTest, OnDetachResetsBodyId) {
    auto entity = scene->addEntity<PhysicsSpriteEntity>();

    PhysicsBodyDef def = makeDynamicBoxDef();
    entity->createPhysicsBody(def);

    // After detach the entity should have INVALID body id
    entity->onDetach();
    EXPECT_EQ(entity->getPhysicsBodyId(), INVALID_PHYSICS_BODY_ID);
}

// ============================================================================
// PhysicsSpriteEntity specific
// ============================================================================

TEST_F(PhysicsEntityTest, PhysicsSpriteEntityHasDefaultColor) {
    auto entity = scene->addEntity<PhysicsSpriteEntity>();
    EXPECT_FLOAT_EQ(entity->getColor().r, 1.0f);
    EXPECT_FLOAT_EQ(entity->getColor().g, 1.0f);
    EXPECT_FLOAT_EQ(entity->getColor().b, 1.0f);
}

TEST_F(PhysicsEntityTest, PhysicsSpriteEntitySetColor) {
    auto entity = scene->addEntity<PhysicsSpriteEntity>();
    entity->setColor(Color(1.0f, 0.0f, 0.0f, 1.0f));
    EXPECT_FLOAT_EQ(entity->getColor().r, 1.0f);
    EXPECT_FLOAT_EQ(entity->getColor().g, 0.0f);
}

// ============================================================================
// PhysicsMeshEntity specific
// ============================================================================

TEST_F(PhysicsEntityTest, PhysicsMeshEntityCreateBody) {
    auto entity = scene->addEntity<PhysicsMeshEntity>();

    PhysicsBodyDef def = makeDynamicBoxDef(2.0f, 3.0f);
    PhysicsBodyId id = entity->createPhysicsBody(def);

    EXPECT_NE(id, INVALID_PHYSICS_BODY_ID);
    EXPECT_EQ(entity->getPhysicsBodyId(), id);
}

TEST_F(PhysicsEntityTest, PhysicsMeshEntitySyncFromPhysics) {
    auto entity = scene->addEntity<PhysicsMeshEntity>();

    PhysicsBodyDef def = makeDynamicBoxDef(0.0f, 5.0f);
    entity->createPhysicsBody(def);

    scene->getPhysicsScene()->step(1.0f / 60.0f);
    entity->syncFromPhysics(1.0f);

    auto state = scene->getPhysicsScene()->getBodyState(entity->getPhysicsBodyId());
    const auto& pos = entity->getPosition();
    EXPECT_FLOAT_EQ(pos.x, state.position.x);
    EXPECT_FLOAT_EQ(pos.y, state.position.y);
}

// ============================================================================
// Force/impulse with no body — should be safe no-ops
// ============================================================================

TEST_F(PhysicsEntityTest, ForceWithNoBodyIsSafe) {
    auto entity = scene->addEntity<PhysicsSpriteEntity>();
    // No body created — should not crash
    EXPECT_NO_THROW(entity->applyForce(glm::vec2(1.0f, 0.0f)));
    EXPECT_NO_THROW(entity->applyImpulse(glm::vec2(0.0f, 1.0f)));
    EXPECT_NO_THROW(entity->setLinearVelocity(glm::vec2(0.0f, 0.0f)));
}

TEST_F(PhysicsEntityTest, SyncWithNoBodyIsSafe) {
    auto entity = scene->addEntity<PhysicsSpriteEntity>();
    EXPECT_NO_THROW(entity->syncFromPhysics(0.5f));
    EXPECT_NO_THROW(entity->syncToPhysics());
}

// ============================================================================
// Re-creating a body replaces the existing one
// ============================================================================

TEST_F(PhysicsEntityTest, RecreateBodyReplacesExisting) {
    auto entity = scene->addEntity<PhysicsSpriteEntity>();

    PhysicsBodyDef def1 = makeDynamicBoxDef(0.0f, 1.0f);
    PhysicsBodyId id1 = entity->createPhysicsBody(def1);

    PhysicsBodyDef def2 = makeDynamicBoxDef(0.0f, 2.0f);
    PhysicsBodyId id2 = entity->createPhysicsBody(def2);

    EXPECT_NE(id1, id2);
    EXPECT_FALSE(scene->getPhysicsScene()->hasBody(id1));
    EXPECT_TRUE(scene->getPhysicsScene()->hasBody(id2));
    EXPECT_EQ(entity->getPhysicsBodyId(), id2);
}

}  // namespace vde::test
