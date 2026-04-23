/**
 * @file VLauncherUtilities_test.cpp
 * @brief Unit tests for VLauncher executable scanning and process launch helpers.
 */

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#include "../tools/vlauncher/ExecutableScanner.h"
#include "../tools/vlauncher/ProcessLauncher.h"
#include <gtest/gtest.h>

#if defined(_WIN32)
#include <windows.h>
#undef min
#undef max
#endif

namespace vde::test {

namespace {

using namespace std::chrono_literals;

std::string makeUniqueSuffix() {
    return std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
}

void writeTextFile(const std::filesystem::path& path, const std::string& contents) {
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    ASSERT_FALSE(error) << "Failed to create parent directory for " << path.string();

    std::ofstream file(path, std::ios::binary);
    ASSERT_TRUE(file.is_open()) << "Failed to open " << path.string();

    file << contents;
    file.close();

    ASSERT_TRUE(file.good()) << "Failed to write " << path.string();
}

vde::tools::ScanSnapshot waitForSnapshotEntries(vde::tools::ExecutableScanner& scanner,
                                                size_t minimumEntries) {
    for (int attempt = 0; attempt < 100; ++attempt) {
        auto snapshot = scanner.getSnapshot();
        if (snapshot && snapshot->entries.size() >= minimumEntries) {
            return *snapshot;
        }
        std::this_thread::sleep_for(20ms);
    }

    auto snapshot = scanner.getSnapshot();
    return snapshot ? *snapshot : vde::tools::ScanSnapshot{};
}

#if defined(_WIN32)
std::filesystem::path findExecutableOnPath(const char* executableName) {
    char buffer[32768] = {};
    DWORD length = SearchPathA(nullptr, executableName, nullptr,
                               static_cast<DWORD>(std::size(buffer)), buffer, nullptr);
    if (length == 0 || length >= std::size(buffer)) {
        return {};
    }

    return std::filesystem::path(buffer);
}
#endif

class VLauncherUtilitiesTest : public ::testing::Test {
  protected:
    void SetUp() override {
        std::error_code error;
        m_tempRoot = std::filesystem::temp_directory_path(error) /
                     ("vde_vlauncher_test_" + makeUniqueSuffix());
        ASSERT_FALSE(error) << "Failed to query temporary directory";

        std::filesystem::create_directories(m_tempRoot, error);
        ASSERT_FALSE(error) << "Failed to create test directory " << m_tempRoot.string();
    }

    void TearDown() override {
        std::error_code error;
        std::filesystem::remove_all(m_tempRoot, error);
    }

    std::filesystem::path m_tempRoot;
};

TEST_F(VLauncherUtilitiesTest, ScannerRejectsSmokeScriptPathsFromVdeToml) {
    const std::filesystem::path repoRoot = m_tempRoot / "repo";
    const std::filesystem::path sourceDir = repoRoot / "examples" / "sample";
    const std::filesystem::path gameSourceDir = repoRoot / "games" / "sample_game";
    const std::filesystem::path executablePath =
        repoRoot / "build_ninja" / "examples" / "vde_sample.exe";
    const std::filesystem::path gameExecutablePath =
        repoRoot / "build_ninja" / "games" / "sample_game" / "vde_sample_game.exe";

    std::error_code error;
    std::filesystem::create_directories(sourceDir, error);
    ASSERT_FALSE(error);
    std::filesystem::create_directories(gameSourceDir, error);
    ASSERT_FALSE(error);
    std::filesystem::create_directories(repoRoot / "tools", error);
    ASSERT_FALSE(error);
    std::filesystem::create_directories(repoRoot / "src", error);
    ASSERT_FALSE(error);
    std::filesystem::create_directories(executablePath.parent_path(), error);
    ASSERT_FALSE(error);
    std::filesystem::create_directories(gameExecutablePath.parent_path(), error);
    ASSERT_FALSE(error);

    writeTextFile(repoRoot / "CMakeLists.txt",
                  "cmake_minimum_required(VERSION 3.20)\nproject(VLauncherScannerTest)\n");
    writeTextFile(sourceDir / "CMakeLists.txt", "add_executable(vde_sample \"main.cpp\")\n");
    writeTextFile(sourceDir / "main.cpp", "int main() { return 0; }\n");
    writeTextFile(repoRoot / "games" / "CMakeLists.txt", "add_subdirectory(sample_game)\n");
    writeTextFile(gameSourceDir / "CMakeLists.txt", "add_vde_game(vde_sample_game \"main.cpp\")\n");
    writeTextFile(gameSourceDir / "main.cpp", "int main() { return 0; }\n");
    writeTextFile(sourceDir / "vde.toml",
                  "[smoke]\n"
                  "scripts = [\"fallback.vdescript\"]\n\n"
                  "[smoke.vde_sample]\n"
                  "scripts = [\"smoke_valid.vdescript\", \"../escape.vdescript\", "
                  "\"nested/escape.vdescript\", \"..\\\\windows_escape.vdescript\", "
                  "\"\"]\n");
    writeTextFile(gameSourceDir / "vde.toml", "[smoke]\n"
                                              "scripts = [\"smoke_game.vdescript\"]\n"
                                              "priority = 2\n");
    writeTextFile(executablePath, "");
    writeTextFile(gameExecutablePath, "");

    vde::tools::ExecutableScanner scanner(repoRoot, std::chrono::seconds(1),
                                          std::chrono::seconds(1));
    scanner.start();
    const auto snapshot = waitForSnapshotEntries(scanner, 2);
    scanner.stop();

    ASSERT_EQ(snapshot.entries.size(), 2u);

    const auto sampleIt = std::find_if(
        snapshot.entries.begin(), snapshot.entries.end(),
        [](const vde::tools::ExecutableEntry& entry) { return entry.targetName == "vde_sample"; });
    ASSERT_NE(sampleIt, snapshot.entries.end());
    const std::vector<std::string> expectedScripts = {"smoke_valid.vdescript"};
    EXPECT_EQ(sampleIt->smokeScripts, expectedScripts);
    EXPECT_EQ(sampleIt->kind, "Example");

    const auto gameIt = std::find_if(snapshot.entries.begin(), snapshot.entries.end(),
                                     [](const vde::tools::ExecutableEntry& entry) {
                                         return entry.targetName == "vde_sample_game";
                                     });
    ASSERT_NE(gameIt, snapshot.entries.end());
    const std::vector<std::string> expectedGameScripts = {"smoke_game.vdescript"};
    EXPECT_EQ(gameIt->smokeScripts, expectedGameScripts);
    EXPECT_EQ(gameIt->smokePriority, 2);
    EXPECT_EQ(gameIt->kind, "Game");
}

TEST_F(VLauncherUtilitiesTest, ScannerIncludesUnbuiltGameTargetsFromSourceMap) {
    const std::filesystem::path repoRoot = m_tempRoot / "repo";
    const std::filesystem::path gameSourceDir = repoRoot / "games" / "sample_game";

    std::error_code error;
    std::filesystem::create_directories(repoRoot / "examples", error);
    ASSERT_FALSE(error);
    std::filesystem::create_directories(gameSourceDir, error);
    ASSERT_FALSE(error);
    std::filesystem::create_directories(repoRoot / "tools", error);
    ASSERT_FALSE(error);
    std::filesystem::create_directories(repoRoot / "src", error);
    ASSERT_FALSE(error);

    writeTextFile(repoRoot / "CMakeLists.txt",
                  "cmake_minimum_required(VERSION 3.20)\nproject(VLauncherScannerTest)\n");
    writeTextFile(repoRoot / "games" / "CMakeLists.txt", "add_subdirectory(sample_game)\n");
    writeTextFile(gameSourceDir / "CMakeLists.txt", "add_vde_game(vde_sample_game\n"
                                                    "    main.cpp\n"
                                                    ")\n");
    writeTextFile(gameSourceDir / "main.cpp", "int main() { return 0; }\n");
    writeTextFile(gameSourceDir / "vde.toml", "[smoke]\n"
                                              "scripts = [\"smoke_game.vdescript\"]\n"
                                              "priority = 2\n");

    vde::tools::ExecutableScanner scanner(repoRoot, std::chrono::seconds(1),
                                          std::chrono::seconds(1));
    scanner.start();
    const auto snapshot = waitForSnapshotEntries(scanner, 1);
    scanner.stop();

    ASSERT_EQ(snapshot.entries.size(), 1u);
    const auto& entry = snapshot.entries.front();
    EXPECT_EQ(entry.targetName, "vde_sample_game");
    EXPECT_EQ(entry.kind, "Game");
    EXPECT_TRUE(entry.sourceFound);
    EXPECT_FALSE(entry.executableFound);
    EXPECT_TRUE(entry.executablePath.empty());
    EXPECT_TRUE(entry.outOfDate);
    EXPECT_EQ(entry.outOfDateReason, "Executable missing");
    const std::vector<std::string> expectedScripts = {"smoke_game.vdescript"};
    EXPECT_EQ(entry.smokeScripts, expectedScripts);
    EXPECT_EQ(entry.smokePriority, 2);
}

#if defined(_WIN32)
TEST_F(VLauncherUtilitiesTest, LaunchWithOutputCapturePreservesExplicitEmptyArguments) {
    const std::filesystem::path powerShellPath = findExecutableOnPath("powershell.exe");
    ASSERT_FALSE(powerShellPath.empty()) << "powershell.exe was not found on PATH";

    const std::filesystem::path scriptPath = m_tempRoot / "echo_args.ps1";
    writeTextFile(scriptPath, "param(\n"
                              "    [Parameter(ValueFromRemainingArguments = $true)]\n"
                              "    [string[]]$Values\n"
                              ")\n"
                              "Write-Output (\"COUNT=\" + $Values.Count)\n"
                              "foreach ($value in $Values) {\n"
                              "    Write-Output (\"ARG=[\" + $value + \"]\")\n"
                              "}\n");

    vde::tools::LaunchedProcess launchedProcess;
    std::string error;
    const std::vector<std::string> extraArgs = {
        "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", scriptPath.string(), "alpha", "",
        "omega"};

    ASSERT_TRUE(vde::tools::ProcessLauncher::launchWithOutputCapture(
        powerShellPath, launchedProcess, error, extraArgs))
        << error;

    bool completed = false;
    uint32_t exitCode = 0;
    for (int attempt = 0; attempt < 200 && !completed; ++attempt) {
        ASSERT_TRUE(vde::tools::ProcessLauncher::pollCompletion(launchedProcess, completed,
                                                                exitCode, error))
            << error;
        if (!completed) {
            std::this_thread::sleep_for(20ms);
        }
    }

    ASSERT_TRUE(completed) << "PowerShell test process did not exit in time";

    std::string output;
    ASSERT_TRUE(
        vde::tools::ProcessLauncher::readOutputFile(launchedProcess.outputPath, output, error))
        << error;

    vde::tools::ProcessLauncher::release(launchedProcess);

    std::error_code removeError;
    std::filesystem::remove(launchedProcess.outputPath, removeError);

    EXPECT_EQ(exitCode, 0u) << output;
    EXPECT_NE(output.find("COUNT=3"), std::string::npos) << output;
    EXPECT_NE(output.find("ARG=[alpha]"), std::string::npos) << output;
    EXPECT_NE(output.find("ARG=[]"), std::string::npos) << output;
    EXPECT_NE(output.find("ARG=[omega]"), std::string::npos) << output;
    EXPECT_NE(launchedProcess.commandLine.find("\"\""), std::string::npos)
        << launchedProcess.commandLine;
}
#else
TEST_F(VLauncherUtilitiesTest, LaunchWithOutputCapturePreservesExplicitEmptyArguments) {
    GTEST_SKIP() << "ProcessLauncher output capture is only implemented on Windows.";
}
#endif

}  // namespace

}  // namespace vde::test