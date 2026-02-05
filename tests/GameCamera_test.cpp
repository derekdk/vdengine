/**
 * @file GameCamera_test.cpp
 * @brief Unit tests for GameCamera classes (Phase 1)
 *
 * Tests SimpleCamera, OrbitCamera, and Camera2D classes.
 */

#include <vde/api/GameCamera.h>

#include <cmath>

#include <gtest/gtest.h>

using namespace vde;

// ============================================================================
// SimpleCamera Tests
// ============================================================================

class SimpleCameraTest : public ::testing::Test {
  protected:
    std::unique_ptr<SimpleCamera> camera;

    void SetUp() override { camera = std::make_unique<SimpleCamera>(); }
};

TEST_F(SimpleCameraTest, DefaultConstructor) {
    EXPECT_NE(camera.get(), nullptr);
}

TEST_F(SimpleCameraTest, ConstructorWithPositionAndDirection) {
    Position pos(10.0f, 5.0f, 0.0f);
    Direction dir(0.0f, 0.0f, -1.0f);

    SimpleCamera cam(pos, dir);

    Position result = cam.getPosition();
    EXPECT_FLOAT_EQ(result.x, 10.0f);
    EXPECT_FLOAT_EQ(result.y, 5.0f);
}

TEST_F(SimpleCameraTest, SetPosition) {
    Position pos(100.0f, 50.0f, 25.0f);
    camera->setPosition(pos);

    Position result = camera->getPosition();
    EXPECT_FLOAT_EQ(result.x, 100.0f);
    EXPECT_FLOAT_EQ(result.y, 50.0f);
    EXPECT_FLOAT_EQ(result.z, 25.0f);
}

TEST_F(SimpleCameraTest, SetDirection) {
    Direction dir(1.0f, 0.0f, 0.0f);
    camera->setDirection(dir);

    // Direction affects view matrix - the matrix should change
    glm::mat4 view = camera->getViewMatrix();
    // View matrix should be valid (non-zero elements somewhere)
    bool hasNonZero = false;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            if (view[i][j] != 0.0f)
                hasNonZero = true;
        }
    }
    EXPECT_TRUE(hasNonZero);
}

TEST_F(SimpleCameraTest, SetFieldOfView) {
    camera->setFieldOfView(90.0f);
    EXPECT_FLOAT_EQ(camera->getFieldOfView(), 90.0f);
}

TEST_F(SimpleCameraTest, DefaultFieldOfView) {
    EXPECT_FLOAT_EQ(camera->getFieldOfView(), 60.0f);
}

TEST_F(SimpleCameraTest, MoveAddsToPosition) {
    camera->setPosition(Position(0.0f, 0.0f, 0.0f));
    camera->move(Direction(5.0f, 3.0f, 1.0f));

    Position result = camera->getPosition();
    EXPECT_FLOAT_EQ(result.x, 5.0f);
    EXPECT_FLOAT_EQ(result.y, 3.0f);
    EXPECT_FLOAT_EQ(result.z, 1.0f);
}

TEST_F(SimpleCameraTest, SetAspectRatio) {
    camera->setAspectRatio(16.0f / 9.0f);
    EXPECT_NEAR(camera->getAspectRatio(), 16.0f / 9.0f, 0.001f);
}

TEST_F(SimpleCameraTest, GetViewMatrix) {
    glm::mat4 view = camera->getViewMatrix();
    // View matrix should be valid (not all zeros)
    bool hasNonZero = false;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            if (view[i][j] != 0.0f)
                hasNonZero = true;
        }
    }
    EXPECT_TRUE(hasNonZero);
}

TEST_F(SimpleCameraTest, GetProjectionMatrix) {
    glm::mat4 proj = camera->getProjectionMatrix();
    // Projection matrix should be valid
    EXPECT_NE(proj[0][0], 0.0f);
}

TEST_F(SimpleCameraTest, GetViewProjectionMatrix) {
    glm::mat4 vp = camera->getViewProjectionMatrix();
    // VP matrix should be valid
    EXPECT_NE(vp[0][0], 0.0f);
}

// ============================================================================
// OrbitCamera Tests
// ============================================================================

class OrbitCameraTest : public ::testing::Test {
  protected:
    std::unique_ptr<OrbitCamera> camera;

    void SetUp() override {
        camera = std::make_unique<OrbitCamera>(Position(0.0f, 0.0f, 0.0f),  // target
                                               10.0f,                       // distance
                                               45.0f,                       // pitch
                                               0.0f                         // yaw
        );
    }
};

TEST_F(OrbitCameraTest, DefaultConstructor) {
    OrbitCamera cam;
    EXPECT_GT(cam.getDistance(), 0.0f);
}

TEST_F(OrbitCameraTest, ConstructorWithParams) {
    EXPECT_FLOAT_EQ(camera->getDistance(), 10.0f);
    EXPECT_FLOAT_EQ(camera->getPitch(), 45.0f);
    EXPECT_FLOAT_EQ(camera->getYaw(), 0.0f);
}

TEST_F(OrbitCameraTest, SetTarget) {
    camera->setTarget(Position(10.0f, 5.0f, 0.0f));
    Position target = camera->getTarget();

    EXPECT_FLOAT_EQ(target.x, 10.0f);
    EXPECT_FLOAT_EQ(target.y, 5.0f);
}

TEST_F(OrbitCameraTest, SetDistance) {
    camera->setDistance(20.0f);
    EXPECT_FLOAT_EQ(camera->getDistance(), 20.0f);
}

TEST_F(OrbitCameraTest, SetDistanceClampsMin) {
    camera->setZoomLimits(5.0f, 50.0f);
    camera->setDistance(1.0f);  // Below minimum
    EXPECT_FLOAT_EQ(camera->getDistance(), 5.0f);
}

TEST_F(OrbitCameraTest, SetDistanceClampsMax) {
    camera->setZoomLimits(5.0f, 50.0f);
    camera->setDistance(100.0f);  // Above maximum
    EXPECT_FLOAT_EQ(camera->getDistance(), 50.0f);
}

TEST_F(OrbitCameraTest, SetPitch) {
    camera->setPitch(30.0f);
    EXPECT_FLOAT_EQ(camera->getPitch(), 30.0f);
}

TEST_F(OrbitCameraTest, SetPitchClampsMin) {
    camera->setPitchLimits(10.0f, 80.0f);
    camera->setPitch(5.0f);  // Below minimum
    EXPECT_FLOAT_EQ(camera->getPitch(), 10.0f);
}

TEST_F(OrbitCameraTest, SetPitchClampsMax) {
    camera->setPitchLimits(10.0f, 80.0f);
    camera->setPitch(85.0f);  // Above maximum
    EXPECT_FLOAT_EQ(camera->getPitch(), 80.0f);
}

TEST_F(OrbitCameraTest, SetYaw) {
    camera->setYaw(90.0f);
    EXPECT_FLOAT_EQ(camera->getYaw(), 90.0f);
}

TEST_F(OrbitCameraTest, SetFieldOfView) {
    camera->setFieldOfView(75.0f);
    // No direct getter in OrbitCamera, but it should not crash
}

TEST_F(OrbitCameraTest, RotateUpdatesPitchYaw) {
    float initialPitch = camera->getPitch();
    float initialYaw = camera->getYaw();

    camera->rotate(10.0f, 15.0f);

    EXPECT_FLOAT_EQ(camera->getPitch(), initialPitch + 10.0f);
    EXPECT_FLOAT_EQ(camera->getYaw(), initialYaw + 15.0f);
}

TEST_F(OrbitCameraTest, ZoomChangesDistance) {
    float initialDist = camera->getDistance();
    camera->zoom(-2.0f);  // Zoom out
    EXPECT_GT(camera->getDistance(), initialDist);

    camera->zoom(2.0f);  // Zoom in
    EXPECT_FLOAT_EQ(camera->getDistance(), initialDist);
}

TEST_F(OrbitCameraTest, PanMovesTarget) {
    Position initialTarget = camera->getTarget();
    camera->pan(1.0f, 1.0f);
    Position newTarget = camera->getTarget();

    // Target should have moved
    EXPECT_NE(newTarget.x, initialTarget.x);
}

TEST_F(OrbitCameraTest, GetViewMatrix) {
    glm::mat4 view = camera->getViewMatrix();
    // View matrix should be valid
    bool hasNonZero = false;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            if (view[i][j] != 0.0f)
                hasNonZero = true;
        }
    }
    EXPECT_TRUE(hasNonZero);
}

// ============================================================================
// Camera2D Tests
// ============================================================================

class Camera2DTest : public ::testing::Test {
  protected:
    std::unique_ptr<Camera2D> camera;

    void SetUp() override {
        camera = std::make_unique<Camera2D>(16.0f, 9.0f);  // 16x9 viewport
    }
};

TEST_F(Camera2DTest, DefaultConstructor) {
    Camera2D cam;
    EXPECT_FLOAT_EQ(cam.getZoom(), 1.0f);
    EXPECT_FLOAT_EQ(cam.getRotation(), 0.0f);
}

TEST_F(Camera2DTest, ConstructorWithSize) {
    glm::vec2 pos = camera->getPosition();
    EXPECT_FLOAT_EQ(pos.x, 0.0f);
    EXPECT_FLOAT_EQ(pos.y, 0.0f);
}

TEST_F(Camera2DTest, SetPositionFloats) {
    camera->setPosition(10.0f, 5.0f);
    glm::vec2 pos = camera->getPosition();

    EXPECT_FLOAT_EQ(pos.x, 10.0f);
    EXPECT_FLOAT_EQ(pos.y, 5.0f);
}

TEST_F(Camera2DTest, SetPositionStruct) {
    Position pos(20.0f, 15.0f, 0.0f);
    camera->setPosition(pos);
    glm::vec2 result = camera->getPosition();

    EXPECT_FLOAT_EQ(result.x, 20.0f);
    EXPECT_FLOAT_EQ(result.y, 15.0f);
}

TEST_F(Camera2DTest, SetZoom) {
    camera->setZoom(2.0f);
    EXPECT_FLOAT_EQ(camera->getZoom(), 2.0f);
}

TEST_F(Camera2DTest, SetRotation) {
    camera->setRotation(45.0f);
    EXPECT_FLOAT_EQ(camera->getRotation(), 45.0f);
}

TEST_F(Camera2DTest, SetViewportSize) {
    camera->setViewportSize(1920.0f, 1080.0f);
    // Should not crash, affects projection
}

TEST_F(Camera2DTest, MoveAddsToPosition) {
    camera->setPosition(0.0f, 0.0f);
    camera->move(5.0f, 3.0f);

    glm::vec2 pos = camera->getPosition();
    EXPECT_FLOAT_EQ(pos.x, 5.0f);
    EXPECT_FLOAT_EQ(pos.y, 3.0f);
}

TEST_F(Camera2DTest, ProjectionIsOrthographic) {
    glm::mat4 proj = camera->getProjectionMatrix();

    // Orthographic projection has specific characteristics:
    // proj[3][3] should be 1.0 (unlike perspective which is 0)
    EXPECT_FLOAT_EQ(proj[3][3], 1.0f);
}

TEST_F(Camera2DTest, GetViewMatrix) {
    glm::mat4 view = camera->getViewMatrix();
    // View matrix should be valid
    bool hasNonZero = false;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            if (view[i][j] != 0.0f)
                hasNonZero = true;
        }
    }
    EXPECT_TRUE(hasNonZero);
}
