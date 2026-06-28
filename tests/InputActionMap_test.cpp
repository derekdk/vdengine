/**
 * @file InputActionMap_test.cpp
 * @brief Unit tests for vde::InputActionMap.
 */

#include <vde/api/InputActionMap.h>
#include <vde/api/KeyCodes.h>
#include <vde/api/StorageManager.h>

#include <cstdint>
#include <filesystem>
#include <string>

#if defined(_WIN32)
#include <windows.h>
#undef min
#undef max
#else
#include <unistd.h>
#endif

#include <gtest/gtest.h>

namespace vde::test {

namespace {

std::filesystem::path testDbPath(const std::string& appName) {
#if defined(_WIN32)
    char buf[32768] = {};
    GetEnvironmentVariableA("APPDATA", buf, sizeof(buf));
    return std::filesystem::path(buf) / appName / "storage.db";
#elif defined(__APPLE__)
    const char* home = std::getenv("HOME");
    return std::filesystem::path(home ? home : ".") / "Library" / "Application Support" / appName /
           "storage.db";
#else
    const char* xdg = std::getenv("XDG_DATA_HOME");
    if (xdg && xdg[0] != '\0') {
        return std::filesystem::path(xdg) / appName / "storage.db";
    }
    const char* home = std::getenv("HOME");
    return std::filesystem::path(home ? home : ".") / ".local" / "share" / appName / "storage.db";
#endif
}

std::string testAppName() {
#if defined(_WIN32)
    return "vde_test_input_actions_" + std::to_string(GetCurrentProcessId());
#else
    return "vde_test_input_actions_" + std::to_string(getpid());
#endif
}

void removeTestDb(const std::string& appName) {
    std::error_code ec;
    const auto path = testDbPath(appName);
    std::filesystem::remove(path, ec);
    std::filesystem::remove(path.parent_path(), ec);
}

}  // namespace

class InputActionMapTest : public ::testing::Test {
  protected:
    void SetUp() override {
        StorageManager::getInstance().shutdown();
        removeTestDb(testAppName());
        ASSERT_TRUE(StorageManager::getInstance().init_storage(testAppName()));
    }

    void TearDown() override {
        StorageManager::getInstance().shutdown();
        removeTestDb(testAppName());
    }

    InputActionMap map;
};

TEST_F(InputActionMapTest, KeyBindingTracksPressedHeldReleasedStates) {
    map.addBinding("jump", InputActionBinding::key(KEY_SPACE));

    map.handleKeyPress(KEY_SPACE);
    EXPECT_TRUE(map.isPressed("jump"));
    EXPECT_TRUE(map.isHeld("jump"));
    EXPECT_FALSE(map.isReleased("jump"));

    map.advanceFrame();
    EXPECT_FALSE(map.isPressed("jump"));
    EXPECT_TRUE(map.isHeld("jump"));

    map.handleKeyRelease(KEY_SPACE);
    EXPECT_FALSE(map.isHeld("jump"));
    EXPECT_TRUE(map.isReleased("jump"));

    map.advanceFrame();
    EXPECT_FALSE(map.isReleased("jump"));
}

TEST_F(InputActionMapTest, ConsumePressedClearsEdgeWithoutClearingHeldState) {
    map.addBinding("fire", InputActionBinding::key(KEY_F));

    map.handleKeyPress(KEY_F);
    EXPECT_TRUE(map.consumePressed("fire"));
    EXPECT_FALSE(map.consumePressed("fire"));
    EXPECT_TRUE(map.isHeld("fire"));
}

TEST_F(InputActionMapTest, MultipleBindingsStayHeldUntilEverySourceReleases) {
    map.addBinding("move_left", InputActionBinding::key(KEY_A));
    map.addBinding("move_left", InputActionBinding::key(KEY_LEFT));

    map.handleKeyPress(KEY_A);
    map.handleKeyPress(KEY_LEFT);
    EXPECT_TRUE(map.isHeld("move_left"));

    map.handleKeyRelease(KEY_A);
    EXPECT_TRUE(map.isHeld("move_left"));

    map.handleKeyRelease(KEY_LEFT);
    EXPECT_FALSE(map.isHeld("move_left"));
    EXPECT_TRUE(map.isReleased("move_left"));
}

TEST_F(InputActionMapTest, AxisBindingsRespectThresholdsAndDirection) {
    map.addBinding("move_right",
                   InputActionBinding::gamepadAxisPositive(GAMEPAD_AXIS_LEFT_X, 0.5f));
    map.addBinding("move_left", InputActionBinding::gamepadAxisNegative(GAMEPAD_AXIS_LEFT_X, 0.5f));

    map.handleGamepadAxis(JOYSTICK_1, GAMEPAD_AXIS_LEFT_X, 0.4f);
    EXPECT_FALSE(map.isHeld("move_right"));

    map.handleGamepadAxis(JOYSTICK_1, GAMEPAD_AXIS_LEFT_X, 0.8f);
    EXPECT_TRUE(map.isPressed("move_right"));
    EXPECT_TRUE(map.isHeld("move_right"));
    EXPECT_FALSE(map.isHeld("move_left"));

    map.advanceFrame();
    map.handleGamepadAxis(JOYSTICK_1, GAMEPAD_AXIS_LEFT_X, -0.8f);
    EXPECT_TRUE(map.isReleased("move_right"));
    EXPECT_FALSE(map.isHeld("move_right"));
    EXPECT_TRUE(map.isPressed("move_left"));
    EXPECT_TRUE(map.isHeld("move_left"));
}

TEST_F(InputActionMapTest, SetBindingsReplacesPreviousBindingSet) {
    map.setBindings("jump", {InputActionBinding::key(KEY_SPACE)});

    map.handleKeyPress(KEY_SPACE);
    EXPECT_TRUE(map.isHeld("jump"));
    map.handleKeyRelease(KEY_SPACE);
    map.advanceFrame();

    map.setBindings("jump", {InputActionBinding::key(KEY_J)});
    map.handleKeyPress(KEY_SPACE);
    EXPECT_FALSE(map.isHeld("jump"));

    map.handleKeyPress(KEY_J);
    EXPECT_TRUE(map.isHeld("jump"));
}

TEST_F(InputActionMapTest, SaveAndLoadBindingsRoundTripThroughStorageManager) {
    map.addBinding("jump", InputActionBinding::key(KEY_SPACE));
    map.addBinding("jump", InputActionBinding::gamepadButton(GAMEPAD_BUTTON_A));
    map.addBinding("move_right",
                   InputActionBinding::gamepadAxisPositive(GAMEPAD_AXIS_LEFT_X, 0.65f));

    ASSERT_TRUE(map.saveBindings("input_actions"));

    InputActionMap loaded;
    ASSERT_TRUE(loaded.loadBindings("input_actions"));

    ASSERT_TRUE(loaded.hasAction("jump"));
    ASSERT_TRUE(loaded.hasAction("move_right"));
    const auto& jumpBindings = loaded.getBindings("jump");
    const auto& moveRightBindings = loaded.getBindings("move_right");

    EXPECT_EQ(jumpBindings.size(), 2u);
    ASSERT_EQ(moveRightBindings.size(), 1u);
    EXPECT_EQ(moveRightBindings.front().type, InputActionBindingType::GamepadAxisPositive);
    EXPECT_EQ(moveRightBindings.front().code, GAMEPAD_AXIS_LEFT_X);
    EXPECT_FLOAT_EQ(moveRightBindings.front().threshold, 0.65f);
}

TEST_F(InputActionMapTest, MissingStoredBindingsReturnFalseWithoutMutatingMap) {
    map.addBinding("jump", InputActionBinding::key(KEY_SPACE));

    EXPECT_FALSE(map.loadBindings("missing_input_actions"));
    EXPECT_TRUE(map.hasAction("jump"));
    EXPECT_EQ(map.getBindings("jump").size(), 1u);
}

}  // namespace vde::test