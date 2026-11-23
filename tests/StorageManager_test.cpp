/**
 * @file StorageManager_test.cpp
 * @brief Unit tests for vde::StorageManager persistent key-value storage.
 *
 * All tests use an isolated app-name ("vde_test_storage") so they never
 * touch real application data.  The fixture deletes the database file in
 * TearDown so each test starts from a clean state.
 *
 * Pure CPU / file-system tests — no window or Vulkan context required.
 */

#include <vde/api/Game.h>
#include <vde/api/StorageManager.h>

#include <cstdint>
#include <filesystem>
#include <numbers>
#include <optional>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#undef min
#undef max
#else
#include <unistd.h>
#endif

#include <gtest/gtest.h>

namespace vde {
namespace test {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/// Returns the path that StorageManager::init_storage uses for the given name.
/// Mirrors the platform logic in StorageManager::resolveStoragePath.
static std::filesystem::path testDbPath(const std::string& appName) {
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

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------

/// Returns a per-process unique app name so parallel CTest invocations each
/// get their own database file and cannot lock each other out.
static std::string testAppName() {
#if defined(_WIN32)
    return "vde_test_storage_" + std::to_string(GetCurrentProcessId());
#else
    return "vde_test_storage_" + std::to_string(getpid());
#endif
}

class StorageManagerTest : public ::testing::Test {
  protected:
    void SetUp() override {
        // Ensure shutdown in case a previous test left the singleton open
        StorageManager::getInstance().shutdown();

        // Remove any leftover DB from prior runs
        removeTestDb(testAppName());

        const bool ok = StorageManager::getInstance().init_storage(testAppName());
        ASSERT_TRUE(ok) << "init_storage failed during SetUp";
        ASSERT_TRUE(StorageManager::getInstance().isInitialized());
    }

    void TearDown() override {
        StorageManager::getInstance().shutdown();
        removeTestDb(testAppName());
    }

    StorageManager& storage() { return StorageManager::getInstance(); }

  private:
    static void removeTestDb(const std::string& appName) {
        std::error_code ec;
        const auto path = testDbPath(appName);
        std::filesystem::remove(path, ec);
        // Also try to remove the empty parent dir (best-effort, ignore errors)
        std::filesystem::remove(path.parent_path(), ec);
    }
};

struct SavedProgress {
    std::string player;
    int score = 0;
};

static void configureStateSerializer(GameState& shared) {
    shared.setSerializer<SavedProgress>(
        "progress-v1",
        [](const SavedProgress& value) { return std::to_string(value.score) + ":" + value.player; },
        [](const std::string& text) {
            const auto separator = text.find(':');
            if (separator == std::string::npos) {
                throw std::runtime_error("invalid progress");
            }
            return SavedProgress{text.substr(separator + 1), std::stoi(text.substr(0, separator))};
        });
}

TEST_F(StorageManagerTest, GameStateSaveSlotsRoundTripCommittedTypedAndLooseState) {
    Game game;
    auto& shared = game.getSharedState();
    shared.initialize(SavedProgress{"Alice", 7});
    configureStateSerializer(shared);
    shared.setValue("coins", int64_t{9007199254740993LL});
    shared.setValue("ready", true);
    shared.setValue("ratio", 0.25);
    shared.setValue("label", std::string("hello"));
    game.commitSharedState();
    shared.update<SavedProgress>([](auto& value) { value.score = 99; });
    shared.setValue("coins", int64_t{2});
    shared.save("one");
    game.commitSharedState();
    shared.save("two");
    shared.setValue("extra", true);
    game.commitSharedState();
    shared.load("one");
    EXPECT_EQ(game.state<SavedProgress>().score, 99);
    EXPECT_TRUE(shared.getValue("extra"));
    game.commitSharedState();
    EXPECT_EQ(game.state<SavedProgress>().score, 7);
    EXPECT_EQ(game.state<SavedProgress>().player, "Alice");
    EXPECT_EQ(std::get<int64_t>(*shared.getValue("coins")), 9007199254740993LL);
    EXPECT_EQ(std::get<bool>(*shared.getValue("ready")), true);
    EXPECT_EQ(std::get<double>(*shared.getValue("ratio")), 0.25);
    EXPECT_EQ(std::get<std::string>(*shared.getValue("label")), "hello");
    EXPECT_FALSE(shared.getValue("extra"));
    shared.load("two");
    game.commitSharedState();
    EXPECT_EQ(game.state<SavedProgress>().score, 99);
}

TEST_F(StorageManagerTest, GameStateLoadFailuresPreserveCommittedAndPendingState) {
    Game game;
    auto& shared = game.getSharedState();
    shared.initialize(SavedProgress{"Alice", 7});
    configureStateSerializer(shared);
    shared.save("valid");
    shared.update<SavedProgress>([](auto& value) { value.score = 8; });
    shared.setValue("pending", true);
    ASSERT_TRUE(storage().setStringData("vde.game_state.slot.corrupt", "{"));
    EXPECT_THROW(shared.load("corrupt"), std::runtime_error);
    EXPECT_THROW(shared.load("missing"), std::runtime_error);
    ASSERT_TRUE(storage().setStringData(
        "vde.game_state.slot.bad",
        R"({"version":1,"schema":"progress-v1","typed":"bad","values":{}})"));
    EXPECT_THROW(shared.load("bad"), std::runtime_error);
    shared.setSerializer<SavedProgress>(
        "progress-v2", [](const SavedProgress&) { return ""; },
        [](const std::string&) { return SavedProgress{}; });
    EXPECT_THROW(shared.load("valid"), std::runtime_error);
    EXPECT_EQ(game.state<SavedProgress>().score, 7);
    game.commitSharedState();
    EXPECT_EQ(game.state<SavedProgress>().score, 8);
    EXPECT_EQ(shared.getScriptStateValue("pending"), 1.0);
}

TEST_F(StorageManagerTest, GameStateLooseOnlySlotsAndSerializerRequirements) {
    Game game;
    auto& shared = game.getSharedState();
    shared.setValue("score", int64_t{4});
    game.commitSharedState();
    shared.save("loose");
    shared.eraseValue("score");
    game.commitSharedState();
    shared.load("loose");
    game.commitSharedState();
    EXPECT_EQ(shared.getScriptStateValue("score"), 4.0);
    EXPECT_THROW(shared.save(""), std::invalid_argument);
    shared.initialize(SavedProgress{});
    EXPECT_THROW(shared.save("typed"), std::logic_error);
    EXPECT_THROW(shared.load("loose"), std::runtime_error);
}

TEST_F(StorageManagerTest, GameStateRejectsMalformedLooseSaveTypesWithoutMutation) {
    Game game;
    auto& shared = game.getSharedState();
    shared.setValue("pending", true);
    for (
        const auto* payload :
        {R"({"version":2,"schema":"","typed":null,"values":{}})",
         R"({"version":1,"schema":"","typed":null,"values":{"x":{"type":0.5,"value":true}}})",
         R"({"version":1,"schema":"","typed":null,"values":{"x":{"type":4294967296,"value":true}}})",
         R"({"version":1,"schema":"","typed":null,"values":{"x":{"type":1,"value":9223372036854775808}}})",
         R"({"version":1,"schema":"","typed":null,"values":{"x":{"type":0,"value":1}}})",
         R"({"version":1,"schema":"","typed":null,"values":{"X":{"type":0,"value":true}}})"}) {
        ASSERT_TRUE(storage().setStringData("vde.game_state.slot.invalid", payload));
        EXPECT_THROW(shared.load("invalid"), std::runtime_error) << payload;
        EXPECT_FALSE(shared.getValue("pending"));
    }
    game.commitSharedState();
    EXPECT_EQ(shared.getScriptStateValue("pending"), 1.0);
}

TEST_F(StorageManagerTest, GameStateLoadPublishesReplacementWithChangeNotifications) {
    Game game;
    auto& shared = game.getSharedState();
    shared.setValue("x", int64_t{1});
    game.commitSharedState();
    shared.save("original");
    shared.setValue("x", int64_t{2});
    shared.setValue("y", true);
    game.commitSharedState();
    int changes = 0;
    shared.subscribe([&](const auto&, const auto&, const auto&) {
        ++changes;
        EXPECT_EQ(shared.getScriptStateValue("x"), 1.0);
        EXPECT_FALSE(shared.getValue("y"));
    });
    shared.setValue("discarded", true);
    shared.load("original");
    EXPECT_EQ(changes, 0);
    game.commitSharedState();
    EXPECT_EQ(changes, 2);
    EXPECT_FALSE(shared.getValue("discarded"));
}

// ===========================================================================
// Lifecycle tests
// ===========================================================================

TEST_F(StorageManagerTest, IsInitializedAfterInit) {
    EXPECT_TRUE(storage().isInitialized());
}

TEST_F(StorageManagerTest, DoubleInitIsNoOp) {
    // Calling init_storage a second time should succeed and stay open
    EXPECT_TRUE(storage().init_storage(testAppName()));
    EXPECT_TRUE(storage().isInitialized());
}

TEST_F(StorageManagerTest, ShutdownClearsInitializedFlag) {
    storage().shutdown();
    EXPECT_FALSE(storage().isInitialized());
}

TEST_F(StorageManagerTest, ReinitAfterShutdownWorks) {
    storage().shutdown();
    ASSERT_FALSE(storage().isInitialized());
    ASSERT_TRUE(storage().init_storage(testAppName()));
    EXPECT_TRUE(storage().isInitialized());
}

TEST(StorageManagerUninitTest, OperationsReturnFalseOrNulloptWhenNotInitialized) {
    // Make sure we start with the singleton closed
    StorageManager::getInstance().shutdown();
    ASSERT_FALSE(StorageManager::getInstance().isInitialized());

    StorageManager& s = StorageManager::getInstance();

    EXPECT_FALSE(s.setStringData("k", "v"));
    EXPECT_FALSE(s.getStringData("k").has_value());
    EXPECT_FALSE(s.setBinData("k", std::vector<uint8_t>{1, 2, 3}));
    EXPECT_FALSE(s.getBinData("k").has_value());
    EXPECT_FALSE(s.setBinData<int>("k", 42));
    EXPECT_FALSE(s.getBinData<int>("k").has_value());
}

// ===========================================================================
// String data tests
// ===========================================================================

TEST_F(StorageManagerTest, StringRoundTrip) {
    ASSERT_TRUE(storage().setStringData("greeting", "hello world"));
    const auto val = storage().getStringData("greeting");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "hello world");
}

TEST_F(StorageManagerTest, StringMissingKeyReturnsNullopt) {
    const auto val = storage().getStringData("no_such_key");
    EXPECT_FALSE(val.has_value());
}

TEST_F(StorageManagerTest, StringOverwriteReplacesValue) {
    ASSERT_TRUE(storage().setStringData("score", "100"));
    ASSERT_TRUE(storage().setStringData("score", "9001"));
    const auto val = storage().getStringData("score");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "9001");
}

TEST_F(StorageManagerTest, StringEmptyValue) {
    ASSERT_TRUE(storage().setStringData("empty", ""));
    const auto val = storage().getStringData("empty");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "");
}

TEST_F(StorageManagerTest, StringMultipleIndependentKeys) {
    ASSERT_TRUE(storage().setStringData("a", "alpha"));
    ASSERT_TRUE(storage().setStringData("b", "beta"));
    ASSERT_TRUE(storage().setStringData("c", "gamma"));

    EXPECT_EQ(storage().getStringData("a").value_or(""), "alpha");
    EXPECT_EQ(storage().getStringData("b").value_or(""), "beta");
    EXPECT_EQ(storage().getStringData("c").value_or(""), "gamma");
}

TEST_F(StorageManagerTest, StringUnicodeValue) {
    // UTF-8 encoding of "こんにちは" as a plain string literal
    const std::string utf8 = "\xe3\x81\x93\xe3\x82\x93\xe3\x81\xab\xe3\x81\xa1\xe3\x81\xaf";
    ASSERT_TRUE(storage().setStringData("lang", utf8));
    const auto val = storage().getStringData("lang");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, utf8);
}

TEST_F(StorageManagerTest, StringPersistsAcrossReopenedDb) {
    ASSERT_TRUE(storage().setStringData("persist", "yes"));
    storage().shutdown();

    // Re-open the same database file
    ASSERT_TRUE(storage().init_storage(testAppName()));
    const auto val = storage().getStringData("persist");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "yes");
}

// ===========================================================================
// Raw binary data tests
// ===========================================================================

TEST_F(StorageManagerTest, RawBinaryRoundTrip) {
    const std::vector<uint8_t> data = {0xDE, 0xAD, 0xBE, 0xEF};
    ASSERT_TRUE(storage().setBinData("raw", data));
    const auto result = storage().getBinData("raw");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, data);
}

TEST_F(StorageManagerTest, RawBinaryMissingKeyReturnsNullopt) {
    EXPECT_FALSE(storage().getBinData("missing_blob").has_value());
}

TEST_F(StorageManagerTest, RawBinaryOverwrite) {
    const std::vector<uint8_t> first = {1, 2, 3};
    const std::vector<uint8_t> second = {9, 8, 7, 6};
    ASSERT_TRUE(storage().setBinData("blob", first));
    ASSERT_TRUE(storage().setBinData("blob", second));
    const auto result = storage().getBinData("blob");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, second);
}

TEST_F(StorageManagerTest, RawBinaryEmptyBlob) {
    const std::vector<uint8_t> empty;
    ASSERT_TRUE(storage().setBinData("empty_blob", empty));
    const auto result = storage().getBinData("empty_blob");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result.value().empty());
}

TEST_F(StorageManagerTest, RawBinaryAllByteValues) {
    std::vector<uint8_t> all256(256);
    for (int i = 0; i < 256; ++i) {
        all256[static_cast<std::size_t>(i)] = static_cast<uint8_t>(i);
    }
    ASSERT_TRUE(storage().setBinData("all_bytes", all256));
    const auto result = storage().getBinData("all_bytes");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, all256);
}

// ===========================================================================
// Typed binary template tests
// ===========================================================================

TEST_F(StorageManagerTest, TypedInt) {
    ASSERT_TRUE(storage().setBinData<int>("lives", 3));
    const auto val = storage().getBinData<int>("lives");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, 3);
}

TEST_F(StorageManagerTest, TypedFloat) {
    ASSERT_TRUE(storage().setBinData<float>("speed", 1.5f));
    const auto val = storage().getBinData<float>("speed");
    ASSERT_TRUE(val.has_value());
    EXPECT_FLOAT_EQ(*val, 1.5f);
}

TEST_F(StorageManagerTest, TypedDouble) {
    constexpr double pi = std::numbers::pi;
    ASSERT_TRUE(storage().setBinData<double>("pi", pi));
    const auto val = storage().getBinData<double>("pi");
    ASSERT_TRUE(val.has_value());
    EXPECT_DOUBLE_EQ(*val, pi);
}

TEST_F(StorageManagerTest, TypedUint64) {
    constexpr uint64_t big = 0xDEADBEEFCAFEBABEULL;
    ASSERT_TRUE(storage().setBinData<uint64_t>("id", big));
    const auto val = storage().getBinData<uint64_t>("id");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, big);
}

TEST_F(StorageManagerTest, TypedBool) {
    ASSERT_TRUE(storage().setBinData<bool>("flag", true));
    const auto val = storage().getBinData<bool>("flag");
    ASSERT_TRUE(val.has_value());
    EXPECT_TRUE(*val);

    ASSERT_TRUE(storage().setBinData<bool>("flag", false));
    const auto val2 = storage().getBinData<bool>("flag");
    ASSERT_TRUE(val2.has_value());
    EXPECT_FALSE(*val2);
}

TEST_F(StorageManagerTest, TypedPODStruct) {
    struct PlayerStats {
        int level;
        float health;
        uint32_t gold;
    };
    static_assert(std::is_trivially_copyable_v<PlayerStats>);

    const PlayerStats original{42, 98.6f, 1234u};
    ASSERT_TRUE(storage().setBinData<PlayerStats>("stats", original));

    const auto result = storage().getBinData<PlayerStats>("stats");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().level, original.level);
    EXPECT_FLOAT_EQ(result.value().health, original.health);
    EXPECT_EQ(result.value().gold, original.gold);
}

TEST_F(StorageManagerTest, TypedGetWithWrongSizeReturnsNullopt) {
    // Store 4 bytes (int), then read as double (8 bytes) — should return nullopt
    ASSERT_TRUE(storage().setBinData<int>("small", 7));
    const auto val = storage().getBinData<double>("small");
    EXPECT_FALSE(val.has_value());
}

TEST_F(StorageManagerTest, TypedMissingKeyReturnsNullopt) {
    const auto val = storage().getBinData<int>("no_such_typed_key");
    EXPECT_FALSE(val.has_value());
}

TEST_F(StorageManagerTest, TypedOverwrite) {
    ASSERT_TRUE(storage().setBinData<int>("counter", 1));
    ASSERT_TRUE(storage().setBinData<int>("counter", 99));
    const auto val = storage().getBinData<int>("counter");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, 99);
}

// ===========================================================================
// Namespace isolation tests  (string vs binary keys are independent)
// ===========================================================================

TEST_F(StorageManagerTest, StringAndBinaryKeysDontCollide) {
    // Same key name stored in both tables should be independent
    ASSERT_TRUE(storage().setStringData("shared", "text_value"));
    ASSERT_TRUE(storage().setBinData<int>("shared", 12345));

    EXPECT_EQ(storage().getStringData("shared").value_or(""), "text_value");
    EXPECT_EQ(storage().getBinData<int>("shared").value_or(0), 12345);
}

}  // namespace test
}  // namespace vde
