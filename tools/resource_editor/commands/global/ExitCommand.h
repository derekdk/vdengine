#pragma once

/**
 * @file ExitCommand.h
 * @brief Command to quit the resource editor application.
 */

#include <vde/api/Game.h>

#include "../../CanvasRegistry.h"
#include "../../ImageDocument.h"
#include "../CommandBase.h"
#include "../CommandRegistry.h"
#include "../EditorContext.h"

namespace vde::tools {

/**
 * @brief Quits the resource editor application.
 *
 * Syntax: exit
 */
class ExitCommand final : public GlobalCommand {
  public:
    const CommandMetadata& metadata() const override {
        static const CommandMetadata meta{
            .name = "exit",
            .aliases = {"quit"},
            .category = "Utility",
            .summary = "Quit the resource editor.",
            .description = "Exits the application immediately.",
            .scope = CommandScope::Global,
            .params = {},
            .syntaxExample = "exit",
        };
        return meta;
    }

  protected:
    CommandResult executeGlobal(const CommandArgs& /*args*/, EditorContext& ctx) override {
        if (ctx.game) {
            ctx.game->quit();
        }
        return {true, "Exiting..."};
    }
};

REGISTER_COMMAND(ExitCommand)

}  // namespace vde::tools
