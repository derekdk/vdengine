/**
 * @file FlipCompare_test.cpp
 * @brief Unit tests for the FLIP-based image comparison used by the compare command.
 */

#include <cmath>
#include <string>
#include <vector>

#include "FLIP.h"
#include "stb_image.h"
#include <gtest/gtest.h>

namespace vde::test {

namespace {

/// Replicates the computeFlipMeanError() logic from InputScriptExecutor.cpp
/// so we can test the FLIP integration without needing the full executor.
double computeFlipMeanError(const unsigned char* imageA, const unsigned char* imageB, int width,
                            int height) {
    const size_t pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height);
    std::vector<float> refLinear(pixelCount * 3);
    std::vector<float> testLinear(pixelCount * 3);

    for (size_t i = 0; i < pixelCount; ++i) {
        for (int c = 0; c < 3; ++c) {
            float srgb = static_cast<float>(imageA[i * 4 + c]) / 255.0f;
            refLinear[i * 3 + c] = FLIP::color3::sRGBToLinearRGB(srgb);

            srgb = static_cast<float>(imageB[i * 4 + c]) / 255.0f;
            testLinear[i * 3 + c] = FLIP::color3::sRGBToLinearRGB(srgb);
        }
    }

    FLIP::Parameters params;
    params.PPD = FLIP::calculatePPD(0.7f, static_cast<float>(width), 0.4f);

    float meanError = 0.0f;
    float* errorMap = nullptr;

    FLIP::evaluate(refLinear.data(), testLinear.data(), width, height, false, params, false, true,
                   meanError, &errorMap);

    delete[] errorMap;
    return static_cast<double>(meanError);
}

struct TestImage {
    unsigned char* data = nullptr;
    int width = 0;
    int height = 0;

    ~TestImage() {
        if (data)
            stbi_image_free(data);
    }

    bool load(const std::string& filename) {
        int channels = 0;
        std::string path = std::string(VDE_TEST_DATA_DIR) + "/" + filename;
        data = stbi_load(path.c_str(), &width, &height, &channels, 4);
        return data != nullptr;
    }

    // Non-copyable
    TestImage(const TestImage&) = delete;
    TestImage& operator=(const TestImage&) = delete;
    TestImage() = default;
    TestImage(TestImage&& other) noexcept
        : data(other.data), width(other.width), height(other.height) {
        other.data = nullptr;
    }
};

}  // namespace

// --- Identical images ---

TEST(FlipCompareTest, IdenticalImagesReturnZero) {
    TestImage img;
    ASSERT_TRUE(img.load("solid_red_8x8.png"));
    double result = computeFlipMeanError(img.data, img.data, img.width, img.height);
    EXPECT_DOUBLE_EQ(result, 0.0);
}

TEST(FlipCompareTest, ByteIdenticalCopiesReturnZero) {
    TestImage imgA, imgB;
    ASSERT_TRUE(imgA.load("solid_red_8x8.png"));
    ASSERT_TRUE(imgB.load("solid_red_8x8_copy.png"));
    double result = computeFlipMeanError(imgA.data, imgB.data, imgA.width, imgA.height);
    EXPECT_DOUBLE_EQ(result, 0.0);
}

// --- Completely different images ---

TEST(FlipCompareTest, RedVsBlueReturnsHighError) {
    TestImage imgA, imgB;
    ASSERT_TRUE(imgA.load("solid_red_8x8.png"));
    ASSERT_TRUE(imgB.load("solid_blue_8x8.png"));
    double result = computeFlipMeanError(imgA.data, imgB.data, imgA.width, imgA.height);
    EXPECT_GT(result, 0.1);
    EXPECT_LE(result, 1.0);
}

// --- Minor noise tolerance ---

TEST(FlipCompareTest, SmallNoiseReturnsBelowReasonableThreshold) {
    TestImage imgA, imgB;
    ASSERT_TRUE(imgA.load("solid_red_8x8.png"));
    ASSERT_TRUE(imgB.load("solid_red_8x8_noise.png"));
    double result = computeFlipMeanError(imgA.data, imgB.data, imgA.width, imgA.height);
    EXPECT_LT(result, 0.05);
    EXPECT_GT(result, 0.0);
}

// --- Structural/edge differences ---

TEST(FlipCompareTest, ShiftedGradientDetectsEdgeDifference) {
    TestImage imgA, imgB;
    ASSERT_TRUE(imgA.load("gradient_32x32.png"));
    ASSERT_TRUE(imgB.load("gradient_32x32_shifted.png"));
    double result = computeFlipMeanError(imgA.data, imgB.data, imgA.width, imgA.height);
    EXPECT_GT(result, 0.0);
    EXPECT_LT(result, 0.5);
}

// --- Blank detection ---

TEST(FlipCompareTest, CheckerboardVsBlankIsHighError) {
    TestImage imgA, imgB;
    ASSERT_TRUE(imgA.load("checkerboard_16x16.png"));
    ASSERT_TRUE(imgB.load("blank_16x16.png"));
    double result = computeFlipMeanError(imgA.data, imgB.data, imgA.width, imgA.height);
    EXPECT_GT(result, 0.2);
}

// --- Score bounds ---

TEST(FlipCompareTest, ResultIsAlwaysInZeroOneRange) {
    struct Pair {
        const char* a;
        const char* b;
        int width;
        int height;
    };

    Pair pairs[] = {
        {"solid_red_8x8.png", "solid_blue_8x8.png", 8, 8},
        {"solid_red_8x8.png", "solid_red_8x8_noise.png", 8, 8},
        {"gradient_32x32.png", "gradient_32x32_shifted.png", 32, 32},
        {"checkerboard_16x16.png", "blank_16x16.png", 16, 16},
    };

    for (const auto& p : pairs) {
        TestImage imgA, imgB;
        ASSERT_TRUE(imgA.load(p.a));
        ASSERT_TRUE(imgB.load(p.b));
        double r = computeFlipMeanError(imgA.data, imgB.data, p.width, p.height);
        EXPECT_GE(r, 0.0) << p.a << " vs " << p.b;
        EXPECT_LE(r, 1.0) << p.a << " vs " << p.b;
    }
}

// --- Symmetry ---

TEST(FlipCompareTest, ComparisonIsSymmetric) {
    TestImage imgA, imgB;
    ASSERT_TRUE(imgA.load("solid_red_8x8.png"));
    ASSERT_TRUE(imgB.load("solid_blue_8x8.png"));
    double resultAB = computeFlipMeanError(imgA.data, imgB.data, imgA.width, imgA.height);
    double resultBA = computeFlipMeanError(imgB.data, imgA.data, imgA.width, imgA.height);
    EXPECT_NEAR(resultAB, resultBA, 1e-6);
}

}  // namespace vde::test
