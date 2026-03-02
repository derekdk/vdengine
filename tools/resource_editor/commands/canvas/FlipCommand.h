#pragma once

/**
 * @file FlipCommand.h
 * @brief Command to flip a canvas horizontally or vertically.
 */

#include "../../CanvasRegistry.h"
#include "../CommandBase.h"
#include "../CommandRegistry.h"
#include "../EditorContext.h"

namespace vde::tools {

/**
 * @brief Flips the canvas along the specified axis.
 *
 * Syntax: flip horizontal|vertical
 */
class FlipCommand final : public CanvasCommand {
  public:
    const CommandMetadata& metadata() const override {
        static const CommandMetadata meta{
            .name = "flip",
            .aliases = {"mirror"},
            .category = "Edit",
            .summary = "Flip the canvas horizontally or vertically.",
            .description = "Mirrors the canvas pixels along the horizontal or vertical axis.",
            .scope = CommandScope::Canvas,
            .params =
                {
                    {.name = "direction",
                     .type = ParamType::Enum,
                     .required = true,
                     .description = "Flip direction",
                     .enumValues = {"horizontal", "vertical"}},
                },
            .syntaxExample = "flip horizontal",
        };
        return meta;
    }

  protected:
    CommandResult executeCanvas(Canvas& canvas, const CommandArgs& args,
                                EditorContext& /*ctx*/) override {
        const std::string& direction = args.getString("direction");

        canvas.document->snapshotForUndo();

        if (direction == "horizontal") {
            canvas.document->flipHorizontal();
        } else {
            canvas.document->flipVertical();
        }

        return {true, "Flipped " + direction};
    }
};

REGISTER_COMMAND(FlipCommand)

}  // namespace vde::tools
