#pragma once

/**
 * @file HistoryCommand.h
 * @brief Command to display the command execution history.
 */

#include <sstream>
#include <string>

#include "../../CanvasRegistry.h"
#include "../../ImageDocument.h"
#include "../CommandBase.h"
#include "../CommandRegistry.h"
#include "../EditorContext.h"

namespace vde::tools {

/**
 * @brief Displays the last N entries from the command execution log.
 *
 * Syntax: history [n]
 */
class HistoryCommand final : public GlobalCommand {
  public:
    const CommandMetadata& metadata() const override {
        static const CommandMetadata meta{
            .name = "history",
            .aliases = {"log"},
            .category = "Utility",
            .summary = "Show recent command history.",
            .description = "Displays the last N entries (default 20) from the command log.",
            .scope = CommandScope::Global,
            .params =
                {
                    {.name = "n",
                     .type = ParamType::Int,
                     .required = false,
                     .description = "Number of entries to show",
                     .defaultValue = "20"},
                },
            .syntaxExample = "history 10",
        };
        return meta;
    }

  protected:
    CommandResult executeGlobal(const CommandArgs& args, EditorContext& ctx) override {
        int n = args.has("n") ? args.getInt("n") : 20;
        if (n <= 0)
            n = 20;

        const auto& log = ctx.commands->getLog();
        if (log.empty()) {
            return {true, "No command history."};
        }

        size_t count = static_cast<size_t>(n);
        size_t start = log.size() > count ? log.size() - count : 0;

        std::ostringstream os;
        os << "Command history (last " << (log.size() - start) << " of " << log.size() << "):\n";

        for (size_t i = start; i < log.size(); ++i) {
            const auto& entry = log[i];
            os << "  " << (i + 1) << ". ";
            if (!entry.timestamp.empty()) {
                os << "[" << entry.timestamp << "] ";
            }
            os << entry.commandLine;
            if (!entry.canvasName.empty()) {
                os << " @" << entry.canvasName;
            }
            os << " => " << (entry.success ? "OK" : "FAIL");
            if (!entry.result.empty()) {
                os << ": " << entry.result;
            }
            os << "\n";
        }

        return {true, os.str()};
    }
};

REGISTER_COMMAND(HistoryCommand)

}  // namespace vde::tools
