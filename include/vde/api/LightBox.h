#pragma once

/**
 * @file LightBox.h
 * @brief Lighting system for VDE games
 * 
 * Provides lighting classes for scene illumination including
 * simple ambient lighting and more complex lighting setups.
 */

#include "GameTypes.h"
#include <glm/glm.hpp>
#include <vector>
#include <memory>

namespace vde {

/**
 * @brief Type of light source.
 */
enum class LightType {
    Directional,  ///< Parallel light rays (like the sun)
    Point,        ///< Light emanating from a point
    Spot          ///< Cone-shaped light
};

/**
 * @brief Represents a single light source.
 */
struct Light {
    LightType type = LightType::Directional;
    Position position;           ///< Position for point/spot lights
    Direction direction;         ///< Direction for directional/spot lights
    Color color = Color::white();
    float intensity = 1.0f;
    float range = 10.0f;         ///< Range for point/spot lights
    float spotAngle = 45.0f;     ///< Inner cone angle for spot lights
    float spotOuterAngle = 60.0f; ///< Outer cone angle for spot lights
    bool castsShadows = false;

    static Light directional(const Direction& dir, const Color& color = Color::white(), float intensity = 1.0f) {
        Light light;
        light.type = LightType::Directional;
        light.direction = dir;
        light.color = color;
        light.intensity = intensity;
        return light;
    }

    static Light point(const Position& pos, const Color& color = Color::white(), float intensity = 1.0f, float range = 10.0f) {
        Light light;
        light.type = LightType::Point;
        light.position = pos;
        light.color = color;
        light.intensity = intensity;
        light.range = range;
        return light;
    }

    static Light spot(const Position& pos, const Direction& dir, float angle = 45.0f, const Color& color = Color::white(), float intensity = 1.0f) {
        Light light;
        light.type = LightType::Spot;
        light.position = pos;
        light.direction = dir;
        light.spotAngle = angle;
        light.color = color;
        light.intensity = intensity;
        return light;
    }
};

/**
 * @brief Base class for lighting configurations.
 * 
 * A LightBox defines the lighting environment for a scene,
 * including ambient light and individual light sources.
 */
class LightBox {
public:
    virtual ~LightBox() = default;

    /**
     * @brief Set the ambient light color.
     */
    void setAmbientColor(const Color& color) { m_ambientColor = color; }

    /**
     * @brief Get the ambient light color.
     */
    const Color& getAmbientColor() const { return m_ambientColor; }

    /**
     * @brief Set the ambient light intensity.
     */
    void setAmbientIntensity(float intensity) { m_ambientIntensity = intensity; }

    /**
     * @brief Get the ambient light intensity.
     */
    float getAmbientIntensity() const { return m_ambientIntensity; }

    /**
     * @brief Add a light to the scene.
     * @param light The light to add
     * @return Index of the added light
     */
    size_t addLight(const Light& light);

    /**
     * @brief Remove a light by index.
     */
    void removeLight(size_t index);

    /**
     * @brief Get all lights.
     */
    const std::vector<Light>& getLights() const { return m_lights; }

    /**
     * @brief Get a light by index.
     */
    Light& getLight(size_t index) { return m_lights[index]; }
    const Light& getLight(size_t index) const { return m_lights[index]; }

    /**
     * @brief Get the number of lights.
     */
    size_t getLightCount() const { return m_lights.size(); }

    /**
     * @brief Clear all lights.
     */
    void clearLights() { m_lights.clear(); }

protected:
    Color m_ambientColor = Color(0.1f, 0.1f, 0.1f);
    float m_ambientIntensity = 1.0f;
    std::vector<Light> m_lights;
};

/**
 * @brief Simple lighting with just ambient color.
 * 
 * Use this for scenes that don't need complex lighting,
 * such as 2D games or stylized graphics.
 */
class SimpleColorLightBox : public LightBox {
public:
    SimpleColorLightBox() = default;
    
    /**
     * @brief Create with a specific ambient color.
     */
    explicit SimpleColorLightBox(const Color& ambientColor) {
        m_ambientColor = ambientColor;
        m_ambientIntensity = 1.0f;
    }

    /**
     * @brief Create with ambient color and a single directional light.
     */
    SimpleColorLightBox(const Color& ambientColor, const Light& mainLight) {
        m_ambientColor = ambientColor;
        m_ambientIntensity = 1.0f;
        addLight(mainLight);
    }
};

/**
 * @brief Standard three-point lighting setup.
 * 
 * Creates a professional-looking lighting setup with:
 * - Key light (main light)
 * - Fill light (softens shadows)
 * - Back light (rim/separation light)
 */
class ThreePointLightBox : public LightBox {
public:
    /**
     * @brief Create a three-point lighting setup.
     * @param keyColor Color of the main light
     * @param keyIntensity Intensity of the main light
     */
    ThreePointLightBox(const Color& keyColor = Color::white(), float keyIntensity = 1.0f);

    /**
     * @brief Get the key light.
     */
    Light& getKeyLight() { return m_lights[0]; }

    /**
     * @brief Get the fill light.
     */
    Light& getFillLight() { return m_lights[1]; }

    /**
     * @brief Get the back light.
     */
    Light& getBackLight() { return m_lights[2]; }
};

} // namespace vde
