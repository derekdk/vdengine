#pragma once

/**
 * @file FillCommand.h
 * @brief Command to fill an entire canvas with a solid color.
 */

#include "../../CanvasRegistry.h"
#include "../CommandBase.h"
#include "../CommandRegistry.h"
#include "../EditorContext.h"

namespace vde::tools {

/**
 * @brief Fills the entire canvas with a single color.
 *
 * Syntax: fill <color>
 */
class FillCommand final : public CanvasCommand {
  public:
    const CommandMetadata& metadata() const override {
        static const CommandMetadata meta{
            .name = "fill",
            .aliases = {},
            .category = "Drawing",
            .summary = "Fill the entire canvas with a solid color.",
            .description = "Replaces every pixel in the active canvas with the specified color.",
            .scope = CommandScope::Canvas,
            .params =
                {
                    {.name = "color",
                     .type = ParamType::Color,
                     .required = true,
                     .description = "Fill color"},
                },
            .syntaxExample = "fill #FF0000",
        };
        return meta;
    }

  protected:
    CommandResult executeCanvas(Canvas& canvas, const CommandArgs& args,
                                EditorContext& /*ctx*/) override {
        auto color = args.getColor("color");

        canvas.document->snapshotForUndo();
        canvas.document->fill(color);

        return {true, "Filled canvas with " + color.toHex()};
    }
};

REGISTER_COMMAND(FillCommand)

}  // namespace vde::tools
