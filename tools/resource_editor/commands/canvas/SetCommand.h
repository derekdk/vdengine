#pragma once

/**
 * @file SetCommand.h
 * @brief Command to set a single pixel on the canvas.
 */

#include "../CommandBase.h"
#include "../CommandRegistry.h"
#include "../EditorContext.h"
#include "../../CanvasRegistry.h"

namespace vde::tools {

/**
 * @brief Sets a single pixel at the given coordinate.
 *
 * Syntax: set (x, y) <color>
 */
class SetCommand final : public CanvasCommand {
public:
    const CommandMetadata& metadata() const override {
        static const CommandMetadata meta{
            .name = "set",
            .aliases = {},
            .category = "Drawing",
            .summary = "Set a single pixel on the canvas.",
            .description = "Sets the pixel at the given (x, y) coordinate to the specified color.",
            .scope = CommandScope::Canvas,
            .params =
                {
                    {.name = "point",
                     .type = ParamType::Point,
                     .required = true,
                     .description = "Pixel coordinate"},
                    {.name = "color",
                     .type = ParamType::Color,
                     .required = true,
                     .description = "Pixel color"},
                },
            .syntaxExample = "set (10,20) #FF0000",
        };
        return meta;
    }

protected:
    CommandResult executeCanvas(Canvas& canvas, const CommandArgs& args,
                                EditorContext& /*ctx*/) override {
        auto point = args.getPoint("point");
        auto color = args.getColor("color");

        canvas.document->snapshotForUndo();
        canvas.document->setPixel(static_cast<uint32_t>(point.x),
                                  static_cast<uint32_t>(point.y), color);

        return {true, "Set pixel (" + std::to_string(point.x) + "," +
                           std::to_string(point.y) + ") to " + color.toHex()};
    }
};

REGISTER_COMMAND(SetCommand)

}  // namespace vde::tools
