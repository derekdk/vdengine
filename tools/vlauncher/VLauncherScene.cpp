#include "VLauncherScene.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

#include <vde/api/StorageManager.h>

namespace vde::tools {

VLauncherScene::VLauncherScene(ToolMode mode) : BaseToolScene(mode) {}

VLauncherScene::~VLauncherScene() {
    clearActiveRuns();

    if (m_scanner) {
        m_scanner->stop();
    }
}

void VLauncherScene::onEnter() {
    setBackgroundColor(vde::Color::fromHex(0x111827));

    auto* camera = new vde::OrbitCamera(vde::Position(0.0f, 0.0f, 0.0f), 10.0f, 30.0f, 45.0f);
    setCamera(camera);

    m_scanner = std::make_unique<ExecutableScanner>(std::filesystem::current_path());
    m_scanner->start();

    bool storageOk = vde::StorageManager::getInstance().init_storage("vde_vlauncher");
    if (storageOk) {
        addConsoleMessage("Run-log storage initialized (app='vde_vlauncher').");
    } else {
        addConsoleMessage("WARNING: Failed to initialize run-log storage.");
    }

    addConsoleMessage("VLauncher started. Monitoring examples/tools for executable updates.");
}

void VLauncherScene::onExit() {
    if (m_scanner) {
        m_scanner->stop();
    }

    clearActiveRuns();
}

void VLauncherScene::update(float deltaTime) {
    BaseToolScene::update(deltaTime);

    if (m_scanner) {
        m_snapshot = m_scanner->getSnapshot();
    }

    updateActiveRuns();

    (void)deltaTime;
}

void VLauncherScene::drawDebugUI() {
    ImGui::SetNextWindowPos(ImVec2(16, 16), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(1220, 680), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("VLauncher")) {
        ImGui::End();
        return;
    }

    if (m_snapshot.repositoryRoot.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f),
                           "Repository root not found. Run this tool from a VDE build output.");
        ImGui::End();
        return;
    }

    ImGui::Text("Repository: %s", m_snapshot.repositoryRoot.string().c_str());
    ImGui::Text("Last scan: %s", formatTimestamp(m_snapshot.scanTime).c_str());
    ImGui::Text("Git: %s", m_snapshot.gitAvailable ? "available" : "not available");

    if (ImGui::Button("Refresh now") && m_scanner) {
        m_scanner->requestRefresh();
    }
    ImGui::SameLine();
    ImGui::Checkbox("Show up-to-date", &m_showUpToDate);
    ImGui::SameLine();
    ImGui::Checkbox("Show missing source", &m_showMissingSource);

    ImGui::Separator();

    auto entries = getSortedEntries();
    ImGui::Text("Detected launch targets: %d", static_cast<int>(entries.size()));
    ImGui::Text("Active launches: %d", static_cast<int>(m_activeRuns.size()));

    ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders |
                            ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY;

    if (ImGui::BeginTable("launch_table", 9, flags, ImVec2(0.0f, 0.0f))) {
        ImGui::TableSetupColumn("Target", ImGuiTableColumnFlags_WidthStretch, 2.3f);
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("Executable Age", ImGuiTableColumnFlags_WidthFixed, 130.0f);
        ImGui::TableSetupColumn("Source Age", ImGuiTableColumnFlags_WidthFixed, 120.0f);
        ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 220.0f);
        ImGui::TableSetupColumn("Git", ImGuiTableColumnFlags_WidthFixed, 180.0f);
        ImGui::TableSetupColumn("Path", ImGuiTableColumnFlags_WidthStretch, 3.6f);
        ImGui::TableSetupColumn("Run", ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ImGui::TableSetupColumn("Logs", ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ImGui::TableHeadersRow();

        auto now = std::chrono::system_clock::now();

        for (const auto& entry : entries) {
            const std::string targetId = buildTargetId(entry);

            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            const bool selected = (targetId == m_selectedTargetId);
            std::string targetLabel = entry.targetName + "##target_" + targetId;
            if (ImGui::Selectable(targetLabel.c_str(), selected, ImGuiSelectableFlags_AllowOverlap)) {
                selectTargetForLogView(entry);
            }

            if (ImGui::BeginPopupContextItem(("target_menu_" + targetId).c_str())) {
                if (entry.sourceFound) {
                    if (ImGui::MenuItem("Open in VS Code")) {
                        auto sourceFile = findMainSourceFile(entry.sourceDirectory);
                        std::string err;
                        if (ProcessLauncher::openFileInVSCode(sourceFile, err)) {
                            addConsoleMessage("Opening in VS Code: " + sourceFile.string());
                        } else {
                            addConsoleMessage("Failed to open VS Code: " + err);
                        }
                    }
                } else {
                    ImGui::BeginDisabled();
                    ImGui::MenuItem("Open in VS Code");
                    ImGui::EndDisabled();
                    ImGui::TextDisabled("(source directory not found)");
                }
                ImGui::EndPopup();
            }

            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(entry.kind.c_str());

            ImGui::TableSetColumnIndex(2);
            ImGui::TextUnformatted(formatAge(entry.executableWriteTime, now).c_str());

            ImGui::TableSetColumnIndex(3);
            if (entry.hasNewestSourceWriteTime) {
                ImGui::TextUnformatted(formatAge(entry.newestSourceWriteTime, now).c_str());
            } else {
                ImGui::TextUnformatted("-");
            }

            ImGui::TableSetColumnIndex(4);
            if (entry.outOfDate) {
                ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.35f, 1.0f), "%s",
                                   entry.outOfDateReason.c_str());
            } else {
                ImGui::TextColored(ImVec4(0.35f, 1.0f, 0.45f, 1.0f), "Up to date");
            }

            ImGui::TableSetColumnIndex(5);
            if (!entry.gitAvailable) {
                ImGui::TextUnformatted("Git unavailable");
            } else if (entry.sourceDirty) {
                ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "Uncommitted changes");
            } else if (entry.hasLastSourceCommitTime) {
                std::string age = formatAge(entry.lastSourceCommitTime, now);
                ImGui::Text("Last commit: %s", age.c_str());
            } else {
                ImGui::TextUnformatted("No history");
            }

            ImGui::TableSetColumnIndex(6);
            std::error_code relError;
            std::filesystem::path displayPath = std::filesystem::relative(
                entry.executablePath, m_snapshot.repositoryRoot, relError);
            if (relError || displayPath.empty()) {
                displayPath = entry.executablePath;
            }
            ImGui::TextUnformatted(displayPath.generic_string().c_str());

            ImGui::TableSetColumnIndex(7);
            std::string buttonLabel = "Launch##" + entry.executablePath.string();
            if (ImGui::Button(buttonLabel.c_str())) {
                std::string launchError;
                LaunchedProcess launchedProcess;
                if (ProcessLauncher::launchWithOutputCapture(entry.executablePath, launchedProcess,
                                                             launchError)) {
                    ActiveRun activeRun;
                    activeRun.entry = entry;
                    activeRun.targetId = targetId;
                    activeRun.process = std::move(launchedProcess);
                    m_activeRuns.push_back(std::move(activeRun));

                    addConsoleMessage("Launched: " + entry.targetName +
                                      " (capturing command line output)");
                    selectTargetForLogView(entry);
                } else {
                    addConsoleMessage("Launch failed for " + entry.targetName + ": " +
                                      launchError);
                }
            }

            ImGui::TableSetColumnIndex(8);
            std::string logsButton = "View##logs_" + targetId;
            if (ImGui::Button(logsButton.c_str())) {
                selectTargetForLogView(entry);
            }
        }

        ImGui::EndTable();
    }

    drawRunLogViewer();

    ImGui::End();
}

std::filesystem::path
VLauncherScene::findMainSourceFile(const std::filesystem::path& sourceDirectory) {
    // Prefer main.cpp as the canonical entry point
    auto mainCpp = sourceDirectory / "main.cpp";
    if (std::filesystem::exists(mainCpp)) {
        return mainCpp;
    }

    // Fall back to the first .cpp file in the directory (alphabetical)
    std::error_code ec;
    std::vector<std::filesystem::path> cppFiles;
    for (const auto& dirEntry : std::filesystem::directory_iterator(sourceDirectory, ec)) {
        if (dirEntry.is_regular_file() && dirEntry.path().extension() == ".cpp") {
            cppFiles.push_back(dirEntry.path());
        }
    }
    if (!cppFiles.empty()) {
        std::sort(cppFiles.begin(), cppFiles.end());
        return cppFiles.front();
    }

    // Last resort: open the source directory itself
    return sourceDirectory;
}

void VLauncherScene::executeCommand(const std::string& cmdLine) {
    if (cmdLine == "refresh") {
        if (m_scanner) {
            m_scanner->requestRefresh();
        }
        addConsoleMessage("Refresh requested");
        return;
    }

    addConsoleMessage("Unknown command: " + cmdLine);
    addConsoleMessage("Available commands: refresh");
}

std::string VLauncherScene::buildTargetId(const ExecutableEntry& entry) const {
    std::error_code error;
    std::filesystem::path root = m_snapshot.repositoryRoot;
    if (root.empty()) {
        root = std::filesystem::current_path(error);
        if (error || root.empty()) {
            root = std::filesystem::path(".");
        }
    }

    return RunLogStorage::buildTargetId(root, entry.executablePath);
}

void VLauncherScene::selectTargetForLogView(const ExecutableEntry& entry) {
    m_selectedTargetId = buildTargetId(entry);
    m_selectedTargetName = entry.targetName;
    refreshSelectedRunLogs();
}

void VLauncherScene::refreshSelectedRunLogs() {
    if (m_selectedTargetId.empty()) {
        m_selectedTargetRuns = {};
        return;
    }

    m_selectedTargetRuns = RunLogStorage::loadRecentRuns(m_selectedTargetId);
}

void VLauncherScene::drawRunLogViewer() {
    ImGui::Separator();
    ImGui::TextUnformatted("Run Logs");

    if (m_selectedTargetId.empty()) {
        ImGui::TextDisabled("Select a target row or click View to inspect stored run logs.");
        return;
    }

    ImGui::Text("Target: %s", m_selectedTargetName.c_str());
    if (ImGui::Button("Reload logs")) {
        refreshSelectedRunLogs();
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear selection")) {
        m_selectedTargetId.clear();
        m_selectedTargetName.clear();
        m_selectedTargetRuns = {};
        return;
    }

    if (!m_selectedTargetRuns[0].has_value() && !m_selectedTargetRuns[1].has_value()) {
        ImGui::TextDisabled("No captured runs saved for this target yet.");
        return;
    }

    for (size_t slot = 0; slot < m_selectedTargetRuns.size(); ++slot) {
        const char* slotName = (slot == 0) ? "Latest run" : "Previous run";
        if (!m_selectedTargetRuns[slot].has_value()) {
            ImGui::TextDisabled("%s: (none)", slotName);
            continue;
        }

        const auto& run = *m_selectedTargetRuns[slot];
        std::string headerId = std::string(slotName) + "##slot_" + std::to_string(slot);
        ImGuiTreeNodeFlags nodeFlags = (slot == 0) ? ImGuiTreeNodeFlags_DefaultOpen : 0;
        if (!ImGui::CollapsingHeader(headerId.c_str(), nodeFlags)) {
            continue;
        }

        ImGui::Text("Timestamp: %s", run.timestamp.c_str());
        ImGui::Text("Exit code: %d", run.exitCode);
        ImGui::TextWrapped("Command: %s", run.commandLine.c_str());

        std::string childId = "run_output_##" + std::to_string(slot);
        if (ImGui::BeginChild(childId.c_str(), ImVec2(0.0f, 170.0f), true,
                              ImGuiWindowFlags_HorizontalScrollbar)) {
            if (run.output.empty()) {
                ImGui::TextDisabled("(no command line output)");
            } else {
                ImGui::TextUnformatted(run.output.c_str());
            }
        }
        ImGui::EndChild();
    }
}

void VLauncherScene::updateActiveRuns() {
    if (m_activeRuns.empty()) {
        return;
    }

    size_t index = 0;
    while (index < m_activeRuns.size()) {
        auto& activeRun = m_activeRuns[index];

        bool completed = false;
        uint32_t exitCode = 0;
        std::string pollError;
        if (!ProcessLauncher::pollCompletion(activeRun.process, completed, exitCode, pollError)) {
            addConsoleMessage("Failed to poll process for " + activeRun.entry.targetName + ": " +
                              pollError);

            ProcessLauncher::release(activeRun.process);
            std::error_code removeError;
            std::filesystem::remove(activeRun.process.outputPath, removeError);
            m_activeRuns.erase(m_activeRuns.begin() + static_cast<std::ptrdiff_t>(index));
            continue;
        }

        if (!completed) {
            ++index;
            continue;
        }

        std::string output;
        std::string outputError;
        if (!ProcessLauncher::readOutputFile(activeRun.process.outputPath, output, outputError)) {
            output = "[vlauncher] Failed to read command line output: " + outputError;
        }

        std::error_code removeError;
        std::filesystem::remove(activeRun.process.outputPath, removeError);

        StoredRunLog runLog;
        runLog.timestamp = formatTimestamp(std::chrono::system_clock::now());
        runLog.exitCode = static_cast<int>(exitCode);
        runLog.commandLine = activeRun.process.commandLine;
        runLog.output = truncateOutput(output, kMaxStoredOutputBytes);

        std::string saveError;
        if (RunLogStorage::saveLatestRun(activeRun.targetId, runLog, saveError)) {
            addConsoleMessage("Run finished: " + activeRun.entry.targetName +
                              " (exit code " + std::to_string(runLog.exitCode) + ")");
        } else {
            addConsoleMessage("Run finished for " + activeRun.entry.targetName +
                              " but saving logs failed: " + saveError);
        }

        if (activeRun.targetId == m_selectedTargetId) {
            refreshSelectedRunLogs();
        }

        ProcessLauncher::release(activeRun.process);
        m_activeRuns.erase(m_activeRuns.begin() + static_cast<std::ptrdiff_t>(index));
    }
}

void VLauncherScene::clearActiveRuns() {
    for (auto& activeRun : m_activeRuns) {
        ProcessLauncher::release(activeRun.process);
        std::error_code removeError;
        std::filesystem::remove(activeRun.process.outputPath, removeError);
    }
    m_activeRuns.clear();
}

std::vector<ExecutableEntry> VLauncherScene::getSortedEntries() const {
    std::vector<ExecutableEntry> filtered;

    for (const auto& entry : m_snapshot.entries) {
        if (!m_showUpToDate && !entry.outOfDate) {
            continue;
        }

        if (!m_showMissingSource && !entry.sourceFound) {
            continue;
        }

        filtered.push_back(entry);
    }

    const auto now = std::chrono::system_clock::now();

    auto gitRecency = [now](const ExecutableEntry& entry) {
        if (entry.sourceDirty) {
            return now;
        }
        if (entry.hasLastSourceCommitTime) {
            return entry.lastSourceCommitTime;
        }
        return std::chrono::system_clock::time_point{};
    };

    std::sort(filtered.begin(), filtered.end(),
              [gitRecency](const ExecutableEntry& a, const ExecutableEntry& b) {
                  auto aRecency = gitRecency(a);
                  auto bRecency = gitRecency(b);
                  if (aRecency != bRecency) {
                      return aRecency > bRecency;
                  }
                  if (a.outOfDate != b.outOfDate) {
                      return a.outOfDate > b.outOfDate;
                  }
                  if (a.kind != b.kind) {
                      return a.kind < b.kind;
                  }
                  return a.targetName < b.targetName;
              });

    return filtered;
}

std::string VLauncherScene::truncateOutput(const std::string& output, size_t maxBytes) {
    if (output.size() <= maxBytes) {
        return output;
    }

    const std::string suffix = "\n\n[output truncated by VLauncher]\n";
    if (maxBytes <= suffix.size()) {
        return suffix.substr(0, maxBytes);
    }

    return output.substr(0, maxBytes - suffix.size()) + suffix;
}

std::string VLauncherScene::formatAge(std::chrono::system_clock::time_point from,
                                      std::chrono::system_clock::time_point now) {
    if (from.time_since_epoch().count() == 0) {
        return "unknown";
    }

    auto diff = now - from;
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(diff).count();
    if (seconds < 0) {
        seconds = 0;
    }

    if (seconds < 60) {
        return std::to_string(seconds) + "s";
    }

    auto minutes = seconds / 60;
    if (minutes < 60) {
        return std::to_string(minutes) + "m";
    }

    auto hours = minutes / 60;
    if (hours < 24) {
        return std::to_string(hours) + "h";
    }

    auto days = hours / 24;
    return std::to_string(days) + "d";
}

std::string VLauncherScene::formatTimestamp(std::chrono::system_clock::time_point value) {
    if (value.time_since_epoch().count() == 0) {
        return "never";
    }

    std::time_t timeValue = std::chrono::system_clock::to_time_t(value);
    std::tm localTime{};
#ifdef _WIN32
    localtime_s(&localTime, &timeValue);
#else
    localtime_r(&timeValue, &localTime);
#endif

    std::ostringstream stream;
    stream << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S");
    return stream.str();
}

}  // namespace vde::tools
