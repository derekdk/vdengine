#pragma once

/**
 * @file ShaderCompiler.h
 * @brief GLSL to SPIR-V shader compilation
 */

#include <vde/ShaderStage.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace vde {

/**
 * @brief Result of a shader compilation attempt
 */
struct CompilationResult {
    bool success = false;
    std::vector<uint32_t> spirv;
    std::string errorLog;
    std::string warningLog;
};

/**
 * @brief Shader compiler that wraps glslang for GLSL to SPIR-V compilation
 *
 * This class provides a clean interface for compiling GLSL shader source code
 * to SPIR-V bytecode using the glslang library. It handles all the complexity
 * of glslang configuration and provides meaningful error messages.
 *
 * Note: glslang must be initialized before using this class. Call
 * initializeGlslang() at application startup and finalizeGlslang() at shutdown.
 *
 * Usage:
 * @code
 * ShaderCompiler compiler;
 * auto result = compiler.compile(vertexSource, ShaderStage::Vertex);
 * if (result.success) {
 *     // Use result.spirv to create VkShaderModule
 * } else {
 *     std::cerr << "Error: " << result.errorLog << std::endl;
 * }
 * @endcode
 */
class ShaderCompiler {
  public:
    ShaderCompiler();
    ~ShaderCompiler();

    // Non-copyable
    ShaderCompiler(const ShaderCompiler&) = delete;
    ShaderCompiler& operator=(const ShaderCompiler&) = delete;

    // Move-constructible
    ShaderCompiler(ShaderCompiler&&) noexcept;
    ShaderCompiler& operator=(ShaderCompiler&&) noexcept;

    /**
     * @brief Compile GLSL source to SPIR-V
     * @param source The GLSL source code
     * @param stage The shader stage (vertex, fragment, etc.)
     * @param sourceName Optional name for error messages
     * @return CompilationResult with success status and SPIR-V or error log
     */
    CompilationResult compile(const std::string& source, ShaderStage stage,
                              const std::string& sourceName = "shader");

    /**
     * @brief Compile GLSL from file to SPIR-V
     * @param filePath Path to the shader file
     * @param stage Optional stage (inferred from extension if not provided)
     * @return CompilationResult with success status and SPIR-V or error log
     */
    CompilationResult compileFile(const std::string& filePath,
                                  std::optional<ShaderStage> stage = std::nullopt);

    /**
     * @brief Get last error message
     * @return The most recent error message
     */
    const std::string& getLastError() const { return m_lastError; }

    /**
     * @brief Set optimization level
     * @param level 0 = none, 1 = size optimization, 2 = performance optimization
     */
    void setOptimizationLevel(int level);

    /**
     * @brief Enable or disable debug info generation
     * @param enable True to include debug info in SPIR-V
     */
    void setGenerateDebugInfo(bool enable);

    /**
     * @brief Set target Vulkan version
     * @param major Major version (1)
     * @param minor Minor version (0, 1, 2, or 3)
     */
    void setTargetVulkanVersion(int major, int minor);

    /**
     * @brief Set target SPIR-V version
     * @param major Major version (1)
     * @param minor Minor version (0, 1, 2, 3, 4, or 5)
     */
    void setTargetSpvVersion(int major, int minor);

  private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;

    std::string m_lastError;
    int m_optimizationLevel = 0;
    bool m_generateDebugInfo = true;
    int m_vulkanMajor = 1;
    int m_vulkanMinor = 0;
    int m_spvMajor = 1;
    int m_spvMinor = 0;

    std::string readFile(const std::string& path) const;
    std::string getFileExtension(const std::string& path) const;
};

/**
 * @brief Initialize glslang - call once at application startup
 * @return true if initialization succeeded
 */
bool initializeGlslang();

/**
 * @brief Finalize glslang - call once at application shutdown
 */
void finalizeGlslang();

}  // namespace vde
