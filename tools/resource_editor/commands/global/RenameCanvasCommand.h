#pragma once

/**
 * @file RenameCanvasCommand.h
 * @brief Command to rename an existing canvas.
 */

#include "../CommandBase.h"
#include "../CommandRegistry.h"
#include "../EditorContext.h"
#include "../../CanvasRegistry.h"
#include "../../ImageDocument.h"

#include <string>

namespace vde::tools {

/**
 * @brief Renames a canvas from an old name to a new name.
 *
 * Syntax: rename <oldname> <newname>
 */
class RenameCanvasCommand final : public GlobalCommand {
public:
    const CommandMetadata& metadata() const override {
        static const CommandMetadata meta{
            .name = "rename",
            .aliases = {},
            .category = "Canvas",
            .summary = "Rename a canvas.",
            .description = "Changes the name of an existing canvas. The new name must not "
                           "already be in use.",
            .scope = CommandScope::Global,
            .params =
                {
                    {.name = "oldname",
                     .type = ParamType::String,
                     .required = true,
                     .description = "Current canvas name"},
                    {.name = "newname",
                     .type = ParamType::String,
                     .required = true,
                     .description = "New canvas name"},
                },
            .syntaxExample = "rename mysprite hero",
        };
        return meta;
    }

protected:
    CommandResult executeGlobal(const CommandArgs& args, EditorContext& ctx) override {
        const std::string& oldName = args.getString("oldname");
        const std::string& newName = args.getString("newname");

        Canvas* canvas = ctx.canvases->resolve(oldName);
        if (!canvas) {
            return {false, "Canvas '" + oldName + "' not found"};
        }

        if (ctx.canvases->hasName(newName)) {
            return {false, "A canvas named '" + newName + "' already exists"};
        }

        if (!ctx.canvases->rename(canvas->id, newName)) {
            return {false, "Failed to rename canvas"};
        }

        return {true, "Renamed canvas '" + oldName + "' to '" + newName + "'"};
    }
};

REGISTER_COMMAND(RenameCanvasCommand)

}  // namespace vde::tools
