#pragma once

/**
 * @file ShaderCache.h
 * @brief Caching compiled SPIR-V shaders to disk with hash-based invalidation
 */

#include <vde/ShaderCompiler.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <optional>
#include <chrono>

namespace vde {

/**
 * @brief Utility class for computing content hashes of shader source files
 * 
 * Uses FNV-1a 64-bit hash algorithm for fast, reliable content hashing.
 */
class ShaderHash {
public:
    /**
     * @brief Hash a string (shader source content)
     * @param content The content to hash
     * @return 64-bit hash value
     */
    static uint64_t hash(const std::string& content);
    
    /**
     * @brief Hash a file's contents
     * @param filePath Path to the file
     * @return 64-bit hash value, or 0 if file cannot be read
     */
    static uint64_t hashFile(const std::string& filePath);
    
    /**
     * @brief Convert hash to hexadecimal string for filenames
     * @param hash The hash value
     * @return 16-character hex string
     */
    static std::string toHexString(uint64_t hash);
    
    /**
     * @brief Parse hexadecimal string back to hash
     * @param hex The hex string
     * @return Hash value
     */
    static uint64_t fromHexString(const std::string& hex);
};

/**
 * @brief Represents a cached shader entry with metadata
 */
struct ShaderCacheEntry {
    std::string sourcePath;                              ///< Original source file path
    uint64_t sourceHash = 0;                             ///< Hash of source content
    std::string spvFileName;                             ///< Cached SPIR-V filename
    ShaderStage stage = ShaderStage::Vertex;             ///< Shader stage type
    std::chrono::system_clock::time_point compileTime;   ///< When compiled
    
    /**
     * @brief Check if entry has valid data
     * @return true if entry is usable
     */
    bool isValid() const {
        return sourceHash != 0 && !spvFileName.empty();
    }
};

/**
 * @brief Caches compiled SPIR-V shaders to disk with hash-based invalidation
 * 
 * The ShaderCache eliminates redundant shader compilation by:
 * - Caching compiled SPIR-V to disk
 * - Using content hashes to detect source changes
 * - Automatically recompiling when sources are modified
 * 
 * This significantly reduces startup time and supports hot-reload
 * during development.
 * 
 * Usage:
 * @code
 * ShaderCache cache("cache/shaders");
 * cache.initialize();
 * 
 * auto spirv = cache.loadShader("assets/shaders/triangle.vert");
 * if (spirv.empty()) {
 *     std::cerr << cache.getLastError() << std::endl;
 * }
 * @endcode
 */
class ShaderCache {
public:
    /**
     * @brief Construct a shader cache
     * @param cacheDirectory Directory to store cached shaders
     */
    explicit ShaderCache(const std::string& cacheDirectory = "cache/shaders");
    ~ShaderCache();
    
    // Non-copyable
    ShaderCache(const ShaderCache&) = delete;
    ShaderCache& operator=(const ShaderCache&) = delete;
    
    /**
     * @brief Initialize cache (load manifest, create directory)
     * @return true if initialization succeeded
     */
    bool initialize();
    
    /**
     * @brief Load shader (from cache if valid, otherwise compile)
     * @param sourcePath Path to GLSL source file
     * @param stage Optional shader stage (inferred from extension if not provided)
     * @return Compiled SPIR-V bytecode, or empty vector on failure
     */
    std::vector<uint32_t> loadShader(
        const std::string& sourcePath,
        std::optional<ShaderStage> stage = std::nullopt
    );
    
    /**
     * @brief Force recompilation of a shader
     * @param sourcePath Path to GLSL source file
     * @return Freshly compiled SPIR-V bytecode
     */
    std::vector<uint32_t> reloadShader(const std::string& sourcePath);
    
    /**
     * @brief Check if shader source has changed since last cache
     * @param sourcePath Path to GLSL source file
     * @return true if source differs from cached version
     */
    bool hasSourceChanged(const std::string& sourcePath) const;
    
    /**
     * @brief Invalidate specific cache entry
     * @param sourcePath Path to GLSL source file
     */
    void invalidate(const std::string& sourcePath);
    
    /**
     * @brief Clear entire cache (delete all cached files)
     */
    void clearCache();
    
    /**
     * @brief Save manifest to disk
     * @return true if save succeeded
     */
    bool saveManifest();
    
    /** @brief Get number of entries in cache */
    size_t getCacheEntryCount() const { return m_entries.size(); }
    
    /** @brief Get number of cache hits since initialization */
    size_t getCacheHits() const { return m_cacheHits; }
    
    /** @brief Get number of cache misses since initialization */
    size_t getCacheMisses() const { return m_cacheMisses; }
    
    /** @brief Get last error message */
    const std::string& getLastError() const { return m_lastError; }
    
    /** @brief Enable or disable caching */
    void setEnabled(bool enabled) { m_enabled = enabled; }
    
    /** @brief Check if caching is enabled */
    bool isEnabled() const { return m_enabled; }
    
    /** @brief Check if cache is initialized */
    bool isInitialized() const { return m_initialized; }
    
    /**
     * @brief Hot-reload: check all shaders and recompile changed ones
     * @return List of paths that were reloaded
     */
    std::vector<std::string> hotReload();
    
    /** @brief Get the cache directory path */
    const std::string& getCacheDirectory() const { return m_cacheDirectory; }
    
private:
    std::string m_cacheDirectory;
    std::string m_manifestPath;
    std::unordered_map<std::string, ShaderCacheEntry> m_entries;
    std::unique_ptr<ShaderCompiler> m_compiler;
    
    std::string m_lastError;
    bool m_enabled = true;
    bool m_initialized = false;
    
    // Statistics
    mutable size_t m_cacheHits = 0;
    mutable size_t m_cacheMisses = 0;
    
    // Internal methods
    bool loadManifest();
    bool createCacheDirectory();
    
    std::string getSpvPath(const std::string& spvFileName) const;
    std::string generateSpvFileName(uint64_t hash) const;
    
    std::vector<uint32_t> loadSpvFromDisk(const std::string& spvPath) const;
    bool saveSpvToDisk(const std::string& spvPath, const std::vector<uint32_t>& spirv) const;
    
    std::vector<uint32_t> compileAndCache(
        const std::string& sourcePath,
        ShaderStage stage
    );
};

} // namespace vde
