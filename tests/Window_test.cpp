/**
 * @file Window_test.cpp
 * @brief Unit tests for vde::Window class.
 */

#include <vde/Window.h>

#include <gtest/gtest.h>

namespace vde {
namespace test {

class WindowTest : public ::testing::Test {
  protected:
    void SetUp() override {
        // GLFW must be initialized for window tests
    }

    void TearDown() override {}
};

TEST_F(WindowTest, ResolutionStructIsValid) {
    Resolution res{1920, 1080, "1080p"};
    EXPECT_EQ(1920u, res.width);
    EXPECT_EQ(1080u, res.height);
    EXPECT_STREQ("1080p", res.name);
}

TEST_F(WindowTest, ResolutionAspectRatioCalculation) {
    Resolution res{1920, 1080, "1080p"};
    float aspect = static_cast<float>(res.width) / static_cast<float>(res.height);
    EXPECT_NEAR(aspect, 16.0f / 9.0f, 0.01f);
}

TEST_F(WindowTest, MultipleResolutionsCanBeCreated) {
    Resolution resolutions[] = {
        {1280, 720, "720p"}, {1920, 1080, "1080p"}, {2560, 1440, "1440p"}, {3840, 2160, "4K"}};

    EXPECT_EQ(4u, sizeof(resolutions) / sizeof(resolutions[0]));
    EXPECT_EQ(1280u, resolutions[0].width);
    EXPECT_EQ(3840u, resolutions[3].width);
}

// Note: Window creation tests require a display and GLFW initialization
// These are typically integration tests and may be skipped in CI environments

}  // namespace test
}  // namespace vde
