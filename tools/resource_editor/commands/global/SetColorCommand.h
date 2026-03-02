#pragma once

/**
 * @file SetColorCommand.h
 * @brief Command to set the active drawing color on the tool palette.
 */

#include "../../CanvasRegistry.h"
#include "../../ImageDocument.h"
#include "../../ToolPalette.h"
#include "../CommandBase.h"
#include "../CommandRegistry.h"
#include "../EditorContext.h"

namespace vde::tools {

/**
 * @brief Sets the active drawing color on the tool palette.
 *
 * Syntax: setcolor <color>
 */
class SetColorCommand final : public GlobalCommand {
  public:
    const CommandMetadata& metadata() const override {
        static const CommandMetadata meta{
            .name = "setcolor",
            .aliases = {"color"},
            .category = "Tool",
            .summary = "Set the active drawing color.",
            .description =
                "Sets the palette drawing color to the specified hex color or named color.",
            .scope = CommandScope::Global,
            .params =
                {
                    {.name = "color",
                     .type = ParamType::Color,
                     .required = true,
                     .description = "Color in #RRGGBB or #RRGGBBAA format, or a named color"},
                },
            .syntaxExample = "setcolor #FF0000",
        };
        return meta;
    }

  protected:
    CommandResult executeGlobal(const CommandArgs& args, EditorContext& ctx) override {
        auto color = args.getColor("color");

        if (!ctx.palette) {
            return {false, "Tool palette not available"};
        }

        ctx.palette->getStateMutable().color = color;
        return {true, "Color set to " + color.toHex()};
    }
};

REGISTER_COMMAND(SetColorCommand)

}  // namespace vde::tools
