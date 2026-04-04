#pragma once

#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace vde::tools {

class GitUtils;

struct ExecutableEntry {
    std::string targetName;
    std::string kind;
    std::filesystem::path executablePath;
    std::filesystem::path sourceDirectory;

    bool sourceFound = false;
    bool outOfDate = false;
    bool sourceNewerThanExecutable = false;
    bool sourceDirty = false;
    bool gitAvailable = false;

    std::string outOfDateReason;

    std::chrono::system_clock::time_point executableWriteTime{};
    std::chrono::system_clock::time_point newestSourceWriteTime{};
    std::chrono::system_clock::time_point lastSourceCommitTime{};

    bool hasNewestSourceWriteTime = false;
    bool hasLastSourceCommitTime = false;

    // Smoke test script filenames discovered from vde.toml in the source directory.
    // Stored as basenames (e.g. "smoke_breakout.vdescript"), resolved by VLauncher
    // against the smoketests/scripts/ directory.
    std::vector<std::string> smokeScripts;

    // Smoke test priority (1 = core / default run, 2 = extended).
    // 0 means no priority was specified in vde.toml.
    int smokePriority = 0;
};

struct ScanSnapshot {
    std::filesystem::path repositoryRoot;
    std::chrono::system_clock::time_point scanTime{};
    bool gitAvailable = false;
    std::vector<ExecutableEntry> entries;
};

class ExecutableScanner {
  public:
    /// Default idle interval is 10 seconds; drops to fast interval after changes detected.
    explicit ExecutableScanner(std::filesystem::path startPath,
                               std::chrono::seconds idleInterval = std::chrono::seconds(10),
                               std::chrono::seconds fastInterval = std::chrono::seconds(4));
    ~ExecutableScanner();

    void start();
    void stop();
    void requestRefresh();

    /// Returns a shared pointer to the latest snapshot (mutex-protected, no deep copy).
    std::shared_ptr<const ScanSnapshot> getSnapshot() const;

  private:
    std::filesystem::path m_startPath;
    std::chrono::seconds m_idleInterval;
    std::chrono::seconds m_fastInterval;

    // Shared-pointer snapshot: UI reads via mutex-protected load, scanner writes via
    // mutex-protected store.
    std::shared_ptr<const ScanSnapshot> m_snapshot;
    mutable std::mutex m_snapshotMutex;

    std::thread m_worker;
    mutable std::mutex m_controlMutex;
    std::condition_variable m_controlCv;
    bool m_running = false;
    bool m_forceRefresh = false;

    // Adaptive interval state (only touched from the worker thread).
    int m_unchangedScanCount = 0;
    static constexpr int kFastToIdleThreshold = 3;

    // Cached target-source map to avoid re-parsing CMakeLists.txt every cycle.
    struct CachedTargetSourceMap {
        std::unordered_map<std::string, std::filesystem::path> targetMap;
        // Timestamps of CMakeLists.txt files at the time of last parse.
        std::unordered_map<std::string, std::filesystem::file_time_type> cmakeTimestamps;
    };
    CachedTargetSourceMap m_cachedTargetMap;

    // Persistent GitUtils instance: reused across scan cycles, owned by this scanner.
    std::unique_ptr<GitUtils> m_git;
    std::filesystem::path m_gitRoot;

    void workerLoop();
    ScanSnapshot buildSnapshot();

    bool isCmakeMapStale(const std::filesystem::path& repoRoot) const;

    static std::filesystem::path findRepositoryRoot(const std::filesystem::path& fromPath);
    static std::vector<std::filesystem::path>
    findExecutablePaths(const std::filesystem::path& repoRoot);

    static std::unordered_map<std::string, std::filesystem::path>
    buildTargetSourceMap(const std::filesystem::path& repoRoot);

    static std::chrono::system_clock::time_point
    fileTimeToSystemClock(const std::filesystem::file_time_type& fileTime);

    static std::optional<std::chrono::system_clock::time_point>
    newestSourceTimestamp(const std::filesystem::path& sourceDir);

    struct SmokeMetadata {
        std::vector<std::string> scripts;
        int priority = 0;
    };

    static SmokeMetadata loadSmokeMetadata(const std::filesystem::path& sourceDir,
                                           const std::string& targetName);

    static std::string inferKind(const std::filesystem::path& sourceDir,
                                 const std::filesystem::path& repoRoot);

    /// Collect the file_time_type of every CMakeLists.txt under examples/ and tools/.
    static std::unordered_map<std::string, std::filesystem::file_time_type>
    collectCmakeTimestamps(const std::filesystem::path& repoRoot);
};

}  // namespace vde::tools
