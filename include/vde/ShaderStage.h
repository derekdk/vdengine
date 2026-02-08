#pragma once

/**
 * @file ShaderStage.h
 * @brief Shader stage enumeration and utilities
 */

#include <string>

namespace vde {

/**
 * @brief Enumeration of shader stages supported by the compiler
 */
enum class ShaderStage { Vertex, Fragment, Compute, Geometry, TessControl, TessEvaluation };

/**
 * @brief Get file extension for shader stage
 * @param stage The shader stage
 * @return File extension including dot (e.g., ".vert")
 */
inline std::string shaderStageExtension(ShaderStage stage) {
    switch (stage) {
    case ShaderStage::Vertex:
        return ".vert";
    case ShaderStage::Fragment:
        return ".frag";
    case ShaderStage::Compute:
        return ".comp";
    case ShaderStage::Geometry:
        return ".geom";
    case ShaderStage::TessControl:
        return ".tesc";
    case ShaderStage::TessEvaluation:
        return ".tese";
    default:
        return ".glsl";
    }
}

/**
 * @brief Get display name for shader stage
 * @param stage The shader stage
 * @return Human-readable stage name
 */
inline std::string shaderStageName(ShaderStage stage) {
    switch (stage) {
    case ShaderStage::Vertex:
        return "Vertex";
    case ShaderStage::Fragment:
        return "Fragment";
    case ShaderStage::Compute:
        return "Compute";
    case ShaderStage::Geometry:
        return "Geometry";
    case ShaderStage::TessControl:
        return "TessControl";
    case ShaderStage::TessEvaluation:
        return "TessEvaluation";
    default:
        return "Unknown";
    }
}

/**
 * @brief Infer shader stage from file extension
 * @param extension File extension (with or without dot)
 * @return Corresponding shader stage (defaults to Vertex)
 */
inline ShaderStage shaderStageFromExtension(const std::string& extension) {
    // Handle both ".vert" and "vert" formats
    std::string ext = extension;
    if (!ext.empty() && ext[0] == '.') {
        ext = ext.substr(1);
    }

    if (ext == "vert")
        return ShaderStage::Vertex;
    if (ext == "frag")
        return ShaderStage::Fragment;
    if (ext == "comp")
        return ShaderStage::Compute;
    if (ext == "geom")
        return ShaderStage::Geometry;
    if (ext == "tesc")
        return ShaderStage::TessControl;
    if (ext == "tese")
        return ShaderStage::TessEvaluation;
    return ShaderStage::Vertex;  // Default
}

}  // namespace vde
