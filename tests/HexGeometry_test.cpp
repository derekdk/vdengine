/**
 * @file HexGeometry_test.cpp
 * @brief Unit tests for vde::HexGeometry class.
 */

#include <vde/HexGeometry.h>

#include <cmath>

#include <gtest/gtest.h>

namespace vde {
namespace test {

class HexGeometryTest : public ::testing::Test {
  protected:
    HexGeometry hexGeom;

    void SetUp() override { hexGeom = HexGeometry(1.0f, HexOrientation::FlatTop); }
};

TEST_F(HexGeometryTest, DefaultConstructorCreatesValidGeometry) {
    HexGeometry defaultHex;
    EXPECT_GT(defaultHex.getSize(), 0.0f);
}

TEST_F(HexGeometryTest, ConstructorSetsSize) {
    HexGeometry hex(2.5f);
    EXPECT_FLOAT_EQ(hex.getSize(), 2.5f);
}

TEST_F(HexGeometryTest, FlatTopWidthIsCorrect) {
    HexGeometry hex(1.0f, HexOrientation::FlatTop);
    // For flat-top: width = 2 * size
    EXPECT_FLOAT_EQ(hex.getWidth(), 2.0f);
}

TEST_F(HexGeometryTest, FlatTopHeightIsCorrect) {
    HexGeometry hex(1.0f, HexOrientation::FlatTop);
    // For flat-top: height = sqrt(3) * size
    EXPECT_NEAR(hex.getHeight(), std::sqrt(3.0f), 0.0001f);
}

TEST_F(HexGeometryTest, PointyTopWidthIsCorrect) {
    HexGeometry hex(1.0f, HexOrientation::PointyTop);
    // For pointy-top: width = sqrt(3) * size
    EXPECT_NEAR(hex.getWidth(), std::sqrt(3.0f), 0.0001f);
}

TEST_F(HexGeometryTest, PointyTopHeightIsCorrect) {
    HexGeometry hex(1.0f, HexOrientation::PointyTop);
    // For pointy-top: height = 2 * size
    EXPECT_FLOAT_EQ(hex.getHeight(), 2.0f);
}

TEST_F(HexGeometryTest, GetCornerPositionsReturns6Corners) {
    auto corners = hexGeom.getCornerPositions();
    EXPECT_EQ(corners.size(), 6);
}

TEST_F(HexGeometryTest, CornersAreAtCorrectDistance) {
    auto corners = hexGeom.getCornerPositions(glm::vec3(0.0f));
    float size = hexGeom.getSize();

    for (const auto& corner : corners) {
        float distance = std::sqrt(corner.x * corner.x + corner.z * corner.z);
        EXPECT_NEAR(distance, size, 0.0001f);
    }
}

TEST_F(HexGeometryTest, GenerateHexReturns7Vertices) {
    HexMesh mesh = hexGeom.generateHex();
    // 1 center + 6 corners = 7 vertices
    EXPECT_EQ(mesh.vertices.size(), 7);
}

TEST_F(HexGeometryTest, GenerateHexReturns18Indices) {
    HexMesh mesh = hexGeom.generateHex();
    // 6 triangles * 3 indices = 18
    EXPECT_EQ(mesh.indices.size(), 18);
}

TEST_F(HexGeometryTest, CenterVertexIsAtCenter) {
    glm::vec3 center(5.0f, 0.0f, 10.0f);
    HexMesh mesh = hexGeom.generateHex(center);

    // First vertex should be at center
    EXPECT_FLOAT_EQ(mesh.vertices[0].position.x, center.x);
    EXPECT_FLOAT_EQ(mesh.vertices[0].position.y, center.y);
    EXPECT_FLOAT_EQ(mesh.vertices[0].position.z, center.z);
}

TEST_F(HexGeometryTest, AllIndicesAreValid) {
    HexMesh mesh = hexGeom.generateHex();

    for (uint32_t index : mesh.indices) {
        EXPECT_LT(index, mesh.vertices.size());
    }
}

TEST_F(HexGeometryTest, VerticesHaveUVCoordinates) {
    HexMesh mesh = hexGeom.generateHex();

    for (const auto& vertex : mesh.vertices) {
        // UVs should be in [0, 1] range (approximately, may slightly exceed due to hex shape)
        EXPECT_GE(vertex.texCoord.x, -0.1f);
        EXPECT_LE(vertex.texCoord.x, 1.1f);
        EXPECT_GE(vertex.texCoord.y, -0.1f);
        EXPECT_LE(vertex.texCoord.y, 1.1f);
    }
}

}  // namespace test
}  // namespace vde
