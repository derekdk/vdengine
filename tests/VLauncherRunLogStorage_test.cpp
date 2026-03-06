/**
 * @file VLauncherRunLogStorage_test.cpp
 * @brief Unit tests for vde::tools::RunLogStorage.
 */

#include <gtest/gtest.h>

#include <vde/api/StorageManager.h>

#include "../tools/vlauncher/RunLogStorage.h"

#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#undef min
#undef max
#endif

namespace vde::test {

namespace {

std::filesystem::path testDbPath(const std::string& appName) {
#if defined(_WIN32)
    char buffer[32768] = {};
    GetEnvironmentVariableA("APPDATA", buffer, sizeof(buffer));
    return std::filesystem::path(buffer) / appName / "storage.db";
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

}  // namespace

static constexpr const char* kTestApp = "vde_test_vlauncher_runlog";

class VLauncherRunLogStorageTest : public ::testing::Test {
  protected:
    void SetUp() override {
        vde::StorageManager::getInstance().shutdown();
        removeTestDb();

        ASSERT_TRUE(vde::StorageManager::getInstance().init_storage(kTestApp));
        ASSERT_TRUE(vde::StorageManager::getInstance().isInitialized());
    }

    void TearDown() override {
        vde::StorageManager::getInstance().shutdown();
        removeTestDb();
    }

    static void removeTestDb() {
        std::error_code error;
        const auto path = testDbPath(kTestApp);
        std::filesystem::remove(path, error);
        std::filesystem::remove(path.parent_path(), error);
    }
};

TEST_F(VLauncherRunLogStorageTest, BuildTargetIdIsStableForEquivalentPaths) {
    const std::filesystem::path repoRoot = std::filesystem::path("repo_root");
    const std::filesystem::path executableA =
        repoRoot / "build_ninja" / "examples" / "vde_triangle.exe";
    const std::filesystem::path executableB =
        repoRoot / "build_ninja" / "examples" / "." / "vde_triangle.exe";

    const std::string idA = vde::tools::RunLogStorage::buildTargetId(repoRoot, executableA);
    const std::string idB = vde::tools::RunLogStorage::buildTargetId(repoRoot, executableB);

    EXPECT_EQ(idA, idB);
}

TEST_F(VLauncherRunLogStorageTest, SaveLatestRunKeepsOnlyTwoMostRecentEntries) {
    const std::string targetId = "target_rotation";

    vde::tools::StoredRunLog first;
    first.timestamp = "2026-03-06 10:00:00";
    first.exitCode = 0;
    first.commandLine = "\"vde_triangle.exe\"";
    first.output = "run one";

    vde::tools::StoredRunLog second;
    second.timestamp = "2026-03-06 10:01:00";
    second.exitCode = 1;
    second.commandLine = "\"vde_triangle.exe\" --foo";
    second.output = "run two";

    vde::tools::StoredRunLog third;
    third.timestamp = "2026-03-06 10:02:00";
    third.exitCode = 2;
    third.commandLine = "\"vde_triangle.exe\" --bar";
    third.output = "run three";

    std::string error;
    ASSERT_TRUE(vde::tools::RunLogStorage::saveLatestRun(targetId, first, error)) << error;

    auto logsAfterFirst = vde::tools::RunLogStorage::loadRecentRuns(targetId);
    ASSERT_TRUE(logsAfterFirst[0].has_value());
    EXPECT_FALSE(logsAfterFirst[1].has_value());
    EXPECT_EQ(logsAfterFirst[0]->output, "run one");

    ASSERT_TRUE(vde::tools::RunLogStorage::saveLatestRun(targetId, second, error)) << error;

    auto logsAfterSecond = vde::tools::RunLogStorage::loadRecentRuns(targetId);
    ASSERT_TRUE(logsAfterSecond[0].has_value());
    ASSERT_TRUE(logsAfterSecond[1].has_value());
    EXPECT_EQ(logsAfterSecond[0]->output, "run two");
    EXPECT_EQ(logsAfterSecond[1]->output, "run one");

    ASSERT_TRUE(vde::tools::RunLogStorage::saveLatestRun(targetId, third, error)) << error;

    auto logsAfterThird = vde::tools::RunLogStorage::loadRecentRuns(targetId);
    ASSERT_TRUE(logsAfterThird[0].has_value());
    ASSERT_TRUE(logsAfterThird[1].has_value());
    EXPECT_EQ(logsAfterThird[0]->output, "run three");
    EXPECT_EQ(logsAfterThird[1]->output, "run two");
    EXPECT_EQ(logsAfterThird[0]->exitCode, 2);
    EXPECT_EQ(logsAfterThird[1]->exitCode, 1);
}

TEST_F(VLauncherRunLogStorageTest, SaveLatestRunFailsWhenStorageIsNotInitialized) {
    vde::StorageManager::getInstance().shutdown();

    vde::tools::StoredRunLog run;
    run.timestamp = "2026-03-06 10:00:00";
    run.exitCode = 0;
    run.commandLine = "\"vde_triangle.exe\"";
    run.output = "hello";

    std::string error;
    EXPECT_FALSE(vde::tools::RunLogStorage::saveLatestRun("target_not_init", run, error));
    EXPECT_FALSE(error.empty());
}

TEST_F(VLauncherRunLogStorageTest, LoadRecentRunsIgnoresCorruptBlobData) {
    const std::string targetId = "target_corrupt_blob";

    const std::vector<uint8_t> corruptBlob = {0xAA, 0xBB, 0xCC};
    ASSERT_TRUE(vde::StorageManager::getInstance().setBinData(
        "vlauncher.runlog." + targetId + ".history", corruptBlob));

    auto logs = vde::tools::RunLogStorage::loadRecentRuns(targetId);
    EXPECT_FALSE(logs[0].has_value());
    EXPECT_FALSE(logs[1].has_value());
}

}  // namespace vde::test
