#pragma once

/**
 * @file DrawBezierCommand.h
 * @brief Command to draw a cubic Bézier curve on the canvas.
 */

#include "../../CanvasRegistry.h"
#include "../CommandBase.h"
#include "../CommandRegistry.h"
#include "../EditorContext.h"

namespace vde::tools {

/**
 * @brief Draws a cubic Bézier curve through four control points.
 *
 * Syntax: draw bezier (x0,y0) (x1,y1) (x2,y2) (x3,y3) with <color> [width <n>]
 */
class DrawBezierCommand final : public CanvasCommand {
  public:
    const CommandMetadata& metadata() const override {
        static const CommandMetadata meta{
            .name = "draw bezier",
            .aliases = {"bezier"},
            .category = "Drawing",
            .summary = "Draw a cubic Bézier curve.",
            .description = "Draws a cubic Bézier curve defined by four control points. "
                           "Optional width for line thickness.",
            .scope = CommandScope::Canvas,
            .params =
                {
                    {.name = "p0",
                     .type = ParamType::Point,
                     .required = true,
                     .description = "First control point"},
                    {.name = "p1",
                     .type = ParamType::Point,
                     .required = true,
                     .description = "Second control point"},
                    {.name = "p2",
                     .type = ParamType::Point,
                     .required = true,
                     .description = "Third control point"},
                    {.name = "p3",
                     .type = ParamType::Point,
                     .required = true,
                     .description = "Fourth control point"},
                    {.name = "with",
                     .type = ParamType::Keyword,
                     .required = true,
                     .description = "Keyword separator"},
                    {.name = "color",
                     .type = ParamType::Color,
                     .required = true,
                     .description = "Curve color"},
                    {.name = "width",
                     .type = ParamType::Keyword,
                     .required = false,
                     .description = "Keyword for thickness value"},
                    {.name = "thickness",
                     .type = ParamType::Int,
                     .required = false,
                     .description = "Line thickness",
                     .defaultValue = "1"},
                },
            .syntaxExample = "draw bezier (0, 0) (10, 20) (20, 20) (30, 0) with #FF0000",
        };
        return meta;
    }

  protected:
    CommandResult executeCanvas(Canvas& canvas, const CommandArgs& args,
                                EditorContext& /*ctx*/) override {
        auto p0 = args.getPoint("p0");
        auto p1 = args.getPoint("p1");
        auto p2 = args.getPoint("p2");
        auto p3 = args.getPoint("p3");
        auto color = args.getColor("color");
        int thickness = args.has("thickness") ? args.getInt("thickness") : 1;

        std::vector<std::pair<int, int>> points = {
            {p0.x, p0.y},
            {p1.x, p1.y},
            {p2.x, p2.y},
            {p3.x, p3.y},
        };

        canvas.document->snapshotForUndo();
        canvas.document->drawBezier(points, color, thickness);

        return {true, "Drew Bézier curve from (" + std::to_string(p0.x) + "," +
                          std::to_string(p0.y) + ") to (" + std::to_string(p3.x) + "," +
                          std::to_string(p3.y) + ") with " + color.toHex()};
    }
};

REGISTER_COMMAND(DrawBezierCommand)

}  // namespace vde::tools
