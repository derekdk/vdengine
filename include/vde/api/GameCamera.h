#pragma once

/**
 * @file GameCamera.h
 * @brief Simplified camera classes for VDE games
 * 
 * Provides easy-to-use camera classes that wrap the engine's
 * camera functionality for common game scenarios.
 */

#include "GameTypes.h"
#include <vde/Camera.h>
#include <memory>

namespace vde {

/**
 * @brief Base class for game cameras.
 * 
 * Provides a simplified interface for camera control in games.
 */
class GameCamera {
public:
    virtual ~GameCamera() = default;

    /**
     * @brief Get the underlying engine camera.
     */
    Camera& getCamera() { return m_camera; }
    const Camera& getCamera() const { return m_camera; }

    /**
     * @brief Get the view matrix.
     */
    glm::mat4 getViewMatrix() const { return m_camera.getViewMatrix(); }

    /**
     * @brief Get the projection matrix.
     */
    glm::mat4 getProjectionMatrix() const { return m_camera.getProjectionMatrix(); }

    /**
     * @brief Get the combined view-projection matrix.
     */
    glm::mat4 getViewProjectionMatrix() const { return m_camera.getViewProjectionMatrix(); }

    /**
     * @brief Set the camera's aspect ratio.
     */
    void setAspectRatio(float aspect) { m_aspectRatio = aspect; updateProjection(); }

    /**
     * @brief Set the near clipping plane.
     */
    void setNearPlane(float near) { m_nearPlane = near; updateProjection(); }

    /**
     * @brief Set the far clipping plane.
     */
    void setFarPlane(float far) { m_farPlane = far; updateProjection(); }

    /**
     * @brief Update camera (called once per frame).
     */
    virtual void update(float deltaTime) {}

protected:
    Camera m_camera;
    float m_aspectRatio = 16.0f / 9.0f;
    float m_nearPlane = 0.1f;
    float m_farPlane = 1000.0f;

    virtual void updateProjection() {}
};

/**
 * @brief Simple perspective camera with position and direction.
 * 
 * Use this for first-person style games or when you need
 * direct control over camera position and orientation.
 */
class SimpleCamera : public GameCamera {
public:
    SimpleCamera();
    
    /**
     * @brief Create camera at position looking in direction.
     * @param position Camera position in world space
     * @param direction Direction the camera is looking
     */
    SimpleCamera(const Position& position, const Direction& direction);

    /**
     * @brief Set the camera position.
     */
    void setPosition(const Position& position);

    /**
     * @brief Get the camera position.
     */
    Position getPosition() const;

    /**
     * @brief Set the direction the camera is looking.
     */
    void setDirection(const Direction& direction);

    /**
     * @brief Get the look direction.
     */
    Direction getDirection() const;

    /**
     * @brief Set the field of view in degrees.
     */
    void setFieldOfView(float fov);

    /**
     * @brief Get the field of view.
     */
    float getFieldOfView() const { return m_fov; }

    /**
     * @brief Move the camera by a delta.
     */
    void move(const Direction& delta);

    /**
     * @brief Rotate the camera by pitch and yaw (degrees).
     */
    void rotate(float deltaPitch, float deltaYaw);

protected:
    void updateProjection() override;

private:
    Position m_position;
    float m_pitch = 0.0f;
    float m_yaw = -90.0f;
    float m_fov = 60.0f;

    void updateVectors();
};

/**
 * @brief Orbital camera that rotates around a target point.
 * 
 * Use this for third-person games, RTS cameras, or any
 * situation where the camera orbits around a focal point.
 */
class OrbitCamera : public GameCamera {
public:
    OrbitCamera();

    /**
     * @brief Create orbital camera.
     * @param target Point to orbit around
     * @param distance Distance from target
     * @param pitch Vertical angle in degrees
     * @param yaw Horizontal angle in degrees
     */
    OrbitCamera(const Position& target, float distance, float pitch = 45.0f, float yaw = 0.0f);

    /**
     * @brief Set the point to orbit around.
     */
    void setTarget(const Position& target);

    /**
     * @brief Get the orbit target.
     */
    Position getTarget() const;

    /**
     * @brief Set distance from target.
     */
    void setDistance(float distance);

    /**
     * @brief Get distance from target.
     */
    float getDistance() const { return m_distance; }

    /**
     * @brief Set the pitch angle (degrees).
     */
    void setPitch(float pitch);

    /**
     * @brief Get the pitch angle.
     */
    float getPitch() const { return m_pitch; }

    /**
     * @brief Set the yaw angle (degrees).
     */
    void setYaw(float yaw);

    /**
     * @brief Get the yaw angle.
     */
    float getYaw() const { return m_yaw; }

    /**
     * @brief Set field of view in degrees.
     */
    void setFieldOfView(float fov);

    /**
     * @brief Rotate around target.
     * @param deltaPitch Change in pitch (degrees)
     * @param deltaYaw Change in yaw (degrees)
     */
    void rotate(float deltaPitch, float deltaYaw);

    /**
     * @brief Zoom in/out.
     * @param delta Positive = zoom in, negative = zoom out
     */
    void zoom(float delta);

    /**
     * @brief Pan the camera (moves target).
     */
    void pan(float deltaX, float deltaY);

    /**
     * @brief Set minimum and maximum zoom distances.
     */
    void setZoomLimits(float minDistance, float maxDistance);

    /**
     * @brief Set minimum and maximum pitch angles.
     */
    void setPitchLimits(float minPitch, float maxPitch);

protected:
    void updateProjection() override;

private:
    Position m_target;
    float m_distance = 10.0f;
    float m_pitch = 45.0f;
    float m_yaw = 0.0f;
    float m_fov = 60.0f;

    float m_minDistance = 1.0f;
    float m_maxDistance = 100.0f;
    float m_minPitch = 5.0f;
    float m_maxPitch = 85.0f;

    void updateCamera();
};

/**
 * @brief 2D orthographic camera.
 * 
 * Use this for 2D games, UI rendering, or top-down views.
 */
class Camera2D : public GameCamera {
public:
    Camera2D();

    /**
     * @brief Create 2D camera with viewport size.
     * @param width Viewport width in world units
     * @param height Viewport height in world units
     */
    Camera2D(float width, float height);

    /**
     * @brief Set the camera position (center point).
     */
    void setPosition(float x, float y);
    void setPosition(const Position& pos);

    /**
     * @brief Get the camera position.
     */
    glm::vec2 getPosition() const { return glm::vec2(m_position.x, m_position.y); }

    /**
     * @brief Set the zoom level (1.0 = normal).
     */
    void setZoom(float zoom);

    /**
     * @brief Get the zoom level.
     */
    float getZoom() const { return m_zoom; }

    /**
     * @brief Set the rotation in degrees.
     */
    void setRotation(float degrees);

    /**
     * @brief Get the rotation in degrees.
     */
    float getRotation() const { return m_rotation; }

    /**
     * @brief Set the viewport size.
     */
    void setViewportSize(float width, float height);

    /**
     * @brief Move the camera by a delta.
     */
    void move(float deltaX, float deltaY);

protected:
    void updateProjection() override;

private:
    Position m_position;
    float m_zoom = 1.0f;
    float m_rotation = 0.0f;
    float m_viewportWidth = 1920.0f;
    float m_viewportHeight = 1080.0f;

    void updateCamera();
};

} // namespace vde
