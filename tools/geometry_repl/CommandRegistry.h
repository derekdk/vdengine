/**
 * @file CommandRegistry.h
 * @brief Dynamic command registry for REPL tools.
 *
 * Provides a registry that supports:
 * - Runtime add/remove of commands
 * - Enable/disable without removing
 * - Metadata (help text, usage, argument hints)
 * - Tab-completion of command names
 * - Argument completion callbacks per command
 */

#pragma once

#include <functional>
#include <map>
#include <string>
#include <vector>

namespace vde {
namespace tools {

/**
 * @brief Completion callback type.
 *
 * Given the partial argument text and the full list of tokens so far,
 * returns a list of possible completions.
 */
using CompletionCallback = std::function<std::vector<std::string>(
    const std::string& partial, const std::vector<std::string>& tokens)>;

/**
 * @brief Command handler callback type.
 *
 * Receives the full argument string (everything after the command name).
 */
using CommandHandler = std::function<void(const std::string& args)>;

/**
 * @brief Describes a single registered command.
 */
struct CommandInfo {
    std::string name;              ///< Command name (lowercase)
    std::string usage;             ///< Usage string, e.g. "create <name> <type>"
    std::string description;       ///< Brief description for help listing
    CommandHandler handler;        ///< Callback executed when command is invoked
    CompletionCallback completer;  ///< Optional tab-completion for arguments
    bool enabled = true;           ///< Whether the command is currently active
};

/**
 * @brief Dynamic command registry with metadata and completion support.
 *
 * Commands are stored by name. The registry supports:
 * - Adding and removing commands at runtime
 * - Enabling/disabling commands without removing them
 * - Querying command metadata for help display
 * - Tab-completing command names and arguments
 */
class CommandRegistry {
  public:
    CommandRegistry() = default;
    ~CommandRegistry() = default;

    /**
     * @brief Register a new command.
     * @param name Command name (will be lowercased)
     * @param usage Usage string shown in help, e.g. "create <name> <type>"
     * @param description Brief description
     * @param handler Callback for execution
     * @param completer Optional argument completion callback
     * @return true if registered, false if name already taken
     */
    bool add(const std::string& name, const std::string& usage, const std::string& description,
             CommandHandler handler, CompletionCallback completer = nullptr);

    /**
     * @brief Remove a command entirely.
     * @param name Command name
     * @return true if removed, false if not found
     */
    bool remove(const std::string& name);

    /**
     * @brief Enable a previously disabled command.
     * @param name Command name
     * @return true if enabled, false if not found
     */
    bool enable(const std::string& name);

    /**
     * @brief Disable a command (still registered, but won't execute or complete).
     * @param name Command name
     * @return true if disabled, false if not found
     */
    bool disable(const std::string& name);

    /**
     * @brief Check if a command exists (enabled or disabled).
     */
    bool has(const std::string& name) const;

    /**
     * @brief Check if a command is enabled.
     */
    bool isEnabled(const std::string& name) const;

    /**
     * @brief Execute a full command line.
     *
     * Parses the first token as the command name, passes the rest to the handler.
     * @param cmdLine Full command string
     * @return true if command was found and enabled, false otherwise
     */
    bool execute(const std::string& cmdLine);

    /**
     * @brief Get tab-completions for a partial input string.
     *
     * If the input has no spaces, completes command names.
     * If the input has spaces, delegates to the command's completer.
     *
     * @param input Current input text
     * @return List of possible completions (full replacement strings)
     */
    std::vector<std::string> getCompletions(const std::string& input) const;

    /**
     * @brief Get all enabled commands (sorted by name).
     */
    std::vector<const CommandInfo*> getEnabledCommands() const;

    /**
     * @brief Get all commands (sorted by name).
     */
    std::vector<const CommandInfo*> getAllCommands() const;

    /**
     * @brief Get info for a specific command.
     * @return Pointer to CommandInfo or nullptr if not found
     */
    const CommandInfo* getCommand(const std::string& name) const;

  private:
    std::map<std::string, CommandInfo> m_commands;

    /**
     * @brief Lowercase a string in place.
     */
    static std::string toLower(const std::string& str);
};

}  // namespace tools
}  // namespace vde
