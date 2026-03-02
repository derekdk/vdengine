#pragma once

/**
 * @file HelpCommand.h
 * @brief Command to display help for all commands or a specific command.
 */

#include <algorithm>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "../../CanvasRegistry.h"
#include "../../ImageDocument.h"
#include "../CommandBase.h"
#include "../CommandRegistry.h"
#include "../EditorContext.h"

namespace vde::tools {

/**
 * @brief Displays help text for all commands or a specific command.
 *
 * Syntax: help [command]
 */
class HelpCommand final : public GlobalCommand {
  public:
    const CommandMetadata& metadata() const override {
        static const CommandMetadata meta{
            .name = "help",
            .aliases = {"?"},
            .category = "Utility",
            .summary = "Show help for commands.",
            .description = "Without arguments, lists all commands grouped by category. "
                           "With a command name, displays detailed help for that command.",
            .scope = CommandScope::Global,
            .params = {},
            .syntaxExample = "help draw line",
        };
        return meta;
    }

    bool usesCustomParsing() const override { return true; }

  protected:
    CommandResult executeGlobal(const CommandArgs& args, EditorContext& /*ctx*/) override {
        std::string remainder = args.remainder();

        // Trim whitespace
        while (!remainder.empty() && remainder.front() == ' ')
            remainder.erase(remainder.begin());
        while (!remainder.empty() && remainder.back() == ' ')
            remainder.pop_back();

        if (!remainder.empty()) {
            // Show help for a specific command
            CommandBase* cmd = CommandRegistry::instance().find(remainder);
            if (!cmd) {
                return {false, "Unknown command: " + remainder};
            }
            return {true, cmd->metadata().formatHelp()};
        }

        // List all commands grouped by category
        auto allMeta = CommandRegistry::instance().getAllMetadata();

        std::map<std::string, std::vector<const CommandMetadata*>> byCategory;
        for (const auto* m : allMeta) {
            byCategory[m->category].push_back(m);
        }

        std::ostringstream os;
        os << "Available commands:\n\n";

        for (auto& [category, commands] : byCategory) {
            os << "  " << category << ":\n";

            // Sort by name within category
            std::sort(commands.begin(), commands.end(),
                      [](const CommandMetadata* a, const CommandMetadata* b) {
                          return a->name < b->name;
                      });

            for (const auto* m : commands) {
                os << "    " << m->formatUsage();
                if (!m->summary.empty()) {
                    os << "  — " << m->summary;
                }
                os << "\n";
            }
            os << "\n";
        }

        os << "Type 'help <command>' for detailed usage.";
        return {true, os.str()};
    }
};

REGISTER_COMMAND(HelpCommand)

}  // namespace vde::tools
