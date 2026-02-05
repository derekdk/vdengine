/**
 * @file Mesh_test.cpp
 * @brief Unit tests for vde::Mesh class
 */

#include <vde/api/Mesh.h>

#include <cmath>

#include <gtest/gtest.h>

using namespace vde;

class MeshTest : public ::testing::Test {
  protected:
    void SetUp() override {
        // Test setup if needed
    }

    void TearDown() override {
        // Test cleanup if needed
    }

    // Helper to check if a point is within bounds
    bool isWithinBounds(const glm::vec3& point, const glm::vec3& min, const glm::vec3& max) {
        return point.x >= min.x && point.x <= max.x && point.y >= min.y && point.y <= max.y &&
               point.z >= min.z && point.z <= max.z;
    }
};

// ============================================================================
// Basic Mesh Operations
// ============================================================================

TEST_F(MeshTest, DefaultConstructor) {
    Mesh mesh;

    EXPECT_EQ(mesh.getVertexCount(), 0);
    EXPECT_EQ(mesh.getIndexCount(), 0);
}

TEST_F(MeshTest, SetDataWithVerticesOnly) {
    Mesh mesh;

    std::vector<Vertex> vertices = {{{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
                                    {{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
                                    {{0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}}};

    mesh.setData(vertices, {});

    EXPECT_EQ(mesh.getVertexCount(), 3);
    EXPECT_EQ(mesh.getIndexCount(), 0);
}

TEST_F(MeshTest, SetDataWithIndices) {
    Mesh mesh;

    std::vector<Vertex> vertices = {{{0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}},
                                    {{1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}},
                                    {{1.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},
                                    {{0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}}};

    std::vector<uint32_t> indices = {0, 1, 2, 0, 2, 3};

    mesh.setData(vertices, indices);

    EXPECT_EQ(mesh.getVertexCount(), 4);
    EXPECT_EQ(mesh.getIndexCount(), 6);
}

TEST_F(MeshTest, BoundsCalculation) {
    Mesh mesh;

    std::vector<Vertex> vertices = {{{-1.0f, -2.0f, -3.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}},
                                    {{1.0f, 2.0f, 3.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},
                                    {{0.5f, -1.5f, 2.0f}, {1.0f, 1.0f, 1.0f}, {0.5f, 0.5f}}};

    mesh.setData(vertices, {});

    glm::vec3 min = mesh.getBoundsMin();
    glm::vec3 max = mesh.getBoundsMax();

    EXPECT_FLOAT_EQ(min.x, -1.0f);
    EXPECT_FLOAT_EQ(min.y, -2.0f);
    EXPECT_FLOAT_EQ(min.z, -3.0f);

    EXPECT_FLOAT_EQ(max.x, 1.0f);
    EXPECT_FLOAT_EQ(max.y, 2.0f);
    EXPECT_FLOAT_EQ(max.z, 3.0f);
}

TEST_F(MeshTest, EmptyMeshBounds) {
    Mesh mesh;
    glm::vec3 min = mesh.getBoundsMin();
    glm::vec3 max = mesh.getBoundsMax();

    // Empty mesh should have zero bounds
    EXPECT_FLOAT_EQ(min.x, 0.0f);
    EXPECT_FLOAT_EQ(max.x, 0.0f);
}

// ============================================================================
// Primitive Generators
// ============================================================================

TEST_F(MeshTest, CreateCube) {
    auto mesh = Mesh::createCube(2.0f);

    EXPECT_GT(mesh->getVertexCount(), 0);
    EXPECT_GT(mesh->getIndexCount(), 0);

    // Cube should have 8 unique vertices (but we duplicate for face normals, so 24)
    EXPECT_EQ(mesh->getVertexCount(), 24);  // 6 faces * 4 vertices per face

    // Cube should have 12 triangles (2 per face * 6 faces) = 36 indices
    EXPECT_EQ(mesh->getIndexCount(), 36);

    // Check bounds
    glm::vec3 min = mesh->getBoundsMin();
    glm::vec3 max = mesh->getBoundsMax();
    EXPECT_FLOAT_EQ(min.x, -1.0f);
    EXPECT_FLOAT_EQ(max.x, 1.0f);
}

TEST_F(MeshTest, CreateCubeAllVerticesWithinBounds) {
    auto mesh = Mesh::createCube(2.0f);
    glm::vec3 min = mesh->getBoundsMin();
    glm::vec3 max = mesh->getBoundsMax();

    const auto& vertices = mesh->getVertices();
    for (const auto& vertex : vertices) {
        EXPECT_TRUE(isWithinBounds(vertex.position, min, max));
    }
}

TEST_F(MeshTest, CreateSphere) {
    auto mesh = Mesh::createSphere(1.0f, 16, 16);

    EXPECT_GT(mesh->getVertexCount(), 0);
    EXPECT_GT(mesh->getIndexCount(), 0);

    // Check that all vertices are roughly 1 unit from origin
    const auto& vertices = mesh->getVertices();
    for (const auto& vertex : vertices) {
        float distance = glm::length(vertex.position);
        EXPECT_NEAR(distance, 1.0f, 0.01f);  // Allow small tolerance
    }
}

TEST_F(MeshTest, CreateSphereLowResolution) {
    auto mesh = Mesh::createSphere(1.0f, 4, 4);

    // Low-res sphere should have fewer vertices
    EXPECT_LT(mesh->getVertexCount(), 100);
    EXPECT_GT(mesh->getVertexCount(), 10);
}

TEST_F(MeshTest, CreateSphereHighResolution) {
    auto mesh = Mesh::createSphere(1.0f, 32, 32);

    // High-res sphere should have more vertices
    EXPECT_GT(mesh->getVertexCount(), 500);
}

TEST_F(MeshTest, CreatePlane) {
    auto mesh = Mesh::createPlane(10.0f, 10.0f, 5, 5);

    EXPECT_GT(mesh->getVertexCount(), 0);
    EXPECT_GT(mesh->getIndexCount(), 0);

    // Plane with 5x5 subdivisions should have (5+1)*(5+1) = 36 vertices
    EXPECT_EQ(mesh->getVertexCount(), 36);

    // Check bounds (plane is in XY, with Z=0)
    glm::vec3 min = mesh->getBoundsMin();
    glm::vec3 max = mesh->getBoundsMax();
    EXPECT_FLOAT_EQ(min.z, 0.0f);
    EXPECT_FLOAT_EQ(max.z, 0.0f);
    float width = max.x - min.x;
    float height = max.y - min.y;
    EXPECT_NEAR(width, 10.0f, 0.01f);
    EXPECT_NEAR(height, 10.0f, 0.01f);
}

TEST_F(MeshTest, CreatePlaneSubdivisions) {
    auto mesh1x1 = Mesh::createPlane(1.0f, 1.0f, 1, 1);
    auto mesh5x5 = Mesh::createPlane(1.0f, 1.0f, 5, 5);

    EXPECT_EQ(mesh1x1->getVertexCount(), 4);   // (1+1)*(1+1)
    EXPECT_EQ(mesh5x5->getVertexCount(), 36);  // (5+1)*(5+1)
}

TEST_F(MeshTest, CreateCylinder) {
    auto mesh = Mesh::createCylinder(1.0f, 2.0f, 16);

    EXPECT_GT(mesh->getVertexCount(), 0);
    EXPECT_GT(mesh->getIndexCount(), 0);

    // Check bounds (cylinder of height 2, radius 1)
    glm::vec3 min = mesh->getBoundsMin();
    glm::vec3 max = mesh->getBoundsMax();
    float height = max.y - min.y;
    EXPECT_NEAR(height, 2.0f, 0.01f);
    EXPECT_NEAR(max.x, 1.0f, 0.01f);
    EXPECT_NEAR(min.x, -1.0f, 0.01f);
}

TEST_F(MeshTest, CreateCylinderSegments) {
    auto meshLowRes = Mesh::createCylinder(1.0f, 1.0f, 6);
    auto meshHighRes = Mesh::createCylinder(1.0f, 1.0f, 32);

    // More segments = more vertices
    EXPECT_LT(meshLowRes->getVertexCount(), meshHighRes->getVertexCount());
}

// ============================================================================
// Data Access
// ============================================================================

TEST_F(MeshTest, GetVertices) {
    auto mesh = Mesh::createCube(1.0f);

    const auto& vertices = mesh->getVertices();
    EXPECT_EQ(vertices.size(), mesh->getVertexCount());
}

TEST_F(MeshTest, GetIndices) {
    auto mesh = Mesh::createCube(1.0f);

    const auto& indices = mesh->getIndices();
    EXPECT_EQ(indices.size(), mesh->getIndexCount());
}

TEST_F(MeshTest, IndicesValidRange) {
    auto mesh = Mesh::createCube(1.0f);

    const auto& indices = mesh->getIndices();
    size_t vertexCount = mesh->getVertexCount();

    for (size_t index : indices) {
        EXPECT_LT(index, vertexCount) << "Index out of bounds";
    }
}

// ============================================================================
// Color Gradients in Primitives
// ============================================================================

TEST_F(MeshTest, CubeHasColoredVertices) {
    auto mesh = Mesh::createCube(1.0f);

    const auto& vertices = mesh->getVertices();
    bool hasVariedColors = false;

    glm::vec3 firstColor = vertices[0].color;
    for (const auto& vertex : vertices) {
        if (vertex.color.r != firstColor.r || vertex.color.g != firstColor.g ||
            vertex.color.b != firstColor.b) {
            hasVariedColors = true;
            break;
        }
    }

    EXPECT_TRUE(hasVariedColors) << "Cube should have color gradient";
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(MeshTest, ZeroSizeCube) {
    auto mesh = Mesh::createCube(0.0f);

    // Should still create valid mesh structure
    EXPECT_GT(mesh->getVertexCount(), 0);
}

TEST_F(MeshTest, ZeroRadiusSphere) {
    auto mesh = Mesh::createSphere(0.0f, 8, 8);

    // All vertices should be at origin
    const auto& vertices = mesh->getVertices();
    for (const auto& vertex : vertices) {
        float distance = glm::length(vertex.position);
        EXPECT_NEAR(distance, 0.0f, 0.01f);
    }
}

TEST_F(MeshTest, MinimalSphereSegments) {
    auto mesh = Mesh::createSphere(1.0f, 3, 3);

    // Should create valid mesh even with minimal segments
    EXPECT_GT(mesh->getVertexCount(), 0);
    EXPECT_GT(mesh->getIndexCount(), 0);
}

TEST_F(MeshTest, MinimalPlaneSubdivisions) {
    auto mesh = Mesh::createPlane(1.0f, 1.0f, 1, 1);

    // 1x1 plane should have 4 vertices (one quad)
    EXPECT_EQ(mesh->getVertexCount(), 4);
    EXPECT_EQ(mesh->getIndexCount(), 6);  // 2 triangles
}

TEST_F(MeshTest, MinimalCylinderSegments) {
    auto mesh = Mesh::createCylinder(1.0f, 1.0f, 3);

    // Minimal cylinder (triangle) should still be valid
    EXPECT_GT(mesh->getVertexCount(), 0);
    EXPECT_GT(mesh->getIndexCount(), 0);
}

// ============================================================================
// GPU Buffer Management Tests
// ============================================================================

TEST_F(MeshTest, IsOnGPUDefaultsFalse) {
    Mesh mesh;

    // New mesh should not be on GPU
    EXPECT_FALSE(mesh.isOnGPU());
}

TEST_F(MeshTest, IsOnGPUFalseAfterSetData) {
    Mesh mesh;

    std::vector<Vertex> vertices = {{{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
                                    {{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
                                    {{0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}}};

    mesh.setData(vertices, {});

    // Just setting data shouldn't upload to GPU
    EXPECT_FALSE(mesh.isOnGPU());
}

TEST_F(MeshTest, PrimitiveMeshNotOnGPU) {
    auto cubeMesh = Mesh::createCube(1.0f);
    auto sphereMesh = Mesh::createSphere(0.5f);
    auto planeMesh = Mesh::createPlane(1.0f, 1.0f);
    auto cylinderMesh = Mesh::createCylinder(0.5f, 1.0f);

    // Factory-created primitives should not be on GPU initially
    EXPECT_FALSE(cubeMesh->isOnGPU());
    EXPECT_FALSE(sphereMesh->isOnGPU());
    EXPECT_FALSE(planeMesh->isOnGPU());
    EXPECT_FALSE(cylinderMesh->isOnGPU());
}

TEST_F(MeshTest, UploadToGPUWithNullContextDoesNothing) {
    auto mesh = Mesh::createCube(1.0f);

    // Uploading with null context should be safe and do nothing
    mesh->uploadToGPU(nullptr);

    EXPECT_FALSE(mesh->isOnGPU());
}

TEST_F(MeshTest, UploadEmptyMeshDoesNothing) {
    Mesh mesh;

    // Mesh with no data should not crash on upload attempt
    // Can't test with real context, but we can test the empty case
    mesh.uploadToGPU(nullptr);

    EXPECT_FALSE(mesh.isOnGPU());
}

TEST_F(MeshTest, FreeGPUBuffersWithNullHandlesIsSafe) {
    Mesh mesh;
    VkDevice nullDevice = VK_NULL_HANDLE;

    // Freeing on mesh with no GPU buffers should be safe
    // This tests the defensive code paths
    mesh.freeGPUBuffers(nullDevice);

    EXPECT_FALSE(mesh.isOnGPU());
}

TEST_F(MeshTest, MeshResourceTypeIsCorrect) {
    Mesh mesh;

    EXPECT_STREQ(mesh.getTypeName(), "Mesh");
}

TEST_F(MeshTest, DestructorDoesNotCrashWithoutGPUBuffers) {
    // Create and destroy a mesh without GPU upload
    // This should not crash (tests the destructor code path)
    {
        Mesh mesh;
        std::vector<Vertex> vertices = {{{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
                                        {{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
                                        {{0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}}};
        mesh.setData(vertices, {});
    }  // Mesh destructor called here

    SUCCEED();  // If we reach here, destructor didn't crash
}

TEST_F(MeshTest, SharedPtrMeshDestructorDoesNotCrash) {
    // Test with shared_ptr which is how primitives are typically created
    {
        auto mesh = Mesh::createCube(1.0f);
        EXPECT_GT(mesh->getVertexCount(), 0);
    }  // shared_ptr destructor called here, which calls Mesh destructor

    SUCCEED();
}
