#include <vde/ShaderCache.h>
#include <fstream>
#include <sstream>
#include <iomanip>

namespace vde {

uint64_t ShaderHash::hash(const std::string& content) {
    // FNV-1a 64-bit hash
    const uint64_t FNV_PRIME = 0x00000100000001B3ULL;
    const uint64_t FNV_OFFSET_BASIS = 0xcbf29ce484222325ULL;
    
    uint64_t h = FNV_OFFSET_BASIS;
    for (char c : content) {
        h ^= static_cast<uint64_t>(static_cast<unsigned char>(c));
        h *= FNV_PRIME;
    }
    return h;
}

uint64_t ShaderHash::hashFile(const std::string& filePath) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        return 0;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return hash(buffer.str());
}

std::string ShaderHash::toHexString(uint64_t hash) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0') << std::setw(16) << hash;
    return ss.str();
}

uint64_t ShaderHash::fromHexString(const std::string& hex) {
    uint64_t result = 0;
    std::stringstream ss;
    ss << std::hex << hex;
    ss >> result;
    return result;
}

} // namespace vde
