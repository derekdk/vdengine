#pragma once

/**
 * @file ResizeCommand.h
 * @brief Command to resize a canvas using nearest-neighbor sampling.
 */

#include "../CommandBase.h"
#include "../CommandRegistry.h"
#include "../EditorContext.h"
#include "../../CanvasRegistry.h"

namespace vde::tools {

/**
 * @brief Resizes the canvas to the specified dimensions.
 *
 * Syntax: resize <width> <height>
 */
class ResizeCommand final : public CanvasCommand {
public:
    const CommandMetadata& metadata() const override {
        static const CommandMetadata meta{
            .name = "resize",
            .aliases = {},
            .category = "Edit",
            .summary = "Resize the canvas.",
            .description = "Resizes the canvas to the given width and height using "
                           "nearest-neighbor sampling.",
            .scope = CommandScope::Canvas,
            .params =
                {
                    {.name = "width",
                     .type = ParamType::Int,
                     .required = true,
                     .description = "New width in pixels"},
                    {.name = "height",
                     .type = ParamType::Int,
                     .required = true,
                     .description = "New height in pixels"},
                },
            .syntaxExample = "resize 64 64",
        };
        return meta;
    }

protected:
    CommandResult executeCanvas(Canvas& canvas, const CommandArgs& args,
                                EditorContext& /*ctx*/) override {
        int w = args.getInt("width");
        int h = args.getInt("height");

        if (w <= 0 || h <= 0) {
            return {false, "Width and height must be positive"};
        }

        canvas.document->snapshotForUndo();
        canvas.document->resize(static_cast<uint32_t>(w), static_cast<uint32_t>(h));

        return {true, "Resized to " + std::to_string(w) + "x" + std::to_string(h)};
    }
};

REGISTER_COMMAND(ResizeCommand)

}  // namespace vde::tools
