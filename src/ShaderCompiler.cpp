#include <vde/ShaderCompiler.h>

// glslang headers for GLSL to SPIR-V compilation
#include <algorithm>
#include <filesystem>
#include <fstream>

#include <glslang/Public/ResourceLimits.h>
#include <glslang/Public/ShaderLang.h>
#include <glslang/SPIRV/GlslangToSpv.h>

namespace vde {

// Map shader file extension to glslang shader stage
static EShLanguage toGlslangStage(const std::string& extension) {
    // Handle both ".vert" and "vert" formats
    std::string ext = extension;
    if (!ext.empty() && ext[0] == '.') {
        ext = ext.substr(1);
    }

    if (ext == "vert") {
        return EShLangVertex;
    } else if (ext == "frag") {
        return EShLangFragment;
    } else if (ext == "comp") {
        return EShLangCompute;
    } else if (ext == "geom") {
        return EShLangGeometry;
    } else if (ext == "tesc") {
        return EShLangTessControl;
    } else if (ext == "tese") {
        return EShLangTessEvaluation;
    }
    return EShLangVertex;  // Default to vertex shader
}

// Internal implementation to hide glslang includes from header
struct ShaderCompiler::Impl {
    bool initialized = false;
};

ShaderCompiler::ShaderCompiler() : m_impl(std::make_unique<Impl>()) {
    // Note: Assumes glslang::InitializeProcess() called at app startup
    m_impl->initialized = true;
}

ShaderCompiler::~ShaderCompiler() = default;

ShaderCompiler::ShaderCompiler(ShaderCompiler&&) noexcept = default;
ShaderCompiler& ShaderCompiler::operator=(ShaderCompiler&&) noexcept = default;

CompilationResult ShaderCompiler::compile(const std::string& source, ShaderStage stage,
                                          const std::string& sourceName) {
    CompilationResult result;

    // Map to glslang stage
    EShLanguage glslangStage = toGlslangStage(shaderStageExtension(stage));

    // Create shader object
    glslang::TShader shader(glslangStage);

    // Set source
    const char* sources[] = {source.c_str()};
    const int lengths[] = {static_cast<int>(source.length())};
    shader.setStrings(sources, 1);

    // Set environment
    shader.setEnvInput(glslang::EShSourceGlsl, glslangStage, glslang::EShClientVulkan, 100);

    // Configure target Vulkan version
    glslang::EShTargetClientVersion vulkanVersion = glslang::EShTargetVulkan_1_0;
    if (m_vulkanMajor >= 1 && m_vulkanMinor >= 3) {
        vulkanVersion = glslang::EShTargetVulkan_1_3;
    } else if (m_vulkanMajor >= 1 && m_vulkanMinor >= 2) {
        vulkanVersion = glslang::EShTargetVulkan_1_2;
    } else if (m_vulkanMajor >= 1 && m_vulkanMinor >= 1) {
        vulkanVersion = glslang::EShTargetVulkan_1_1;
    }
    shader.setEnvClient(glslang::EShClientVulkan, vulkanVersion);

    // Configure target SPIR-V version
    glslang::EShTargetLanguageVersion spvVersion = glslang::EShTargetSpv_1_0;
    if (m_spvMajor >= 1 && m_spvMinor >= 5) {
        spvVersion = glslang::EShTargetSpv_1_5;
    } else if (m_spvMajor >= 1 && m_spvMinor >= 4) {
        spvVersion = glslang::EShTargetSpv_1_4;
    } else if (m_spvMajor >= 1 && m_spvMinor >= 3) {
        spvVersion = glslang::EShTargetSpv_1_3;
    } else if (m_spvMajor >= 1 && m_spvMinor >= 2) {
        spvVersion = glslang::EShTargetSpv_1_2;
    } else if (m_spvMajor >= 1 && m_spvMinor >= 1) {
        spvVersion = glslang::EShTargetSpv_1_1;
    }
    shader.setEnvTarget(glslang::EShTargetSpv, spvVersion);

    // Get default resource limits
    const TBuiltInResource* resources = GetDefaultResources();

    // Parse shader - use EShMsgDefault for GLSL
    EShMessages messages = EShMsgDefault;

    bool parseSuccess = shader.parse(resources, 100, false, messages);

    // Capture logs
    result.warningLog = shader.getInfoLog();
    result.errorLog = shader.getInfoDebugLog();

    if (!parseSuccess) {
        result.success = false;
        if (result.errorLog.empty()) {
            result.errorLog = result.warningLog;
        }
        m_lastError = result.errorLog;
        return result;
    }

    // Create program and add shader
    glslang::TProgram program;
    program.addShader(&shader);

    // Link program
    bool linkSuccess = program.link(EShMsgDefault);

    if (!linkSuccess) {
        result.success = false;
        result.errorLog = program.getInfoLog();
        m_lastError = result.errorLog;
        return result;
    }

    // Generate SPIR-V
    const glslang::TIntermediate* intermediate = program.getIntermediate(glslangStage);
    if (!intermediate) {
        result.success = false;
        result.errorLog = "Failed to get intermediate representation";
        m_lastError = result.errorLog;
        return result;
    }

    // Use the simple GlslangToSpv overload
    glslang::GlslangToSpv(*intermediate, result.spirv);

    result.success = !result.spirv.empty();
    if (!result.success) {
        result.errorLog = "SPIR-V generation produced empty output";
        m_lastError = result.errorLog;
    }

    return result;
}

CompilationResult ShaderCompiler::compileFile(const std::string& filePath,
                                              std::optional<ShaderStage> stage) {
    CompilationResult result;

    // Read file
    std::string source = readFile(filePath);
    if (source.empty()) {
        result.success = false;
        result.errorLog = "Failed to read shader file: " + filePath;
        m_lastError = result.errorLog;
        return result;
    }

    // Determine stage from extension if not provided
    ShaderStage actualStage;
    if (stage.has_value()) {
        actualStage = stage.value();
    } else {
        std::string ext = getFileExtension(filePath);
        actualStage = shaderStageFromExtension(ext);
    }

    // Compile
    return compile(source, actualStage, filePath);
}

void ShaderCompiler::setOptimizationLevel(int level) {
    m_optimizationLevel = std::clamp(level, 0, 2);
}

void ShaderCompiler::setGenerateDebugInfo(bool enable) {
    m_generateDebugInfo = enable;
}

void ShaderCompiler::setTargetVulkanVersion(int major, int minor) {
    m_vulkanMajor = major;
    m_vulkanMinor = minor;
}

void ShaderCompiler::setTargetSpvVersion(int major, int minor) {
    m_spvMajor = major;
    m_spvMinor = minor;
}

std::string ShaderCompiler::readFile(const std::string& path) const {
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        return "";
    }

    size_t fileSize = static_cast<size_t>(file.tellg());
    file.seekg(0);

    std::string content(fileSize, '\0');
    file.read(content.data(), static_cast<std::streamsize>(fileSize));

    return content;
}

std::string ShaderCompiler::getFileExtension(const std::string& path) const {
    std::filesystem::path fsPath(path);
    return fsPath.extension().string();
}

bool initializeGlslang() {
    return glslang::InitializeProcess();
}

void finalizeGlslang() {
    glslang::FinalizeProcess();
}

}  // namespace vde
