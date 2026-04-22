#pragma once

/**
 * @file DrawArcCommand.h
 * @brief Command to draw a circular arc on the canvas.
 */

#include "../../CanvasRegistry.h"
#include "../CommandBase.h"
#include "../CommandRegistry.h"
#include "../EditorContext.h"

namespace vde::tools {

/**
 * @brief Draws a circular arc between two angles.
 *
 * Syntax: draw arc (cx,cy) radius <r> from <start> to <end> with <color> [width <n>]
 */
class DrawArcCommand final : public CanvasCommand {
  public:
    const CommandMetadata& metadata() const override {
        static const CommandMetadata meta{
            .name = "draw arc",
            .aliases = {"arc"},
            .category = "Drawing",
            .summary = "Draw a circular arc.",
            .description = "Draws a circular arc centered at the given point with the specified "
                           "radius, sweeping from startAngle to endAngle (degrees, 0 = right, "
                           "counter-clockwise). Optional width for line thickness.",
            .scope = CommandScope::Canvas,
            .params =
                {
                    {.name = "center",
                     .type = ParamType::Point,
                     .required = true,
                     .description = "Center point (cx, cy)",
                     .defaultValue = "",
                     .enumValues = {}},
                    {.name = "radius",
                     .type = ParamType::Keyword,
                     .required = true,
                     .description = "Keyword for radius value",
                     .defaultValue = "",
                     .enumValues = {}},
                    {.name = "r",
                     .type = ParamType::Int,
                     .required = true,
                     .description = "Arc radius",
                     .defaultValue = "",
                     .enumValues = {}},
                    {.name = "from",
                     .type = ParamType::Keyword,
                     .required = true,
                     .description = "Keyword for start angle",
                     .defaultValue = "",
                     .enumValues = {}},
                    {.name = "startAngle",
                     .type = ParamType::Int,
                     .required = true,
                     .description = "Start angle in degrees",
                     .defaultValue = "",
                     .enumValues = {}},
                    {.name = "to",
                     .type = ParamType::Keyword,
                     .required = true,
                     .description = "Keyword for end angle",
                     .defaultValue = "",
                     .enumValues = {}},
                    {.name = "endAngle",
                     .type = ParamType::Int,
                     .required = true,
                     .description = "End angle in degrees",
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
                     .description = "Arc color",
                     .defaultValue = "",
                     .enumValues = {}},
                    {.name = "width",
                     .type = ParamType::Keyword,
                     .required = false,
                     .description = "Keyword for thickness value",
                     .defaultValue = "",
                     .enumValues = {}},
                    {.name = "thickness",
                     .type = ParamType::Int,
                     .required = false,
                     .description = "Line thickness",
                     .defaultValue = "1",
                     .enumValues = {}},
                },
            .syntaxExample = "draw arc (16, 16) radius 8 from 0 to 180 with #FF0000",
        };
        return meta;
    }

  protected:
    CommandResult executeCanvas(Canvas& canvas, const CommandArgs& args,
                                EditorContext& /*ctx*/) override {
        auto center = args.getPoint("center");
        int r = args.getInt("r");
        int startAngle = args.getInt("startAngle");
        int endAngle = args.getInt("endAngle");
        auto color = args.getColor("color");
        int thickness = args.has("thickness") ? args.getInt("thickness") : 1;

        canvas.document->snapshotForUndo();
        canvas.document->drawArc(center.x, center.y, r, static_cast<float>(startAngle),
                                 static_cast<float>(endAngle), color, thickness);

        return {true, "Drew arc at (" + std::to_string(center.x) + "," + std::to_string(center.y) +
                          ") r=" + std::to_string(r) + " from " + std::to_string(startAngle) +
                          " to " + std::to_string(endAngle) + " with " + color.toHex()};
    }
};

REGISTER_COMMAND(DrawArcCommand)

}  // namespace vde::tools
