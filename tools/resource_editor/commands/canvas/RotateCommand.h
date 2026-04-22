#pragma once

/**
 * @file RotateCommand.h
 * @brief Command to rotate a canvas by 90, 180, or 270 degrees clockwise.
 */

#include "../../CanvasRegistry.h"
#include "../../ImageDocument.h"
#include "../CommandBase.h"
#include "../CommandRegistry.h"
#include "../EditorContext.h"

namespace vde::tools {

/**
 * @brief Rotates the canvas by the specified angle (90, 180, or 270 degrees clockwise).
 *
 * Syntax: rotate 90|180|270
 */
class RotateCommand final : public CanvasCommand {
  public:
    const CommandMetadata& metadata() const override {
        static const CommandMetadata meta{
            .name = "rotate",
            .aliases = {},
            .category = "Edit",
            .summary = "Rotate the canvas by 90, 180, or 270 degrees.",
            .description = "Rotates the canvas clockwise by the given angle. "
                           "90/270 swap width and height; 180 flips both axes.",
            .scope = CommandScope::Canvas,
            .params =
                {
                    {.name = "angle",
                     .type = ParamType::Enum,
                     .required = true,
                     .description = "Rotation angle in degrees (clockwise)",
                     .defaultValue = "",
                     .enumValues = {"90", "180", "270"}},
                },
            .syntaxExample = "rotate 90",
        };
        return meta;
    }

  protected:
    CommandResult executeCanvas(Canvas& canvas, const CommandArgs& args,
                                EditorContext& /*ctx*/) override {
        const std::string& angle = args.getString("angle");

        canvas.document->snapshotForUndo();

        if (angle == "180") {
            canvas.document->flipHorizontal();
            canvas.document->flipVertical();
        } else {
            // For 90/270, build rotated pixels and replace the document's pixel
            // data in-place so the undo stack (which already has the pre-snapshot)
            // stays valid.
            uint32_t oldW = canvas.document->getWidth();
            uint32_t oldH = canvas.document->getHeight();
            std::vector<uint8_t> rotated(static_cast<size_t>(oldH) * oldW * 4);

            for (uint32_t y = 0; y < oldH; ++y) {
                for (uint32_t x = 0; x < oldW; ++x) {
                    RGBAColor pixel = canvas.document->getPixel(x, y);
                    size_t dstIdx = 0;
                    if (angle == "90") {
                        // 90 CW: dst(oldH-1-y, x) in image of size (oldH x oldW)
                        dstIdx = (static_cast<size_t>(x) * oldH + (oldH - 1 - y)) * 4;
                    } else {
                        // 270 CW: dst(y, oldW-1-x) in image of size (oldH x oldW)
                        dstIdx = (static_cast<size_t>(oldW - 1 - x) * oldH + y) * 4;
                    }
                    rotated[dstIdx + 0] = pixel.r;
                    rotated[dstIdx + 1] = pixel.g;
                    rotated[dstIdx + 2] = pixel.b;
                    rotated[dstIdx + 3] = pixel.a;
                }
            }

            // Replace the document's internal pixel buffer in-place.
            // This preserves the undo stack and file path.
            canvas.document->replacePixels(std::move(rotated), oldH, oldW);
        }

        return {true, "Rotated " + angle + " degrees"};
    }
};

REGISTER_COMMAND(RotateCommand)

}  // namespace vde::tools
