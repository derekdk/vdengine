#include "VLauncherScene.h"

#include <vde/api/StorageManager.h>

#include <algorithm>
#include <iomanip>
#include <map>
#include <sstream>

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
        loadViewPreferences();
        if (m_compactView) {
            m_compactResizePending = true;
        }
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

void VLauncherScene::loadViewPreferences() {
    auto& storage = vde::StorageManager::getInstance();
    if (!storage.isInitialized()) {
        return;
    }

    const auto compactValue = storage.getBinData<uint8_t>(kCompactViewStorageKey);
    if (compactValue.has_value()) {
        m_compactView = (*compactValue != 0);
    }
}

void VLauncherScene::saveCompactViewPreference() const {
    auto& storage = vde::StorageManager::getInstance();
    if (!storage.isInitialized()) {
        return;
    }

    const uint8_t compactValue = m_compactView ? 1u : 0u;
    storage.setBinData(kCompactViewStorageKey, compactValue);
}

void VLauncherScene::applyCompactWindowPresetIfRequested() {
    if (!m_compactResizePending) {
        return;
    }

    m_compactResizePending = false;

    auto* game = getGame();
    if (!game || !game->getWindow()) {
        return;
    }

    auto* window = game->getWindow();
    float dpiScale = window->getDPIScale();
    if (dpiScale <= 0.0f) {
        dpiScale = 1.0f;
    }

    uint32_t targetWidth = window->getWidth();
    uint32_t targetHeight = window->getHeight();

    if (!window->isFullscreen()) {
        targetWidth = static_cast<uint32_t>(kCompactAppWidth * dpiScale);
        targetHeight = static_cast<uint32_t>(kCompactAppHeight * dpiScale);
        game->scheduleWindowResize(targetWidth, targetHeight);
    }

    const float contentMargin = 32.0f * dpiScale;
    const float minPanelWidth = 320.0f * dpiScale;
    const float minPanelHeight = 280.0f * dpiScale;

    m_forcedLauncherWindowSize =
        ImVec2(std::max(minPanelWidth, static_cast<float>(targetWidth) - contentMargin),
               std::max(minPanelHeight, static_cast<float>(targetHeight) - contentMargin));
    m_forceLauncherWindowSize = true;
}

void VLauncherScene::update(float deltaTime) {
    BaseToolScene::update(deltaTime);

    // Apply pending window-size changes before ImGui/vulkan draw commands begin.
    applyCompactWindowPresetIfRequested();

    if (m_scanner) {
        m_snapshot = m_scanner->getSnapshot();
    }

    updateActiveRuns();

    (void)deltaTime;
}

void VLauncherScene::drawDebugUI() {
    ImGui::SetNextWindowPos(ImVec2(16, 16), ImGuiCond_FirstUseEver);
    if (m_forceLauncherWindowSize) {
        ImGui::SetNextWindowSize(m_forcedLauncherWindowSize, ImGuiCond_Always);
        m_forceLauncherWindowSize = false;
    } else {
        ImGui::SetNextWindowSize(ImVec2(1220, 680), ImGuiCond_FirstUseEver);
    }

    if (!ImGui::Begin("VLauncher")) {
        ImGui::End();
        drawRunLogViewer();
        return;
    }

    if (!m_snapshot || m_snapshot->repositoryRoot.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f),
                           "Repository root not found. Run this tool from a VDE build output.");
        ImGui::End();
        drawRunLogViewer();
        return;
    }

    ImGui::Text("Repository: %s", m_snapshot->repositoryRoot.string().c_str());
    ImGui::Text("Last scan: %s", formatTimestamp(m_snapshot->scanTime).c_str());
    ImGui::Text("Git: %s", m_snapshot->gitAvailable ? "available" : "not available");

    if (ImGui::Button("Refresh now") && m_scanner) {
        m_scanner->requestRefresh();
    }
    ImGui::SameLine();
    if (ImGui::Checkbox("Compact view", &m_compactView)) {
        saveCompactViewPreference();
        if (m_compactView) {
            m_compactResizePending = true;
        }
    }
    ImGui::SameLine();
    ImGui::Checkbox("Show up-to-date", &m_showUpToDate);
    ImGui::SameLine();
    ImGui::Checkbox("Show missing source", &m_showMissingSource);

    ImGui::Separator();

    auto groups = buildGroupedEntries();

    size_t totalEntries = 0;
    for (const auto& g : groups) {
        totalEntries += g.entries.size();
    }
    ImGui::Text("Detected launch targets: %d (%d groups)", static_cast<int>(totalEntries),
                static_cast<int>(groups.size()));
    ImGui::Text("Active launches: %d", static_cast<int>(m_activeRuns.size()));

    if (m_compactView) {
        ImGui::TextDisabled("Compact mode: double-click a group to launch the default.");

        ImGuiTableFlags compactFlags =
            ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY;

        if (ImGui::BeginTable("launch_table_compact", 1, compactFlags, ImVec2(0.0f, 0.0f))) {
            ImGui::TableSetupColumn("Executable", ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableHeadersRow();

            for (auto& group : groups) {
                const bool isMulti = group.entries.size() > 1;
                const auto& defaultEntry = group.defaultEntry();
                const std::string defaultTargetId = buildTargetId(defaultEntry);
                const bool expanded = m_expandedGroups.count(group.targetName) > 0;

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);

                // Disclosure toggle for multi-entry groups
                if (isMulti) {
                    std::string arrowId = "##disc_compact_" + group.targetName;
                    const char* arrow = expanded ? "v" : ">";
                    float arrowWidth = ImGui::CalcTextSize("v ").x;
                    if (ImGui::InvisibleButton(arrowId.c_str(),
                                               ImVec2(arrowWidth, ImGui::GetTextLineHeight()))) {
                        if (expanded) {
                            m_expandedGroups.erase(group.targetName);
                        } else {
                            m_expandedGroups.insert(group.targetName);
                        }
                    }
                    ImVec2 arrowPos = ImGui::GetItemRectMin();
                    ImGui::GetWindowDrawList()->AddText(
                        arrowPos, ImGui::GetColorU32(ImGuiCol_TextDisabled), arrow);
                    ImGui::SameLine(0.0f, 0.0f);
                }

                const bool selected = (defaultTargetId == m_selectedTargetId);
                std::string groupLabel = group.targetName;
                if (defaultEntry.smokePriority == 2) {
                    groupLabel += "  [P2]";
                }
                groupLabel += "##compact_group_" + group.targetName;
                ImGuiSelectableFlags selectFlags =
                    ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick;
                if (ImGui::Selectable(groupLabel.c_str(), selected, selectFlags)) {
                    selectTargetForLogView(defaultEntry);
                }

                drawEntryContextMenu(defaultEntry, "compact_group_menu_" + group.targetName);

                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    launchEntry(defaultEntry, defaultTargetId);
                }

                if (isMulti && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
                    ImGui::SetTooltip("%zu versions available", group.entries.size());
                }

                // Expanded sub-entries
                if (isMulti && expanded) {
                    for (size_t i = 0; i < group.entries.size(); ++i) {
                        const auto& entry = group.entries[i];
                        const std::string targetId = buildTargetId(entry);
                        const bool isDefault = (i == group.defaultIndex);

                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);

                        std::error_code relError;
                        std::filesystem::path displayPath = std::filesystem::relative(
                            entry.executablePath, m_snapshot->repositoryRoot, relError);
                        if (relError || displayPath.empty()) {
                            displayPath = entry.executablePath;
                        }

                        std::string subLabel = "    ";
                        if (isDefault) {
                            subLabel += "[default] ";
                        }
                        subLabel += displayPath.generic_string();
                        subLabel += "##compact_sub_" + targetId;

                        const bool subSelected = (targetId == m_selectedTargetId);
                        if (ImGui::Selectable(subLabel.c_str(), subSelected,
                                              ImGuiSelectableFlags_SpanAllColumns |
                                                  ImGuiSelectableFlags_AllowDoubleClick)) {
                            selectTargetForLogView(entry);
                        }

                        if (ImGui::IsItemHovered() &&
                            ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                            launchEntry(entry, targetId);
                        }

                        if (ImGui::BeginPopupContextItem(
                                ("compact_sub_menu_" + targetId).c_str())) {
                            if (!isDefault && ImGui::MenuItem("Set as default")) {
                                saveGroupDefault(group.targetName, entry);
                            }

                            if (entry.sourceFound) {
                                if (ImGui::MenuItem("Open in VS Code")) {
                                    auto sourceFile = findMainSourceFile(entry.sourceDirectory);
                                    std::string err;
                                    if (ProcessLauncher::openFileInVSCode(sourceFile, err)) {
                                        addConsoleMessage("Opening in VS Code: " +
                                                          sourceFile.string());
                                    } else {
                                        addConsoleMessage("Failed to open VS Code: " + err);
                                    }
                                }
                            }

                            ImGui::EndPopup();
                        }
                    }
                }
            }

            ImGui::EndTable();
        }
    } else {
        ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders |
                                ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY;

        if (ImGui::BeginTable("launch_table", 10, flags, ImVec2(0.0f, 0.0f))) {
            ImGui::TableSetupColumn("Target", ImGuiTableColumnFlags_WidthStretch, 2.3f);
            ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 80.0f);
            ImGui::TableSetupColumn("Pri", ImGuiTableColumnFlags_WidthFixed, 30.0f);
            ImGui::TableSetupColumn("Executable Age", ImGuiTableColumnFlags_WidthFixed, 130.0f);
            ImGui::TableSetupColumn("Source Age", ImGuiTableColumnFlags_WidthFixed, 120.0f);
            ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 220.0f);
            ImGui::TableSetupColumn("Git", ImGuiTableColumnFlags_WidthFixed, 180.0f);
            ImGui::TableSetupColumn("Path", ImGuiTableColumnFlags_WidthStretch, 3.6f);
            ImGui::TableSetupColumn("Run", ImGuiTableColumnFlags_WidthFixed, 90.0f);
            ImGui::TableSetupColumn("Logs", ImGuiTableColumnFlags_WidthFixed, 90.0f);
            ImGui::TableHeadersRow();

            auto now = std::chrono::system_clock::now();

            for (auto& group : groups) {
                const bool isMulti = group.entries.size() > 1;
                const auto& defaultEntry = group.defaultEntry();
                const std::string defaultTargetId = buildTargetId(defaultEntry);
                const bool expanded = m_expandedGroups.count(group.targetName) > 0;

                ImGui::TableNextRow();

                // Target column — group header with disclosure icon
                ImGui::TableSetColumnIndex(0);
                {
                    if (isMulti) {
                        std::string arrowId = "##disc_full_" + group.targetName;
                        const char* arrow = expanded ? "v" : ">";
                        float arrowWidth = ImGui::CalcTextSize("v ").x;
                        if (ImGui::InvisibleButton(
                                arrowId.c_str(), ImVec2(arrowWidth, ImGui::GetTextLineHeight()))) {
                            if (expanded) {
                                m_expandedGroups.erase(group.targetName);
                            } else {
                                m_expandedGroups.insert(group.targetName);
                            }
                        }
                        ImVec2 arrowPos = ImGui::GetItemRectMin();
                        ImGui::GetWindowDrawList()->AddText(
                            arrowPos, ImGui::GetColorU32(ImGuiCol_TextDisabled), arrow);
                        ImGui::SameLine(0.0f, 0.0f);
                    }

                    const bool selected = (defaultTargetId == m_selectedTargetId);
                    std::string targetLabel = group.targetName + "##group_" + group.targetName;
                    ImGuiSelectableFlags selectFlags =
                        ImGuiSelectableFlags_AllowOverlap | ImGuiSelectableFlags_AllowDoubleClick;
                    if (ImGui::Selectable(targetLabel.c_str(), selected, selectFlags)) {
                        selectTargetForLogView(defaultEntry);
                    }

                    if (ImGui::IsItemHovered() &&
                        ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                        launchEntry(defaultEntry, defaultTargetId);
                    }

                    drawEntryContextMenu(defaultEntry, "group_menu_" + group.targetName);
                }

                // Type column
                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(defaultEntry.kind.c_str());

                // Priority column
                ImGui::TableSetColumnIndex(2);
                if (defaultEntry.smokePriority > 0) {
                    ImGui::Text("%d", defaultEntry.smokePriority);
                } else {
                    ImGui::TextUnformatted("-");
                }

                // Executable Age column
                ImGui::TableSetColumnIndex(3);
                drawAgeIndicator(defaultEntry.executableWriteTime, now);

                // Source Age column
                ImGui::TableSetColumnIndex(4);
                if (defaultEntry.hasNewestSourceWriteTime) {
                    drawAgeIndicator(defaultEntry.newestSourceWriteTime, now);
                } else {
                    ImGui::TextUnformatted("-");
                }

                // Status column
                ImGui::TableSetColumnIndex(5);
                if (defaultEntry.outOfDate) {
                    ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.35f, 1.0f), "%s",
                                       defaultEntry.outOfDateReason.c_str());
                } else {
                    ImGui::TextColored(ImVec4(0.35f, 1.0f, 0.45f, 1.0f), "Up to date");
                }

                // Git column
                ImGui::TableSetColumnIndex(6);
                if (!defaultEntry.gitAvailable) {
                    ImGui::TextUnformatted("Git unavailable");
                } else if (defaultEntry.sourceDirty) {
                    ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "Uncommitted changes");
                } else if (defaultEntry.hasLastSourceCommitTime) {
                    std::string age = formatAge(defaultEntry.lastSourceCommitTime, now);
                    ImGui::Text("Last commit: %s", age.c_str());
                } else {
                    ImGui::TextUnformatted("No history");
                }

                // Path column
                ImGui::TableSetColumnIndex(7);
                {
                    std::error_code relError;
                    std::filesystem::path displayPath = std::filesystem::relative(
                        defaultEntry.executablePath, m_snapshot->repositoryRoot, relError);
                    if (relError || displayPath.empty()) {
                        displayPath = defaultEntry.executablePath;
                    }
                    if (isMulti) {
                        std::string pathText = displayPath.generic_string() + " (+" +
                                               std::to_string(group.entries.size() - 1) + " more)";
                        ImGui::TextUnformatted(pathText.c_str());
                    } else {
                        ImGui::TextUnformatted(displayPath.generic_string().c_str());
                    }
                }

                // Run column
                ImGui::TableSetColumnIndex(8);
                {
                    std::string buttonLabel = "Launch##" + defaultEntry.executablePath.string();
                    if (ImGui::Button(buttonLabel.c_str())) {
                        launchEntry(defaultEntry, defaultTargetId);
                    }
                }

                // Logs column
                ImGui::TableSetColumnIndex(9);
                {
                    std::string logsButton = "View##logs_" + defaultTargetId;
                    if (ImGui::Button(logsButton.c_str())) {
                        selectTargetForLogView(defaultEntry);
                        m_showRunLogDialog = true;
                    }
                }

                // Expanded sub-entries for multi-entry groups
                if (isMulti && expanded) {
                    for (size_t i = 0; i < group.entries.size(); ++i) {
                        const auto& entry = group.entries[i];
                        const std::string targetId = buildTargetId(entry);
                        const bool isDefault = (i == group.defaultIndex);

                        ImGui::TableNextRow();

                        // Target column — indented sub-entry
                        ImGui::TableSetColumnIndex(0);
                        {
                            std::string subLabel = "    ";
                            if (isDefault) {
                                subLabel += "*  ";
                            }

                            std::error_code relError;
                            std::filesystem::path displayPath = std::filesystem::relative(
                                entry.executablePath, m_snapshot->repositoryRoot, relError);
                            if (relError || displayPath.empty()) {
                                displayPath = entry.executablePath;
                            }
                            subLabel += displayPath.generic_string();
                            subLabel += "##sub_" + targetId;

                            const bool subSelected = (targetId == m_selectedTargetId);
                            if (ImGui::Selectable(subLabel.c_str(), subSelected,
                                                  ImGuiSelectableFlags_AllowOverlap |
                                                      ImGuiSelectableFlags_AllowDoubleClick)) {
                                selectTargetForLogView(entry);
                            }

                            if (ImGui::IsItemHovered() &&
                                ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                                launchEntry(entry, targetId);
                            }

                            if (ImGui::BeginPopupContextItem(("sub_menu_" + targetId).c_str())) {
                                if (!isDefault && ImGui::MenuItem("Set as default")) {
                                    saveGroupDefault(group.targetName, entry);
                                }

                                if (entry.sourceFound) {
                                    if (ImGui::MenuItem("Open in VS Code")) {
                                        auto sourceFile = findMainSourceFile(entry.sourceDirectory);
                                        std::string err;
                                        if (ProcessLauncher::openFileInVSCode(sourceFile, err)) {
                                            addConsoleMessage("Opening in VS Code: " +
                                                              sourceFile.string());
                                        } else {
                                            addConsoleMessage("Failed to open VS Code: " + err);
                                        }
                                    }
                                }

                                ImGui::EndPopup();
                            }
                        }

                        // Type
                        ImGui::TableSetColumnIndex(1);
                        ImGui::TextUnformatted(entry.kind.c_str());

                        // Priority
                        ImGui::TableSetColumnIndex(2);
                        if (entry.smokePriority > 0) {
                            ImGui::Text("%d", entry.smokePriority);
                        } else {
                            ImGui::TextUnformatted("-");
                        }

                        // Executable Age
                        ImGui::TableSetColumnIndex(3);
                        drawAgeIndicator(entry.executableWriteTime, now);

                        // Source Age
                        ImGui::TableSetColumnIndex(4);
                        if (entry.hasNewestSourceWriteTime) {
                            drawAgeIndicator(entry.newestSourceWriteTime, now);
                        } else {
                            ImGui::TextUnformatted("-");
                        }

                        // Status
                        ImGui::TableSetColumnIndex(5);
                        if (entry.outOfDate) {
                            ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.35f, 1.0f), "%s",
                                               entry.outOfDateReason.c_str());
                        } else {
                            ImGui::TextColored(ImVec4(0.35f, 1.0f, 0.45f, 1.0f), "Up to date");
                        }

                        // Git
                        ImGui::TableSetColumnIndex(6);
                        ImGui::TextUnformatted("");

                        // Path
                        ImGui::TableSetColumnIndex(7);
                        {
                            std::error_code relError;
                            std::filesystem::path displayPath = std::filesystem::relative(
                                entry.executablePath, m_snapshot->repositoryRoot, relError);
                            if (relError || displayPath.empty()) {
                                displayPath = entry.executablePath;
                            }
                            ImGui::TextUnformatted(displayPath.generic_string().c_str());
                        }

                        // Run
                        ImGui::TableSetColumnIndex(8);
                        {
                            std::string buttonLabel = "Launch##" + entry.executablePath.string();
                            if (ImGui::Button(buttonLabel.c_str())) {
                                launchEntry(entry, targetId);
                            }
                        }

                        // Logs
                        ImGui::TableSetColumnIndex(9);
                        {
                            std::string logsButton = "View##logs_" + targetId;
                            if (ImGui::Button(logsButton.c_str())) {
                                selectTargetForLogView(entry);
                                m_showRunLogDialog = true;
                            }
                        }
                    }
                }
            }

            ImGui::EndTable();
        }
    }

    ImGui::End();
    drawRunLogViewer();
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

void VLauncherScene::launchEntryWithSmokeTest(const ExecutableEntry& entry,
                                              const std::string& targetId,
                                              const std::string& scriptName) {
    std::error_code fsError;
    std::filesystem::path repoRoot;
    if (m_snapshot) {
        repoRoot = m_snapshot->repositoryRoot;
    }
    if (repoRoot.empty()) {
        repoRoot = std::filesystem::current_path(fsError);
        if (fsError || repoRoot.empty()) {
            repoRoot = std::filesystem::path(".");
        }
    }

    const std::filesystem::path smokeScript = repoRoot / "smoketests" / "scripts" / scriptName;

    if (!std::filesystem::exists(smokeScript)) {
        addConsoleMessage("Smoke test script not found: " + smokeScript.string());
        return;
    }

    std::string launchError;
    LaunchedProcess launchedProcess;
    const std::vector<std::string> extraArgs = {"--input-script", smokeScript.string()};
    if (ProcessLauncher::launchWithOutputCapture(entry.executablePath, launchedProcess, launchError,
                                                 extraArgs)) {
        ActiveRun activeRun;
        activeRun.entry = entry;
        activeRun.targetId = targetId;
        activeRun.process = std::move(launchedProcess);
        m_activeRuns.push_back(std::move(activeRun));

        addConsoleMessage("Smoke test launched: " + entry.targetName + " with " + scriptName);
        selectTargetForLogView(entry);
    } else {
        addConsoleMessage("Smoke test launch failed for " + entry.targetName + ": " + launchError);
    }
}

void VLauncherScene::drawEntryContextMenu(const ExecutableEntry& entry,
                                          const std::string& popupId) {
    if (!ImGui::BeginPopupContextItem(popupId.c_str())) {
        return;
    }

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

    if (!entry.smokeScripts.empty()) {
        if (entry.smokeScripts.size() == 1) {
            if (ImGui::MenuItem("Run Smoke Test")) {
                launchEntryWithSmokeTest(entry, buildTargetId(entry), entry.smokeScripts.front());
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", entry.smokeScripts.front().c_str());
            }
        } else {
            if (ImGui::BeginMenu("Run Smoke Test")) {
                for (const auto& script : entry.smokeScripts) {
                    if (ImGui::MenuItem(script.c_str())) {
                        launchEntryWithSmokeTest(entry, buildTargetId(entry), script);
                    }
                }
                ImGui::EndMenu();
            }
        }
    } else {
        ImGui::BeginDisabled();
        ImGui::MenuItem("Run Smoke Test");
        ImGui::EndDisabled();
        ImGui::TextDisabled("(no smoke scripts in vde.toml)");
    }

    ImGui::EndPopup();
}

void VLauncherScene::launchEntry(const ExecutableEntry& entry, const std::string& targetId) {
    std::string launchError;
    LaunchedProcess launchedProcess;
    if (ProcessLauncher::launchWithOutputCapture(entry.executablePath, launchedProcess,
                                                 launchError)) {
        ActiveRun activeRun;
        activeRun.entry = entry;
        activeRun.targetId = targetId;
        activeRun.process = std::move(launchedProcess);
        m_activeRuns.push_back(std::move(activeRun));

        addConsoleMessage("Launched: " + entry.targetName + " (capturing command line output)");
        selectTargetForLogView(entry);
    } else {
        addConsoleMessage("Launch failed for " + entry.targetName + ": " + launchError);
    }
}

std::string VLauncherScene::buildTargetId(const ExecutableEntry& entry) const {
    std::error_code error;
    std::filesystem::path root;
    if (m_snapshot) {
        root = m_snapshot->repositoryRoot;
    }
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
    if (!m_showRunLogDialog) {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(880.0f, 540.0f), ImGuiCond_FirstUseEver);
    bool open = m_showRunLogDialog;
    bool closeRequested = false;

    if (ImGui::Begin("Run Logs", &open)) {
        if (m_selectedTargetId.empty()) {
            ImGui::TextDisabled("Select a target and click View to inspect run logs.");
            if (ImGui::Button("Close")) {
                closeRequested = true;
            }
        } else {
            ImGui::Text("Target: %s", m_selectedTargetName.c_str());
            if (ImGui::Button("Reload logs")) {
                refreshSelectedRunLogs();
            }
            ImGui::SameLine();
            if (ImGui::Button("Clear selection")) {
                m_selectedTargetId.clear();
                m_selectedTargetName.clear();
                m_selectedTargetRuns = {};
            }
            ImGui::SameLine();
            if (ImGui::Button("Close")) {
                closeRequested = true;
            }

            ImGui::Separator();

            if (!m_selectedTargetRuns[0].has_value() && !m_selectedTargetRuns[1].has_value()) {
                const bool hasActiveRun = std::any_of(
                    m_activeRuns.begin(), m_activeRuns.end(),
                    [this](const ActiveRun& run) { return run.targetId == m_selectedTargetId; });

                if (hasActiveRun) {
                    ImGui::TextDisabled(
                        "Run in progress. Logs appear here after the process exits.");
                } else {
                    ImGui::TextDisabled("No captured runs saved for this target yet.");
                }
            } else {
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

                    const float minOutputHeight = 170.0f;
                    float outputHeight = ImGui::GetContentRegionAvail().y;

                    // Reserve some room for remaining slot headers/buttons so the
                    // first expanded section doesn't completely hide them.
                    const size_t remainingSlots = m_selectedTargetRuns.size() - slot - 1;
                    const float reserveForRemaining = 36.0f * static_cast<float>(remainingSlots);
                    outputHeight -= reserveForRemaining;
                    if (outputHeight < minOutputHeight) {
                        outputHeight = minOutputHeight;
                    }

                    std::string childId = "run_output_##" + std::to_string(slot);
                    if (ImGui::BeginChild(childId.c_str(), ImVec2(0.0f, outputHeight), true,
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
        }
    }

    ImGui::End();
    m_showRunLogDialog = open && !closeRequested;
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
            addConsoleMessage("Run finished: " + activeRun.entry.targetName + " (exit code " +
                              std::to_string(runLog.exitCode) + ")");
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

std::vector<VLauncherScene::TargetGroup> VLauncherScene::buildGroupedEntries() {
    auto sorted = getSortedEntries();

    // Collect entries into groups by targetName, preserving first-seen order.
    std::map<std::string, size_t> nameToGroupIndex;
    std::vector<TargetGroup> groups;

    for (auto& entry : sorted) {
        auto it = nameToGroupIndex.find(entry.targetName);
        if (it == nameToGroupIndex.end()) {
            nameToGroupIndex[entry.targetName] = groups.size();
            TargetGroup group;
            group.targetName = entry.targetName;
            group.entries.push_back(std::move(entry));
            groups.push_back(std::move(group));
        } else {
            groups[it->second].entries.push_back(std::move(entry));
        }
    }

    // Incrementally load persisted defaults for any multi-entry groups whose
    // saved preference has not been loaded yet (handles groups that appear
    // after startup when new build outputs are created).
    loadGroupDefaults(groups);

    for (auto& group : groups) {
        resolveGroupDefault(group);
    }

    return groups;
}

void VLauncherScene::resolveGroupDefault(TargetGroup& group) const {
    if (group.entries.size() <= 1) {
        group.defaultIndex = 0;
        return;
    }

    auto it = m_groupDefaults.find(group.targetName);
    if (it != m_groupDefaults.end()) {
        const auto& savedPath = it->second;
        for (size_t i = 0; i < group.entries.size(); ++i) {
            std::error_code ec;
            std::filesystem::path root;
            if (m_snapshot) {
                root = m_snapshot->repositoryRoot;
            }
            if (root.empty()) {
                root = std::filesystem::current_path(ec);
            }
            auto relPath = std::filesystem::relative(group.entries[i].executablePath, root, ec);
            if (!ec && relPath.generic_string() == savedPath) {
                group.defaultIndex = i;
                return;
            }
            // Fallback: compare using the absolute path so that defaults saved
            // when relative() failed (absolute format) can still match.
            if (ec && group.entries[i].executablePath.generic_string() == savedPath) {
                group.defaultIndex = i;
                return;
            }
        }
    }

    // No saved default or saved path not found; pick the first entry.
    group.defaultIndex = 0;
}

void VLauncherScene::saveGroupDefault(const std::string& targetName, const ExecutableEntry& entry) {
    auto& storage = vde::StorageManager::getInstance();
    if (!storage.isInitialized()) {
        return;
    }

    std::error_code ec;
    std::filesystem::path root;
    if (m_snapshot) {
        root = m_snapshot->repositoryRoot;
    }
    if (root.empty()) {
        root = std::filesystem::current_path(ec);
    }

    auto relPath = std::filesystem::relative(entry.executablePath, root, ec);
    std::string pathStr = ec ? entry.executablePath.generic_string() : relPath.generic_string();

    m_groupDefaults[targetName] = pathStr;
    storage.setStringData(std::string(kGroupDefaultKeyPrefix) + targetName, pathStr);
}

void VLauncherScene::loadGroupDefaults(const std::vector<TargetGroup>& groups) {
    auto& storage = vde::StorageManager::getInstance();
    if (!storage.isInitialized()) {
        return;
    }

    for (const auto& group : groups) {
        if (group.entries.size() <= 1) {
            continue;
        }

        // Skip groups whose default has already been loaded.
        if (m_groupDefaults.count(group.targetName) > 0) {
            continue;
        }

        std::string key = std::string(kGroupDefaultKeyPrefix) + group.targetName;
        auto value = storage.getStringData(key);
        if (value.has_value()) {
            m_groupDefaults[group.targetName] = *value;
        }
    }
}

std::vector<ExecutableEntry> VLauncherScene::getSortedEntries() const {
    std::vector<ExecutableEntry> filtered;

    if (!m_snapshot) {
        return filtered;
    }

    for (const auto& entry : m_snapshot->entries) {
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

void VLauncherScene::drawAgeIndicator(std::chrono::system_clock::time_point from,
                                      std::chrono::system_clock::time_point now) {
    if (from.time_since_epoch().count() == 0) {
        ImGui::TextDisabled("-");
        return;
    }

    auto diff = now - from;
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(diff).count();
    if (seconds < 0) {
        seconds = 0;
    }

    ImVec4 color;
    if (seconds < 3600) {
        color = ImVec4(0.35f, 1.0f, 0.45f, 1.0f);  // green
    } else if (seconds < 86400) {
        color = ImVec4(1.0f, 0.85f, 0.2f, 1.0f);  // yellow
    } else {
        color = ImVec4(1.0f, 0.45f, 0.35f, 1.0f);  // red
    }

    const float radius = 5.0f;
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const float lineH = ImGui::GetTextLineHeight();
    ImVec2 center = ImVec2(pos.x + radius, pos.y + lineH * 0.5f);
    ImGui::GetWindowDrawList()->AddCircleFilled(center, radius,
                                                ImGui::ColorConvertFloat4ToU32(color));
    ImGui::Dummy(ImVec2(radius * 2.0f, lineH));
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        // Show age as a decimal value with the largest applicable unit
        char ageText[64];
        if (seconds < 60) {
            std::snprintf(ageText, sizeof(ageText), "%.1f seconds ago",
                          static_cast<double>(seconds));
        } else if (seconds < 3600) {
            std::snprintf(ageText, sizeof(ageText), "%.1f minutes ago",
                          static_cast<double>(seconds) / 60.0);
        } else if (seconds < 86400) {
            std::snprintf(ageText, sizeof(ageText), "%.1f hours ago",
                          static_cast<double>(seconds) / 3600.0);
        } else {
            std::snprintf(ageText, sizeof(ageText), "%.1f days ago",
                          static_cast<double>(seconds) / 86400.0);
        }
        ImGui::TextUnformatted(ageText);
        ImGui::EndTooltip();
    }
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
