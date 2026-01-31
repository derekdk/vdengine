/**
 * @file Mesh.cpp
 * @brief Implementation of Mesh class for 3D geometry
 */

#include <vde/api/Mesh.h>
#include <vde/BufferUtils.h>
#include <vde/VulkanContext.h>
#include <glm/gtc/constants.hpp>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <limits>

namespace vde {

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

    if (vertices.empty() || indices.empty()) {
        return false;
    }

    setData(vertices, indices);
    return true;
}

void Mesh::setData(const std::vector<Vertex>& vertices, 
                   const std::vector<uint32_t>& indices) {
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
        {{-halfSize, -halfSize,  halfSize}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{ halfSize, -halfSize,  halfSize}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
        {{ halfSize,  halfSize,  halfSize}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
        {{-halfSize,  halfSize,  halfSize}, {1.0f, 1.0f, 0.0f}, {0.0f, 1.0f}},
        
        // Back face (Z-)
        {{ halfSize, -halfSize, -halfSize}, {1.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
        {{-halfSize, -halfSize, -halfSize}, {0.0f, 1.0f, 1.0f}, {1.0f, 0.0f}},
        {{-halfSize,  halfSize, -halfSize}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},
        {{ halfSize,  halfSize, -halfSize}, {0.5f, 0.5f, 0.5f}, {0.0f, 1.0f}},
        
        // Top face (Y+)
        {{-halfSize,  halfSize,  halfSize}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{ halfSize,  halfSize,  halfSize}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
        {{ halfSize,  halfSize, -halfSize}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
        {{-halfSize,  halfSize, -halfSize}, {1.0f, 1.0f, 0.0f}, {0.0f, 1.0f}},
        
        // Bottom face (Y-)
        {{-halfSize, -halfSize, -halfSize}, {1.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
        {{ halfSize, -halfSize, -halfSize}, {0.0f, 1.0f, 1.0f}, {1.0f, 0.0f}},
        {{ halfSize, -halfSize,  halfSize}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},
        {{-halfSize, -halfSize,  halfSize}, {0.5f, 0.5f, 0.5f}, {0.0f, 1.0f}},
        
        // Right face (X+)
        {{ halfSize, -halfSize,  halfSize}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{ halfSize, -halfSize, -halfSize}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
        {{ halfSize,  halfSize, -halfSize}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
        {{ halfSize,  halfSize,  halfSize}, {1.0f, 1.0f, 0.0f}, {0.0f, 1.0f}},
        
        // Left face (X-)
        {{-halfSize, -halfSize, -halfSize}, {1.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
        {{-halfSize, -halfSize,  halfSize}, {0.0f, 1.0f, 1.0f}, {1.0f, 0.0f}},
        {{-halfSize,  halfSize,  halfSize}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},
        {{-halfSize,  halfSize, -halfSize}, {0.5f, 0.5f, 0.5f}, {0.0f, 1.0f}},
    };
    
    std::vector<uint32_t> indices = {
        0,  1,  2,   2,  3,  0,   // Front
        4,  5,  6,   6,  7,  4,   // Back
        8,  9,  10,  10, 11, 8,   // Top
        12, 13, 14,  14, 15, 12,  // Bottom
        16, 17, 18,  18, 19, 16,  // Right
        20, 21, 22,  22, 23, 20   // Left
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
            float phi = static_cast<float>(seg) / static_cast<float>(segments) * 2.0f * glm::pi<float>();
            float sinPhi = std::sin(phi);
            float cosPhi = std::cos(phi);
            
            Vertex vertex;
            vertex.position = glm::vec3(
                radius * sinTheta * cosPhi,
                radius * cosTheta,
                radius * sinTheta * sinPhi
            );
            
            // Normalize position to get normal, use as color
            glm::vec3 normal = glm::normalize(vertex.position);
            vertex.color = glm::abs(normal);
            
            vertex.texCoord = glm::vec2(
                static_cast<float>(seg) / static_cast<float>(segments),
                static_cast<float>(ring) / static_cast<float>(rings)
            );
            
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

ResourcePtr<Mesh> Mesh::createPlane(float width, float height, 
                                    int subdivisionsX, int subdivisionsY) {
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
        float theta = static_cast<float>(i) / static_cast<float>(segments) * 2.0f * glm::pi<float>();
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

void Mesh::uploadToGPU(VulkanContext* context) {
    if (!context || m_vertices.empty()) {
        return;
    }

    // Free existing buffers if already uploaded
    if (m_vertexBuffer != VK_NULL_HANDLE) {
        freeGPUBuffers(context->getDevice());
    }

    // Ensure BufferUtils is initialized
    if (!BufferUtils::isInitialized()) {
        BufferUtils::init(
            context->getDevice(),
            context->getPhysicalDevice(),
            context->getCommandPool(),
            context->getGraphicsQueue()
        );
    }

    // Upload vertex buffer
    VkDeviceSize vertexBufferSize = sizeof(Vertex) * m_vertices.size();
    BufferUtils::createDeviceLocalBuffer(
        m_vertices.data(),
        vertexBufferSize,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        m_vertexBuffer,
        m_vertexBufferMemory
    );

    // Upload index buffer if we have indices
    if (!m_indices.empty()) {
        VkDeviceSize indexBufferSize = sizeof(uint32_t) * m_indices.size();
        BufferUtils::createDeviceLocalBuffer(
            m_indices.data(),
            indexBufferSize,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            m_indexBuffer,
            m_indexBufferMemory
        );
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

} // namespace vde
