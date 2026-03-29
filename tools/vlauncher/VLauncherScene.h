#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
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

    struct TargetGroup {
        std::string targetName;
        std::vector<ExecutableEntry> entries;
        size_t defaultIndex = 0;

        const ExecutableEntry& defaultEntry() const { return entries[defaultIndex]; }
    };

    std::unique_ptr<ExecutableScanner> m_scanner;
    std::shared_ptr<const ScanSnapshot> m_snapshot;
    std::vector<ActiveRun> m_activeRuns;

    std::string m_selectedTargetId;
    std::string m_selectedTargetName;
    std::array<std::optional<StoredRunLog>, 2> m_selectedTargetRuns;
    bool m_showRunLogDialog = false;

    bool m_showUpToDate = true;
    bool m_showMissingSource = true;
    bool m_compactView = false;
    bool m_compactResizePending = false;
    bool m_forceLauncherWindowSize = false;
    ImVec2 m_forcedLauncherWindowSize = ImVec2(0.0f, 0.0f);

    // Grouping state
    std::unordered_set<std::string> m_expandedGroups;
    std::unordered_map<std::string, std::string>
        m_groupDefaults;  // targetName -> relative exe path
    bool m_groupDefaultsLoaded = false;

    static constexpr size_t kMaxStoredOutputBytes = 256 * 1024;
    static constexpr const char* kCompactViewStorageKey = "vlauncher.ui.compactView";
    static constexpr const char* kGroupDefaultKeyPrefix = "vlauncher.group.default.";
    static constexpr float kCompactAppWidth = 560.0f;
    static constexpr float kCompactAppHeight = 440.0f;

    std::vector<TargetGroup> buildGroupedEntries() const;
    void resolveGroupDefault(TargetGroup& group) const;
    void saveGroupDefault(const std::string& targetName, const ExecutableEntry& entry);
    void loadGroupDefaults(const std::vector<TargetGroup>& groups);

    std::vector<ExecutableEntry> getSortedEntries() const;
    std::string buildTargetId(const ExecutableEntry& entry) const;
    void launchEntry(const ExecutableEntry& entry, const std::string& targetId);

    void selectTargetForLogView(const ExecutableEntry& entry);
    void refreshSelectedRunLogs();
    void drawRunLogViewer();
    void loadViewPreferences();
    void saveCompactViewPreference() const;
    void applyCompactWindowPresetIfRequested();

    void updateActiveRuns();
    void clearActiveRuns();

    void launchEntryWithSmokeTest(const ExecutableEntry& entry, const std::string& targetId,
                                  const std::string& scriptName);
    void drawEntryContextMenu(const ExecutableEntry& entry, const std::string& popupId);

    static std::filesystem::path findMainSourceFile(const std::filesystem::path& sourceDirectory);
    static std::string truncateOutput(const std::string& output, size_t maxBytes);

    static std::string formatAge(std::chrono::system_clock::time_point from,
                                 std::chrono::system_clock::time_point now);
    static std::string formatTimestamp(std::chrono::system_clock::time_point value);
    static void drawAgeIndicator(std::chrono::system_clock::time_point from,
                                 std::chrono::system_clock::time_point now);
};

}  // namespace vde::tools
