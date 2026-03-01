/**
 * @file CommandSystem.h
 * @brief Central command dispatch with canvas resolution, logging, and script I/O.
 *
 * The CommandSystem parses command strings, resolves optional @canvas prefixes,
 * dispatches to registered handlers, and maintains a timestamped execution log.
 */

#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "CanvasRegistry.h"

namespace vde {
namespace tools {

/**
 * @brief A single entry in the command execution log.
 */
struct CommandLogEntry {
    size_t index = 0;
    std::string timestamp;    ///< HH:MM:SS.mmm format
    std::string commandLine;  ///< Original command text
    std::string result;       ///< Output/error message
    bool success = true;
    uint32_t canvasId = 0;   ///< 0 = global
    std::string canvasName;  ///< For display
};

/**
 * @brief Resolved command after parsing optional @canvas prefix.
 */
struct ResolvedCommand {
    uint32_t canvasId = 0;           ///< 0 = no canvas (global or unresolved)
    std::string canvasName;          ///< For display in log
    std::string commandName;         ///< Parsed command name
    std::string argsString;          ///< Everything after the command name
    bool hasExplicitCanvas = false;  ///< True if @name/@id prefix was present
};

/**
 * @brief Handler for global commands (not canvas-scoped).
 */
using GlobalHandler = std::function<void(const std::string& args)>;

/**
 * @brief Handler for canvas-targeted commands.
 */
using CanvasHandler = std::function<void(uint32_t canvasId, const std::string& args)>;

/**
 * @brief Central command dispatch with canvas resolution and logging.
 *
 * Commands can be:
 * - Global: `help`, `list`, `new 32 32 test`
 * - Canvas-targeted: `paint 5 5 #FF0000FF` (targets active canvas)
 * - Explicitly targeted: `@hero paint 5 5 #FF0000FF` (targets named canvas)
 */
class CommandSystem {
  public:
    CommandSystem() = default;
    ~CommandSystem() = default;

    /**
     * @brief Set the canvas registry for name/ID resolution.
     */
    void setRegistry(CanvasRegistry* registry) { m_registry = registry; }

    /**
     * @brief Set the active canvas ID (used when no @prefix is given).
     */
    void setActiveCanvasId(uint32_t id) { m_activeCanvasId = id; }

    /**
     * @brief Get the current active canvas ID.
     */
    uint32_t getActiveCanvasId() const { return m_activeCanvasId; }

    /**
     * @brief Register a global command handler.
     * @param name Command name (lowercase)
     * @param help Help text for the command
     * @param handler Handler function
     */
    void registerGlobalCommand(const std::string& name, const std::string& help,
                               GlobalHandler handler);

    /**
     * @brief Register a canvas-targeted command handler.
     * @param name Command name (lowercase)
     * @param help Help text for the command
     * @param handler Handler function taking (canvasId, args)
     */
    void registerCanvasCommand(const std::string& name, const std::string& help,
                               CanvasHandler handler);

    /**
     * @brief Execute a command line string.
     * @param commandLine Full command text (may include @canvas prefix)
     * @return true if command executed successfully
     */
    bool execute(const std::string& commandLine);

    /**
     * @brief Execute all commands from a script file.
     * @param filePath Path to the script file
     * @return true if all commands succeeded
     */
    bool executeScript(const std::string& filePath);

    /**
     * @brief Save the full command log to a file.
     * @param filePath Output file path
     * @return true if successful
     */
    bool saveFullLog(const std::string& filePath) const;

    /**
     * @brief Save a range of the command log to a file.
     * @param startIdx Start index (inclusive)
     * @param endIdx End index (exclusive)
     * @param filePath Output file path
     * @return true if successful
     */
    bool saveLogRange(size_t startIdx, size_t endIdx, const std::string& filePath) const;

    /**
     * @brief Get the full command log.
     */
    const std::vector<CommandLogEntry>& getLog() const { return m_log; }

    /**
     * @brief Get all registered command names.
     */
    std::vector<std::string> getCommandNames() const;

    /**
     * @brief Get help text for a command.
     * @return Help text, or empty string if not found
     */
    std::string getHelpText(const std::string& command) const;

    /**
     * @brief Clear the command log.
     */
    void clearLog() { m_log.clear(); }

    /**
     * @brief Set the result for the last executed command.
     *
     * Called by command handlers to set success/error messages.
     */
    void setLastResult(const std::string& result, bool success = true);

  private:
    /**
     * @brief Parse optional @canvas prefix and extract command name + args.
     */
    ResolvedCommand resolveCommand(const std::string& commandLine) const;

    /**
     * @brief Get current timestamp in HH:MM:SS.mmm format.
     */
    static std::string getCurrentTimestamp();

    CanvasRegistry* m_registry = nullptr;
    uint32_t m_activeCanvasId = 0;

    std::map<std::string, GlobalHandler> m_globalHandlers;
    std::map<std::string, CanvasHandler> m_canvasHandlers;
    std::map<std::string, std::string> m_helpText;

    std::vector<CommandLogEntry> m_log;
};

}  // namespace tools
}  // namespace vde
