/**
 * @file GeometryObject.h
 * @brief Geometry object data structure for the REPL tool
 */

#pragma once

#include <vde/api/GameAPI.h>

#include <glm/glm.hpp>

#include <fstream>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace vde {
namespace tools {

/**
 * @brief Geometry type enumeration
 */
enum class GeometryType { POLYGON, LINE };

/**
 * @brief Represents a user-created geometry object
 */
struct GeometryObject {
    std::string name;
    GeometryType type;
    std::vector<glm::vec3> points;
    glm::vec3 color{1.0f, 1.0f, 1.0f};     // Default white
    glm::vec3 position{0.0f, 0.0f, 0.0f};  // Position offset for the geometry
    bool visible = false;
    bool showWireframe = false;                  // Whether to show wireframe overlay
    glm::vec3 wireframeColor{0.0f, 1.0f, 0.0f};  // Default green wireframe
    std::shared_ptr<vde::MeshEntity> entity;
    std::shared_ptr<vde::MeshEntity> wireframeEntity;  // Wireframe overlay entity

    // For loaded meshes, preserve original triangulation
    bool isLoadedMesh = false;
    std::vector<vde::Vertex> loadedVertices;
    std::vector<uint32_t> loadedIndices;

    /**
     * @brief Create a mesh from the current points
     * @return Mesh resource pointer, or nullptr if insufficient points
     */
    vde::ResourcePtr<vde::Mesh> createMesh() const {
        // If this is a loaded mesh, use the original triangulation
        if (isLoadedMesh && !loadedVertices.empty() && !loadedIndices.empty()) {
            // Apply current color to all vertices
            std::vector<vde::Vertex> coloredVertices = loadedVertices;
            for (auto& v : coloredVertices) {
                v.color = color;
            }

            auto mesh = std::make_shared<vde::Mesh>();
            mesh->setData(coloredVertices, loadedIndices);
            return mesh;
        }

        // Otherwise, use procedural triangulation for user-created geometry
        if (points.size() < 3 && type == GeometryType::POLYGON) {
            return nullptr;  // Need at least 3 points for a polygon
        }
        if (points.size() < 2 && type == GeometryType::LINE) {
            return nullptr;  // Need at least 2 points for a line
        }

        std::vector<vde::Vertex> vertices;
        std::vector<uint32_t> indices;

        if (type == GeometryType::POLYGON) {
            // Create vertices from points
            for (const auto& point : points) {
                vde::Vertex v;
                v.position = point;
                v.color = color;
                v.texCoord = glm::vec2(0.0f, 0.0f);
                vertices.push_back(v);
            }

            // Create triangle fan indices (assumes convex polygon)
            for (size_t i = 1; i < points.size() - 1; ++i) {
                indices.push_back(0);
                indices.push_back(static_cast<uint32_t>(i));
                indices.push_back(static_cast<uint32_t>(i + 1));
            }

            // Add reverse faces for double-sided rendering
            size_t baseIndex = vertices.size();
            for (const auto& point : points) {
                vde::Vertex v;
                v.position = point;
                v.color = color;
                v.texCoord = glm::vec2(0.0f, 0.0f);
                vertices.push_back(v);
            }

            for (size_t i = 1; i < points.size() - 1; ++i) {
                indices.push_back(static_cast<uint32_t>(baseIndex));
                indices.push_back(static_cast<uint32_t>(baseIndex + i + 1));
                indices.push_back(static_cast<uint32_t>(baseIndex + i));
            }
        } else if (type == GeometryType::LINE) {
            // Create line segments as thin rectangles
            const float lineWidth = 0.02f;
            for (size_t i = 0; i < points.size() - 1; ++i) {
                glm::vec3 p1 = points[i];
                glm::vec3 p2 = points[i + 1];
                glm::vec3 dir = glm::normalize(p2 - p1);

                // Create perpendicular vector
                glm::vec3 perp;
                if (std::abs(dir.y) < 0.9f) {
                    perp = glm::normalize(glm::cross(dir, glm::vec3(0, 1, 0)));
                } else {
                    perp = glm::normalize(glm::cross(dir, glm::vec3(1, 0, 0)));
                }
                perp *= lineWidth;

                uint32_t baseIdx = static_cast<uint32_t>(vertices.size());

                // Create 4 vertices for the line segment
                vde::Vertex v1, v2, v3, v4;
                v1.position = p1 - perp;
                v1.color = color;
                v1.texCoord = glm::vec2(0, 0);

                v2.position = p1 + perp;
                v2.color = color;
                v2.texCoord = glm::vec2(0, 0);

                v3.position = p2 + perp;
                v3.color = color;
                v3.texCoord = glm::vec2(0, 0);

                v4.position = p2 - perp;
                v4.color = color;
                v4.texCoord = glm::vec2(0, 0);

                vertices.push_back(v1);
                vertices.push_back(v2);
                vertices.push_back(v3);
                vertices.push_back(v4);

                // Two triangles for the rectangle
                indices.push_back(baseIdx + 0);
                indices.push_back(baseIdx + 1);
                indices.push_back(baseIdx + 2);

                indices.push_back(baseIdx + 0);
                indices.push_back(baseIdx + 2);
                indices.push_back(baseIdx + 3);
            }
        }

        if (vertices.empty() || indices.empty()) {
            return nullptr;
        }

        auto mesh = std::make_shared<vde::Mesh>();
        mesh->setData(vertices, indices);
        return mesh;
    }

    /**
     * @brief Export geometry to OBJ format
     * @param filename Output filename
     * @return true if successful, false on error
     */
    bool exportToOBJ(const std::string& filename) const {
        if (points.empty()) {
            return false;
        }

        std::ofstream file(filename);
        if (!file.is_open()) {
            return false;
        }

        file << "# Geometry: " << name << "\n";
        file << "# Type: " << (type == GeometryType::POLYGON ? "Polygon" : "Line") << "\n";
        file << "# Created with VDE Geometry REPL Tool\n\n";

        // Write vertices
        for (const auto& point : points) {
            file << "v " << point.x << " " << point.y << " " << point.z << "\n";
        }

        file << "\n";

        // Write faces (OBJ uses 1-based indexing)
        if (type == GeometryType::POLYGON && points.size() >= 3) {
            file << "# Face\nf";
            for (size_t i = 0; i < points.size(); ++i) {
                file << " " << (i + 1);
            }
            file << "\n";
        } else if (type == GeometryType::LINE && points.size() >= 2) {
            // Write line segments
            for (size_t i = 0; i < points.size() - 1; ++i) {
                file << "l " << (i + 1) << " " << (i + 2) << "\n";
            }
        }

        file.close();
        return true;
    }

    /**
     * @brief Create a wireframe mesh from perimeter edges only (not internal triangulation)
     * @param thickness Thickness of wireframe tubes
     * @return Mesh resource pointer showing only outline edges
     */
    vde::ResourcePtr<vde::Mesh> createWireframeMesh(float thickness = 0.01f) const {
        // For loaded meshes, extract edges from the triangulation
        if (isLoadedMesh && !loadedVertices.empty() && !loadedIndices.empty()) {
            std::vector<vde::Vertex> vertices;
            std::vector<uint32_t> indices;

            // Collect unique edges from triangles
            std::set<std::pair<uint32_t, uint32_t>> edgeSet;
            for (size_t i = 0; i + 2 < loadedIndices.size(); i += 3) {
                auto addEdge = [&edgeSet](uint32_t a, uint32_t b) {
                    if (a > b)
                        std::swap(a, b);
                    edgeSet.insert({a, b});
                };
                addEdge(loadedIndices[i], loadedIndices[i + 1]);
                addEdge(loadedIndices[i + 1], loadedIndices[i + 2]);
                addEdge(loadedIndices[i + 2], loadedIndices[i]);
            }

            // Create tubes for all edges
            for (const auto& edge : edgeSet) {
                addWireframeEdge(vertices, indices, loadedVertices[edge.first].position,
                                 loadedVertices[edge.second].position, thickness);
            }

            if (vertices.empty() || indices.empty()) {
                return nullptr;
            }

            auto mesh = std::make_shared<vde::Mesh>();
            mesh->setData(vertices, indices);
            return mesh;
        }

        // For user-created geometry, use perimeter edges only
        if (points.size() < 2) {
            return nullptr;
        }

        std::vector<vde::Vertex> vertices;
        std::vector<uint32_t> indices;

        // For polygon type, create edges along the perimeter only
        if (type == GeometryType::POLYGON && points.size() >= 3) {
            // Create edge tubes for each perimeter edge
            for (size_t i = 0; i < points.size(); ++i) {
                size_t next = (i + 1) % points.size();
                addWireframeEdge(vertices, indices, points[i], points[next], thickness);
            }
        }
        // For line type, create edges between consecutive points
        else if (type == GeometryType::LINE && points.size() >= 2) {
            for (size_t i = 0; i < points.size() - 1; ++i) {
                addWireframeEdge(vertices, indices, points[i], points[i + 1], thickness);
            }
        }

        if (vertices.empty() || indices.empty()) {
            return nullptr;
        }

        auto mesh = std::make_shared<vde::Mesh>();
        mesh->setData(vertices, indices);
        return mesh;
    }

  private:
    /**
     * @brief Add a wireframe edge tube between two points
     */
    void addWireframeEdge(std::vector<vde::Vertex>& vertices, std::vector<uint32_t>& indices,
                          const glm::vec3& p1, const glm::vec3& p2, float thickness) const {
        glm::vec3 dir = glm::normalize(p2 - p1);

        // Create perpendicular vectors for tube cross-section (using 4 sides for simplicity)
        glm::vec3 perp1, perp2;
        if (std::abs(dir.y) < 0.9f) {
            perp1 = glm::normalize(glm::cross(dir, glm::vec3(0, 1, 0))) * thickness;
        } else {
            perp1 = glm::normalize(glm::cross(dir, glm::vec3(1, 0, 0))) * thickness;
        }
        perp2 = glm::normalize(glm::cross(dir, perp1)) * thickness;

        // Create 8 vertices (4 at each end of the tube)
        uint32_t baseIdx = static_cast<uint32_t>(vertices.size());
        glm::vec3 offsets[4] = {perp1, perp2, -perp1, -perp2};

        for (int i = 0; i < 4; ++i) {
            vde::Vertex v1, v2;
            v1.position = p1 + offsets[i];
            v1.color = wireframeColor;
            v1.texCoord = glm::vec2(0, 0);
            v2.position = p2 + offsets[i];
            v2.color = wireframeColor;
            v2.texCoord = glm::vec2(0, 0);
            vertices.push_back(v1);
            vertices.push_back(v2);
        }

        // Create quad faces for the 4 sides of the tube
        for (int i = 0; i < 4; ++i) {
            int next = (i + 1) % 4;
            uint32_t i0 = baseIdx + i * 2;
            uint32_t i1 = baseIdx + i * 2 + 1;
            uint32_t i2 = baseIdx + next * 2 + 1;
            uint32_t i3 = baseIdx + next * 2;

            // Two triangles per quad face
            indices.push_back(i0);
            indices.push_back(i1);
            indices.push_back(i2);

            indices.push_back(i0);
            indices.push_back(i2);
            indices.push_back(i3);
        }
    }
};

}  // namespace tools
}  // namespace vde
