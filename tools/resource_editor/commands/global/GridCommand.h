#pragma once

/**
 * @file GridCommand.h
 * @brief Command to toggle the grid overlay on the active canvas.
 */

#include <string>

#include "../../CanvasRegistry.h"
#include "../../ImageDocument.h"
#include "../CommandBase.h"
#include "../CommandRegistry.h"
#include "../EditorContext.h"

namespace vde::tools {

/**
 * @brief Toggles the grid display on or off.
 *
 * Syntax: grid on|off
 */
class GridCommand final : public GlobalCommand {
  public:
    const CommandMetadata& metadata() const override {
        static const CommandMetadata meta{
            .name = "grid",
            .aliases = {},
            .category = "View",
            .summary = "Toggle the pixel grid overlay.",
            .description = "Enables or disables the grid overlay on the active canvas viewport.",
            .scope = CommandScope::Global,
            .params =
                {
                    {.name = "state",
                     .type = ParamType::Enum,
                     .required = true,
                     .description = "Grid visibility",
                     .enumValues = {"on", "off"}},
                },
            .syntaxExample = "grid on",
        };
        return meta;
    }

  protected:
    CommandResult executeGlobal(const CommandArgs& args, EditorContext& /*ctx*/) override {
        const std::string& state = args.getString("state");
        bool enabled = (state == "on");

        // TODO: Wire to canvas/viewport grid state once rendering supports it.
        return {true, std::string("Grid ") + (enabled ? "enabled" : "disabled")};
    }
};

REGISTER_COMMAND(GridCommand)

}  // namespace vde::tools
