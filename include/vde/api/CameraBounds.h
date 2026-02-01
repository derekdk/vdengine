#pragma once

/**
 * @file CameraBounds.h
 * @brief Camera bounds and pixel-to-world coordinate mapping
 * 
 * Provides types for defining camera view bounds in 2D games and
 * converting between screen (pixel) coordinates and world coordinates.
 * 
 * @example
 * @code
 * using namespace vde;
 * 
 * // Setup camera for 1920x1080 screen showing 16 meters of world width
 * CameraBounds2D camera;
 * camera.setScreenSize(1920_px, 1080_px);
 * camera.setWorldWidth(16_m);
 * camera.centerOn(0_m, 0_m);
 * 
 * // Convert mouse position to world coordinates
 * glm::vec2 worldPos = camera.screenToWorld(mouseX_px, mouseY_px);
 * 
 * // Check if entity is visible
 * if (camera.isVisible(entity.getPosition().x, entity.getPosition().y)) {
 *     // Render entity
 * }
 * @endcode
 */

#include "WorldUnits.h"
#include "WorldBounds.h"
#include <glm/glm.hpp>

namespace vde {

/**
 * @brief Pixel coordinate (screen space).
 * 
 * Type-safe wrapper for pixel values to distinguish from world units.
 */
struct Pixels {
    float value;
    
    constexpr Pixels() : value(0.0f) {}
    constexpr Pixels(float v) : value(v) {}
    constexpr Pixels(int v) : value(static_cast<float>(v)) {}
    constexpr Pixels(uint32_t v) : value(static_cast<float>(v)) {}
    constexpr operator float() const { return value; }
    
    constexpr Pixels operator-() const { return Pixels(-value); }
    constexpr Pixels operator+(Pixels other) const { return Pixels(value + other.value); }
    constexpr Pixels operator-(Pixels other) const { return Pixels(value - other.value); }
    constexpr Pixels operator*(float scalar) const { return Pixels(value * scalar); }
    constexpr Pixels operator/(float scalar) const { return Pixels(value / scalar); }
};

/// User-defined literal for pixels (e.g., 1920_px)
constexpr Pixels operator""_px(long double v) { return Pixels(static_cast<float>(v)); }
/// User-defined literal for pixels from integer (e.g., 1920_px)
constexpr Pixels operator""_px(unsigned long long v) { return Pixels(static_cast<float>(v)); }

/**
 * @brief Screen dimensions in pixels.
 */
struct ScreenSize {
    Pixels width;
    Pixels height;
    
    ScreenSize() : width(1920.0f), height(1080.0f) {}
    ScreenSize(Pixels w, Pixels h) : width(w), height(h) {}
    ScreenSize(uint32_t w, uint32_t h) 
        : width(static_cast<float>(w)), height(static_cast<float>(h)) {}
    
    /// @return Width divided by height
    float aspectRatio() const { return width.value / height.value; }
};

/**
 * @brief Defines how screen pixels map to world units.
 * 
 * Used for 2D games to establish the relationship between
 * screen coordinates and world coordinates. This mapping
 * determines the effective "zoom level" of the camera.
 * 
 * @example
 * @code
 * // 100 pixels = 1 meter (default)
 * PixelToWorldMapping mapping = PixelToWorldMapping::fromPixelsPerMeter(100.0f);
 * 
 * // Fit 20 meters across a 1920 pixel wide screen
 * PixelToWorldMapping fit = PixelToWorldMapping::fitWidth(20_m, 1920_px);
 * 
 * // Convert coordinates
 * Meters worldDist = mapping.toWorld(500_px);  // 5 meters
 * Pixels screenDist = mapping.toPixels(10_m);  // 1000 pixels
 * @endcode
 */
struct PixelToWorldMapping {
    Meters metersPerPixel;  ///< World meters per screen pixel
    
    /// Default: 100 pixels = 1 meter
    PixelToWorldMapping() : metersPerPixel(1.0f / 100.0f) {}
    explicit PixelToWorldMapping(Meters mpp) : metersPerPixel(mpp) {}
    
    /**
     * @brief Create mapping from pixels-per-meter ratio.
     * 
     * @param ppm Number of pixels that represent one meter
     * @return PixelToWorldMapping with the specified ratio
     */
    static PixelToWorldMapping fromPixelsPerMeter(float ppm) {
        return PixelToWorldMapping(Meters(1.0f / ppm));
    }
    
    /**
     * @brief Create mapping to fit world width to screen width.
     * 
     * @param worldWidth World width in meters to display
     * @param screenWidth Screen width in pixels
     * @return PixelToWorldMapping that fits the world width
     */
    static PixelToWorldMapping fitWidth(Meters worldWidth, Pixels screenWidth) {
        return PixelToWorldMapping(Meters(worldWidth.value / screenWidth.value));
    }
    
    /**
     * @brief Create mapping to fit world height to screen height.
     * 
     * @param worldHeight World height in meters to display
     * @param screenHeight Screen height in pixels
     * @return PixelToWorldMapping that fits the world height
     */
    static PixelToWorldMapping fitHeight(Meters worldHeight, Pixels screenHeight) {
        return PixelToWorldMapping(Meters(worldHeight.value / screenHeight.value));
    }
    
    // Conversion functions
    
    /// @brief Convert pixels to world meters
    Meters toWorld(Pixels px) const { return Meters(px.value * metersPerPixel.value); }
    
    /// @brief Convert world meters to pixels
    Pixels toPixels(Meters m) const { return Pixels(m.value / metersPerPixel.value); }
    
    /// @brief Convert screen position to world position
    glm::vec2 toWorld(const glm::vec2& screenPos) const {
        return glm::vec2(toWorld(Pixels(screenPos.x)), toWorld(Pixels(screenPos.y)));
    }
    
    /// @brief Convert world position to screen position
    glm::vec2 toPixels(const glm::vec2& worldPos) const {
        return glm::vec2(toPixels(Meters(worldPos.x)), toPixels(Meters(worldPos.y)));
    }
    
    /// @return Pixels per meter (inverse of metersPerPixel)
    float getPixelsPerMeter() const { return 1.0f / metersPerPixel.value; }
};

/**
 * @brief Camera bounds for 2D games.
 * 
 * Defines what portion of the world is visible on screen and provides
 * coordinate conversion between screen space and world space.
 * 
 * The camera maintains:
 * - A center position in world space
 * - A visible world width (height derived from screen aspect ratio)
 * - A zoom level (affects visible world size)
 * - Optional constraint bounds to limit camera movement
 * 
 * @example
 * @code
 * using namespace vde;
 * 
 * CameraBounds2D camera;
 * camera.setScreenSize(1920_px, 1080_px);
 * camera.setWorldWidth(16_m);
 * camera.centerOn(0_m, 0_m);
 * 
 * // Constrain camera to stay within world bounds
 * camera.setConstraintBounds(WorldBounds2D::fromCenter(0_m, 0_m, 100_m, 100_m));
 * 
 * // Handle zoom
 * camera.setZoom(2.0f);  // 2x zoom (shows 8m width instead of 16m)
 * 
 * // Convert mouse click to world position
 * glm::vec2 worldPos = camera.screenToWorld(mouseX_px, mouseY_px);
 * @endcode
 */
class CameraBounds2D {
public:
    CameraBounds2D();
    
    // Configuration
    
    /**
     * @brief Set the screen/viewport size in pixels.
     * 
     * @param width Screen width in pixels
     * @param height Screen height in pixels
     */
    void setScreenSize(Pixels width, Pixels height);
    
    /**
     * @brief Set the screen/viewport size.
     */
    void setScreenSize(const ScreenSize& size);
    
    /**
     * @brief Set the visible world width (height derived from aspect ratio).
     * 
     * This is the base width before zoom is applied.
     * Actual visible width = worldWidth / zoom
     * 
     * @param width World width in meters
     */
    void setWorldWidth(Meters width);
    
    /**
     * @brief Set the visible world height (width derived from aspect ratio).
     * 
     * This is the base height before zoom is applied.
     * Actual visible height = worldHeight / zoom
     * 
     * @param height World height in meters
     */
    void setWorldHeight(Meters height);
    
    /**
     * @brief Set zoom level.
     * 
     * @param zoom Zoom multiplier (1.0 = normal, 2.0 = 2x zoom in, 0.5 = zoom out)
     */
    void setZoom(float zoom);
    
    /**
     * @brief Get the current zoom level.
     */
    float getZoom() const { return m_zoom; }
    
    /**
     * @brief Center the camera on a world point.
     * 
     * @param worldX World X coordinate
     * @param worldY World Y coordinate
     */
    void centerOn(Meters worldX, Meters worldY);
    
    /**
     * @brief Center the camera on a world point.
     */
    void centerOn(const glm::vec2& worldPos);
    
    /**
     * @brief Move the camera by a delta in world units.
     * 
     * @param deltaX Movement in X direction (meters)
     * @param deltaY Movement in Y direction (meters)
     */
    void move(Meters deltaX, Meters deltaY);
    
    /**
     * @brief Constrain camera to stay within world bounds.
     * 
     * When set, the camera will not show areas outside these bounds.
     * If the visible area is larger than the constraint bounds,
     * the camera will center on the constraint bounds.
     * 
     * @param bounds World bounds to constrain to
     */
    void setConstraintBounds(const WorldBounds2D& bounds);
    
    /**
     * @brief Remove constraint bounds.
     */
    void clearConstraintBounds();
    
    /**
     * @brief Check if constraint bounds are set.
     */
    bool hasConstraintBounds() const { return m_hasConstraints; }
    
    // Queries
    
    /**
     * @brief Get the current visible world bounds.
     * 
     * @return The rectangular area of the world that is visible on screen
     */
    WorldBounds2D getVisibleBounds() const;
    
    /**
     * @brief Get the pixel-to-world mapping.
     */
    PixelToWorldMapping getMapping() const { return m_mapping; }
    
    /**
     * @brief Get the visible world width (after zoom).
     */
    Meters getVisibleWidth() const;
    
    /**
     * @brief Get the visible world height (after zoom).
     */
    Meters getVisibleHeight() const;
    
    /**
     * @brief Get the screen size.
     */
    ScreenSize getScreenSize() const { return m_screenSize; }
    
    /**
     * @brief Get the camera center position in world space.
     */
    glm::vec2 getCenter() const { return glm::vec2(m_centerX, m_centerY); }
    
    // Coordinate conversion
    
    /**
     * @brief Convert screen coordinates to world coordinates.
     * 
     * Screen origin (0,0) is at top-left, Y increases downward.
     * World coordinates use the game's coordinate system.
     * 
     * @param screenX Screen X in pixels (0 = left)
     * @param screenY Screen Y in pixels (0 = top)
     * @return World position
     */
    glm::vec2 screenToWorld(Pixels screenX, Pixels screenY) const;
    
    /**
     * @brief Convert screen coordinates to world coordinates.
     */
    glm::vec2 screenToWorld(const glm::vec2& screenPos) const;
    
    /**
     * @brief Convert world coordinates to screen coordinates.
     * 
     * @param worldX World X in meters
     * @param worldY World Y in meters
     * @return Screen position in pixels
     */
    glm::vec2 worldToScreen(Meters worldX, Meters worldY) const;
    
    /**
     * @brief Convert world coordinates to screen coordinates.
     */
    glm::vec2 worldToScreen(const glm::vec2& worldPos) const;
    
    // Visibility testing
    
    /**
     * @brief Check if a world point is visible on screen.
     * 
     * @param worldX World X coordinate
     * @param worldY World Y coordinate
     * @return true if the point is within the visible bounds
     */
    bool isVisible(Meters worldX, Meters worldY) const;
    
    /**
     * @brief Check if a world point is visible on screen.
     */
    bool isVisible(const glm::vec2& worldPos) const;
    
    /**
     * @brief Check if any part of a bounds rectangle is visible.
     * 
     * @param bounds World bounds to test
     * @return true if any part of the bounds overlaps visible area
     */
    bool isVisible(const WorldBounds2D& bounds) const;
    
private:
    ScreenSize m_screenSize{1920_px, 1080_px};
    Meters m_centerX{0.0f};
    Meters m_centerY{0.0f};
    Meters m_baseWorldWidth{16.0f};
    float m_zoom{1.0f};
    PixelToWorldMapping m_mapping;
    
    bool m_hasConstraints{false};
    WorldBounds2D m_constraints;
    
    void updateMapping();
    void applyConstraints();
};

} // namespace vde
