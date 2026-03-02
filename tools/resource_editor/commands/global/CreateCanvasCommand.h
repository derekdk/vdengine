#pragma once

/**
 * @file CreateCanvasCommand.h
 * @brief Command to create a new canvas with given dimensions.
 */

#include <string>

#include "../../CanvasRegistry.h"
#include "../../ImageDocument.h"
#include "../CommandBase.h"
#include "../CommandRegistry.h"
#include "../EditorContext.h"

namespace vde::tools {

/**
 * @brief Creates a new canvas with the specified name and dimensions.
 *
 * Syntax: create canvas <name> <width> <height>
 */
class CreateCanvasCommand final : public GlobalCommand {
  public:
    const CommandMetadata& metadata() const override {
        static const CommandMetadata meta{
            .name = "create canvas",
            .aliases = {"new canvas", "new"},
            .category = "Canvas",
            .summary = "Create a new canvas with the given dimensions.",
            .description = "Creates a blank RGBA canvas of the specified size, registers it, "
                           "and sets it as the active canvas.",
            .scope = CommandScope::Global,
            .params =
                {
                    {.name = "name",
                     .type = ParamType::String,
                     .required = true,
                     .description = "Canvas name"},
                    {.name = "width",
                     .type = ParamType::Int,
                     .required = true,
                     .description = "Width in pixels"},
                    {.name = "height",
                     .type = ParamType::Int,
                     .required = true,
                     .description = "Height in pixels"},
                },
            .syntaxExample = "create canvas mysprite 32 32",
        };
        return meta;
    }

  protected:
    CommandResult executeGlobal(const CommandArgs& args, EditorContext& ctx) override {
        const std::string& name = args.getString("name");
        int w = args.getInt("width");
        int h = args.getInt("height");

        if (w <= 0 || h <= 0) {
            return {false, "Width and height must be positive"};
        }

        if (ctx.canvases->hasName(name)) {
            return {false, "Canvas '" + name + "' already exists"};
        }

        auto doc = ImageDocument::createNew(static_cast<uint32_t>(w), static_cast<uint32_t>(h));
        Canvas* canvas = ctx.canvases->create(name, std::move(doc));
        if (!canvas) {
            return {false, "Failed to create canvas"};
        }

        ctx.commands->setActiveCanvasId(canvas->id);
        return {true, "Created canvas '" + canvas->name + "' (" + std::to_string(w) + "x" +
                          std::to_string(h) + ")"};
    }
};

REGISTER_COMMAND(CreateCanvasCommand)

}  // namespace vde::tools
