#pragma once

/**
 * @file DrawCircleCommand.h
 * @brief Command to draw a circle on the canvas.
 */

#include "../../CanvasRegistry.h"
#include "../CommandBase.h"
#include "../CommandRegistry.h"
#include "../EditorContext.h"

namespace vde::tools {

/**
 * @brief Draws a circle with a given center and radius.
 *
 * Syntax: draw circle (cx,cy) radius <r> with <color> [filled|outline]
 */
class DrawCircleCommand final : public CanvasCommand {
  public:
    const CommandMetadata& metadata() const override {
        static const CommandMetadata meta{
            .name = "draw circle",
            .aliases = {"circle"},
            .category = "Drawing",
            .summary = "Draw a circle on the canvas.",
            .description = "Draws a circle with the specified center and radius. "
                           "Defaults to filled; specify 'outline' for stroke only.",
            .scope = CommandScope::Canvas,
            .params =
                {
                    {.name = "center",
                     .type = ParamType::Point,
                     .required = true,
                     .description = "Center point",
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
                     .description = "Radius in pixels",
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
                     .description = "Circle color",
                     .defaultValue = "",
                     .enumValues = {}},
                    {.name = "mode",
                     .type = ParamType::Enum,
                     .required = false,
                     .description = "Fill mode",
                     .defaultValue = "filled",
                     .enumValues = {"filled", "outline"}},
                },
            .syntaxExample = "draw circle (16,16) radius 10 with #0000FF filled",
        };
        return meta;
    }

  protected:
    CommandResult executeCanvas(Canvas& canvas, const CommandArgs& args,
                                EditorContext& /*ctx*/) override {
        auto center = args.getPoint("center");
        int r = args.getInt("r");
        auto color = args.getColor("color");
        bool isFilled = !args.has("mode") || args.getString("mode") != "outline";

        canvas.document->snapshotForUndo();
        canvas.document->drawCircle(center.x, center.y, r, color, isFilled);

        return {true, "Drew " + std::string(isFilled ? "filled" : "outline") + " circle at (" +
                          std::to_string(center.x) + "," + std::to_string(center.y) +
                          ") r=" + std::to_string(r) + " with " + color.toHex()};
    }
};

REGISTER_COMMAND(DrawCircleCommand)

}  // namespace vde::tools
