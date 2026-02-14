#include "VLauncherScene.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

#include "ProcessLauncher.h"

namespace vde::tools {

VLauncherScene::VLauncherScene(ToolMode mode) : BaseToolScene(mode) {}

VLauncherScene::~VLauncherScene() {
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

    addConsoleMessage("VLauncher started. Monitoring examples/tools for executable updates.");
}

void VLauncherScene::onExit() {
    if (m_scanner) {
        m_scanner->stop();
    }
}

void VLauncherScene::update(float deltaTime) {
    BaseToolScene::update(deltaTime);

    if (m_scanner) {
        m_snapshot = m_scanner->getSnapshot();
    }

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

    ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders |
                            ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY;

    if (ImGui::BeginTable("launch_table", 8, flags, ImVec2(0.0f, 0.0f))) {
        ImGui::TableSetupColumn("Target", ImGuiTableColumnFlags_WidthStretch, 2.3f);
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("Executable Age", ImGuiTableColumnFlags_WidthFixed, 130.0f);
        ImGui::TableSetupColumn("Source Age", ImGuiTableColumnFlags_WidthFixed, 120.0f);
        ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 220.0f);
        ImGui::TableSetupColumn("Git", ImGuiTableColumnFlags_WidthFixed, 180.0f);
        ImGui::TableSetupColumn("Path", ImGuiTableColumnFlags_WidthStretch, 3.6f);
        ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ImGui::TableHeadersRow();

        auto now = std::chrono::system_clock::now();

        for (const auto& entry : entries) {
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(entry.targetName.c_str());

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
            ImGui::TextUnformatted(entry.executablePath.string().c_str());

            ImGui::TableSetColumnIndex(7);
            std::string buttonLabel = "Launch##" + entry.executablePath.string();
            if (ImGui::Button(buttonLabel.c_str())) {
                std::string error;
                if (ProcessLauncher::launchDetached(entry.executablePath, error)) {
                    addConsoleMessage("Launched: " + entry.targetName);
                } else {
                    addConsoleMessage("Launch failed for " + entry.targetName + ": " + error);
                }
            }
        }

        ImGui::EndTable();
    }

    ImGui::End();
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

    std::sort(filtered.begin(), filtered.end(),
              [](const ExecutableEntry& a, const ExecutableEntry& b) {
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
