#pragma once

/**
 * @file Material.h
 * @brief Material system for VDE games
 *
 * Provides material classes for defining surface properties of rendered
 * objects. Materials control how objects interact with light including
 * albedo color, roughness, metallic, and emissive properties.
 */

#include <glm/glm.hpp>

#include <memory>
#include <string>

#include "GameTypes.h"

namespace vde {

// Forward declarations
class Texture;

/**
 * @brief Defines the visual surface properties of a mesh.
 *
 * Material controls how an object's surface responds to lighting:
 * - Albedo: Base color of the surface
 * - Roughness: How rough/smooth the surface is (0=mirror, 1=rough)
 * - Metallic: How metallic the surface is (0=dielectric, 1=metal)
 * - Emission: Self-illumination color and intensity
 *
 * Example usage:
 * @code
 * auto material = std::make_shared<Material>();
 * material->setAlbedo(Color::red());
 * material->setRoughness(0.3f);
 * material->setMetallic(0.0f);
 * meshEntity->setMaterial(material);
 * @endcode
 */
class Material {
  public:
    using Ref = std::shared_ptr<Material>;

    Material() = default;
    virtual ~Material() = default;

    /**
     * @brief Create a material with albedo color.
     * @param albedo Base color of the material
     */
    explicit Material(const Color& albedo);

    /**
     * @brief Create a material with full PBR properties.
     * @param albedo Base color
     * @param roughness Surface roughness (0-1)
     * @param metallic Metallic factor (0-1)
     */
    Material(const Color& albedo, float roughness, float metallic);

    // =========================================================================
    // Albedo (Base Color)
    // =========================================================================

    /**
     * @brief Set the albedo (base) color.
     * @param color The base color of the material
     */
    void setAlbedo(const Color& color) { m_albedo = color; }

    /**
     * @brief Get the albedo color.
     */
    const Color& getAlbedo() const { return m_albedo; }

    /**
     * @brief Set the albedo texture.
     * @param texture Texture to use for albedo, or nullptr to use color only
     */
    void setAlbedoTexture(std::shared_ptr<Texture> texture) {
        m_albedoTexture = std::move(texture);
    }

    /**
     * @brief Get the albedo texture.
     */
    std::shared_ptr<Texture> getAlbedoTexture() const { return m_albedoTexture; }

    /**
     * @brief Check if material has an albedo texture.
     */
    bool hasAlbedoTexture() const { return m_albedoTexture != nullptr; }

    // =========================================================================
    // Roughness
    // =========================================================================

    /**
     * @brief Set the roughness factor.
     * @param roughness Value from 0 (smooth/mirror) to 1 (rough/diffuse)
     */
    void setRoughness(float roughness) { m_roughness = glm::clamp(roughness, 0.0f, 1.0f); }

    /**
     * @brief Get the roughness factor.
     */
    float getRoughness() const { return m_roughness; }

    // =========================================================================
    // Metallic
    // =========================================================================

    /**
     * @brief Set the metallic factor.
     * @param metallic Value from 0 (dielectric) to 1 (metal)
     */
    void setMetallic(float metallic) { m_metallic = glm::clamp(metallic, 0.0f, 1.0f); }

    /**
     * @brief Get the metallic factor.
     */
    float getMetallic() const { return m_metallic; }

    // =========================================================================
    // Emission
    // =========================================================================

    /**
     * @brief Set the emission color.
     * @param color Emission color (RGB values > 1 for HDR glow)
     */
    void setEmission(const Color& color) { m_emission = color; }

    /**
     * @brief Get the emission color.
     */
    const Color& getEmission() const { return m_emission; }

    /**
     * @brief Set the emission intensity multiplier.
     * @param intensity Intensity multiplier for emission
     */
    void setEmissionIntensity(float intensity) { m_emissionIntensity = intensity; }

    /**
     * @brief Get the emission intensity.
     */
    float getEmissionIntensity() const { return m_emissionIntensity; }

    /**
     * @brief Check if material is emissive.
     */
    bool isEmissive() const { return m_emissionIntensity > 0.0f; }

    // =========================================================================
    // Normal Mapping
    // =========================================================================

    /**
     * @brief Set the normal map texture.
     * @param texture Normal map texture for surface detail
     */
    void setNormalMap(std::shared_ptr<Texture> texture) { m_normalMap = std::move(texture); }

    /**
     * @brief Get the normal map texture.
     */
    std::shared_ptr<Texture> getNormalMap() const { return m_normalMap; }

    /**
     * @brief Check if material has a normal map.
     */
    bool hasNormalMap() const { return m_normalMap != nullptr; }

    /**
     * @brief Set the normal map strength.
     * @param strength Scale factor for normal map effect (0-2, default 1)
     */
    void setNormalStrength(float strength) { m_normalStrength = strength; }

    /**
     * @brief Get the normal map strength.
     */
    float getNormalStrength() const { return m_normalStrength; }

    // =========================================================================
    // Additional Properties
    // =========================================================================

    /**
     * @brief Set whether the material receives shadows.
     */
    void setReceivesShadows(bool receives) { m_receivesShadows = receives; }

    /**
     * @brief Check if material receives shadows.
     */
    bool receivesShadows() const { return m_receivesShadows; }

    /**
     * @brief Set whether the material casts shadows.
     */
    void setCastsShadows(bool casts) { m_castsShadows = casts; }

    /**
     * @brief Check if material casts shadows.
     */
    bool castsShadows() const { return m_castsShadows; }

    /**
     * @brief Set the opacity of the material.
     * @param opacity Value from 0 (transparent) to 1 (opaque)
     */
    void setOpacity(float opacity) { m_opacity = glm::clamp(opacity, 0.0f, 1.0f); }

    /**
     * @brief Get the opacity.
     */
    float getOpacity() const { return m_opacity; }

    /**
     * @brief Check if material is transparent (opacity < 1).
     */
    bool isTransparent() const { return m_opacity < 1.0f; }

    // =========================================================================
    // GPU Data
    // =========================================================================

    /**
     * @brief Material data packed for GPU push constants.
     *
     * This structure is designed to be pushed to the shader as push constants.
     * Size: 48 bytes (fits within typical 128-byte push constant limit)
     */
    struct GPUData {
        glm::vec4 albedo;      ///< RGB albedo + opacity
        glm::vec4 emission;    ///< RGB emission + intensity
        float roughness;       ///< Surface roughness
        float metallic;        ///< Metallic factor
        float normalStrength;  ///< Normal map strength
        float padding;         ///< Padding for alignment
    };

    /**
     * @brief Get material data packed for GPU.
     * @return GPUData structure ready for push constants
     */
    GPUData getGPUData() const;

    // =========================================================================
    // Factory Methods
    // =========================================================================

    /**
     * @brief Create a default white material.
     */
    static Ref createDefault();

    /**
     * @brief Create a simple colored material.
     * @param color Base color
     */
    static Ref createColored(const Color& color);

    /**
     * @brief Create a metallic material.
     * @param color Base color
     * @param roughness Surface roughness
     */
    static Ref createMetallic(const Color& color, float roughness = 0.3f);

    /**
     * @brief Create an emissive material.
     * @param color Emission color
     * @param intensity Emission intensity
     */
    static Ref createEmissive(const Color& color, float intensity = 1.0f);

    /**
     * @brief Create a glass-like transparent material.
     * @param tint Color tint
     * @param opacity Opacity (default 0.3)
     */
    static Ref createGlass(const Color& tint = Color::white(), float opacity = 0.3f);

  protected:
    // Base properties
    Color m_albedo = Color::white();
    float m_roughness = 0.5f;
    float m_metallic = 0.0f;
    float m_opacity = 1.0f;

    // Emission
    Color m_emission = Color::black();
    float m_emissionIntensity = 0.0f;

    // Textures
    std::shared_ptr<Texture> m_albedoTexture;
    std::shared_ptr<Texture> m_normalMap;
    float m_normalStrength = 1.0f;

    // Shadow properties
    bool m_receivesShadows = true;
    bool m_castsShadows = true;
};

}  // namespace vde
