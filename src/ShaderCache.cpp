#include <vde/ShaderCache.h>

#include <filesystem>
#include <fstream>
#include <sstream>

namespace vde {

ShaderCache::ShaderCache(const std::string& cacheDirectory)
    : m_cacheDirectory(cacheDirectory), m_manifestPath(cacheDirectory + "/manifest.json"),
      m_compiler(std::make_unique<ShaderCompiler>()) {}

ShaderCache::~ShaderCache() {
    if (m_initialized) {
        saveManifest();
    }
}

bool ShaderCache::initialize() {
    if (m_initialized) {
        return true;
    }

    if (!createCacheDirectory()) {
        return false;
    }

    // Load existing manifest if present
    loadManifest();

    m_initialized = true;
    return true;
}

bool ShaderCache::createCacheDirectory() {
    try {
        std::filesystem::create_directories(m_cacheDirectory);
        return true;
    } catch (const std::exception& e) {
        m_lastError = "Failed to create cache directory: " + std::string(e.what());
        return false;
    }
}

bool ShaderCache::loadManifest() {
    std::ifstream file(m_manifestPath);
    if (!file.is_open()) {
        // No manifest yet, that's okay
        return true;
    }

    try {
        // Simple JSON parsing for manifest
        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string content = buffer.str();

        // Find entries section
        size_t entriesPos = content.find("\"entries\"");
        if (entriesPos == std::string::npos) {
            return true;  // Empty or old format
        }

        // Parse each entry
        size_t pos = entriesPos;
        while ((pos = content.find("\"sourcePath\"", pos)) != std::string::npos) {
            ShaderCacheEntry entry;

            size_t entryStart = content.rfind("{", pos);
            size_t entryEnd = content.find("}", pos);
            if (entryStart == std::string::npos || entryEnd == std::string::npos) {
                pos++;
                continue;
            }

            std::string entryStr = content.substr(entryStart, entryEnd - entryStart + 1);

            // Extract sourcePath
            size_t pathPos = entryStr.find("\"sourcePath\"");
            if (pathPos != std::string::npos) {
                size_t pathStart = entryStr.find("\"", pathPos + 12) + 1;
                size_t pathEnd = entryStr.find("\"", pathStart);
                entry.sourcePath = entryStr.substr(pathStart, pathEnd - pathStart);
            }

            // Extract sourceHash
            size_t hashPos = entryStr.find("\"sourceHash\"");
            if (hashPos != std::string::npos) {
                size_t hashStart = entryStr.find("\"", hashPos + 12) + 1;
                size_t hashEnd = entryStr.find("\"", hashStart);
                entry.sourceHash =
                    ShaderHash::fromHexString(entryStr.substr(hashStart, hashEnd - hashStart));
            }

            // Extract spvFile
            size_t spvPos = entryStr.find("\"spvFile\"");
            if (spvPos != std::string::npos) {
                size_t spvStart = entryStr.find("\"", spvPos + 9) + 1;
                size_t spvEnd = entryStr.find("\"", spvStart);
                entry.spvFileName = entryStr.substr(spvStart, spvEnd - spvStart);
            }

            // Extract stage
            size_t stagePos = entryStr.find("\"stage\"");
            if (stagePos != std::string::npos) {
                size_t stageStart = entryStr.find("\"", stagePos + 7) + 1;
                size_t stageEnd = entryStr.find("\"", stageStart);
                std::string stageName = entryStr.substr(stageStart, stageEnd - stageStart);

                if (stageName == "vertex")
                    entry.stage = ShaderStage::Vertex;
                else if (stageName == "fragment")
                    entry.stage = ShaderStage::Fragment;
                else if (stageName == "compute")
                    entry.stage = ShaderStage::Compute;
                else if (stageName == "geometry")
                    entry.stage = ShaderStage::Geometry;
                else if (stageName == "tess_control")
                    entry.stage = ShaderStage::TessControl;
                else if (stageName == "tess_eval")
                    entry.stage = ShaderStage::TessEvaluation;
            }

            if (entry.isValid()) {
                m_entries[entry.sourcePath] = entry;
            }

            pos = entryEnd + 1;
        }

        return true;
    } catch (const std::exception& e) {
        m_lastError = "Failed to parse manifest: " + std::string(e.what());
        return false;
    }
}

bool ShaderCache::saveManifest() {
    std::ofstream file(m_manifestPath);
    if (!file.is_open()) {
        m_lastError = "Failed to open manifest for writing";
        return false;
    }

    // Write JSON manifest
    file << "{\n";
    file << "  \"version\": 1,\n";
    file << "  \"entries\": [\n";

    bool first = true;
    for (const auto& [path, entry] : m_entries) {
        if (!first)
            file << ",\n";
        first = false;

        std::string stageName;
        switch (entry.stage) {
        case ShaderStage::Vertex:
            stageName = "vertex";
            break;
        case ShaderStage::Fragment:
            stageName = "fragment";
            break;
        case ShaderStage::Compute:
            stageName = "compute";
            break;
        case ShaderStage::Geometry:
            stageName = "geometry";
            break;
        case ShaderStage::TessControl:
            stageName = "tess_control";
            break;
        case ShaderStage::TessEvaluation:
            stageName = "tess_eval";
            break;
        default:
            stageName = "unknown";
            break;
        }

        file << "    {\n";
        file << "      \"sourcePath\": \"" << entry.sourcePath << "\",\n";
        file << "      \"sourceHash\": \"" << ShaderHash::toHexString(entry.sourceHash) << "\",\n";
        file << "      \"spvFile\": \"" << entry.spvFileName << "\",\n";
        file << "      \"stage\": \"" << stageName << "\"\n";
        file << "    }";
    }

    file << "\n  ]\n";
    file << "}\n";

    return file.good();
}

std::vector<uint32_t> ShaderCache::loadShader(const std::string& sourcePath,
                                              std::optional<ShaderStage> stage) {
    if (!m_initialized) {
        initialize();
    }

    // Determine shader stage
    ShaderStage actualStage;
    if (stage.has_value()) {
        actualStage = stage.value();
    } else {
        std::filesystem::path fsPath(sourcePath);
        actualStage = shaderStageFromExtension(fsPath.extension().string());
    }

    // If caching disabled, compile directly
    if (!m_enabled) {
        m_cacheMisses++;
        return compileAndCache(sourcePath, actualStage);
    }

    // Check cache
    auto it = m_entries.find(sourcePath);
    if (it != m_entries.end()) {
        // Verify source hasn't changed
        uint64_t currentHash = ShaderHash::hashFile(sourcePath);

        if (currentHash == it->second.sourceHash) {
            // Load from cache
            std::string spvPath = getSpvPath(it->second.spvFileName);
            auto spirv = loadSpvFromDisk(spvPath);

            if (!spirv.empty()) {
                m_cacheHits++;
                return spirv;
            }
        }
    }

    // Cache miss - compile and cache
    m_cacheMisses++;
    return compileAndCache(sourcePath, actualStage);
}

std::vector<uint32_t> ShaderCache::reloadShader(const std::string& sourcePath) {
    invalidate(sourcePath);

    std::filesystem::path fsPath(sourcePath);
    ShaderStage stage = shaderStageFromExtension(fsPath.extension().string());

    return compileAndCache(sourcePath, stage);
}

bool ShaderCache::hasSourceChanged(const std::string& sourcePath) const {
    auto it = m_entries.find(sourcePath);
    if (it == m_entries.end()) {
        return true;  // Not in cache = changed
    }

    uint64_t currentHash = ShaderHash::hashFile(sourcePath);
    return currentHash != it->second.sourceHash;
}

void ShaderCache::invalidate(const std::string& sourcePath) {
    auto it = m_entries.find(sourcePath);
    if (it != m_entries.end()) {
        // Delete cached SPV file
        std::string spvPath = getSpvPath(it->second.spvFileName);
        std::error_code ec;
        std::filesystem::remove(spvPath, ec);

        // Remove from entries
        m_entries.erase(it);
    }
}

void ShaderCache::clearCache() {
    // Delete all SPV files
    for (const auto& [path, entry] : m_entries) {
        std::string spvPath = getSpvPath(entry.spvFileName);
        std::error_code ec;
        std::filesystem::remove(spvPath, ec);
    }

    // Clear entries
    m_entries.clear();

    // Delete manifest
    std::error_code ec;
    std::filesystem::remove(m_manifestPath, ec);

    // Reset statistics
    m_cacheHits = 0;
    m_cacheMisses = 0;
}

std::vector<std::string> ShaderCache::hotReload() {
    std::vector<std::string> reloadedShaders;

    // Copy keys since we may modify the map
    std::vector<std::string> paths;
    for (const auto& [path, entry] : m_entries) {
        paths.push_back(path);
    }

    for (const auto& path : paths) {
        if (hasSourceChanged(path)) {
            auto spirv = reloadShader(path);
            if (!spirv.empty()) {
                reloadedShaders.push_back(path);
            }
        }
    }

    return reloadedShaders;
}

std::string ShaderCache::getSpvPath(const std::string& spvFileName) const {
    return m_cacheDirectory + "/" + spvFileName;
}

std::string ShaderCache::generateSpvFileName(uint64_t hash) const {
    return ShaderHash::toHexString(hash) + ".spv";
}

std::vector<uint32_t> ShaderCache::loadSpvFromDisk(const std::string& spvPath) const {
    std::ifstream file(spvPath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return {};
    }

    size_t fileSize = static_cast<size_t>(file.tellg());
    if (fileSize % sizeof(uint32_t) != 0) {
        return {};  // Invalid SPIR-V file
    }

    file.seekg(0);

    std::vector<uint32_t> spirv(fileSize / sizeof(uint32_t));
    file.read(reinterpret_cast<char*>(spirv.data()), static_cast<std::streamsize>(fileSize));

    // Validate SPIR-V magic number
    if (spirv.empty() || spirv[0] != 0x07230203) {
        return {};  // Invalid SPIR-V
    }

    return spirv;
}

bool ShaderCache::saveSpvToDisk(const std::string& spvPath,
                                const std::vector<uint32_t>& spirv) const {
    std::ofstream file(spvPath, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    file.write(reinterpret_cast<const char*>(spirv.data()),
               static_cast<std::streamsize>(spirv.size() * sizeof(uint32_t)));

    return file.good();
}

std::vector<uint32_t> ShaderCache::compileAndCache(const std::string& sourcePath,
                                                   ShaderStage stage) {
    // Compile shader
    auto result = m_compiler->compileFile(sourcePath, stage);

    if (!result.success) {
        m_lastError = "Shader compilation failed: " + result.errorLog;
        return {};
    }

    // Calculate source hash
    uint64_t sourceHash = ShaderHash::hashFile(sourcePath);

    // Generate cache filename
    std::string spvFileName = generateSpvFileName(sourceHash);
    std::string spvPath = getSpvPath(spvFileName);

    // Save to disk
    if (!saveSpvToDisk(spvPath, result.spirv)) {
        m_lastError = "Failed to save compiled shader to cache";
        // Still return the compiled shader even if caching failed
    }

    // Update cache entry
    ShaderCacheEntry entry;
    entry.sourcePath = sourcePath;
    entry.sourceHash = sourceHash;
    entry.spvFileName = spvFileName;
    entry.stage = stage;
    entry.compileTime = std::chrono::system_clock::now();

    m_entries[sourcePath] = entry;

    // Save manifest
    saveManifest();

    return result.spirv;
}

}  // namespace vde
