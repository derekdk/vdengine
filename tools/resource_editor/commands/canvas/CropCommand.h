#pragma once

/**
 * @file CropCommand.h
 * @brief Command to crop a canvas to a sub-region.
 */

#include "../CommandBase.h"
#include "../CommandRegistry.h"
#include "../EditorContext.h"
#include "../../CanvasRegistry.h"

namespace vde::tools {

/**
 * @brief Crops the canvas to the specified rectangular region.
 *
 * Syntax: crop (x, y) (w, h)
 */
class CropCommand final : public CanvasCommand {
public:
    const CommandMetadata& metadata() const override {
        static const CommandMetadata meta{
            .name = "crop",
            .aliases = {},
            .category = "Edit",
            .summary = "Crop the canvas to a sub-region.",
            .description = "Crops the canvas starting at the given origin with the given size. "
                           "The origin and size must lie within the canvas bounds.",
            .scope = CommandScope::Canvas,
            .params =
                {
                    {.name = "origin",
                     .type = ParamType::Point,
                     .required = true,
                     .description = "Top-left corner (x, y)"},
                    {.name = "size",
                     .type = ParamType::Size,
                     .required = true,
                     .description = "Crop dimensions (w, h)"},
                },
            .syntaxExample = "crop (4, 4) (24, 24)",
        };
        return meta;
    }

protected:
    CommandResult executeCanvas(Canvas& canvas, const CommandArgs& args,
                                EditorContext& /*ctx*/) override {
        auto origin = args.getPoint("origin");
        auto size = args.getSize("size");

        if (size.x <= 0 || size.y <= 0) {
            return {false, "Crop width and height must be positive"};
        }

        uint32_t docW = canvas.document->getWidth();
        uint32_t docH = canvas.document->getHeight();

        if (origin.x < 0 || origin.y < 0 ||
            static_cast<uint32_t>(origin.x + size.x) > docW ||
            static_cast<uint32_t>(origin.y + size.y) > docH) {
            return {false, "Crop region exceeds canvas bounds (" + std::to_string(docW) + "x" +
                               std::to_string(docH) + ")"};
        }

        canvas.document->snapshotForUndo();
        canvas.document->crop(origin.x, origin.y, static_cast<uint32_t>(size.x),
                              static_cast<uint32_t>(size.y));

        return {true, "Cropped to (" + std::to_string(origin.x) + "," +
                           std::to_string(origin.y) + ") " + std::to_string(size.x) + "x" +
                           std::to_string(size.y)};
    }
};

REGISTER_COMMAND(CropCommand)

}  // namespace vde::tools
