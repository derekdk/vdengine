/**
 * @file Material_test.cpp
 * @brief Unit tests for Material class
 */

#include <vde/api/GameTypes.h>
#include <vde/api/Material.h>

#include <gtest/gtest.h>

namespace vde {
namespace {

// ============================================================================
// Construction Tests
// ============================================================================

TEST(MaterialTest, DefaultConstruction) {
    Material material;

    // Check default values - white albedo
    EXPECT_FLOAT_EQ(material.getAlbedo().r, 1.0f);
    EXPECT_FLOAT_EQ(material.getAlbedo().g, 1.0f);
    EXPECT_FLOAT_EQ(material.getAlbedo().b, 1.0f);
    EXPECT_FLOAT_EQ(material.getAlbedo().a, 1.0f);
    EXPECT_FLOAT_EQ(material.getRoughness(), 0.5f);
    EXPECT_FLOAT_EQ(material.getMetallic(), 0.0f);
    EXPECT_FLOAT_EQ(material.getOpacity(), 1.0f);

    // Default should have black emission (no glow)
    Color emissionColor = material.getEmission();
    EXPECT_FLOAT_EQ(emissionColor.r, 0.0f);
    EXPECT_FLOAT_EQ(emissionColor.g, 0.0f);
    EXPECT_FLOAT_EQ(emissionColor.b, 0.0f);
    EXPECT_FLOAT_EQ(material.getEmissionIntensity(), 0.0f);
}

TEST(MaterialTest, ConstructionWithAlbedo) {
    Color albedo(1.0f, 0.0f, 0.0f, 1.0f);  // Red
    Material material(albedo);

    EXPECT_FLOAT_EQ(material.getAlbedo().r, 1.0f);
    EXPECT_FLOAT_EQ(material.getAlbedo().g, 0.0f);
    EXPECT_FLOAT_EQ(material.getAlbedo().b, 0.0f);
    EXPECT_FLOAT_EQ(material.getAlbedo().a, 1.0f);
}

TEST(MaterialTest, ConstructionWithPBRParameters) {
    Color albedo(0.5f, 0.6f, 0.7f, 1.0f);
    Material material(albedo, 0.3f, 0.8f);

    EXPECT_FLOAT_EQ(material.getAlbedo().r, 0.5f);
    EXPECT_FLOAT_EQ(material.getAlbedo().g, 0.6f);
    EXPECT_FLOAT_EQ(material.getAlbedo().b, 0.7f);
    EXPECT_FLOAT_EQ(material.getRoughness(), 0.3f);
    EXPECT_FLOAT_EQ(material.getMetallic(), 0.8f);
}

// ============================================================================
// Setter/Getter Tests
// ============================================================================

TEST(MaterialTest, SetAlbedo) {
    Material material;
    Color newAlbedo(0.2f, 0.4f, 0.6f, 0.9f);

    material.setAlbedo(newAlbedo);

    EXPECT_FLOAT_EQ(material.getAlbedo().r, 0.2f);
    EXPECT_FLOAT_EQ(material.getAlbedo().g, 0.4f);
    EXPECT_FLOAT_EQ(material.getAlbedo().b, 0.6f);
    EXPECT_FLOAT_EQ(material.getAlbedo().a, 0.9f);
}

TEST(MaterialTest, SetRoughness) {
    Material material;

    material.setRoughness(0.75f);
    EXPECT_FLOAT_EQ(material.getRoughness(), 0.75f);

    // Test clamping at min
    material.setRoughness(-0.5f);
    EXPECT_FLOAT_EQ(material.getRoughness(), 0.0f);

    // Test clamping at max
    material.setRoughness(1.5f);
    EXPECT_FLOAT_EQ(material.getRoughness(), 1.0f);
}

TEST(MaterialTest, SetMetallic) {
    Material material;

    material.setMetallic(0.9f);
    EXPECT_FLOAT_EQ(material.getMetallic(), 0.9f);

    // Test clamping at min
    material.setMetallic(-0.2f);
    EXPECT_FLOAT_EQ(material.getMetallic(), 0.0f);

    // Test clamping at max
    material.setMetallic(2.0f);
    EXPECT_FLOAT_EQ(material.getMetallic(), 1.0f);
}

TEST(MaterialTest, SetEmission) {
    Material material;
    Color emissionColor(1.0f, 0.5f, 0.0f, 1.0f);  // Orange

    material.setEmission(emissionColor);

    Color result = material.getEmission();
    EXPECT_FLOAT_EQ(result.r, 1.0f);
    EXPECT_FLOAT_EQ(result.g, 0.5f);
    EXPECT_FLOAT_EQ(result.b, 0.0f);
}

TEST(MaterialTest, SetEmissionIntensity) {
    Material material;

    material.setEmissionIntensity(0.5f);
    EXPECT_FLOAT_EQ(material.getEmissionIntensity(), 0.5f);
    EXPECT_TRUE(material.isEmissive());

    material.setEmissionIntensity(0.0f);
    EXPECT_FALSE(material.isEmissive());

    // Intensity can exceed 1.0 (HDR)
    material.setEmissionIntensity(5.0f);
    EXPECT_FLOAT_EQ(material.getEmissionIntensity(), 5.0f);
}

TEST(MaterialTest, SetOpacity) {
    Material material;

    material.setOpacity(0.5f);
    EXPECT_FLOAT_EQ(material.getOpacity(), 0.5f);
    EXPECT_TRUE(material.isTransparent());

    material.setOpacity(1.0f);
    EXPECT_FALSE(material.isTransparent());

    // Test clamping
    material.setOpacity(-0.5f);
    EXPECT_FLOAT_EQ(material.getOpacity(), 0.0f);

    material.setOpacity(1.5f);
    EXPECT_FLOAT_EQ(material.getOpacity(), 1.0f);
}

// ============================================================================
// GPU Data Tests
// ============================================================================

TEST(MaterialTest, GetGPUDataBasic) {
    Color albedo(0.3f, 0.4f, 0.5f, 1.0f);
    Material material(albedo, 0.6f, 0.7f);

    Material::GPUData gpuData = material.getGPUData();

    // Check albedo (rgb + opacity in alpha)
    EXPECT_FLOAT_EQ(gpuData.albedo.r, 0.3f);
    EXPECT_FLOAT_EQ(gpuData.albedo.g, 0.4f);
    EXPECT_FLOAT_EQ(gpuData.albedo.b, 0.5f);
    EXPECT_FLOAT_EQ(gpuData.albedo.a, 1.0f);  // opacity

    // Check properties
    EXPECT_FLOAT_EQ(gpuData.roughness, 0.6f);
    EXPECT_FLOAT_EQ(gpuData.metallic, 0.7f);
}

TEST(MaterialTest, GetGPUDataEmission) {
    Material material;
    material.setEmission(Color(1.0f, 0.0f, 0.5f, 1.0f));  // Pink
    material.setEmissionIntensity(2.0f);

    Material::GPUData gpuData = material.getGPUData();

    // Emission color stored in emission (rgb + intensity in alpha)
    EXPECT_FLOAT_EQ(gpuData.emission.r, 1.0f);
    EXPECT_FLOAT_EQ(gpuData.emission.g, 0.0f);
    EXPECT_FLOAT_EQ(gpuData.emission.b, 0.5f);
    EXPECT_FLOAT_EQ(gpuData.emission.a, 2.0f);  // intensity
}

// ============================================================================
// Factory Method Tests
// ============================================================================

TEST(MaterialTest, CreateDefault) {
    auto material = Material::createDefault();

    EXPECT_NE(material, nullptr);
    EXPECT_FLOAT_EQ(material->getAlbedo().r, 1.0f);
    EXPECT_FLOAT_EQ(material->getAlbedo().g, 1.0f);
    EXPECT_FLOAT_EQ(material->getAlbedo().b, 1.0f);
    EXPECT_FLOAT_EQ(material->getRoughness(), 0.5f);
    EXPECT_FLOAT_EQ(material->getMetallic(), 0.0f);
}

TEST(MaterialTest, CreateColored) {
    Color color(1.0f, 0.0f, 1.0f, 1.0f);  // Magenta
    auto material = Material::createColored(color);

    EXPECT_NE(material, nullptr);
    EXPECT_FLOAT_EQ(material->getAlbedo().r, 1.0f);
    EXPECT_FLOAT_EQ(material->getAlbedo().g, 0.0f);
    EXPECT_FLOAT_EQ(material->getAlbedo().b, 1.0f);
    EXPECT_FLOAT_EQ(material->getRoughness(), 0.5f);  // Default roughness
    EXPECT_FLOAT_EQ(material->getMetallic(), 0.0f);   // Non-metallic
}

TEST(MaterialTest, CreateMetallic) {
    Color color(0.8f, 0.8f, 0.9f, 1.0f);                    // Silvery
    auto material = Material::createMetallic(color, 0.3f);  // roughness 0.3

    EXPECT_NE(material, nullptr);
    EXPECT_FLOAT_EQ(material->getAlbedo().r, 0.8f);
    EXPECT_FLOAT_EQ(material->getAlbedo().g, 0.8f);
    EXPECT_FLOAT_EQ(material->getAlbedo().b, 0.9f);
    EXPECT_FLOAT_EQ(material->getRoughness(), 0.3f);
    EXPECT_FLOAT_EQ(material->getMetallic(), 1.0f);  // Fully metallic
}

TEST(MaterialTest, CreateEmissive) {
    Color color(1.0f, 0.5f, 0.0f, 1.0f);                    // Orange
    auto material = Material::createEmissive(color, 3.0f);  // intensity 3.0

    EXPECT_NE(material, nullptr);
    EXPECT_FLOAT_EQ(material->getEmissionIntensity(), 3.0f);
    EXPECT_TRUE(material->isEmissive());

    // Emission color should be set
    Color emissionColor = material->getEmission();
    EXPECT_FLOAT_EQ(emissionColor.r, 1.0f);
    EXPECT_FLOAT_EQ(emissionColor.g, 0.5f);
    EXPECT_FLOAT_EQ(emissionColor.b, 0.0f);
}

TEST(MaterialTest, CreateGlass) {
    auto material = Material::createGlass(Color::white(), 0.3f);  // opacity 0.3

    EXPECT_NE(material, nullptr);

    // Glass should have low roughness (smooth)
    EXPECT_LT(material->getRoughness(), 0.2f);

    // Glass is non-metallic
    EXPECT_FLOAT_EQ(material->getMetallic(), 0.0f);

    // Should be transparent
    EXPECT_TRUE(material->isTransparent());
    EXPECT_FLOAT_EQ(material->getOpacity(), 0.3f);
}

// ============================================================================
// Texture Flag Tests
// ============================================================================

TEST(MaterialTest, HasNoTexturesByDefault) {
    Material material;

    EXPECT_FALSE(material.hasAlbedoTexture());
    EXPECT_FALSE(material.hasNormalMap());
    EXPECT_EQ(material.getAlbedoTexture(), nullptr);
    EXPECT_EQ(material.getNormalMap(), nullptr);
}

// ============================================================================
// Shadow Property Tests
// ============================================================================

TEST(MaterialTest, ShadowProperties) {
    Material material;

    // Default: receives and casts shadows
    EXPECT_TRUE(material.receivesShadows());
    EXPECT_TRUE(material.castsShadows());

    material.setReceivesShadows(false);
    EXPECT_FALSE(material.receivesShadows());

    material.setCastsShadows(false);
    EXPECT_FALSE(material.castsShadows());
}

// ============================================================================
// Normal Map Tests
// ============================================================================

TEST(MaterialTest, NormalMapStrength) {
    Material material;

    EXPECT_FLOAT_EQ(material.getNormalStrength(), 1.0f);  // Default

    material.setNormalStrength(0.5f);
    EXPECT_FLOAT_EQ(material.getNormalStrength(), 0.5f);
}

// ============================================================================
// Edge Case Tests
// ============================================================================

TEST(MaterialTest, ZeroRoughnessMetallic) {
    Material material(Color::white(), 0.0f, 0.0f);

    EXPECT_FLOAT_EQ(material.getRoughness(), 0.0f);
    EXPECT_FLOAT_EQ(material.getMetallic(), 0.0f);
}

TEST(MaterialTest, MaxRoughnessMetallic) {
    Material material(Color::white(), 1.0f, 1.0f);

    EXPECT_FLOAT_EQ(material.getRoughness(), 1.0f);
    EXPECT_FLOAT_EQ(material.getMetallic(), 1.0f);
}

TEST(MaterialTest, CopyConstruction) {
    Material original(Color::green(), 0.3f, 0.7f);
    Material copy(original);

    EXPECT_FLOAT_EQ(copy.getAlbedo().r, original.getAlbedo().r);
    EXPECT_FLOAT_EQ(copy.getAlbedo().g, original.getAlbedo().g);
    EXPECT_FLOAT_EQ(copy.getAlbedo().b, original.getAlbedo().b);
    EXPECT_FLOAT_EQ(copy.getRoughness(), original.getRoughness());
    EXPECT_FLOAT_EQ(copy.getMetallic(), original.getMetallic());
}

}  // namespace
}  // namespace vde
