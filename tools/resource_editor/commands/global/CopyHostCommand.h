#pragma once

/**
 * @file CopyHostCommand.h
 * @brief Command to copy a resource from one canvas to another.
 */

#include <string>

#include "../../CanvasRegistry.h"
#include "../CommandBase.h"
#include "../CommandRegistry.h"
#include "../EditorContext.h"

namespace vde::tools {

/**
 * @brief Copies a named resource from one canvas to another.
 *
 * Syntax: copyhost image <name> [from <source>] to <dest> [as <newName>]
 */
class CopyHostCommand final : public GlobalCommand {
  public:
    const CommandMetadata& metadata() const override {
        static const CommandMetadata meta{
            .name = "copyhost",
            .aliases = {},
            .category = "Canvas",
            .summary = "Copy a resource from one canvas to another.",
            .description =
                "Creates a deep copy of a named resource from the source canvas and places "
                "it in the destination canvas. If 'from' is omitted, the active canvas is "
                "used as the source. Use 'as' to give the copy a different name.",
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
                    {.name = "as",
                     .type = ParamType::Keyword,
                     .required = false,
                     .description = "Keyword separator"},
                    {.name = "newName",
                     .type = ParamType::String,
                     .required = false,
                     .description = "New name in destination"},
                },
            .syntaxExample = "copyhost image face from hero to body as face_copy",
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

        // Determine copy name
        std::string copyName;
        if (args.has("newName")) {
            copyName = args.getString("newName");
        }

        if (!ctx.canvases->copyResource(name, srcCanvas->id, dstCanvas->id, copyName)) {
            return {false,
                    "Failed to copy resource '" + name + "' from '" + srcCanvas->name + "' to '" +
                        dstCanvas->name + "'"};
        }

        std::string finalName = copyName.empty() ? name : copyName;
        return {true, "Copied '" + name + "' from '" + srcCanvas->name + "' to '" +
                          dstCanvas->name + "' as '" + finalName + "'"};
    }
};

REGISTER_COMMAND(CopyHostCommand)

}  // namespace vde::tools
