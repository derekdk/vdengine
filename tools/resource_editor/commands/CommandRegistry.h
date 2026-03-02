#pragma once

/**
 * @file CommandRegistry.h
 * @brief Singleton registry of all available Resource Editor commands.
 *
 * Commands self-register via the REGISTER_COMMAND macro.  The registry
 * supports lookup by primary name or alias with longest-match semantics
 * for compound command names.
 */

#include "CommandBase.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace vde::tools {

/**
 * @brief Singleton that owns all registered CommandBase instances.
 */
class CommandRegistry {
public:
    /** @brief Access the global registry singleton. */
    static CommandRegistry& instance();

    /**
     * @brief Register a command.  The registry takes ownership.
     * @param cmd Unique pointer to the command instance.
     */
    void registerCommand(std::unique_ptr<CommandBase> cmd);

    /**
     * @brief Find a command by name or alias (longest-match on spaces).
     * @param name Input string (may be a compound name like "draw line").
     * @return Pointer to the command, or nullptr if not found.
     */
    CommandBase* find(const std::string& name) const;

    /** @brief Return metadata pointers for every registered command. */
    std::vector<const CommandMetadata*> getAllMetadata() const;

    /** @brief Return metadata pointers filtered by category. */
    std::vector<const CommandMetadata*> getByCategory(const std::string& category) const;

private:
    CommandRegistry() = default;

    std::vector<std::unique_ptr<CommandBase>> m_commands;
    std::map<std::string, CommandBase*> m_nameIndex;
    std::map<std::string, CommandBase*> m_aliasIndex;
};

/**
 * @brief Self-registration macro.  Place in a .cpp file after the command class.
 *
 * Example:
 * @code
 *   REGISTER_COMMAND(DrawLineCommand);
 * @endcode
 */
#define REGISTER_COMMAND(CommandClass)                                  \
    static struct CommandClass##_Registrar {                            \
        CommandClass##_Registrar() {                                    \
            CommandRegistry::instance().registerCommand(                \
                std::make_unique<CommandClass>());                      \
        }                                                              \
    } s_##CommandClass##_registrar;

}  // namespace vde::tools
