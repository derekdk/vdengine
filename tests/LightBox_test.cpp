/**
 * @file LightBox_test.cpp
 * @brief Unit tests for LightBox classes (Phase 1)
 *
 * Tests LightBox, SimpleColorLightBox, and ThreePointLightBox classes.
 */

#include <vde/api/LightBox.h>

#include <gtest/gtest.h>

using namespace vde;

// ============================================================================
// Light Structure Tests
// ============================================================================

class LightTest : public ::testing::Test {};

TEST_F(LightTest, DirectionalLightFactory) {
    Light light = Light::directional(Direction(0.0f, -1.0f, 0.0f), Color::white(), 0.8f);

    EXPECT_EQ(light.type, LightType::Directional);
    EXPECT_FLOAT_EQ(light.direction.y, -1.0f);
    EXPECT_FLOAT_EQ(light.intensity, 0.8f);
}

TEST_F(LightTest, PointLightFactory) {
    Light light = Light::point(Position(10.0f, 5.0f, 0.0f), Color::red(), 1.5f, 20.0f);

    EXPECT_EQ(light.type, LightType::Point);
    EXPECT_FLOAT_EQ(light.position.x, 10.0f);
    EXPECT_FLOAT_EQ(light.range, 20.0f);
}

TEST_F(LightTest, SpotLightFactory) {
    Light light = Light::spot(Position(0.0f, 10.0f, 0.0f), Direction(0.0f, -1.0f, 0.0f), 30.0f,
                              Color::blue(), 2.0f);

    EXPECT_EQ(light.type, LightType::Spot);
    EXPECT_FLOAT_EQ(light.spotAngle, 30.0f);
    EXPECT_FLOAT_EQ(light.color.b, 1.0f);
}

TEST_F(LightTest, DefaultValues) {
    Light light;

    EXPECT_EQ(light.type, LightType::Directional);
    EXPECT_FLOAT_EQ(light.intensity, 1.0f);
    EXPECT_FLOAT_EQ(light.range, 10.0f);
    EXPECT_FALSE(light.castsShadows);
}

// ============================================================================
// LightBox Tests
// ============================================================================

class LightBoxTest : public ::testing::Test {
  protected:
    std::unique_ptr<LightBox> lightBox;

    void SetUp() override { lightBox = std::make_unique<SimpleColorLightBox>(); }
};

TEST_F(LightBoxTest, DefaultAmbientColor) {
    const Color& ambient = lightBox->getAmbientColor();
    // Default is dark gray (0.1, 0.1, 0.1)
    EXPECT_NEAR(ambient.r, 0.1f, 0.01f);
    EXPECT_NEAR(ambient.g, 0.1f, 0.01f);
    EXPECT_NEAR(ambient.b, 0.1f, 0.01f);
}

TEST_F(LightBoxTest, SetAmbientColor) {
    lightBox->setAmbientColor(Color(0.5f, 0.6f, 0.7f));

    const Color& ambient = lightBox->getAmbientColor();
    EXPECT_FLOAT_EQ(ambient.r, 0.5f);
    EXPECT_FLOAT_EQ(ambient.g, 0.6f);
    EXPECT_FLOAT_EQ(ambient.b, 0.7f);
}

TEST_F(LightBoxTest, DefaultAmbientIntensity) {
    EXPECT_FLOAT_EQ(lightBox->getAmbientIntensity(), 1.0f);
}

TEST_F(LightBoxTest, SetAmbientIntensity) {
    lightBox->setAmbientIntensity(0.5f);
    EXPECT_FLOAT_EQ(lightBox->getAmbientIntensity(), 0.5f);
}

TEST_F(LightBoxTest, AddLightReturnsIndex) {
    Light light1 = Light::directional(Direction(0, -1, 0));
    Light light2 = Light::point(Position(0, 5, 0));

    size_t idx1 = lightBox->addLight(light1);
    size_t idx2 = lightBox->addLight(light2);

    EXPECT_EQ(idx1, 0);
    EXPECT_EQ(idx2, 1);
}

TEST_F(LightBoxTest, GetLightCount) {
    EXPECT_EQ(lightBox->getLightCount(), 0);

    lightBox->addLight(Light::directional(Direction(0, -1, 0)));
    EXPECT_EQ(lightBox->getLightCount(), 1);

    lightBox->addLight(Light::point(Position(0, 5, 0)));
    EXPECT_EQ(lightBox->getLightCount(), 2);
}

TEST_F(LightBoxTest, GetLightByIndex) {
    Light light = Light::directional(Direction(1, 0, 0), Color::red());
    lightBox->addLight(light);

    const Light& retrieved = lightBox->getLight(0);
    EXPECT_EQ(retrieved.type, LightType::Directional);
    EXPECT_FLOAT_EQ(retrieved.direction.x, 1.0f);
    EXPECT_FLOAT_EQ(retrieved.color.r, 1.0f);
}

TEST_F(LightBoxTest, GetLightMutable) {
    lightBox->addLight(Light::directional(Direction(0, -1, 0)));

    Light& light = lightBox->getLight(0);
    light.intensity = 0.5f;

    EXPECT_FLOAT_EQ(lightBox->getLight(0).intensity, 0.5f);
}

TEST_F(LightBoxTest, RemoveLight) {
    lightBox->addLight(Light::directional(Direction(0, -1, 0)));
    lightBox->addLight(Light::point(Position(0, 5, 0)));

    EXPECT_EQ(lightBox->getLightCount(), 2);
    lightBox->removeLight(0);
    EXPECT_EQ(lightBox->getLightCount(), 1);
}

TEST_F(LightBoxTest, ClearLights) {
    lightBox->addLight(Light::directional(Direction(0, -1, 0)));
    lightBox->addLight(Light::point(Position(0, 5, 0)));
    lightBox->addLight(Light::spot(Position(0, 10, 0), Direction(0, -1, 0)));

    EXPECT_EQ(lightBox->getLightCount(), 3);
    lightBox->clearLights();
    EXPECT_EQ(lightBox->getLightCount(), 0);
}

TEST_F(LightBoxTest, GetLightsVector) {
    lightBox->addLight(Light::directional(Direction(0, -1, 0)));
    lightBox->addLight(Light::point(Position(0, 5, 0)));

    const std::vector<Light>& lights = lightBox->getLights();
    EXPECT_EQ(lights.size(), 2);
}

// ============================================================================
// SimpleColorLightBox Tests
// ============================================================================

class SimpleColorLightBoxTest : public ::testing::Test {};

TEST_F(SimpleColorLightBoxTest, DefaultConstructor) {
    SimpleColorLightBox lightBox;
    // Should have default ambient
    EXPECT_GE(lightBox.getAmbientIntensity(), 0.0f);
}

TEST_F(SimpleColorLightBoxTest, ConstructorWithAmbientColor) {
    SimpleColorLightBox lightBox(Color(0.5f, 0.5f, 0.5f));

    const Color& ambient = lightBox.getAmbientColor();
    EXPECT_FLOAT_EQ(ambient.r, 0.5f);
    EXPECT_FLOAT_EQ(ambient.g, 0.5f);
    EXPECT_FLOAT_EQ(ambient.b, 0.5f);
}

TEST_F(SimpleColorLightBoxTest, ConstructorWithAmbientAndLight) {
    Light mainLight = Light::directional(Direction(0, -1, 1), Color::white(), 0.8f);
    SimpleColorLightBox lightBox(Color(0.2f, 0.2f, 0.2f), mainLight);

    EXPECT_FLOAT_EQ(lightBox.getAmbientColor().r, 0.2f);
    EXPECT_EQ(lightBox.getLightCount(), 1);
    EXPECT_FLOAT_EQ(lightBox.getLight(0).intensity, 0.8f);
}

// ============================================================================
// ThreePointLightBox Tests
// ============================================================================

class ThreePointLightBoxTest : public ::testing::Test {
  protected:
    std::unique_ptr<ThreePointLightBox> lightBox;

    void SetUp() override { lightBox = std::make_unique<ThreePointLightBox>(); }
};

TEST_F(ThreePointLightBoxTest, HasThreeLights) {
    EXPECT_EQ(lightBox->getLightCount(), 3);
}

TEST_F(ThreePointLightBoxTest, ConstructorWithColor) {
    ThreePointLightBox customLightBox(Color::yellow(), 0.8f);

    // Key light should have the specified color
    const Light& keyLight = customLightBox.getKeyLight();
    EXPECT_FLOAT_EQ(keyLight.color.r, 1.0f);
    EXPECT_FLOAT_EQ(keyLight.color.g, 1.0f);
    EXPECT_FLOAT_EQ(keyLight.color.b, 0.0f);  // Yellow
}

TEST_F(ThreePointLightBoxTest, GetKeyLight) {
    Light& keyLight = lightBox->getKeyLight();
    EXPECT_EQ(keyLight.type, LightType::Directional);
}

TEST_F(ThreePointLightBoxTest, GetFillLight) {
    Light& fillLight = lightBox->getFillLight();
    EXPECT_EQ(fillLight.type, LightType::Directional);
}

TEST_F(ThreePointLightBoxTest, GetBackLight) {
    Light& backLight = lightBox->getBackLight();
    EXPECT_EQ(backLight.type, LightType::Directional);
}

TEST_F(ThreePointLightBoxTest, LightsAreDifferent) {
    const Light& key = lightBox->getKeyLight();
    const Light& fill = lightBox->getFillLight();

    // Each light should have a different direction
    EXPECT_NE(key.direction.x, fill.direction.x);
}

TEST_F(ThreePointLightBoxTest, ModifyKeyLight) {
    Light& keyLight = lightBox->getKeyLight();
    keyLight.intensity = 1.5f;

    EXPECT_FLOAT_EQ(lightBox->getKeyLight().intensity, 1.5f);
}
