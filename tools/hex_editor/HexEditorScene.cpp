#include "HexEditorScene.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>

#include <imgui.h>

using namespace vde::tools;

// =============================================================================
// Construction / lifecycle
// =============================================================================

HexEditorScene::HexEditorScene(ToolMode mode) : BaseToolScene(mode) {}

void HexEditorScene::onEnter() {
    addConsoleMessage("Welcome to VDE Hex Editor");
    addConsoleMessage("Open files via the toolbar or: open <path>");
    addConsoleMessage("Type 'help' for available commands.");
}

// =============================================================================
// Metadata
// =============================================================================

std::string HexEditorScene::getToolName() const {
    return "Hex Editor";
}

std::string HexEditorScene::getToolDescription() const {
    return "Open, inspect, and compare binary files in hex and ASCII";
}

// =============================================================================
// Command dispatch
// =============================================================================

void HexEditorScene::executeCommand(const std::string& cmdLine) {
    std::istringstream iss(cmdLine);
    std::string cmd;
    iss >> cmd;

    if (cmd == "help") {
        cmdHelp();
    } else if (cmd == "open") {
        cmdOpen(iss);
    } else if (cmd == "close") {
        cmdClose(iss);
    } else if (cmd == "list") {
        cmdList();
    } else if (!cmd.empty()) {
        addConsoleMessage("Unknown command: " + cmd + "  (type 'help')");
    }
}

// =============================================================================
// ImGui UI
// =============================================================================

void HexEditorScene::drawDebugUI() {
    float scale = getGame()->getDPIScale();

    // Full-width main window that fills the available area
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(io.DisplaySize, ImGuiCond_Always);
    ImGuiWindowFlags mainFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                                 ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_MenuBar;

    if (ImGui::Begin("##HexEditorMain", nullptr, mainFlags)) {
        drawToolbar();

        // ---- body ----
        float bottomBarHeight = ImGui::GetFrameHeightWithSpacing() + 4.0f * scale;
        float bodyHeight = ImGui::GetContentRegionAvail().y - bottomBarHeight;
        ImVec2 bodySize(0, bodyHeight);

        if (m_showCompare && m_selectedA >= 0 && m_selectedB >= 0) {
            drawComparePanel();
        } else {
            // Single file + file selector sidebar
            float sidebarWidth = 200.0f * scale;
            float hexWidth = ImGui::GetContentRegionAvail().x - sidebarWidth - 8.0f * scale;

            ImGui::BeginChild("##Sidebar", ImVec2(sidebarWidth, bodyHeight), true);
            drawFileSelector();
            ImGui::EndChild();

            ImGui::SameLine();

            ImGui::BeginChild("##HexView", ImVec2(hexWidth, bodyHeight), true);
            if (m_selectedA >= 0 && m_selectedA < static_cast<int>(m_files.size())) {
                drawHexPanel("##SingleHex", m_selectedA, false, -1);
            } else {
                ImGui::TextDisabled("No file selected. Open a file to begin.");
            }
            ImGui::EndChild();
        }

        drawStatusBar();
    }
    ImGui::End();

    // Console window (collapsible, positioned at bottom of screen)
    float consoleW = io.DisplaySize.x * 0.4f;
    float consoleH = 180.0f * scale;
    ImGui::SetNextWindowPos(ImVec2(0, io.DisplaySize.y - consoleH), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(consoleW, consoleH), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowCollapsed(true, ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Console##HexConsole")) {
        float inputH = ImGui::GetFrameHeightWithSpacing();
        ImGui::BeginChild("##ConsoleOut", ImVec2(0, -inputH), false);
        for (const auto& msg : getConsoleLog()) {
            ImGui::TextWrapped("%s", msg.c_str());
        }
        if (shouldScrollToBottom()) {
            ImGui::SetScrollHereY(1.0f);
        }
        ImGui::EndChild();

        ImGui::Separator();
        ImGui::Text(">");
        ImGui::SameLine();
        static char consoleBuf[512] = {};
        if (ImGui::InputText("##ConsoleInput", consoleBuf, sizeof(consoleBuf),
                             ImGuiInputTextFlags_EnterReturnsTrue)) {
            std::string line(consoleBuf);
            if (!line.empty()) {
                addConsoleMessage("> " + line);
                executeCommand(line);
                consoleBuf[0] = '\0';
            }
            ImGui::SetKeyboardFocusHere(-1);
        }
    }
    ImGui::End();
}

// =============================================================================
// UI sub-routines
// =============================================================================

void HexEditorScene::drawToolbar() {
    float scale = getGame()->getDPIScale();

    if (ImGui::BeginMenuBar()) {
        // Open file via path input
        ImGui::SetNextItemWidth(320.0f * scale);
        if (ImGui::InputText("##openpath", m_openPathBuf, sizeof(m_openPathBuf),
                             ImGuiInputTextFlags_EnterReturnsTrue)) {
            std::string path(m_openPathBuf);
            if (!path.empty() && loadFile(path)) {
                m_openPathBuf[0] = '\0';
                m_selectedA = static_cast<int>(m_files.size()) - 1;
            }
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Type a file path and press Enter to open");
        }

        ImGui::SameLine();
        if (ImGui::Button("Open")) {
            std::string path(m_openPathBuf);
            if (path.empty()) {
                // No path typed — open native file browser
                path = vde::tools::openFileDialog("Open Binary File", {{"All Files", "*.*"}});
            }
            if (!path.empty() && loadFile(path)) {
                m_openPathBuf[0] = '\0';
                m_selectedA = static_cast<int>(m_files.size()) - 1;
            }
        }

        ImGui::SameLine(0, 16.0f * scale);
        ImGui::Text("Bytes/row:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(60.0f * scale);
        ImGui::InputInt("##bpr", &m_bytesPerRow, 0);
        m_bytesPerRow = std::max(4, std::min(64, m_bytesPerRow));

        ImGui::SameLine(0, 16.0f * scale);
        ImGui::Checkbox("ASCII", &m_showAscii);

        ImGui::SameLine(0, 16.0f * scale);
        ImGui::Checkbox("Compare Mode", &m_showCompare);

        ImGui::EndMenuBar();
    }
}

void HexEditorScene::drawFileSelector() {
    ImGui::Text("Open Files (%d)", static_cast<int>(m_files.size()));
    ImGui::Separator();

    for (int i = 0; i < static_cast<int>(m_files.size()); ++i) {
        const auto& f = m_files[i];
        bool selected = (i == m_selectedA);
        if (ImGui::Selectable(f.label.c_str(), selected, ImGuiSelectableFlags_AllowDoubleClick)) {
            m_selectedA = i;
        }

        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::Text("%s", f.path.c_str());
            ImGui::Text("%zu bytes", f.data.size());
            ImGui::EndTooltip();
        }

        // Context menu: set as A / B for compare, close
        if (ImGui::BeginPopupContextItem()) {
            ImGui::Text("%s", f.label.c_str());
            ImGui::Separator();
            if (ImGui::MenuItem("View (Left)")) {
                m_selectedA = i;
            }
            if (ImGui::MenuItem("Compare Right")) {
                m_selectedB = i;
                m_showCompare = true;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Close")) {
                // Adjust selection indices before erasing
                if (m_selectedA == i)
                    m_selectedA = -1;
                else if (m_selectedA > i)
                    --m_selectedA;
                if (m_selectedB == i)
                    m_selectedB = -1;
                else if (m_selectedB > i)
                    --m_selectedB;
                m_files.erase(m_files.begin() + i);
                ImGui::EndPopup();
                break;  // vector invalidated
            }
            ImGui::EndPopup();
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextDisabled("Right-click a file for options");
    if (m_showCompare) {
        ImGui::Spacing();
        ImGui::Text("Compare:");
        ImGui::Text("  L: %s", m_selectedA >= 0 ? m_files[m_selectedA].label.c_str() : "(none)");
        ImGui::Text("  R: %s", m_selectedB >= 0 ? m_files[m_selectedB].label.c_str() : "(none)");
    }
}

void HexEditorScene::drawHexPanel(const char* /*panelId*/, int fileIdx, bool highlightDiffs,
                                  int otherIdx) {
    if (fileIdx < 0 || fileIdx >= static_cast<int>(m_files.size()))
        return;
    const HexFile& f = m_files[fileIdx];
    const HexFile* other =
        (highlightDiffs && otherIdx >= 0 && otherIdx < static_cast<int>(m_files.size()))
            ? &m_files[otherIdx]
            : nullptr;

    const int bpr = m_bytesPerRow;
    const size_t size = f.data.size();

    // Header row
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.8f, 1.0f, 1.0f));
    ImGui::Text("%-10s", "Offset");
    ImGui::SameLine();
    for (int col = 0; col < bpr; ++col) {
        ImGui::Text("%02X ", col);
        ImGui::SameLine();
    }
    if (m_showAscii) {
        ImGui::Text("  ASCII");
    }
    ImGui::PopStyleColor();
    ImGui::Separator();

    // Use a clipper for large files
    ImGuiListClipper clipper;
    int totalRows = static_cast<int>((size + bpr - 1) / bpr);
    clipper.Begin(totalRows);

    char rowBuf[64];
    while (clipper.Step()) {
        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
            size_t offset = static_cast<size_t>(row) * bpr;

            // Offset column
            std::snprintf(rowBuf, sizeof(rowBuf), "%08zX", offset);
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s", rowBuf);
            ImGui::SameLine();

            // Hex bytes
            for (int col = 0; col < bpr; ++col) {
                size_t idx = offset + col;
                if (idx >= size) {
                    ImGui::Text("   ");
                } else {
                    uint8_t byte = f.data[idx];
                    bool diff = other && (idx >= other->data.size() || other->data[idx] != byte);
                    if (diff) {
                        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%02X ", byte);
                    } else {
                        ImGui::Text("%02X ", byte);
                    }
                }
                ImGui::SameLine();
            }

            // ASCII column
            if (m_showAscii) {
                ImGui::Text("  ");
                ImGui::SameLine();
                for (int col = 0; col < bpr; ++col) {
                    size_t idx = offset + col;
                    if (idx < size) {
                        uint8_t byte = f.data[idx];
                        bool diff =
                            other && (idx >= other->data.size() || other->data[idx] != byte);
                        char ch = (byte >= 32 && byte < 127) ? static_cast<char>(byte) : '.';
                        char charStr[2] = {ch, '\0'};
                        if (diff) {
                            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", charStr);
                        } else {
                            ImGui::Text("%s", charStr);
                        }
                        ImGui::SameLine();
                    }
                }
            }

            ImGui::NewLine();
        }
    }
    clipper.End();
}

void HexEditorScene::drawComparePanel() {
    float scale = getGame()->getDPIScale();
    float bodyHeight =
        ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing() - 4.0f * scale;
    float halfW = (ImGui::GetContentRegionAvail().x - 6.0f * scale) * 0.5f;

    // Left panel
    ImGui::BeginChild("##CompareL", ImVec2(halfW, bodyHeight), true);
    if (m_selectedA >= 0 && m_selectedA < static_cast<int>(m_files.size())) {
        ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "%s",
                           m_files[m_selectedA].label.c_str());
        ImGui::Separator();
        drawHexPanel("##CmpL", m_selectedA, true, m_selectedB);
    } else {
        ImGui::TextDisabled("Select a file (Left)");
    }
    ImGui::EndChild();

    ImGui::SameLine();

    // Right panel
    ImGui::BeginChild("##CompareR", ImVec2(halfW, bodyHeight), true);
    if (m_selectedB >= 0 && m_selectedB < static_cast<int>(m_files.size())) {
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.3f, 1.0f), "%s",
                           m_files[m_selectedB].label.c_str());
        ImGui::Separator();
        drawHexPanel("##CmpR", m_selectedB, true, m_selectedA);
    } else {
        ImGui::TextDisabled("Right-click a file in the list and choose 'Compare Right'");
    }
    ImGui::EndChild();
}

void HexEditorScene::drawStatusBar() {
    ImGui::Separator();
    if (m_selectedA >= 0 && m_selectedA < static_cast<int>(m_files.size())) {
        const auto& f = m_files[m_selectedA];
        ImGui::Text("  %s  |  %zu bytes", f.path.c_str(), f.data.size());
    } else {
        ImGui::TextDisabled("  No file selected");
    }

    if (m_showCompare && m_selectedA >= 0 && m_selectedB >= 0 &&
        m_selectedA < static_cast<int>(m_files.size()) &&
        m_selectedB < static_cast<int>(m_files.size())) {
        const auto& a = m_files[m_selectedA];
        const auto& b = m_files[m_selectedB];
        size_t compared = std::min(a.data.size(), b.data.size());
        size_t diffs = 0;
        for (size_t i = 0; i < compared; ++i) {
            if (a.data[i] != b.data[i])
                ++diffs;
        }
        size_t sizeDiff = (a.data.size() > b.data.size()) ? a.data.size() - b.data.size()
                                                          : b.data.size() - a.data.size();
        ImGui::SameLine(0, 32.0f);
        ImGui::Text("Diff: %zu byte(s) differ | size delta: %zu", diffs, sizeDiff);
    }
}

// =============================================================================
// Command handlers
// =============================================================================

void HexEditorScene::cmdHelp() {
    addConsoleMessage("COMMANDS:");
    addConsoleMessage("  open <path>    - Open a file");
    addConsoleMessage("  close <index>  - Close file by index (0-based)");
    addConsoleMessage("  list           - List all open files");
    addConsoleMessage("  help           - Show this message");
    addConsoleMessage("UI TIPS:");
    addConsoleMessage("  Right-click a file in the sidebar for compare options.");
    addConsoleMessage("  Enable 'Compare Mode' in the toolbar to diff two files.");
    addConsoleMessage("  Red bytes indicate differences between compared files.");
}

void HexEditorScene::cmdOpen(std::istringstream& iss) {
    std::string path;
    iss >> path;
    if (path.empty()) {
        addConsoleMessage("ERROR: Usage: open <path>");
        return;
    }
    if (loadFile(path)) {
        m_selectedA = static_cast<int>(m_files.size()) - 1;
    }
}

void HexEditorScene::cmdClose(std::istringstream& iss) {
    int idx = -1;
    if (!(iss >> idx) || idx < 0 || idx >= static_cast<int>(m_files.size())) {
        addConsoleMessage("ERROR: Usage: close <index>  (use 'list' to see indices)");
        return;
    }
    addConsoleMessage("Closed: " + m_files[idx].path);
    if (m_selectedA == idx)
        m_selectedA = -1;
    else if (m_selectedA > idx)
        --m_selectedA;
    if (m_selectedB == idx)
        m_selectedB = -1;
    else if (m_selectedB > idx)
        --m_selectedB;
    m_files.erase(m_files.begin() + idx);
}

void HexEditorScene::cmdList() {
    if (m_files.empty()) {
        addConsoleMessage("No files open.");
        return;
    }
    addConsoleMessage("Open files:");
    for (int i = 0; i < static_cast<int>(m_files.size()); ++i) {
        addConsoleMessage("  [" + std::to_string(i) + "] " + m_files[i].path + "  (" +
                          std::to_string(m_files[i].data.size()) + " bytes)");
    }
}

// =============================================================================
// Internal helpers
// =============================================================================

bool HexEditorScene::loadFile(const std::string& path) {
    // Check for duplicate
    for (const auto& f : m_files) {
        if (f.path == path) {
            addConsoleMessage("Already open: " + path);
            return false;
        }
    }

    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        addConsoleMessage("ERROR: Cannot open file: " + path);
        return false;
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    HexFile hf;
    hf.path = path;
    hf.label = shortName(path);
    hf.data.resize(static_cast<size_t>(size));
    if (!file.read(reinterpret_cast<char*>(hf.data.data()), size)) {
        addConsoleMessage("ERROR: Failed to read file: " + path);
        return false;
    }
    hf.loaded = true;
    m_files.push_back(std::move(hf));
    addConsoleMessage("Opened: " + path + "  (" + std::to_string(size) + " bytes)");
    return true;
}

std::string HexEditorScene::shortName(const std::string& path) {
    std::filesystem::path p(path);
    return p.filename().string();
}
