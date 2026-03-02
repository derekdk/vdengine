/**
 * @file ReplConsole.h
 * @brief ImGui-based REPL console widget with tab-completion and history.
 *
 * Provides a self-contained console UI that can be embedded in any ImGui
 * window.  Features:
 * - Tab-completion of commands and arguments via CommandRegistry
 * - Up/Down arrow command history navigation
 * - Scrollable output log
 * - Configurable prompt string
 */

#pragma once

#include <functional>
#include <string>
#include <vector>

#include "CommandRegistry.h"
#include <imgui.h>

namespace vde {
namespace tools {

/**
 * @brief Callback invoked when the user submits a command line.
 */
using SubmitCallback = std::function<void(const std::string& cmdLine)>;

/**
 * @brief ImGui REPL console widget with tab-completion and history.
 */
class ReplConsole {
  public:
    ReplConsole() = default;
    ~ReplConsole() = default;

    /**
     * @brief Set the command registry used for tab-completion.
     */
    void setCommandRegistry(const CommandRegistry* registry) { m_registry = registry; }

    /**
     * @brief Set the callback invoked when the user submits a command.
     */
    void setSubmitCallback(SubmitCallback callback) { m_submitCallback = std::move(callback); }

    /**
     * @brief Add a message to the output log.
     */
    void addMessage(const std::string& message);

    /**
     * @brief Get the full output log.
     */
    const std::vector<std::string>& getLog() const { return m_log; }

    /**
     * @brief Clear the output log.
     */
    void clearLog() { m_log.clear(); }

    /**
     * @brief Draw the console output area and input field.
     *
     * Call this inside an ImGui::Begin()/End() block.  The console
     * fills the available content region minus space for the input row.
     */
    void draw();

    /**
     * @brief Request focus on the input field next frame.
     */
    void focusInput() { m_focusInput = true; }

  private:
    const CommandRegistry* m_registry = nullptr;
    SubmitCallback m_submitCallback;

    // Output log
    std::vector<std::string> m_log;
    bool m_scrollToBottom = false;

    // Input state
    char m_inputBuffer[512] = {0};
    bool m_focusInput = false;

    // Command history
    std::vector<std::string> m_history;
    int m_historyPos = -1;  ///< -1 means editing new line, 0..N navigating history

    // Tab-completion state
    std::vector<std::string> m_completions;
    int m_completionIndex = -1;
    std::string m_completionBase;  ///< The text when tab was first pressed

    /**
     * @brief ImGui input text callback for history and tab-completion.
     */
    static int inputCallback(ImGuiInputTextCallbackData* data);

    /**
     * @brief Handle tab key for completion cycling.
     */
    void handleTab(ImGuiInputTextCallbackData* data);

    /**
     * @brief Handle up/down keys for history navigation.
     */
    void handleHistory(ImGuiInputTextCallbackData* data, bool up);

    /**
     * @brief Reset completion state (called on any non-tab edit).
     */
    void resetCompletion();
};

}  // namespace tools
}  // namespace vde
