/**
 * @file ShaderCache_test.cpp
 * @brief Unit tests for vde::ShaderHash class.
 */

#include <gtest/gtest.h>
#include <vde/ShaderCache.h>

namespace vde {
namespace test {

class ShaderHashTest : public ::testing::Test {
protected:
    ShaderHash hasher;
};

TEST_F(ShaderHashTest, SameContentProducesSameHash) {
    std::string content = "void main() { gl_Position = vec4(0.0); }";
    
    uint64_t hash1 = hasher.hash(content);
    uint64_t hash2 = hasher.hash(content);
    
    EXPECT_EQ(hash1, hash2);
}

TEST_F(ShaderHashTest, DifferentContentProducesDifferentHash) {
    std::string content1 = "void main() { gl_Position = vec4(0.0); }";
    std::string content2 = "void main() { gl_Position = vec4(1.0); }";
    
    uint64_t hash1 = hasher.hash(content1);
    uint64_t hash2 = hasher.hash(content2);
    
    EXPECT_NE(hash1, hash2);
}

TEST_F(ShaderHashTest, EmptyStringProducesValidHash) {
    uint64_t hash = hasher.hash("");
    // FNV-1a basis should be returned for empty string
    EXPECT_NE(hash, 0);
}

TEST_F(ShaderHashTest, WhitespaceChangesHash) {
    std::string content1 = "void main() {}";
    std::string content2 = "void main()  {}";
    
    uint64_t hash1 = hasher.hash(content1);
    uint64_t hash2 = hasher.hash(content2);
    
    EXPECT_NE(hash1, hash2);
}

TEST_F(ShaderHashTest, HashIsConsistentAcrossInstances) {
    ShaderHash hasher2;
    std::string content = "some shader code";
    
    uint64_t hash1 = hasher.hash(content);
    uint64_t hash2 = hasher2.hash(content);
    
    EXPECT_EQ(hash1, hash2);
}

TEST_F(ShaderHashTest, LongContentHashesCorrectly) {
    std::string longContent(10000, 'x');
    
    // Should not throw or crash
    uint64_t hash = hasher.hash(longContent);
    EXPECT_NE(hash, 0);
}

TEST_F(ShaderHashTest, SpecialCharactersHashCorrectly) {
    std::string content = "#version 450\n\tlayout(location = 0) in vec3 pos;\r\n";
    
    // Should not throw or crash
    uint64_t hash = hasher.hash(content);
    EXPECT_NE(hash, 0);
}

} // namespace test
} // namespace vde
