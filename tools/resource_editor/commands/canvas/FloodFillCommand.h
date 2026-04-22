#pragma once

/**
 * @file FloodFillCommand.h
 * @brief Command to flood-fill a region of the canvas from a seed point.
 */

#include "../../CanvasRegistry.h"
#include "../CommandBase.h"
#include "../CommandRegistry.h"
#include "../EditorContext.h"

namespace vde::tools {

/**
 * @brief Flood-fills a contiguous region starting from a seed point.
 *
 * Syntax: floodfill (x,y) with <color>
 */
class FloodFillCommand final : public CanvasCommand {
  public:
    const CommandMetadata& metadata() const override {
        static const CommandMetadata meta{
            .name = "floodfill",
            .aliases = {"flood"},
            .category = "Drawing",
            .summary = "Flood-fill a region starting from a point.",
            .description = "Performs a flood fill from the given seed point, replacing all "
                           "contiguous pixels of the same color with the specified color.",
            .scope = CommandScope::Canvas,
            .params =
                {
                    {.name = "point",
                     .type = ParamType::Point,
                     .required = true,
                     .description = "Start point",
                     .defaultValue = "",
                     .enumValues = {}},
                    {.name = "with",
                     .type = ParamType::Keyword,
                     .required = true,
                     .description = "Keyword separator",
                     .defaultValue = "",
                     .enumValues = {}},
                    {.name = "color",
                     .type = ParamType::Color,
                     .required = true,
                     .description = "Fill color",
                     .defaultValue = "",
                     .enumValues = {}},
                },
            .syntaxExample = "floodfill (10,10) with #00FF00",
        };
        return meta;
    }

  protected:
    CommandResult executeCanvas(Canvas& canvas, const CommandArgs& args,
                                EditorContext& /*ctx*/) override {
        auto point = args.getPoint("point");
        auto color = args.getColor("color");

        canvas.document->snapshotForUndo();
        canvas.document->floodFill(point.x, point.y, color);

        return {true, "Flood-filled from (" + std::to_string(point.x) + "," +
                          std::to_string(point.y) + ") with " + color.toHex()};
    }
};

REGISTER_COMMAND(FloodFillCommand)

}  // namespace vde::tools
