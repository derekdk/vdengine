/**
 * @file EditorPanels.cpp
 * @brief Implementation of ImGui UI panels for the Resource Editor.
 */

#include "EditorPanels.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

#include "CommandSystem.h"
#include "commands/CommandRegistry.h"
#include "commands/EditorContext.h"

namespace vde {
namespace tools {

// =============================================================================
// Autocomplete helpers
// =============================================================================

void EditorPanels::updateCompletions(const std::string& input, const CommandSystem& cmd) {
    m_completions.clear();
    m_paramHint.clear();
    m_showCompletions = false;

    auto textStart = input.find_first_not_of(" \t");
    if (textStart == std::string::npos) {
        m_selectedCompletion = -1;
        return;
    }

    std::string text = input.substr(textStart);
    size_t cmdOffset = textStart;

    // Skip @canvas prefix if present.
    if (!text.empty() && text[0] == '@') {
        auto sp = text.find(' ');
        if (sp == std::string::npos)
            return;
        cmdOffset += sp + 1;
        text = text.substr(sp + 1);
        auto ns = text.find_first_not_of(" \t");
        if (ns == std::string::npos)
            return;
        cmdOffset += ns;
        text = text.substr(ns);
    }

    if (text.empty())
        return;

    std::string textLower = text;
    std::transform(textLower.begin(), textLower.end(), textLower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    auto& registry = CommandRegistry::instance();
    auto allMeta = registry.getAllMetadata();

    // Find the longest-matching command name at the start of the text.
    const CommandMetadata* matchedCmd = nullptr;
    size_t matchLen = 0;

    for (const auto* meta : allMeta) {
        auto tryMatch = [&](const std::string& name) {
            std::string lower = name;
            std::transform(lower.begin(), lower.end(), lower.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (textLower.size() >= lower.size() + 1 && textLower[lower.size()] == ' ' &&
                textLower.substr(0, lower.size()) == lower && lower.size() > matchLen) {
                matchedCmd = meta;
                matchLen = lower.size();
            }
        };
        tryMatch(meta->name);
        for (const auto& alias : meta->aliases)
            tryMatch(alias);
    }

    if (matchedCmd) {
        // ---- Parameter mode ----
        m_paramHint = getParameterHint(input);

        std::string argsText = text.substr(matchLen + 1);
        std::vector<std::string> tokens;
        {
            std::istringstream iss(argsText);
            std::string tok;
            while (iss >> tok)
                tokens.push_back(tok);
        }

        bool endsWithSpace = !argsText.empty() && argsText.back() == ' ';
        size_t paramIdx =
            endsWithSpace ? tokens.size() : (tokens.empty() ? 0 : tokens.size() - 1);
        std::string currentToken = (!endsWithSpace && !tokens.empty()) ? tokens.back() : "";

        if (paramIdx < matchedCmd->params.size()) {
            const auto& param = matchedCmd->params[paramIdx];
            std::string currentLower = currentToken;
            std::transform(currentLower.begin(), currentLower.end(), currentLower.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

            m_completionReplaceStart = currentToken.empty()
                                           ? static_cast<int>(input.size())
                                           : static_cast<int>(input.size() - currentToken.size());

            if (param.type == ParamType::Enum) {
                for (const auto& ev : param.enumValues) {
                    std::string evLower = ev;
                    std::transform(evLower.begin(), evLower.end(), evLower.begin(),
                                   [](unsigned char c) {
                                       return static_cast<char>(std::tolower(c));
                                   });
                    if (currentToken.empty() ||
                        (evLower.find(currentLower) == 0 && evLower != currentLower)) {
                        m_completions.push_back(ev);
                    }
                }
            } else if (param.type == ParamType::Color) {
                const auto* ctx = cmd.getContext();
                if (ctx) {
                    for (const auto& [name, color] : ctx->namedColors) {
                        std::string nameLower = name;
                        std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(),
                                       [](unsigned char c) {
                                           return static_cast<char>(std::tolower(c));
                                       });
                        if (currentToken.empty() ||
                            (nameLower.find(currentLower) == 0 && nameLower != currentLower)) {
                            m_completions.push_back(name);
                        }
                    }
                }
            }
        }

        m_showCompletions = !m_completions.empty();
        if (!m_completions.empty()) {
            m_selectedCompletion =
                std::clamp(m_selectedCompletion, 0, static_cast<int>(m_completions.size()) - 1);
        } else {
            m_selectedCompletion = -1;
        }
        return;
    }

    // ---- Command-name mode ----
    m_completionReplaceStart = static_cast<int>(cmdOffset);

    for (const auto* meta : allMeta) {
        auto tryPrefix = [&](const std::string& name) {
            std::string lower = name;
            std::transform(lower.begin(), lower.end(), lower.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (lower.find(textLower) == 0 && lower != textLower) {
                m_completions.push_back(name);
            }
        };
        tryPrefix(meta->name);
        for (const auto& alias : meta->aliases)
            tryPrefix(alias);
    }

    std::sort(m_completions.begin(), m_completions.end());
    m_completions.erase(std::unique(m_completions.begin(), m_completions.end()),
                        m_completions.end());

    m_showCompletions = !m_completions.empty();
    if (!m_completions.empty()) {
        m_selectedCompletion =
            std::clamp(m_selectedCompletion, 0, static_cast<int>(m_completions.size()) - 1);
    } else {
        m_selectedCompletion = -1;
    }
}

std::string EditorPanels::getParameterHint(const std::string& input) const {
    std::string text = input;
    auto start = text.find_first_not_of(" \t");
    if (start == std::string::npos)
        return {};
    text = text.substr(start);

    // Skip @canvas prefix.
    if (!text.empty() && text[0] == '@') {
        auto sp = text.find(' ');
        if (sp == std::string::npos)
            return {};
        text = text.substr(sp + 1);
        auto ns = text.find_first_not_of(" \t");
        if (ns == std::string::npos)
            return {};
        text = text.substr(ns);
    }

    if (text.empty())
        return {};

    std::string textLower = text;
    std::transform(textLower.begin(), textLower.end(), textLower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    auto& registry = CommandRegistry::instance();
    auto allMeta = registry.getAllMetadata();

    const CommandMetadata* matched = nullptr;
    size_t matchLen = 0;

    for (const auto* meta : allMeta) {
        auto tryMatch = [&](const std::string& name) {
            std::string lower = name;
            std::transform(lower.begin(), lower.end(), lower.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (textLower.size() >= lower.size() + 1 && textLower[lower.size()] == ' ' &&
                textLower.substr(0, lower.size()) == lower && lower.size() > matchLen) {
                matched = meta;
                matchLen = lower.size();
            }
        };
        tryMatch(meta->name);
        for (const auto& alias : meta->aliases)
            tryMatch(alias);
    }

    if (!matched || matched->params.empty())
        return {};

    // Count how many tokens have been fully typed after the command name.
    std::string argsText = text.substr(matchLen + 1);
    std::vector<std::string> tokens;
    {
        std::istringstream iss(argsText);
        std::string tok;
        while (iss >> tok)
            tokens.push_back(tok);
    }

    bool endsWithSpace = !argsText.empty() && argsText.back() == ' ';
    size_t completedParams =
        endsWithSpace ? tokens.size() : (tokens.empty() ? 0 : tokens.size() - 1);

    if (completedParams >= matched->params.size())
        return {};

    std::ostringstream os;
    for (size_t i = completedParams; i < matched->params.size(); ++i) {
        const auto& p = matched->params[i];
        if (p.required) {
            os << " <" << p.name << ">";
        } else {
            os << " [" << p.name << "]";
        }
    }
    return os.str();
}

// =============================================================================
// Command Console
// =============================================================================

void EditorPanels::drawCommandConsole(CommandSystem& cmd, float dpiScale) {
    ImGui::SetNextWindowPos(ImVec2(10 * dpiScale, 500 * dpiScale), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(600 * dpiScale, 350 * dpiScale), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Command Console")) {
        // Output area
        ImVec2 consoleSize = ImVec2(0, -ImGui::GetFrameHeightWithSpacing() * 2);
        if (ImGui::BeginChild("ConsoleOutput", consoleSize, ImGuiChildFlags_Borders)) {
            const auto& log = cmd.getLog();
            for (const auto& entry : log) {
                if (entry.isRawInput) {
                    // Verbatim echo: bright cyan so it's visually distinct from results
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.9f, 1.0f, 1.0f));
                    ImGui::TextWrapped("[%s] (raw) > %s", entry.timestamp.c_str(),
                                       entry.commandLine.c_str());
                    ImGui::PopStyleColor();
                } else {
                    // Color-code by success/failure
                    if (entry.success) {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
                    } else {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
                    }

                    // Show canvas prefix if present
                    std::string prefix;
                    if (!entry.canvasName.empty()) {
                        prefix = "[" + entry.canvasName + "] ";
                    }

                    ImGui::TextWrapped("[%s] %s%s", entry.timestamp.c_str(), prefix.c_str(),
                                       entry.commandLine.c_str());

                    if (!entry.result.empty()) {
                        ImGui::TextWrapped("  => %s", entry.result.c_str());
                    }

                    ImGui::PopStyleColor();
                }
            }

            if (m_scrollConsoleToBottom) {
                ImGui::SetScrollHereY(1.0f);
                m_scrollConsoleToBottom = false;
            }
        }
        ImGui::EndChild();

        // Input area
        ImGui::Separator();
        ImGui::Text(">");
        ImGui::SameLine();

        // Detect multi-line paste (Ctrl+V) while the console input is focused.
        // ImGui's single-line InputText strips newlines from pasted text, so we
        // intercept the clipboard ourselves, execute each non-blank/non-comment
        // line, and schedule a buffer clear via the InputText callback.
        const bool isCtrlV =
            ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_V, /*repeat=*/false);
        if (m_consoleInputFocused && isCtrlV) {
            const char* clip = ImGui::GetClipboardText();
            if (clip && std::strchr(clip, '\n')) {
                // Split clipboard into lines
                std::istringstream stream(clip);
                std::string line;
                std::vector<std::string> cmds;
                while (std::getline(stream, line)) {
                    // Trim whitespace
                    auto s = line.find_first_not_of(" \t\r\n");
                    if (s == std::string::npos)
                        continue;
                    line = line.substr(s);
                    auto e = line.find_last_not_of(" \t\r\n");
                    if (e != std::string::npos)
                        line = line.substr(0, e + 1);
                    // Skip blank lines and comments
                    if (line.empty() || line[0] == '#')
                        continue;
                    if (line.size() >= 2 && line[0] == '/' && line[1] == '/')
                        continue;
                    cmds.push_back(line);
                }
                if (!cmds.empty()) {
                    for (const auto& c : cmds) {
                        cmd.logRawInput(c);
                        cmd.execute(c);
                    }
                    m_scrollConsoleToBottom = true;
                }
                // Ask the InputText callback to wipe whatever single-line
                // fragment ImGui may have already inserted into the buffer.
                m_pendingClear = true;
            }
        }

        // Callback used for buffer clear (multi-line paste), Tab completion,
        // and Up/Down navigation through autocomplete suggestions.
        struct ConsoleCallbackData {
            bool* pendingClear;
            EditorPanels* panels;
        };
        ConsoleCallbackData cbData{&m_pendingClear, this};
        auto consoleCallback = [](ImGuiInputTextCallbackData* data) -> int {
            auto* cbd = static_cast<ConsoleCallbackData*>(data->UserData);
            auto& panels = *cbd->panels;

            if (data->EventFlag == ImGuiInputTextFlags_CallbackAlways) {
                if (*cbd->pendingClear) {
                    data->DeleteChars(0, data->BufTextLen);
                    *cbd->pendingClear = false;
                }
            }

            if (data->EventFlag == ImGuiInputTextFlags_CallbackCompletion) {
                // Tab — accept the currently selected completion.
                if (panels.m_showCompletions && panels.m_selectedCompletion >= 0 &&
                    panels.m_selectedCompletion <
                        static_cast<int>(panels.m_completions.size())) {
                    const std::string& completion =
                        panels.m_completions[panels.m_selectedCompletion];
                    int start = panels.m_completionReplaceStart;
                    data->DeleteChars(start, data->BufTextLen - start);
                    data->InsertChars(start, (completion + " ").c_str());
                    panels.m_showCompletions = false;
                    panels.m_selectedCompletion = -1;
                }
            }

            if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory) {
                if (panels.m_showCompletions && !panels.m_completions.empty()) {
                    if (data->EventKey == ImGuiKey_UpArrow) {
                        panels.m_selectedCompletion =
                            std::max(0, panels.m_selectedCompletion - 1);
                    } else if (data->EventKey == ImGuiKey_DownArrow) {
                        panels.m_selectedCompletion = std::min(
                            static_cast<int>(panels.m_completions.size()) - 1,
                            panels.m_selectedCompletion + 1);
                    }
                }
            }

            return 0;
        };

        bool reclaim = false;
        if (ImGui::InputText("##consoleinput", m_consoleInputBuffer, sizeof(m_consoleInputBuffer),
                             ImGuiInputTextFlags_EnterReturnsTrue |
                                 ImGuiInputTextFlags_CallbackAlways |
                                 ImGuiInputTextFlags_CallbackCompletion |
                                 ImGuiInputTextFlags_CallbackHistory,
                             consoleCallback, &cbData)) {
            std::string input(m_consoleInputBuffer);
            if (!input.empty()) {
                cmd.logRawInput(input);  // Echo verbatim before any parsing
                cmd.execute(input);
                m_consoleInputBuffer[0] = '\0';
                m_scrollConsoleToBottom = true;
            }
            reclaim = true;
            // Clear completions on command execution.
            m_showCompletions = false;
            m_completions.clear();
            m_paramHint.clear();
            m_selectedCompletion = -1;
        }

        // Capture InputText rect before other widgets change "last item" state.
        ImVec2 inputRectMin = ImGui::GetItemRectMin();
        ImVec2 inputRectMax = ImGui::GetItemRectMax();
        m_consoleInputFocused = ImGui::IsItemFocused();

        // Update autocomplete state.
        if (m_consoleInputFocused) {
            updateCompletions(std::string(m_consoleInputBuffer), cmd);
        } else {
            m_showCompletions = false;
            m_completions.clear();
            m_paramHint.clear();
        }

        // Draw parameter hint as ghost text overlaid inside the input area.
        if (!m_paramHint.empty() && m_consoleInputFocused) {
            float textWidth = ImGui::CalcTextSize(m_consoleInputBuffer).x;
            ImVec2 hintPos =
                ImVec2(inputRectMin.x + textWidth + ImGui::GetStyle().FramePadding.x,
                       inputRectMin.y + ImGui::GetStyle().FramePadding.y);
            ImGui::GetWindowDrawList()->AddText(hintPos, IM_COL32(128, 128, 128, 160),
                                                m_paramHint.c_str());
        }

        // Draw autocomplete popup below the input text.
        if (m_showCompletions && !m_completions.empty() && m_consoleInputFocused) {
            float popupWidth = inputRectMax.x - inputRectMin.x;
            ImGui::SetNextWindowPos(ImVec2(inputRectMin.x, inputRectMax.y));
            ImGui::SetNextWindowSize(ImVec2(popupWidth, 0));
            ImGui::SetNextWindowBgAlpha(0.95f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
            if (ImGui::Begin("##autocomplete", nullptr,
                             ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                 ImGuiWindowFlags_NoMove |
                                 ImGuiWindowFlags_NoFocusOnAppearing |
                                 ImGuiWindowFlags_NoNav |
                                 ImGuiWindowFlags_AlwaysAutoResize |
                                 ImGuiWindowFlags_NoSavedSettings)) {
                int maxVisible = std::min(static_cast<int>(m_completions.size()), 8);
                for (int i = 0; i < maxVisible; ++i) {
                    bool isSelected = (i == m_selectedCompletion);
                    if (isSelected) {
                        ImGui::PushStyleColor(ImGuiCol_Text,
                                              ImVec4(1.0f, 1.0f, 0.4f, 1.0f));
                    }
                    ImGui::TextUnformatted(m_completions[i].c_str());
                    if (isSelected) {
                        ImGui::PopStyleColor();
                    }
                }
                if (static_cast<int>(m_completions.size()) > maxVisible) {
                    ImGui::TextDisabled("... and %d more",
                                        static_cast<int>(m_completions.size()) - maxVisible);
                }
            }
            ImGui::End();
            ImGui::PopStyleVar();
        }

        if (reclaim) {
            ImGui::SetKeyboardFocusHere(-1);
        }
    }
    ImGui::End();
}

// =============================================================================
// Tool Palette
// =============================================================================

void EditorPanels::drawToolPalette(ToolPalette& palette, CommandSystem& cmd, float dpiScale) {
    ImGui::SetNextWindowPos(ImVec2(10 * dpiScale, 10 * dpiScale), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(200 * dpiScale, 300 * dpiScale), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Tools")) {
        auto& state = palette.getStateMutable();

        // Tool radio buttons
        struct ToolEntry {
            EditorTool tool;
            const char* label;
        };
        static const ToolEntry tools[] = {
            {EditorTool::Brush, "Brush"},      {EditorTool::Eraser, "Eraser"},
            {EditorTool::ColorPicker, "Pick"}, {EditorTool::Fill, "Fill"},
            {EditorTool::Line, "Line"},        {EditorTool::Rect, "Rect"},
            {EditorTool::Circle, "Circle"},
        };

        int currentTool = static_cast<int>(state.activeTool);
        for (const auto& t : tools) {
            if (ImGui::RadioButton(t.label, currentTool == static_cast<int>(t.tool))) {
                cmd.execute("settool " + ToolPalette::toolToString(t.tool));
            }
        }

        ImGui::Separator();

        // Brush size
        int size = state.brushSize;
        if (ImGui::SliderInt("Size", &size, 0, 16)) {
            cmd.execute("setsize " + std::to_string(size));
        }

        // Fill toggle for shapes
        bool fill = state.fillShape;
        if (ImGui::Checkbox("Fill Shapes", &fill)) {
            state.fillShape = fill;
        }
    }
    ImGui::End();
}

// =============================================================================
// Color Picker
// =============================================================================

void EditorPanels::drawColorPicker(ToolPalette& palette, CommandSystem& cmd, float dpiScale) {
    ImGui::SetNextWindowPos(ImVec2(10 * dpiScale, 320 * dpiScale), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(200 * dpiScale, 170 * dpiScale), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Color")) {
        auto& state = palette.getStateMutable();

        // Convert to float for ImGui
        float col[4] = {state.color.r / 255.0f, state.color.g / 255.0f, state.color.b / 255.0f,
                        state.color.a / 255.0f};

        if (ImGui::ColorEdit4("##color", col,
                              ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar |
                                  ImGuiColorEditFlags_AlphaPreview)) {
            RGBAColor newColor = {
                static_cast<uint8_t>(col[0] * 255), static_cast<uint8_t>(col[1] * 255),
                static_cast<uint8_t>(col[2] * 255), static_cast<uint8_t>(col[3] * 255)};
            cmd.execute("setcolor " + ToolPalette::colorToHex(newColor));
        }

        // Hex display
        ImGui::Text("Hex: %s", ToolPalette::colorToHex(state.color).c_str());
    }
    ImGui::End();
}

// =============================================================================
// Canvas Tabs
// =============================================================================

void EditorPanels::drawCanvasTabs(CanvasRegistry& canvases, CommandSystem& cmd, float dpiScale) {
    ImGui::SetNextWindowPos(ImVec2(220 * dpiScale, 10 * dpiScale), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(900 * dpiScale, 40 * dpiScale), ImGuiCond_FirstUseEver);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
    if (ImGui::Begin("##CanvasTabs", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize)) {
        if (ImGui::BeginTabBar("CanvasTabs")) {
            auto ids = canvases.getIds();
            for (uint32_t id : ids) {
                Canvas* canvas = canvases.getById(id);
                if (!canvas)
                    continue;

                std::string label = "[" + std::to_string(canvas->id) + "] " + canvas->name;
                if (canvas->document && canvas->document->isDirty()) {
                    label += " *";
                }

                bool isActive = (id == cmd.getActiveCanvasId());
                if (isActive) {
                    ImGui::PushStyleColor(ImGuiCol_Tab, ImVec4(0.3f, 0.5f, 0.7f, 1.0f));
                }

                // Pass SetSelected so ImGui's tab state tracks the engine's active canvas.
                // Without this, ImGui picks its own "open" tab independently and BeginTabItem
                // returns true for the wrong tab, firing spurious select commands.
                ImGuiTabItemFlags flags =
                    isActive ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
                if (ImGui::BeginTabItem(label.c_str(), nullptr, flags)) {
                    // IsItemActivated() is true only on the frame the user actually clicked
                    // the tab — not when it was programmatically set via SetSelected.
                    if (ImGui::IsItemActivated() && !isActive) {
                        cmd.execute("select canvas " + canvas->name);
                    }
                    ImGui::EndTabItem();
                }

                if (isActive) {
                    ImGui::PopStyleColor();
                }
            }
            ImGui::EndTabBar();
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

// =============================================================================
// Canvas Viewport
// =============================================================================

void EditorPanels::drawCanvasViewport(Canvas& canvas, ToolPalette& palette, CommandSystem& cmd,
                                      bool isActive, float dpiScale) {
    std::string winName = canvas.name + " (" + std::to_string(canvas.document->getWidth()) + "x" +
                          std::to_string(canvas.document->getHeight()) + ")###viewport_" +
                          std::to_string(canvas.id);

    ImGui::SetNextWindowSize(ImVec2(400 * dpiScale, 400 * dpiScale), ImGuiCond_FirstUseEver);

    if (ImGui::Begin(winName.c_str())) {
        if (canvas.imguiTextureId != VK_NULL_HANDLE) {
            float zoom = canvas.zoomLevel;
            float texW = static_cast<float>(canvas.document->getWidth()) * zoom;
            float texH = static_cast<float>(canvas.document->getHeight()) * zoom;

            // Get cursor position for mouse coordinate calculation
            ImVec2 cursorPos = ImGui::GetCursorScreenPos();

            ImGui::Image((ImTextureID)canvas.imguiTextureId, ImVec2(texW, texH));

            // Mouse interaction
            if (ImGui::IsItemHovered()) {
                // Zoom with mouse wheel
                float scroll = ImGui::GetIO().MouseWheel;
                if (scroll != 0.0f) {
                    canvas.zoomLevel =
                        std::max(1.0f, std::min(64.0f, canvas.zoomLevel + scroll * 2.0f));
                }

                // Convert mouse position to pixel coordinates
                ImVec2 mousePos = ImGui::GetIO().MousePos;
                int pixelX = static_cast<int>((mousePos.x - cursorPos.x) / zoom);
                int pixelY = static_cast<int>((mousePos.y - cursorPos.y) / zoom);

                // Clamp to canvas bounds
                pixelX = std::max(
                    0, std::min(pixelX, static_cast<int>(canvas.document->getWidth()) - 1));
                pixelY = std::max(
                    0, std::min(pixelY, static_cast<int>(canvas.document->getHeight()) - 1));

                // Status text
                ImGui::Text("Pixel: (%d, %d) Zoom: %.0fx", pixelX, pixelY, canvas.zoomLevel);

                // Handle mouse events
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    if (!isActive) {
                        cmd.execute("select canvas " + canvas.name);
                    }
                    std::string action = palette.onCanvasMouseDown(canvas.id, pixelX, pixelY);
                    if (!action.empty()) {
                        cmd.execute("@" + std::to_string(canvas.id) + " " + action);
                    }
                }

                if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                    std::string action = palette.onCanvasMouseDrag(canvas.id, pixelX, pixelY);
                    if (!action.empty()) {
                        cmd.execute("@" + std::to_string(canvas.id) + " " + action);
                    }
                }

                if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                    std::string action = palette.onCanvasMouseUp(canvas.id, pixelX, pixelY);
                    if (!action.empty()) {
                        cmd.execute("@" + std::to_string(canvas.id) + " " + action);
                    }
                }
            }
        } else {
            ImGui::Text("Loading texture...");
        }
    }
    ImGui::End();
}

// =============================================================================
// All Canvas Viewports
// =============================================================================

void EditorPanels::drawAllCanvasViewports(CanvasRegistry& canvases, ToolPalette& palette,
                                          CommandSystem& cmd, float dpiScale) {
    auto ids = canvases.getIds();
    for (uint32_t id : ids) {
        Canvas* canvas = canvases.getById(id);
        if (!canvas || !canvas->document)
            continue;

        bool isActive = (id == cmd.getActiveCanvasId());
        drawCanvasViewport(*canvas, palette, cmd, isActive, dpiScale);
    }
}

// =============================================================================
// Properties Panel
// =============================================================================

void EditorPanels::drawPropertiesPanel(Canvas* activeCanvas, float dpiScale) {
    ImGui::SetNextWindowPos(ImVec2(1130 * dpiScale, 10 * dpiScale), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(250 * dpiScale, 300 * dpiScale), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Properties")) {
        if (activeCanvas && activeCanvas->document) {
            auto* doc = activeCanvas->document.get();

            ImGui::Text("Canvas ID: %u", activeCanvas->id);
            ImGui::Text("Name: %s", activeCanvas->name.c_str());
            ImGui::Text("Size: %u x %u", doc->getWidth(), doc->getHeight());
            ImGui::Text("File: %s",
                        doc->getFilePath().empty() ? "(unsaved)" : doc->getFilePath().c_str());
            ImGui::Text("Dirty: %s", doc->isDirty() ? "Yes" : "No");
            ImGui::Text("Generation: %llu", static_cast<unsigned long long>(doc->getGeneration()));
            ImGui::Text("Undo: %zu / Redo: %zu", doc->getUndoCount(), doc->getRedoCount());
            ImGui::Text("Zoom: %.0fx", activeCanvas->zoomLevel);
        } else {
            ImGui::TextDisabled("No active canvas");
        }
    }
    ImGui::End();
}

// =============================================================================
// Menu Bar
// =============================================================================

void EditorPanels::drawMenuBar(CommandSystem& cmd, CanvasRegistry& canvases) {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New...", "Ctrl+N")) {
                m_showNewCanvasPopup = true;
                m_newCanvasWidth = 32;
                m_newCanvasHeight = 32;
                std::snprintf(m_newCanvasName, sizeof(m_newCanvasName), "%s",
                              canvases.generateUniqueName().c_str());
            }
            if (ImGui::MenuItem("Open...", "Ctrl+O")) {
                cmd.execute("load");
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Save", "Ctrl+S")) {
                cmd.execute("save");
            }
            if (ImGui::MenuItem("Save As...")) {
                cmd.execute("saveas");
            }
            if (ImGui::MenuItem("Export...")) {
                cmd.execute("export png");
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit", "ESC")) {
                cmd.execute("exit");
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Undo", "Ctrl+Z")) {
                cmd.execute("undo");
            }
            if (ImGui::MenuItem("Redo", "Ctrl+Y")) {
                cmd.execute("redo");
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            if (ImGui::MenuItem("Zoom In", "+")) {
                cmd.execute("zoom in");
            }
            if (ImGui::MenuItem("Zoom Out", "-")) {
                cmd.execute("zoom out");
            }
            if (ImGui::MenuItem("Reset Zoom")) {
                cmd.execute("zoom 1");
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("Commands...")) {
                cmd.execute("help");
            }
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

    // New canvas popup
    if (m_showNewCanvasPopup) {
        ImGui::OpenPopup("New Canvas");
        m_showNewCanvasPopup = false;
    }

    if (ImGui::BeginPopupModal("New Canvas", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText("Name", m_newCanvasName, sizeof(m_newCanvasName));
        ImGui::InputInt("Width", &m_newCanvasWidth);
        ImGui::InputInt("Height", &m_newCanvasHeight);

        m_newCanvasWidth = std::max(1, std::min(4096, m_newCanvasWidth));
        m_newCanvasHeight = std::max(1, std::min(4096, m_newCanvasHeight));

        ImGui::Separator();

        if (ImGui::Button("Create", ImVec2(120, 0))) {
            std::string name(m_newCanvasName);
            if (name.empty()) {
                name = canvases.generateUniqueName();
            }
            cmd.execute("create canvas " + name + " " + std::to_string(m_newCanvasWidth) + " " +
                        std::to_string(m_newCanvasHeight));
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

}  // namespace tools
}  // namespace vde
