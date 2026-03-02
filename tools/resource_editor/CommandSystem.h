#pragma once

/**
 * @file CommandSystem.h
 * @brief Thin dispatch layer between UI/REPL input and commands.
 *
 * Parses command lines, resolves commands from the registry,
 * delegates argument parsing, and maintains an execution log.
 */

#include <cstdint>
#include <string>
#include <vector>

#include "commands/CommandArgParser.h"
#include "commands/CommandBase.h"
#include "commands/CommandRegistry.h"
#include "commands/EditorContext.h"

namespace vde::tools {

/**
 * @brief A single entry in the command execution log.
 */
struct CommandLogEntry {
    std::string timestamp;
    std::string commandLine;
    std::string canvasName;
    std::string result;
    bool success = true;
};

/**
 * @brief Dispatches command strings to registered CommandBase instances.
 *
 * Supports optional @canvasName prefixes for targeting specific canvases,
 * longest-match command lookup, metadata-driven argument parsing, and
 * a bounded execution log.
 */
class CommandSystem {
  public:
    /**
     * @brief Wire the system to the shared editor context.
     * @param ctx Editor context whose lifetime must exceed this object's.
     */
    void initialize(EditorContext& ctx);

    /**
     * @brief Parse and execute a single command line.
     * @param commandLine Full text (may include @canvas prefix).
     * @return true if the command succeeds.
     */
    bool execute(const std::string& commandLine);

    /**
     * @brief Execute every command in a script file.
     * @param filePath Path to the script.
     * @return true if every line succeeds.
     */
    bool executeScript(const std::string& filePath);

    /**
     * @brief Save a range of the log to a text file.
     * @param start First index (inclusive).
     * @param end   Past-the-end index.
     * @param filePath Destination path.
     * @return true on success.
     */
    bool saveLogRange(size_t start, size_t end, const std::string& filePath);

    /**
     * @brief Save the entire log to a text file.
     */
    bool saveFullLog(const std::string& filePath);

    /** @brief Read-only access to the execution log. */
    const std::vector<CommandLogEntry>& getLog() const;

    /**
     * @brief Manually append an entry to the log.
     * @param line    Command text.
     * @param result  Result message.
     * @param success Whether the command succeeded.
     * @param canvasName Optional canvas name for context.
     */
    void addLogEntry(const std::string& line, const std::string& result, bool success,
                     const std::string& canvasName = "");

    /** @brief Clear the log and reset active canvas. */
    void clear();

    void setActiveCanvasId(uint32_t id);
    uint32_t getActiveCanvasId() const;

  private:
    /** @brief Intermediate result of resolving a command line. */
    struct ResolvedCommand {
        CommandBase* command = nullptr;
        std::string commandName;
        std::string rawArgs;
        uint32_t targetCanvasId = 0;
    };

    ResolvedCommand resolveCommand(const std::string& commandLine);
    std::string getCurrentTimestamp() const;

    EditorContext* m_ctx = nullptr;
    std::vector<CommandLogEntry> m_log;
    uint32_t m_activeCanvasId = 0;
};

}  // namespace vde::tools
