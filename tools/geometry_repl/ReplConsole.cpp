/**
 * @file ReplConsole.cpp
 * @brief Implementation of the ImGui REPL console widget.
 */

#include "ReplConsole.h"

#include <algorithm>
#include <cstring>

namespace vde {
namespace tools {

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void ReplConsole::addMessage(const std::string& message) {
    m_log.push_back(message);

    // Cap log size
    if (m_log.size() > 2000) {
        m_log.erase(m_log.begin());
    }

    m_scrollToBottom = true;
}

void ReplConsole::draw() {
    // Reserve space for input line and completion hints (always reserve 2 lines for hints)
    float hintHeight = ImGui::GetTextLineHeightWithSpacing() * 2;
    float reservedHeight = ImGui::GetFrameHeightWithSpacing() + 4 + hintHeight;

    // --- Output area (scrollable) ---
    ImVec2 consoleSize = ImVec2(0, -reservedHeight);
    if (ImGui::BeginChild("##ConsoleOutput", consoleSize, true,
                          ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
        for (const auto& msg : m_log) {
            // Color error lines red
            if (msg.size() > 5 && msg.substr(0, 5) == "ERROR") {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
                ImGui::TextWrapped("%s", msg.c_str());
                ImGui::PopStyleColor();
            } else if (msg.size() > 1 && msg[0] == '>') {
                // Echo lines in dim cyan
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.85f, 0.95f, 1.0f));
                ImGui::TextWrapped("%s", msg.c_str());
                ImGui::PopStyleColor();
            } else if (msg.size() > 3 && msg.substr(0, 4) == "====") {
                // Header lines in yellow
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.9f, 0.5f, 1.0f));
                ImGui::TextWrapped("%s", msg.c_str());
                ImGui::PopStyleColor();
            } else {
                ImGui::TextWrapped("%s", msg.c_str());
            }
        }

        if (m_scrollToBottom) {
            ImGui::SetScrollHereY(1.0f);
            m_scrollToBottom = false;
        }
    }
    ImGui::EndChild();

    // --- Completion hint bar (always present to maintain stable ImGui ID stack) ---
    // Use a fixed-height child region to prevent layout shifts
    if (ImGui::BeginChild("##CompletionHints", ImVec2(0, hintHeight), false)) {
        if (!m_completions.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
            std::string hint;
            for (size_t i = 0; i < m_completions.size() && i < 10; ++i) {
                if (i > 0)
                    hint += "  ";
                if (static_cast<int>(i) == m_completionIndex) {
                    hint += "[" + m_completions[i] + "]";
                } else {
                    hint += m_completions[i];
                }
            }
            if (m_completions.size() > 10) {
                hint += "  (+" + std::to_string(m_completions.size() - 10) + " more)";
            }
            ImGui::TextWrapped("%s", hint.c_str());
            ImGui::PopStyleColor();
        }
    }
    ImGui::EndChild();

    // --- Input line ---
    ImGui::Separator();
    ImGui::Text(">");
    ImGui::SameLine();

    ImGuiInputTextFlags flags =
        ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackCompletion |
        ImGuiInputTextFlags_CallbackHistory | ImGuiInputTextFlags_CallbackEdit;

    // Pass this pointer through UserData
    ImGui::PushItemWidth(-1);
    bool submitted = ImGui::InputText("##ReplInput", m_inputBuffer, sizeof(m_inputBuffer), flags,
                                      &ReplConsole::inputCallback, static_cast<void*>(this));
    ImGui::PopItemWidth();

    if (submitted) {
        std::string cmd(m_inputBuffer);
        if (!cmd.empty()) {
            // Add to history
            // Remove duplicate if it's the same as the last entry
            if (m_history.empty() || m_history.back() != cmd) {
                m_history.push_back(cmd);
            }

            // Submit
            if (m_submitCallback) {
                m_submitCallback(cmd);
            }

            m_inputBuffer[0] = '\0';
            m_historyPos = -1;
            resetCompletion();
        }
        // Keep focus
        ImGui::SetKeyboardFocusHere(-1);
    }

    // Focus on first appearance or on request
    if (m_focusInput || ImGui::IsWindowAppearing()) {
        ImGui::SetKeyboardFocusHere(-1);
        m_focusInput = false;
    }
}

// ---------------------------------------------------------------------------
// ImGui callback
// ---------------------------------------------------------------------------

int ReplConsole::inputCallback(ImGuiInputTextCallbackData* data) {
    auto* console = static_cast<ReplConsole*>(data->UserData);
    if (!console) {
        return 0;
    }

    switch (data->EventFlag) {
    case ImGuiInputTextFlags_CallbackCompletion:
        console->handleTab(data);
        break;

    case ImGuiInputTextFlags_CallbackHistory:
        if (data->EventKey == ImGuiKey_UpArrow) {
            console->handleHistory(data, true);
        } else if (data->EventKey == ImGuiKey_DownArrow) {
            console->handleHistory(data, false);
        }
        break;

    case ImGuiInputTextFlags_CallbackEdit:
        // Any edit that isn't tab resets completion state
        console->resetCompletion();
        break;

    default:
        break;
    }

    return 0;
}

// ---------------------------------------------------------------------------
// Tab-completion
// ---------------------------------------------------------------------------

void ReplConsole::handleTab(ImGuiInputTextCallbackData* data) {
    if (!m_registry) {
        return;
    }

    std::string currentInput(data->Buf, static_cast<size_t>(data->BufTextLen));

    // If completions list is empty or base changed, fetch new completions
    if (m_completions.empty() || m_completionBase != currentInput) {
        m_completionBase = currentInput;
        m_completions = m_registry->getCompletions(currentInput);
        m_completionIndex = -1;
    }

    if (m_completions.empty()) {
        return;
    }

    // Cycle through completions
    m_completionIndex = (m_completionIndex + 1) % static_cast<int>(m_completions.size());

    // Build the completed string
    // Find where the last token starts
    std::string base = m_completionBase;
    size_t lastSpace = base.rfind(' ');
    std::string prefix;
    if (lastSpace != std::string::npos) {
        prefix = base.substr(0, lastSpace + 1);
    }

    std::string completed = prefix + m_completions[m_completionIndex];

    // Replace buffer contents
    data->DeleteChars(0, data->BufTextLen);
    data->InsertChars(0, completed.c_str());
}

// ---------------------------------------------------------------------------
// History navigation
// ---------------------------------------------------------------------------

void ReplConsole::handleHistory(ImGuiInputTextCallbackData* data, bool up) {
    if (m_history.empty()) {
        return;
    }

    if (up) {
        if (m_historyPos == -1) {
            // Save current input before navigating
            m_historyPos = static_cast<int>(m_history.size()) - 1;
        } else if (m_historyPos > 0) {
            m_historyPos--;
        }
    } else {
        if (m_historyPos != -1) {
            m_historyPos++;
            if (m_historyPos >= static_cast<int>(m_history.size())) {
                m_historyPos = -1;
            }
        }
    }

    // Replace buffer
    data->DeleteChars(0, data->BufTextLen);
    if (m_historyPos >= 0) {
        data->InsertChars(0, m_history[m_historyPos].c_str());
    }
}

// ---------------------------------------------------------------------------
// Reset
// ---------------------------------------------------------------------------

void ReplConsole::resetCompletion() {
    m_completions.clear();
    m_completionIndex = -1;
    m_completionBase.clear();
}

}  // namespace tools
}  // namespace vde
