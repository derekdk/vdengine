#pragma once

/**
 * @file DefineColorCommand.h
 * @brief Command to define a named color in the editor context.
 */

#include "../../CanvasRegistry.h"
#include "../../ImageDocument.h"
#include "../../ToolPalette.h"
#include "../CommandBase.h"
#include "../CommandRegistry.h"
#include "../EditorContext.h"

namespace vde::tools {

class DefineColorCommand final : public GlobalCommand {
  public:
    const CommandMetadata& metadata() const override {
        static const CommandMetadata meta{
            .name = "define color",
            .aliases = {},
            .category = "Color",
            .summary = "Define a named color.",
            .description = "Adds or updates a named color in the editor context.",
            .scope = CommandScope::Global,
            .params =
                {
                    {.name = "name",
                     .type = ParamType::String,
                     .required = true,
                     .description = "Name for the color"},
                    {.name = "color",
                     .type = ParamType::Color,
                     .required = true,
                     .description = "Color in #RRGGBB or #RRGGBBAA format"},
                },
            .syntaxExample = "define color sky #87CEEB",
        };
        return meta;
    }

  protected:
    CommandResult executeGlobal(const CommandArgs& args, EditorContext& ctx) override {
        auto name = args.getString("name");
        auto color = args.getColor("color");
        ctx.namedColors[name] = color;
        return {true, "Defined color '" + name + "' as " + color.toHex()};
    }
};

REGISTER_COMMAND(DefineColorCommand)

}  // namespace vde::tools
