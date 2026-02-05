/**
 * @file Types_test.cpp
 * @brief Unit tests for vde::Vertex and vde::UniformBufferObject.
 */

#include <vde/Types.h>

#include <glm/gtc/matrix_transform.hpp>

#include <gtest/gtest.h>

namespace vde {
namespace test {

class VertexTest : public ::testing::Test {};

TEST_F(VertexTest, VertexHasCorrectSize) {
    // Vertex should be reasonably sized for GPU efficiency
    EXPECT_LE(sizeof(Vertex), 64);  // Should fit in a cache line
}

TEST_F(VertexTest, BindingDescriptionIsValid) {
    auto binding = Vertex::getBindingDescription();

    EXPECT_EQ(binding.binding, 0);
    EXPECT_EQ(binding.stride, sizeof(Vertex));
    EXPECT_EQ(binding.inputRate, VK_VERTEX_INPUT_RATE_VERTEX);
}

TEST_F(VertexTest, AttributeDescriptionsAreValid) {
    auto attributes = Vertex::getAttributeDescriptions();

    // Should have 3 attributes: position, color, texCoord
    EXPECT_EQ(attributes.size(), 3);

    // Check positions are sequential
    for (size_t i = 0; i < attributes.size(); i++) {
        EXPECT_EQ(attributes[i].location, i);
        EXPECT_EQ(attributes[i].binding, 0);
    }
}

TEST_F(VertexTest, AttributeFormatsAreCorrect) {
    auto attributes = Vertex::getAttributeDescriptions();

    // Position: vec3
    EXPECT_EQ(attributes[0].format, VK_FORMAT_R32G32B32_SFLOAT);

    // Color: vec3
    EXPECT_EQ(attributes[1].format, VK_FORMAT_R32G32B32_SFLOAT);

    // TexCoord: vec2
    EXPECT_EQ(attributes[2].format, VK_FORMAT_R32G32_SFLOAT);
}

TEST_F(VertexTest, AttributeOffsetsAreCorrect) {
    auto attributes = Vertex::getAttributeDescriptions();

    EXPECT_EQ(attributes[0].offset, offsetof(Vertex, position));
    EXPECT_EQ(attributes[1].offset, offsetof(Vertex, color));
    EXPECT_EQ(attributes[2].offset, offsetof(Vertex, texCoord));
}

class UniformBufferObjectTest : public ::testing::Test {};

TEST_F(UniformBufferObjectTest, UBOHasCorrectSize) {
    // UBO should contain 3 mat4s = 192 bytes (may be padded)
    EXPECT_GE(sizeof(UniformBufferObject), 3 * sizeof(glm::mat4));
}

TEST_F(UniformBufferObjectTest, MatricesAreInitializableAsIdentity) {
    UniformBufferObject ubo;
    ubo.model = glm::mat4(1.0f);
    ubo.view = glm::mat4(1.0f);
    ubo.proj = glm::mat4(1.0f);

    // Diagonal elements should be 1.0
    EXPECT_FLOAT_EQ(ubo.model[0][0], 1.0f);
    EXPECT_FLOAT_EQ(ubo.view[1][1], 1.0f);
    EXPECT_FLOAT_EQ(ubo.proj[2][2], 1.0f);
}

TEST_F(UniformBufferObjectTest, MatricesCanBeMultiplied) {
    UniformBufferObject ubo;
    ubo.model = glm::translate(glm::mat4(1.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    ubo.view = glm::lookAt(glm::vec3(0.0f, 5.0f, 5.0f), glm::vec3(0.0f, 0.0f, 0.0f),
                           glm::vec3(0.0f, 1.0f, 0.0f));
    ubo.proj = glm::perspective(glm::radians(45.0f), 16.0f / 9.0f, 0.1f, 100.0f);

    // MVP should be computable
    glm::mat4 mvp = ubo.proj * ubo.view * ubo.model;

    // Result should not be identity
    EXPECT_NE(mvp[3][0], 0.0f);  // Translation should affect result
}

}  // namespace test
}  // namespace vde
