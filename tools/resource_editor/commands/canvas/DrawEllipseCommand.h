#pragma once

/**
 * @file DrawEllipseCommand.h
 * @brief Command to draw an ellipse on the canvas.
 */

#include "../CommandBase.h"
#include "../CommandRegistry.h"
#include "../EditorContext.h"
#include "../../CanvasRegistry.h"

namespace vde::tools {

/**
 * @brief Draws an ellipse with a given center and radii.
 *
 * Syntax: draw ellipse (cx,cy) (rx,ry) with <color> [filled|outline]
 */
class DrawEllipseCommand final : public CanvasCommand {
public:
    const CommandMetadata& metadata() const override {
        static const CommandMetadata meta{
            .name = "draw ellipse",
            .aliases = {"ellipse"},
            .category = "Drawing",
            .summary = "Draw an ellipse on the canvas.",
            .description = "Draws an ellipse with the specified center and horizontal/vertical "
                           "radii. Defaults to filled; specify 'outline' for stroke only.",
            .scope = CommandScope::Canvas,
            .params =
                {
                    {.name = "center",
                     .type = ParamType::Point,
                     .required = true,
                     .description = "Center point"},
                    {.name = "radii",
                     .type = ParamType::Size,
                     .required = true,
                     .description = "Horizontal and vertical radii"},
                    {.name = "with",
                     .type = ParamType::Keyword,
                     .required = true,
                     .description = "Keyword separator"},
                    {.name = "color",
                     .type = ParamType::Color,
                     .required = true,
                     .description = "Ellipse color"},
                    {.name = "mode",
                     .type = ParamType::Enum,
                     .required = false,
                     .description = "Fill mode",
                     .defaultValue = "filled",
                     .enumValues = {"filled", "outline"}},
                },
            .syntaxExample = "draw ellipse (16,16) (10,5) with #FF00FF filled",
        };
        return meta;
    }

protected:
    CommandResult executeCanvas(Canvas& canvas, const CommandArgs& args,
                                EditorContext& /*ctx*/) override {
        auto center = args.getPoint("center");
        auto radii = args.getSize("radii");
        auto color = args.getColor("color");
        bool isFilled = !args.has("mode") || args.getString("mode") != "outline";

        canvas.document->snapshotForUndo();
        canvas.document->drawEllipse(center.x, center.y, radii.x, radii.y, color, isFilled);

        return {true, "Drew " + std::string(isFilled ? "filled" : "outline") + " ellipse at (" +
                           std::to_string(center.x) + "," + std::to_string(center.y) + ") rx=" +
                           std::to_string(radii.x) + " ry=" + std::to_string(radii.y) + " with " +
                           color.toHex()};
    }
};

REGISTER_COMMAND(DrawEllipseCommand)

}  // namespace vde::tools
