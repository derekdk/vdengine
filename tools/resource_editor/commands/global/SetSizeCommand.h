#pragma once

/**
 * @file SetSizeCommand.h
 * @brief Command to set the brush size on the tool palette.
 */

#include "../../CanvasRegistry.h"
#include "../../ImageDocument.h"
#include "../../ToolPalette.h"
#include "../CommandBase.h"
#include "../CommandRegistry.h"
#include "../EditorContext.h"

namespace vde::tools {

/**
 * @brief Sets the brush size on the tool palette.
 *
 * Syntax: setsize <n>
 */
class SetSizeCommand final : public GlobalCommand {
  public:
    const CommandMetadata& metadata() const override {
        static const CommandMetadata meta{
            .name = "setsize",
            .aliases = {"brushsize"},
            .category = "Tool",
            .summary = "Set the brush size.",
            .description = "Sets the brush diameter in pixels. Must be at least 1.",
            .scope = CommandScope::Global,
            .params =
                {
                    {.name = "size",
                     .type = ParamType::Int,
                     .required = true,
                     .description = "Brush diameter in pixels"},
                },
            .syntaxExample = "setsize 3",
        };
        return meta;
    }

  protected:
    CommandResult executeGlobal(const CommandArgs& args, EditorContext& ctx) override {
        int size = args.getInt("size");

        if (size < 1) {
            return {false, "Brush size must be at least 1"};
        }

        if (!ctx.palette) {
            return {false, "Tool palette not available"};
        }

        ctx.palette->getStateMutable().brushSize = size;
        return {true, "Brush size set to " + std::to_string(size)};
    }
};

REGISTER_COMMAND(SetSizeCommand)

}  // namespace vde::tools
