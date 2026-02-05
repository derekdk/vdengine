/**
 * @file Entity_test.cpp
 * @brief Unit tests for Entity classes (Phase 1)
 *
 * Tests Entity, MeshEntity, and SpriteEntity base functionality.
 */

#include <vde/api/Entity.h>
#include <vde/api/GameTypes.h>

#include <glm/gtc/matrix_transform.hpp>

#include <cmath>

#include <gtest/gtest.h>

using namespace vde;

// ============================================================================
// Entity Base Class Tests
// ============================================================================

class EntityTest : public ::testing::Test {
  protected:
    std::shared_ptr<Entity> entity;

    void SetUp() override {
        // Create a concrete MeshEntity since Entity is intended as base
        entity = std::make_shared<MeshEntity>();
    }
};

TEST_F(EntityTest, DefaultConstructorGeneratesUniqueId) {
    auto entity1 = std::make_shared<MeshEntity>();
    auto entity2 = std::make_shared<MeshEntity>();

    EXPECT_NE(entity1->getId(), entity2->getId());
    EXPECT_GT(entity1->getId(), 0);
    EXPECT_GT(entity2->getId(), 0);
}

TEST_F(EntityTest, SetAndGetName) {
    entity->setName("TestEntity");
    EXPECT_EQ(entity->getName(), "TestEntity");
}

TEST_F(EntityTest, SetPositionFloat3) {
    entity->setPosition(1.0f, 2.0f, 3.0f);

    const Position& pos = entity->getPosition();
    EXPECT_FLOAT_EQ(pos.x, 1.0f);
    EXPECT_FLOAT_EQ(pos.y, 2.0f);
    EXPECT_FLOAT_EQ(pos.z, 3.0f);
}

TEST_F(EntityTest, SetPositionStruct) {
    Position pos(4.0f, 5.0f, 6.0f);
    entity->setPosition(pos);

    const Position& result = entity->getPosition();
    EXPECT_FLOAT_EQ(result.x, 4.0f);
    EXPECT_FLOAT_EQ(result.y, 5.0f);
    EXPECT_FLOAT_EQ(result.z, 6.0f);
}

TEST_F(EntityTest, SetPositionVec3) {
    glm::vec3 vec(7.0f, 8.0f, 9.0f);
    entity->setPosition(vec);

    const Position& result = entity->getPosition();
    EXPECT_FLOAT_EQ(result.x, 7.0f);
    EXPECT_FLOAT_EQ(result.y, 8.0f);
    EXPECT_FLOAT_EQ(result.z, 9.0f);
}

TEST_F(EntityTest, SetRotationFloat3) {
    entity->setRotation(10.0f, 20.0f, 30.0f);

    const Rotation& rot = entity->getRotation();
    EXPECT_FLOAT_EQ(rot.pitch, 10.0f);
    EXPECT_FLOAT_EQ(rot.yaw, 20.0f);
    EXPECT_FLOAT_EQ(rot.roll, 30.0f);
}

TEST_F(EntityTest, SetRotationStruct) {
    Rotation rot(45.0f, 90.0f, 180.0f);
    entity->setRotation(rot);

    const Rotation& result = entity->getRotation();
    EXPECT_FLOAT_EQ(result.pitch, 45.0f);
    EXPECT_FLOAT_EQ(result.yaw, 90.0f);
    EXPECT_FLOAT_EQ(result.roll, 180.0f);
}

TEST_F(EntityTest, SetScaleUniform) {
    entity->setScale(2.0f);

    const Scale& scale = entity->getScale();
    EXPECT_FLOAT_EQ(scale.x, 2.0f);
    EXPECT_FLOAT_EQ(scale.y, 2.0f);
    EXPECT_FLOAT_EQ(scale.z, 2.0f);
}

TEST_F(EntityTest, SetScaleNonUniform) {
    entity->setScale(1.0f, 2.0f, 3.0f);

    const Scale& scale = entity->getScale();
    EXPECT_FLOAT_EQ(scale.x, 1.0f);
    EXPECT_FLOAT_EQ(scale.y, 2.0f);
    EXPECT_FLOAT_EQ(scale.z, 3.0f);
}

TEST_F(EntityTest, SetScaleStruct) {
    Scale scl(4.0f, 5.0f, 6.0f);
    entity->setScale(scl);

    const Scale& result = entity->getScale();
    EXPECT_FLOAT_EQ(result.x, 4.0f);
    EXPECT_FLOAT_EQ(result.y, 5.0f);
    EXPECT_FLOAT_EQ(result.z, 6.0f);
}

TEST_F(EntityTest, GetModelMatrixIdentityAtOrigin) {
    // Default entity at origin with no rotation and scale 1
    glm::mat4 model = entity->getModelMatrix();
    glm::mat4 identity(1.0f);

    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            EXPECT_NEAR(model[i][j], identity[i][j], 0.0001f);
        }
    }
}

TEST_F(EntityTest, GetModelMatrixWithTranslation) {
    entity->setPosition(10.0f, 20.0f, 30.0f);
    glm::mat4 model = entity->getModelMatrix();

    // Translation should be in the last column
    EXPECT_FLOAT_EQ(model[3][0], 10.0f);
    EXPECT_FLOAT_EQ(model[3][1], 20.0f);
    EXPECT_FLOAT_EQ(model[3][2], 30.0f);
}

TEST_F(EntityTest, GetModelMatrixWithScale) {
    entity->setScale(2.0f, 3.0f, 4.0f);
    glm::mat4 model = entity->getModelMatrix();

    // Scale affects the diagonal (approximately, without rotation)
    glm::vec3 scaleVec(glm::length(glm::vec3(model[0])), glm::length(glm::vec3(model[1])),
                       glm::length(glm::vec3(model[2])));

    EXPECT_NEAR(scaleVec.x, 2.0f, 0.0001f);
    EXPECT_NEAR(scaleVec.y, 3.0f, 0.0001f);
    EXPECT_NEAR(scaleVec.z, 4.0f, 0.0001f);
}

TEST_F(EntityTest, VisibilityDefaultTrue) {
    EXPECT_TRUE(entity->isVisible());
}

TEST_F(EntityTest, SetVisibleWorks) {
    entity->setVisible(false);
    EXPECT_FALSE(entity->isVisible());

    entity->setVisible(true);
    EXPECT_TRUE(entity->isVisible());
}

TEST_F(EntityTest, GetTransform) {
    entity->setPosition(1.0f, 2.0f, 3.0f);
    entity->setRotation(10.0f, 20.0f, 30.0f);
    entity->setScale(2.0f);

    const Transform& transform = entity->getTransform();
    EXPECT_FLOAT_EQ(transform.position.x, 1.0f);
    EXPECT_FLOAT_EQ(transform.rotation.pitch, 10.0f);
    EXPECT_FLOAT_EQ(transform.scale.x, 2.0f);
}

TEST_F(EntityTest, SetTransform) {
    Transform t;
    t.position = Position(5.0f, 6.0f, 7.0f);
    t.rotation = Rotation(45.0f, 90.0f, 0.0f);
    t.scale = Scale(3.0f, 3.0f, 3.0f);

    entity->setTransform(t);

    EXPECT_FLOAT_EQ(entity->getPosition().x, 5.0f);
    EXPECT_FLOAT_EQ(entity->getRotation().yaw, 90.0f);
    EXPECT_FLOAT_EQ(entity->getScale().x, 3.0f);
}

// ============================================================================
// MeshEntity Tests
// ============================================================================

class MeshEntityTest : public ::testing::Test {
  protected:
    std::shared_ptr<MeshEntity> meshEntity;

    void SetUp() override { meshEntity = std::make_shared<MeshEntity>(); }
};

TEST_F(MeshEntityTest, DefaultConstructor) {
    EXPECT_EQ(meshEntity->getMesh(), nullptr);
    EXPECT_EQ(meshEntity->getTexture(), nullptr);
    EXPECT_EQ(meshEntity->getMeshId(), INVALID_RESOURCE_ID);
    EXPECT_EQ(meshEntity->getTextureId(), INVALID_RESOURCE_ID);
}

TEST_F(MeshEntityTest, SetMeshId) {
    meshEntity->setMeshId(42);
    EXPECT_EQ(meshEntity->getMeshId(), 42);
}

TEST_F(MeshEntityTest, SetTextureId) {
    meshEntity->setTextureId(123);
    EXPECT_EQ(meshEntity->getTextureId(), 123);
}

TEST_F(MeshEntityTest, SetColor) {
    Color red(1.0f, 0.0f, 0.0f, 1.0f);
    meshEntity->setColor(red);

    const Color& color = meshEntity->getColor();
    EXPECT_FLOAT_EQ(color.r, 1.0f);
    EXPECT_FLOAT_EQ(color.g, 0.0f);
    EXPECT_FLOAT_EQ(color.b, 0.0f);
    EXPECT_FLOAT_EQ(color.a, 1.0f);
}

TEST_F(MeshEntityTest, DefaultColorIsWhite) {
    const Color& color = meshEntity->getColor();
    EXPECT_FLOAT_EQ(color.r, 1.0f);
    EXPECT_FLOAT_EQ(color.g, 1.0f);
    EXPECT_FLOAT_EQ(color.b, 1.0f);
    EXPECT_FLOAT_EQ(color.a, 1.0f);
}

TEST_F(MeshEntityTest, InheritsFromEntity) {
    Entity* basePtr = meshEntity.get();
    EXPECT_NE(basePtr, nullptr);

    meshEntity->setPosition(10.0f, 20.0f, 30.0f);
    EXPECT_FLOAT_EQ(meshEntity->getPosition().x, 10.0f);
}
