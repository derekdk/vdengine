#pragma once

/**
 * @file DrawImageCommand.h
 * @brief Command to draw a named image resource onto the canvas.
 */

#include "../../CanvasRegistry.h"
#include "../CommandBase.h"
#include "../CommandRegistry.h"
#include "../EditorContext.h"

namespace vde::tools {

/**
 * @brief Draws a named image resource onto the canvas at a given position.
 *
 * Supports optional scaling via a target size parameter. Uses nearest-neighbor
 * sampling when the target size differs from the source dimensions.
 *
 * Syntax: draw image <name> (x, y) [(w, h)]
 */
class DrawImageCommand final : public CanvasCommand {
  public:
    const CommandMetadata& metadata() const override {
        static const CommandMetadata meta{
            .name = "draw image",
            .aliases = {},
            .category = "Drawing",
            .summary = "Draw a named image resource onto the canvas.",
            .description =
                "Blits a named image resource onto the active canvas at the specified position. "
                "The resource name may use canvas::name syntax to reference images on other "
                "canvases. If a target size is provided, nearest-neighbor scaling is applied.",
            .scope = CommandScope::Canvas,
            .params =
                {
                    {.name = "name",
                     .type = ParamType::String,
                     .required = true,
                     .description = "Image resource name (may use canvas::name syntax)"},
                    {.name = "position",
                     .type = ParamType::Point,
                     .required = true,
                     .description = "Top-left position (x, y)"},
                    {.name = "size",
                     .type = ParamType::Size,
                     .required = false,
                     .description = "Target size (w, h). Omit to use original dimensions."},
                },
            .syntaxExample = "draw image hero::face (0, 0) (32, 32)",
        };
        return meta;
    }

  protected:
    CommandResult executeCanvas(Canvas& canvas, const CommandArgs& args,
                                EditorContext& ctx) override {
        const std::string& name = args.getString("name");
        auto pos = args.getPoint("position");

        uint32_t activeId = ctx.commands->getActiveCanvasId();
        auto ref = ctx.canvases->resolveResource(name, activeId);
        if (!ref.image) {
            return {false, "Image resource '" + name + "' not found"};
        }

        const ImageDocument* srcImg = ref.image;
        uint32_t srcW = srcImg->getWidth();
        uint32_t srcH = srcImg->getHeight();

        uint32_t dstW = srcW;
        uint32_t dstH = srcH;
        if (args.has("size")) {
            auto sz = args.getSize("size");
            dstW = static_cast<uint32_t>(sz.x);
            dstH = static_cast<uint32_t>(sz.y);
        }

        canvas.document->snapshotForUndo();

        uint32_t canvasW = canvas.document->getWidth();
        uint32_t canvasH = canvas.document->getHeight();

        for (uint32_t dy = 0; dy < dstH; ++dy) {
            for (uint32_t dx = 0; dx < dstW; ++dx) {
                int targetX = pos.x + static_cast<int>(dx);
                int targetY = pos.y + static_cast<int>(dy);
                if (targetX < 0 || targetY < 0 || static_cast<uint32_t>(targetX) >= canvasW ||
                    static_cast<uint32_t>(targetY) >= canvasH) {
                    continue;
                }

                // Nearest-neighbor sampling from source
                uint32_t sx = dx * srcW / dstW;
                uint32_t sy = dy * srcH / dstH;
                RGBAColor pixel = srcImg->getPixel(sx, sy);

                canvas.document->setPixel(static_cast<uint32_t>(targetX),
                                          static_cast<uint32_t>(targetY), pixel);
            }
        }

        return {true, "Drew image '" + name + "' at (" + std::to_string(pos.x) + ", " +
                          std::to_string(pos.y) + ") size " + std::to_string(dstW) + "x" +
                          std::to_string(dstH)};
    }
};

REGISTER_COMMAND(DrawImageCommand)

}  // namespace vde::tools
