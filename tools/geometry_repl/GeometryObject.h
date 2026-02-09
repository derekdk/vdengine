/**
 * @file GeometryObject.h
 * @brief Geometry object data structure for the REPL tool
 */

#pragma once

#include <vde/api/GameAPI.h>

#include <glm/glm.hpp>

#include <fstream>
#include <memory>
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
    glm::vec3 color{1.0f, 1.0f, 1.0f};  // Default white
    bool visible = false;
    std::shared_ptr<vde::MeshEntity> entity;

    /**
     * @brief Create a mesh from the current points
     * @return Mesh resource pointer, or nullptr if insufficient points
     */
    vde::ResourcePtr<vde::Mesh> createMesh() const {
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
};

}  // namespace tools
}  // namespace vde
