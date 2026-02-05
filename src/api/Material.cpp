/**
 * @file Material.cpp
 * @brief Implementation of Material class
 */

#include <vde/Texture.h>
#include <vde/api/Material.h>

namespace vde {

// ============================================================================
// Material Implementation
// ============================================================================

Material::Material(const Color& albedo) : m_albedo(albedo) {}

Material::Material(const Color& albedo, float roughness, float metallic)
    : m_albedo(albedo), m_roughness(glm::clamp(roughness, 0.0f, 1.0f)),
      m_metallic(glm::clamp(metallic, 0.0f, 1.0f)) {}

Material::GPUData Material::getGPUData() const {
    GPUData data{};
    data.albedo = glm::vec4(m_albedo.r, m_albedo.g, m_albedo.b, m_opacity);
    data.emission = glm::vec4(m_emission.r, m_emission.g, m_emission.b, m_emissionIntensity);
    data.roughness = m_roughness;
    data.metallic = m_metallic;
    data.normalStrength = m_normalStrength;
    data.padding = 0.0f;
    return data;
}

// ============================================================================
// Factory Methods
// ============================================================================

Material::Ref Material::createDefault() {
    return std::make_shared<Material>();
}

Material::Ref Material::createColored(const Color& color) {
    auto material = std::make_shared<Material>();
    material->setAlbedo(color);
    material->setRoughness(0.5f);
    material->setMetallic(0.0f);
    return material;
}

Material::Ref Material::createMetallic(const Color& color, float roughness) {
    auto material = std::make_shared<Material>();
    material->setAlbedo(color);
    material->setRoughness(roughness);
    material->setMetallic(1.0f);
    return material;
}

Material::Ref Material::createEmissive(const Color& color, float intensity) {
    auto material = std::make_shared<Material>();
    material->setAlbedo(Color::black());
    material->setEmission(color);
    material->setEmissionIntensity(intensity);
    material->setRoughness(1.0f);
    material->setMetallic(0.0f);
    return material;
}

Material::Ref Material::createGlass(const Color& tint, float opacity) {
    auto material = std::make_shared<Material>();
    material->setAlbedo(tint);
    material->setOpacity(opacity);
    material->setRoughness(0.0f);
    material->setMetallic(0.0f);
    return material;
}

}  // namespace vde
