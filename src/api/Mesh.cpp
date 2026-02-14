/**
 * @file Mesh.cpp
 * @brief Implementation of Mesh class for 3D geometry
 */

#include <vde/BufferUtils.h>
#include <vde/VulkanContext.h>
#include <vde/api/Mesh.h>

#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <limits>
#include <set>
#include <sstream>
#include <utility>

namespace vde {

Mesh::~Mesh() {
    // Clean up GPU resources if they were allocated
    if (m_device != VK_NULL_HANDLE) {
        freeGPUBuffers(m_device);
    }
}

bool Mesh::loadFromFile(const std::string& path) {
    // Simple OBJ loader (supports only vertices, normals, and texture coords)
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }

    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> texCoords;
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string prefix;
        iss >> prefix;

        if (prefix == "v") {
            // Vertex position
            glm::vec3 pos;
            iss >> pos.x >> pos.y >> pos.z;
            positions.push_back(pos);
        } else if (prefix == "vn") {
            // Vertex normal
            glm::vec3 normal;
            iss >> normal.x >> normal.y >> normal.z;
            normals.push_back(normal);
        } else if (prefix == "vt") {
            // Texture coordinate
            glm::vec2 texCoord;
            iss >> texCoord.x >> texCoord.y;
            texCoords.push_back(texCoord);
        } else if (prefix == "f") {
            // Face - parse v/vt/vn format
            std::string vertexStr;
            std::vector<uint32_t> faceIndices;

            while (iss >> vertexStr) {
                std::istringstream vertexStream(vertexStr);
                std::string indexStr;
                int posIdx = -1, texIdx = -1, normIdx = -1;

                // Parse v/vt/vn
                if (std::getline(vertexStream, indexStr, '/')) {
                    posIdx = std::stoi(indexStr) - 1;
                }
                if (std::getline(vertexStream, indexStr, '/')) {
                    if (!indexStr.empty()) {
                        texIdx = std::stoi(indexStr) - 1;
                    }
                }
                if (std::getline(vertexStream, indexStr, '/')) {
                    if (!indexStr.empty()) {
                        normIdx = std::stoi(indexStr) - 1;
                    }
                }

                // Create vertex
                Vertex vertex{};
                if (posIdx >= 0 && posIdx < positions.size()) {
                    vertex.position = positions[posIdx];
                }
                if (texIdx >= 0 && texIdx < texCoords.size()) {
                    vertex.texCoord = texCoords[texIdx];
                }
                // Use normal as color for now (or white if no normal)
                if (normIdx >= 0 && normIdx < normals.size()) {
                    vertex.color = glm::abs(normals[normIdx]);
                } else {
                    vertex.color = glm::vec3(1.0f);
                }

                vertices.push_back(vertex);
                faceIndices.push_back(static_cast<uint32_t>(vertices.size() - 1));
            }

            // Triangulate face (assumes convex polygons)
            for (size_t i = 1; i + 1 < faceIndices.size(); ++i) {
                indices.push_back(faceIndices[0]);
                indices.push_back(faceIndices[i]);
                indices.push_back(faceIndices[i + 1]);
            }
        }
    }

    // Fallback UV generation when OBJ has no usable texture coordinates.
    // Without this, all vertices default to (0,0) and the mesh samples a single texel.
    if (!vertices.empty()) {
        glm::vec2 uvMin(std::numeric_limits<float>::max());
        glm::vec2 uvMax(std::numeric_limits<float>::lowest());
        for (const auto& vertex : vertices) {
            uvMin = glm::min(uvMin, vertex.texCoord);
            uvMax = glm::max(uvMax, vertex.texCoord);
        }

        constexpr float epsilon = 1e-5f;
        bool hasUsableUVs =
            (std::abs(uvMax.x - uvMin.x) > epsilon) || (std::abs(uvMax.y - uvMin.y) > epsilon);

        if (!hasUsableUVs) {
            glm::vec3 boundsMin(std::numeric_limits<float>::max());
            glm::vec3 boundsMax(std::numeric_limits<float>::lowest());
            for (const auto& vertex : vertices) {
                boundsMin = glm::min(boundsMin, vertex.position);
                boundsMax = glm::max(boundsMax, vertex.position);
            }

            glm::vec3 extents = boundsMax - boundsMin;
            std::array<int, 3> axes = {0, 1, 2};
            std::sort(axes.begin(), axes.end(),
                      [&extents](int a, int b) { return extents[a] > extents[b]; });

            int uAxis = axes[0];
            int vAxis = axes[1];

            auto axisValue = [](const glm::vec3& value, int axis) -> float {
                if (axis == 0)
                    return value.x;
                if (axis == 1)
                    return value.y;
                return value.z;
            };

            float minU = axisValue(boundsMin, uAxis);
            float maxU = axisValue(boundsMax, uAxis);
            float minV = axisValue(boundsMin, vAxis);
            float maxV = axisValue(boundsMax, vAxis);
            float rangeU = std::max(maxU - minU, epsilon);
            float rangeV = std::max(maxV - minV, epsilon);

            for (auto& vertex : vertices) {
                float u = (axisValue(vertex.position, uAxis) - minU) / rangeU;
                float v = (axisValue(vertex.position, vAxis) - minV) / rangeV;
                vertex.texCoord = glm::vec2(u, v);
            }
        }
    }

    if (vertices.empty() || indices.empty()) {
        return false;
    }

    setData(vertices, indices);
    return true;
}

void Mesh::setData(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices) {
    m_vertices = vertices;
    m_indices = indices;
    calculateBounds();
}

void Mesh::calculateBounds() {
    if (m_vertices.empty()) {
        m_boundsMin = glm::vec3(0.0f);
        m_boundsMax = glm::vec3(0.0f);
        return;
    }

    m_boundsMin = glm::vec3(std::numeric_limits<float>::max());
    m_boundsMax = glm::vec3(std::numeric_limits<float>::lowest());

    for (const auto& vertex : m_vertices) {
        m_boundsMin = glm::min(m_boundsMin, vertex.position);
        m_boundsMax = glm::max(m_boundsMax, vertex.position);
    }
}

ResourcePtr<Mesh> Mesh::createCube(float size) {
    auto mesh = std::make_shared<Mesh>();

    float halfSize = size * 0.5f;

    std::vector<Vertex> vertices = {
        // Front face (Z+)
        {{-halfSize, -halfSize, halfSize}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{halfSize, -halfSize, halfSize}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
        {{halfSize, halfSize, halfSize}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
        {{-halfSize, halfSize, halfSize}, {1.0f, 1.0f, 0.0f}, {0.0f, 1.0f}},

        // Back face (Z-)
        {{halfSize, -halfSize, -halfSize}, {1.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
        {{-halfSize, -halfSize, -halfSize}, {0.0f, 1.0f, 1.0f}, {1.0f, 0.0f}},
        {{-halfSize, halfSize, -halfSize}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},
        {{halfSize, halfSize, -halfSize}, {0.5f, 0.5f, 0.5f}, {0.0f, 1.0f}},

        // Top face (Y+)
        {{-halfSize, halfSize, halfSize}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{halfSize, halfSize, halfSize}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
        {{halfSize, halfSize, -halfSize}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
        {{-halfSize, halfSize, -halfSize}, {1.0f, 1.0f, 0.0f}, {0.0f, 1.0f}},

        // Bottom face (Y-)
        {{-halfSize, -halfSize, -halfSize}, {1.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
        {{halfSize, -halfSize, -halfSize}, {0.0f, 1.0f, 1.0f}, {1.0f, 0.0f}},
        {{halfSize, -halfSize, halfSize}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},
        {{-halfSize, -halfSize, halfSize}, {0.5f, 0.5f, 0.5f}, {0.0f, 1.0f}},

        // Right face (X+)
        {{halfSize, -halfSize, halfSize}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{halfSize, -halfSize, -halfSize}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
        {{halfSize, halfSize, -halfSize}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
        {{halfSize, halfSize, halfSize}, {1.0f, 1.0f, 0.0f}, {0.0f, 1.0f}},

        // Left face (X-)
        {{-halfSize, -halfSize, -halfSize}, {1.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
        {{-halfSize, -halfSize, halfSize}, {0.0f, 1.0f, 1.0f}, {1.0f, 0.0f}},
        {{-halfSize, halfSize, halfSize}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},
        {{-halfSize, halfSize, -halfSize}, {0.5f, 0.5f, 0.5f}, {0.0f, 1.0f}},
    };

    std::vector<uint32_t> indices = {
        0,  1,  2,  2,  3,  0,   // Front
        4,  5,  6,  6,  7,  4,   // Back
        8,  9,  10, 10, 11, 8,   // Top
        12, 13, 14, 14, 15, 12,  // Bottom
        16, 17, 18, 18, 19, 16,  // Right
        20, 21, 22, 22, 23, 20   // Left
    };

    mesh->setData(vertices, indices);
    return mesh;
}

ResourcePtr<Mesh> Mesh::createSphere(float radius, int segments, int rings) {
    auto mesh = std::make_shared<Mesh>();

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    // Generate vertices
    for (int ring = 0; ring <= rings; ++ring) {
        float theta = static_cast<float>(ring) / static_cast<float>(rings) * glm::pi<float>();
        float sinTheta = std::sin(theta);
        float cosTheta = std::cos(theta);

        for (int seg = 0; seg <= segments; ++seg) {
            float phi =
                static_cast<float>(seg) / static_cast<float>(segments) * 2.0f * glm::pi<float>();
            float sinPhi = std::sin(phi);
            float cosPhi = std::cos(phi);

            Vertex vertex;
            vertex.position = glm::vec3(radius * sinTheta * cosPhi, radius * cosTheta,
                                        radius * sinTheta * sinPhi);

            // Normalize position to get normal, use as color
            glm::vec3 normal = glm::normalize(vertex.position);
            vertex.color = glm::abs(normal);

            vertex.texCoord = glm::vec2(static_cast<float>(seg) / static_cast<float>(segments),
                                        static_cast<float>(ring) / static_cast<float>(rings));

            vertices.push_back(vertex);
        }
    }

    // Generate indices
    for (int ring = 0; ring < rings; ++ring) {
        for (int seg = 0; seg < segments; ++seg) {
            int current = ring * (segments + 1) + seg;
            int next = current + segments + 1;

            indices.push_back(current);
            indices.push_back(next);
            indices.push_back(current + 1);

            indices.push_back(current + 1);
            indices.push_back(next);
            indices.push_back(next + 1);
        }
    }

    mesh->setData(vertices, indices);
    return mesh;
}

ResourcePtr<Mesh> Mesh::createPlane(float width, float height, int subdivisionsX,
                                    int subdivisionsY) {
    auto mesh = std::make_shared<Mesh>();

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    float halfWidth = width * 0.5f;
    float halfHeight = height * 0.5f;

    // Generate vertices
    for (int y = 0; y <= subdivisionsY; ++y) {
        float v = static_cast<float>(y) / static_cast<float>(subdivisionsY);
        float posY = -halfHeight + v * height;

        for (int x = 0; x <= subdivisionsX; ++x) {
            float u = static_cast<float>(x) / static_cast<float>(subdivisionsX);
            float posX = -halfWidth + u * width;

            Vertex vertex;
            vertex.position = glm::vec3(posX, posY, 0.0f);
            vertex.color = glm::vec3(1.0f);  // White
            vertex.texCoord = glm::vec2(u, v);

            vertices.push_back(vertex);
        }
    }

    // Generate indices
    for (int y = 0; y < subdivisionsY; ++y) {
        for (int x = 0; x < subdivisionsX; ++x) {
            int topLeft = y * (subdivisionsX + 1) + x;
            int topRight = topLeft + 1;
            int bottomLeft = topLeft + (subdivisionsX + 1);
            int bottomRight = bottomLeft + 1;

            indices.push_back(topLeft);
            indices.push_back(bottomLeft);
            indices.push_back(topRight);

            indices.push_back(topRight);
            indices.push_back(bottomLeft);
            indices.push_back(bottomRight);
        }
    }

    mesh->setData(vertices, indices);
    return mesh;
}

ResourcePtr<Mesh> Mesh::createCylinder(float radius, float height, int segments) {
    auto mesh = std::make_shared<Mesh>();

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    float halfHeight = height * 0.5f;

    // Generate side vertices
    for (int i = 0; i <= segments; ++i) {
        float theta =
            static_cast<float>(i) / static_cast<float>(segments) * 2.0f * glm::pi<float>();
        float cosTheta = std::cos(theta);
        float sinTheta = std::sin(theta);

        float u = static_cast<float>(i) / static_cast<float>(segments);

        // Bottom vertex
        Vertex bottomVertex;
        bottomVertex.position = glm::vec3(radius * cosTheta, -halfHeight, radius * sinTheta);
        bottomVertex.color = glm::vec3(1.0f, 0.0f, 0.0f);  // Red
        bottomVertex.texCoord = glm::vec2(u, 0.0f);
        vertices.push_back(bottomVertex);

        // Top vertex
        Vertex topVertex;
        topVertex.position = glm::vec3(radius * cosTheta, halfHeight, radius * sinTheta);
        topVertex.color = glm::vec3(0.0f, 1.0f, 0.0f);  // Green
        topVertex.texCoord = glm::vec2(u, 1.0f);
        vertices.push_back(topVertex);
    }

    // Generate side indices
    for (int i = 0; i < segments; ++i) {
        int bottomLeft = i * 2;
        int bottomRight = (i + 1) * 2;
        int topLeft = bottomLeft + 1;
        int topRight = bottomRight + 1;

        indices.push_back(bottomLeft);
        indices.push_back(bottomRight);
        indices.push_back(topLeft);

        indices.push_back(topLeft);
        indices.push_back(bottomRight);
        indices.push_back(topRight);
    }

    // Add cap centers
    uint32_t bottomCenterIdx = static_cast<uint32_t>(vertices.size());
    Vertex bottomCenter;
    bottomCenter.position = glm::vec3(0.0f, -halfHeight, 0.0f);
    bottomCenter.color = glm::vec3(0.0f, 0.0f, 1.0f);  // Blue
    bottomCenter.texCoord = glm::vec2(0.5f, 0.5f);
    vertices.push_back(bottomCenter);

    uint32_t topCenterIdx = static_cast<uint32_t>(vertices.size());
    Vertex topCenter;
    topCenter.position = glm::vec3(0.0f, halfHeight, 0.0f);
    topCenter.color = glm::vec3(1.0f, 1.0f, 0.0f);  // Yellow
    topCenter.texCoord = glm::vec2(0.5f, 0.5f);
    vertices.push_back(topCenter);

    // Generate cap indices
    for (int i = 0; i < segments; ++i) {
        int bottomLeft = i * 2;
        int bottomRight = (i + 1) * 2;
        int topLeft = bottomLeft + 1;
        int topRight = bottomRight + 1;

        // Bottom cap (CCW from bottom)
        indices.push_back(bottomCenterIdx);
        indices.push_back(bottomRight);
        indices.push_back(bottomLeft);

        // Top cap (CCW from top)
        indices.push_back(topCenterIdx);
        indices.push_back(topLeft);
        indices.push_back(topRight);
    }

    mesh->setData(vertices, indices);
    return mesh;
}

ResourcePtr<Mesh> Mesh::createPyramid(float baseSize, float height) {
    auto mesh = std::make_shared<Mesh>();

    float halfBase = baseSize * 0.5f;
    // Centre the pyramid vertically: base at -height/4, apex at +3*height/4
    float baseY = -height * 0.25f;
    float apexY = height * 0.75f;

    glm::vec3 bl(-halfBase, baseY, -halfBase);
    glm::vec3 br(halfBase, baseY, -halfBase);
    glm::vec3 fr(halfBase, baseY, halfBase);
    glm::vec3 fl(-halfBase, baseY, halfBase);
    glm::vec3 apex(0.0f, apexY, 0.0f);

    glm::vec2 uv(0.0f, 0.0f);
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    // Helper: add a triangle with computed face normal in the vertex color
    // field (the mesh shader uses vertex.color as the surface normal).
    auto addTri = [&](const glm::vec3& a, const glm::vec3& b, const glm::vec3& c) {
        glm::vec3 normal = glm::normalize(glm::cross(b - a, c - a));
        uint32_t base = static_cast<uint32_t>(vertices.size());
        vertices.push_back({a, normal, uv});
        vertices.push_back({b, normal, uv});
        vertices.push_back({c, normal, uv});
        indices.push_back(base);
        indices.push_back(base + 1);
        indices.push_back(base + 2);
    };

    // Base (two triangles, normal facing down)
    addTri(bl, br, fr);
    addTri(bl, fr, fl);

    // Side faces (normals face outward)
    addTri(bl, apex, br);  // back
    addTri(br, apex, fr);  // right
    addTri(fr, apex, fl);  // front
    addTri(fl, apex, bl);  // left

    mesh->setData(vertices, indices);
    return mesh;
}

// ---------------------------------------------------------------------------
// Wireframe Helpers (file-local)
// ---------------------------------------------------------------------------

namespace {

/**
 * @brief Append a thin rectangular tube between two 3D points.
 *
 * Each tube has 4 side faces (8 vertices, 24 indices).
 * The vertex color stores the outward-pointing normal.
 */
void addEdgeTube(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices,
                 const glm::vec3& start, const glm::vec3& end, float thickness) {
    glm::vec3 dir = end - start;
    float len = glm::length(dir);
    if (len < 0.0001f)
        return;
    dir /= len;

    // Build a perpendicular frame around the edge direction
    glm::vec3 up = (std::abs(dir.y) < 0.99f) ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
    glm::vec3 right = glm::normalize(glm::cross(dir, up));
    glm::vec3 forward = glm::normalize(glm::cross(right, dir));

    float halfT = thickness * 0.5f;
    uint32_t base = static_cast<uint32_t>(vertices.size());
    glm::vec2 uv(0.0f, 0.0f);

    // Four corner offsets and their outward normals
    glm::vec3 offsets[4] = {
        right * halfT + forward * halfT,
        right * halfT - forward * halfT,
        -right * halfT - forward * halfT,
        -right * halfT + forward * halfT,
    };
    glm::vec3 normals[4] = {
        glm::normalize(right + forward),
        glm::normalize(right - forward),
        glm::normalize(-right - forward),
        glm::normalize(-right + forward),
    };

    // 8 vertices: 4 at start, 4 at end
    for (int i = 0; i < 4; i++) {
        vertices.push_back({start + offsets[i], normals[i], uv});
    }
    for (int i = 0; i < 4; i++) {
        vertices.push_back({end + offsets[i], normals[i], uv});
    }

    // 4 side quads (2 tris each)
    for (int i = 0; i < 4; i++) {
        uint32_t next = static_cast<uint32_t>((i + 1) % 4);
        indices.push_back(base + i);
        indices.push_back(base + i + 4);
        indices.push_back(base + next + 4);
        indices.push_back(base + i);
        indices.push_back(base + next + 4);
        indices.push_back(base + next);
    }
}

/**
 * @brief Build a canonical edge key so (a,b) == (b,a).
 */
std::pair<uint32_t, uint32_t> makeEdgeKey(uint32_t a, uint32_t b) {
    return (a < b) ? std::make_pair(a, b) : std::make_pair(b, a);
}

}  // namespace

ResourcePtr<Mesh> Mesh::createWireframe(const ResourcePtr<Mesh>& sourceMesh, float thickness) {
    if (!sourceMesh) {
        return std::make_shared<Mesh>();
    }
    return createWireframe(sourceMesh->getVertices(), sourceMesh->getIndices(), thickness);
}

ResourcePtr<Mesh> Mesh::createWireframe(const std::vector<Vertex>& srcVertices,
                                        const std::vector<uint32_t>& srcIndices, float thickness) {
    auto mesh = std::make_shared<Mesh>();

    // Collect unique edges from the triangle list
    std::set<std::pair<uint32_t, uint32_t>> edgeSet;
    for (size_t i = 0; i + 2 < srcIndices.size(); i += 3) {
        edgeSet.insert(makeEdgeKey(srcIndices[i], srcIndices[i + 1]));
        edgeSet.insert(makeEdgeKey(srcIndices[i + 1], srcIndices[i + 2]));
        edgeSet.insert(makeEdgeKey(srcIndices[i + 2], srcIndices[i]));
    }

    // Build tube geometry for each unique edge
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    for (const auto& [a, b] : edgeSet) {
        addEdgeTube(vertices, indices, srcVertices[a].position, srcVertices[b].position, thickness);
    }

    mesh->setData(vertices, indices);
    return mesh;
}

void Mesh::uploadToGPU(VulkanContext* context) {
    if (!context || m_vertices.empty()) {
        return;
    }

    // Free existing buffers if already uploaded
    if (m_vertexBuffer != VK_NULL_HANDLE) {
        freeGPUBuffers(context->getDevice());
    }

    // Store the device for cleanup in destructor
    m_device = context->getDevice();

    // Ensure BufferUtils is initialized
    if (!BufferUtils::isInitialized()) {
        BufferUtils::init(context->getDevice(), context->getPhysicalDevice(),
                          context->getCommandPool(), context->getGraphicsQueue());
    }

    // Upload vertex buffer
    VkDeviceSize vertexBufferSize = sizeof(Vertex) * m_vertices.size();
    BufferUtils::createDeviceLocalBuffer(m_vertices.data(), vertexBufferSize,
                                         VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, m_vertexBuffer,
                                         m_vertexBufferMemory);

    // Upload index buffer if we have indices
    if (!m_indices.empty()) {
        VkDeviceSize indexBufferSize = sizeof(uint32_t) * m_indices.size();
        BufferUtils::createDeviceLocalBuffer(m_indices.data(), indexBufferSize,
                                             VK_BUFFER_USAGE_INDEX_BUFFER_BIT, m_indexBuffer,
                                             m_indexBufferMemory);
    }
}

void Mesh::freeGPUBuffers(VkDevice device) {
    if (m_vertexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, m_vertexBuffer, nullptr);
        m_vertexBuffer = VK_NULL_HANDLE;
    }
    if (m_vertexBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, m_vertexBufferMemory, nullptr);
        m_vertexBufferMemory = VK_NULL_HANDLE;
    }
    if (m_indexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, m_indexBuffer, nullptr);
        m_indexBuffer = VK_NULL_HANDLE;
    }
    if (m_indexBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, m_indexBufferMemory, nullptr);
        m_indexBufferMemory = VK_NULL_HANDLE;
    }

    // Reset device handle since we've cleaned up
    m_device = VK_NULL_HANDLE;
}

void Mesh::bind(VkCommandBuffer commandBuffer) const {
    if (m_vertexBuffer == VK_NULL_HANDLE) {
        return;
    }

    VkBuffer vertexBuffers[] = {m_vertexBuffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

    if (m_indexBuffer != VK_NULL_HANDLE) {
        vkCmdBindIndexBuffer(commandBuffer, m_indexBuffer, 0, VK_INDEX_TYPE_UINT32);
    }
}

}  // namespace vde
