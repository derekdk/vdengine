#pragma once

#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "../ToolBase.h"
#include "ExecutableScanner.h"

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
    std::unique_ptr<ExecutableScanner> m_scanner;
    ScanSnapshot m_snapshot;

    bool m_showUpToDate = true;
    bool m_showMissingSource = true;

    std::vector<ExecutableEntry> getSortedEntries() const;

    static std::string formatAge(std::chrono::system_clock::time_point from,
                                 std::chrono::system_clock::time_point now);
    static std::string formatTimestamp(std::chrono::system_clock::time_point value);
};

}  // namespace vde::tools
