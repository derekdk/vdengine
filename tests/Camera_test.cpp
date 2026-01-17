/**
 * @file Camera_test.cpp
 * @brief Unit tests for vde::Camera class.
 */

#include <gtest/gtest.h>
#include <vde/Camera.h>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

namespace vde {
namespace test {

class CameraTest : public ::testing::Test {
protected:
    Camera camera;
    
    void SetUp() override {
        camera = Camera();
    }
};

TEST_F(CameraTest, DefaultPositionIsValid) {
    glm::vec3 pos = camera.getPosition();
    // Camera should have a valid default position (not at origin looking at origin)
    EXPECT_TRUE(pos.x != 0.0f || pos.y != 0.0f || pos.z != 0.0f);
}

TEST_F(CameraTest, SetPositionWorks) {
    glm::vec3 newPos(10.0f, 20.0f, 30.0f);
    camera.setPosition(newPos);
    
    glm::vec3 pos = camera.getPosition();
    EXPECT_FLOAT_EQ(pos.x, 10.0f);
    EXPECT_FLOAT_EQ(pos.y, 20.0f);
    EXPECT_FLOAT_EQ(pos.z, 30.0f);
}

TEST_F(CameraTest, SetTargetWorks) {
    glm::vec3 target(5.0f, 0.0f, 5.0f);
    camera.setTarget(target);
    
    glm::vec3 t = camera.getTarget();
    EXPECT_FLOAT_EQ(t.x, 5.0f);
    EXPECT_FLOAT_EQ(t.y, 0.0f);
    EXPECT_FLOAT_EQ(t.z, 5.0f);
}

TEST_F(CameraTest, ViewMatrixIsValid) {
    glm::mat4 view = camera.getViewMatrix();
    
    // View matrix should not be identity (camera is positioned away from origin)
    glm::mat4 identity(1.0f);
    bool isIdentity = true;
    for (int i = 0; i < 4 && isIdentity; i++) {
        for (int j = 0; j < 4 && isIdentity; j++) {
            if (std::abs(view[i][j] - identity[i][j]) > 0.0001f) {
                isIdentity = false;
            }
        }
    }
    EXPECT_FALSE(isIdentity);
}

TEST_F(CameraTest, ProjectionMatrixIsValid) {
    camera.setPerspective(45.0f, 16.0f/9.0f, 0.1f, 100.0f);
    glm::mat4 proj = camera.getProjectionMatrix();
    
    // Vulkan projection should have Y-flip (proj[1][1] is negative)
    EXPECT_LT(proj[1][1], 0.0f);
}

TEST_F(CameraTest, SetFromPitchYawSetsPosition) {
    float distance = 20.0f;
    float pitch = 45.0f;
    float yaw = 90.0f;
    glm::vec3 target(0.0f, 0.0f, 0.0f);
    
    camera.setFromPitchYaw(distance, pitch, yaw, target);
    
    glm::vec3 pos = camera.getPosition();
    
    // Position should be at specified distance from target
    float actualDistance = glm::length(pos - target);
    EXPECT_NEAR(distance, actualDistance, 0.01f);
}

TEST_F(CameraTest, ZoomChangesDistance) {
    float distance = 20.0f;
    camera.setFromPitchYaw(distance, 45.0f, 0.0f, glm::vec3(0.0f));
    
    float originalDistance = camera.getDistance();
    
    camera.zoom(1.0f);  // Zoom in (positive = toward target)
    
    float newDistance = camera.getDistance();
    EXPECT_LT(newDistance, originalDistance);
}

TEST_F(CameraTest, DistanceClampingWorks) {
    camera.setFromPitchYaw(50.0f, 45.0f, 0.0f, glm::vec3(0.0f));
    
    // Try to zoom way out
    for (int i = 0; i < 100; i++) {
        camera.zoom(-10.0f);  // negative = away from target
    }
    
    float distance = camera.getDistance();
    // Should be clamped to MAX_DISTANCE
    EXPECT_LE(distance, Camera::MAX_DISTANCE);
}

TEST_F(CameraTest, PanMovesCameraAndTarget) {
    glm::vec3 originalPos = camera.getPosition();
    glm::vec3 originalTarget = camera.getTarget();
    
    camera.pan(1.0f, 0.0f);  // Pan right
    
    glm::vec3 newPos = camera.getPosition();
    glm::vec3 newTarget = camera.getTarget();
    
    // Both should have moved
    EXPECT_NE(originalPos, newPos);
    EXPECT_NE(originalTarget, newTarget);
}

TEST_F(CameraTest, AspectRatioAffectsProjection) {
    camera.setPerspective(45.0f, 16.0f/9.0f, 0.1f, 100.0f);
    glm::mat4 proj16x9 = camera.getProjectionMatrix();
    
    camera.setAspectRatio(4.0f/3.0f);
    glm::mat4 proj4x3 = camera.getProjectionMatrix();
    
    // Projections should be different
    EXPECT_NE(proj16x9[0][0], proj4x3[0][0]);
}

} // namespace test
} // namespace vde
