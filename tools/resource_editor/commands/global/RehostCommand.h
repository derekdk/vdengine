#pragma once

/**
 * @file RehostCommand.h
 * @brief Command to transfer a resource from one canvas to another.
 */

#include <string>

#include "../../CanvasRegistry.h"
#include "../CommandBase.h"
#include "../CommandRegistry.h"
#include "../EditorContext.h"

namespace vde::tools {

/**
 * @brief Transfers (moves) a named resource from one canvas to another.
 *
 * Syntax: rehost image <name> [from <source>] to <dest>
 */
class RehostCommand final : public GlobalCommand {
  public:
    const CommandMetadata& metadata() const override {
        static const CommandMetadata meta{
            .name = "rehost",
            .aliases = {},
            .category = "Canvas",
            .summary = "Transfer a resource from one canvas to another.",
            .description =
                "Moves a named resource from the source canvas to the destination canvas. "
                "If 'from' is omitted, the active canvas is used as the source.",
            .scope = CommandScope::Global,
            .params =
                {
                    {.name = "type",
                     .type = ParamType::Enum,
                     .required = true,
                     .description = "Resource type",
                     .enumValues = {"image"}},
                    {.name = "name",
                     .type = ParamType::String,
                     .required = true,
                     .description = "Resource name"},
                    {.name = "from",
                     .type = ParamType::Keyword,
                     .required = false,
                     .description = "Keyword separator"},
                    {.name = "source",
                     .type = ParamType::String,
                     .required = false,
                     .description = "Source canvas name (default: active)"},
                    {.name = "to",
                     .type = ParamType::Keyword,
                     .required = true,
                     .description = "Keyword separator"},
                    {.name = "dest",
                     .type = ParamType::String,
                     .required = true,
                     .description = "Destination canvas name"},
                },
            .syntaxExample = "rehost image face from hero to body",
        };
        return meta;
    }

  protected:
    CommandResult executeGlobal(const CommandArgs& args, EditorContext& ctx) override {
        const std::string& name = args.getString("name");
        const std::string& destName = args.getString("dest");

        // Resolve source canvas
        Canvas* srcCanvas = nullptr;
        if (args.has("source")) {
            srcCanvas = ctx.canvases->resolve(args.getString("source"));
            if (!srcCanvas) {
                return {false, "Source canvas '" + args.getString("source") + "' not found"};
            }
        } else {
            srcCanvas = ctx.getActiveCanvas();
            if (!srcCanvas) {
                return {false, "No active canvas and no source specified"};
            }
        }

        // Resolve destination canvas
        Canvas* dstCanvas = ctx.canvases->resolve(destName);
        if (!dstCanvas) {
            return {false, "Destination canvas '" + destName + "' not found"};
        }

        if (!ctx.canvases->transferResource(name, srcCanvas->id, dstCanvas->id)) {
            return {false,
                    "Failed to transfer resource '" + name + "' from '" + srcCanvas->name +
                        "' to '" + dstCanvas->name + "'"};
        }

        return {true, "Transferred '" + name + "' from '" + srcCanvas->name + "' to '" +
                          dstCanvas->name + "'"};
    }
};

REGISTER_COMMAND(RehostCommand)

}  // namespace vde::tools
