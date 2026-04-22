#pragma once

/**
 * @file UndefineColorCommand.h
 * @brief Command to remove a named color from the editor context.
 */

#include "../../CanvasRegistry.h"
#include "../../ImageDocument.h"
#include "../../ToolPalette.h"
#include "../CommandBase.h"
#include "../CommandRegistry.h"
#include "../EditorContext.h"

namespace vde::tools {

class UndefineColorCommand final : public GlobalCommand {
  public:
    const CommandMetadata& metadata() const override {
        static const CommandMetadata meta{
            .name = "undefine color",
            .aliases = {},
            .category = "Color",
            .summary = "Remove a named color.",
            .description = "Removes a named color from the editor context.",
            .scope = CommandScope::Global,
            .params =
                {
                    {.name = "name",
                     .type = ParamType::String,
                     .required = true,
                     .description = "Name of the color to remove",
                     .defaultValue = "",
                     .enumValues = {}},
                },
            .syntaxExample = "undefine color sky",
        };
        return meta;
    }

  protected:
    CommandResult executeGlobal(const CommandArgs& args, EditorContext& ctx) override {
        auto name = args.getString("name");
        auto it = ctx.namedColors.find(name);
        if (it == ctx.namedColors.end()) {
            return {false, "Named color '" + name + "' not found."};
        }
        ctx.namedColors.erase(it);
        return {true, "Removed named color '" + name + "'."};
    }
};

REGISTER_COMMAND(UndefineColorCommand)

}  // namespace vde::tools
