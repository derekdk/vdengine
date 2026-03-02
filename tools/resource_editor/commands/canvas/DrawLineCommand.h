#pragma once

/**
 * @file DrawLineCommand.h
 * @brief Command to draw a line between two points on the canvas.
 */

#include "../CommandBase.h"
#include "../CommandRegistry.h"
#include "../EditorContext.h"
#include "../../CanvasRegistry.h"

namespace vde::tools {

/**
 * @brief Draws a line from one point to another.
 *
 * Syntax: draw line (x1,y1) to (x2,y2) with <color> [width <n>]
 */
class DrawLineCommand final : public CanvasCommand {
public:
    const CommandMetadata& metadata() const override {
        static const CommandMetadata meta{
            .name = "draw line",
            .aliases = {"line"},
            .category = "Drawing",
            .summary = "Draw a line between two points.",
            .description = "Draws a straight line from the start point to the end point with the "
                           "given color and optional thickness.",
            .scope = CommandScope::Canvas,
            .params =
                {
                    {.name = "start",
                     .type = ParamType::Point,
                     .required = true,
                     .description = "Start point"},
                    {.name = "to",
                     .type = ParamType::Keyword,
                     .required = true,
                     .description = "Keyword separator"},
                    {.name = "end",
                     .type = ParamType::Point,
                     .required = true,
                     .description = "End point"},
                    {.name = "with",
                     .type = ParamType::Keyword,
                     .required = true,
                     .description = "Keyword separator"},
                    {.name = "color",
                     .type = ParamType::Color,
                     .required = true,
                     .description = "Line color"},
                    {.name = "width",
                     .type = ParamType::Keyword,
                     .required = false,
                     .description = "Keyword for thickness"},
                    {.name = "thickness",
                     .type = ParamType::Int,
                     .required = false,
                     .description = "Line thickness",
                     .defaultValue = "1"},
                },
            .syntaxExample = "draw line (0,0) to (31,31) with #FFFFFF width 2",
        };
        return meta;
    }

protected:
    CommandResult executeCanvas(Canvas& canvas, const CommandArgs& args,
                                EditorContext& /*ctx*/) override {
        auto start = args.getPoint("start");
        auto end = args.getPoint("end");
        auto color = args.getColor("color");
        int thickness = args.has("thickness") ? args.getInt("thickness") : 1;

        canvas.document->snapshotForUndo();
        canvas.document->drawLine(start.x, start.y, end.x, end.y, color, thickness);

        return {true, "Drew line (" + std::to_string(start.x) + "," +
                           std::to_string(start.y) + ") to (" + std::to_string(end.x) + "," +
                           std::to_string(end.y) + ") with " + color.toHex()};
    }
};

REGISTER_COMMAND(DrawLineCommand)

}  // namespace vde::tools
