#pragma once

#include <array>
#include <chrono>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "../ToolBase.h"
#include "ExecutableScanner.h"
#include "ProcessLauncher.h"
#include "RunLogStorage.h"

namespace vde::tools {

class VLauncherScene : public BaseToolScene {
  public:
    explicit VLauncherScene(ToolMode mode = ToolMode::INTERACTIVE);
    ~VLauncherScene() override;

    void onEnter() override;
    void onExit() override;
    void update(float deltaTime) override;
    void drawDebugUI() override;

    void executeCommand(const std::string& cmdLine) override;

    std::string getToolName() const override { return "VLauncher"; }
    std::string getToolDescription() const override {
        return "Launch VDE examples/tools and monitor executable freshness";
    }

  private:
    struct ActiveRun {
        ExecutableEntry entry;
        std::string targetId;
        LaunchedProcess process;
    };

    std::unique_ptr<ExecutableScanner> m_scanner;
    ScanSnapshot m_snapshot;
    std::vector<ActiveRun> m_activeRuns;

    std::string m_selectedTargetId;
    std::string m_selectedTargetName;
    std::array<std::optional<StoredRunLog>, 2> m_selectedTargetRuns;
    bool m_showRunLogDialog = false;

    bool m_showUpToDate = true;
    bool m_showMissingSource = true;

    static constexpr size_t kMaxStoredOutputBytes = 256 * 1024;

    std::vector<ExecutableEntry> getSortedEntries() const;
    std::string buildTargetId(const ExecutableEntry& entry) const;

    void selectTargetForLogView(const ExecutableEntry& entry);
    void refreshSelectedRunLogs();
    void drawRunLogViewer();

    void updateActiveRuns();
    void clearActiveRuns();

    static std::filesystem::path findMainSourceFile(const std::filesystem::path& sourceDirectory);
    static std::string truncateOutput(const std::string& output, size_t maxBytes);

    static std::string formatAge(std::chrono::system_clock::time_point from,
                                 std::chrono::system_clock::time_point now);
    static std::string formatTimestamp(std::chrono::system_clock::time_point value);
};

}  // namespace vde::tools
