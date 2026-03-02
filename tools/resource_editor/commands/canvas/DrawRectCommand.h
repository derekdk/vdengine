#pragma once

/**
 * @file DrawRectCommand.h
 * @brief Command to draw a rectangle on the canvas.
 */

#include <algorithm>

#include "../../CanvasRegistry.h"
#include "../CommandBase.h"
#include "../CommandRegistry.h"
#include "../EditorContext.h"

namespace vde::tools {

/**
 * @brief Draws a rectangle defined by two corner points.
 *
 * Syntax: draw rect (x1,y1) to (x2,y2) with <color> [filled|outline]
 */
class DrawRectCommand final : public CanvasCommand {
  public:
    const CommandMetadata& metadata() const override {
        static const CommandMetadata meta{
            .name = "draw rect",
            .aliases = {"rect"},
            .category = "Drawing",
            .summary = "Draw a rectangle on the canvas.",
            .description = "Draws an axis-aligned rectangle between two corner points. "
                           "Defaults to filled; specify 'outline' for stroke only.",
            .scope = CommandScope::Canvas,
            .params =
                {
                    {.name = "start",
                     .type = ParamType::Point,
                     .required = true,
                     .description = "Top-left corner"},
                    {.name = "to",
                     .type = ParamType::Keyword,
                     .required = true,
                     .description = "Keyword separator"},
                    {.name = "end",
                     .type = ParamType::Point,
                     .required = true,
                     .description = "Bottom-right corner"},
                    {.name = "with",
                     .type = ParamType::Keyword,
                     .required = true,
                     .description = "Keyword separator"},
                    {.name = "color",
                     .type = ParamType::Color,
                     .required = true,
                     .description = "Rect color"},
                    {.name = "mode",
                     .type = ParamType::Enum,
                     .required = false,
                     .description = "Fill mode",
                     .defaultValue = "filled",
                     .enumValues = {"filled", "outline"}},
                },
            .syntaxExample = "draw rect (0,0) to (15,15) with #00FF00 filled",
        };
        return meta;
    }

  protected:
    CommandResult executeCanvas(Canvas& canvas, const CommandArgs& args,
                                EditorContext& /*ctx*/) override {
        auto start = args.getPoint("start");
        auto end = args.getPoint("end");
        auto color = args.getColor("color");
        bool isFilled = !args.has("mode") || args.getString("mode") != "outline";

        int x = std::min(start.x, end.x);
        int y = std::min(start.y, end.y);
        int w = std::abs(end.x - start.x) + 1;
        int h = std::abs(end.y - start.y) + 1;

        canvas.document->snapshotForUndo();
        canvas.document->drawRect(x, y, w, h, color, isFilled);

        return {true, "Drew " + std::string(isFilled ? "filled" : "outline") + " rect at (" +
                          std::to_string(x) + "," + std::to_string(y) + ") " + std::to_string(w) +
                          "x" + std::to_string(h) + " with " + color.toHex()};
    }
};

REGISTER_COMMAND(DrawRectCommand)

}  // namespace vde::tools
