#pragma once

#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace vde::tools {

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
};

struct ScanSnapshot {
    std::filesystem::path repositoryRoot;
    std::chrono::system_clock::time_point scanTime{};
    bool gitAvailable = false;
    std::vector<ExecutableEntry> entries;
};

class ExecutableScanner {
  public:
    explicit ExecutableScanner(std::filesystem::path startPath,
                               std::chrono::seconds interval = std::chrono::seconds(4));
    ~ExecutableScanner();

    void start();
    void stop();
    void requestRefresh();

    ScanSnapshot getSnapshot() const;

  private:
    std::filesystem::path m_startPath;
    std::chrono::seconds m_interval;

    mutable std::mutex m_snapshotMutex;
    ScanSnapshot m_snapshot;

    std::thread m_worker;
    mutable std::mutex m_controlMutex;
    std::condition_variable m_controlCv;
    bool m_running = false;
    bool m_forceRefresh = false;

    void workerLoop();
    ScanSnapshot buildSnapshot() const;

    static std::filesystem::path findRepositoryRoot(const std::filesystem::path& fromPath);
    static std::vector<std::filesystem::path>
    findExecutablePaths(const std::filesystem::path& repoRoot);

    static std::unordered_map<std::string, std::filesystem::path>
    buildTargetSourceMap(const std::filesystem::path& repoRoot);

    static std::chrono::system_clock::time_point
    fileTimeToSystemClock(const std::filesystem::file_time_type& fileTime);

    static std::optional<std::chrono::system_clock::time_point>
    newestSourceTimestamp(const std::filesystem::path& sourceDir);

    static std::string inferKind(const std::filesystem::path& sourceDir,
                                 const std::filesystem::path& repoRoot);
};

}  // namespace vde::tools
