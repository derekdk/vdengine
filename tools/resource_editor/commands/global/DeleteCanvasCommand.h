#pragma once

/**
 * @file DeleteCanvasCommand.h
 * @brief Command to delete (close) a canvas by name or ID.
 */

#include "../CommandBase.h"
#include "../CommandRegistry.h"
#include "../EditorContext.h"
#include "../../CanvasRegistry.h"
#include "../../ImageDocument.h"

#include <string>

namespace vde::tools {

/**
 * @brief Deletes a canvas from the registry.
 *
 * Syntax: delete <name>
 *
 * If the deleted canvas was the active canvas, the active canvas is
 * reassigned to the next available canvas (or cleared if none remain).
 */
class DeleteCanvasCommand final : public GlobalCommand {
public:
    const CommandMetadata& metadata() const override {
        static const CommandMetadata meta{
            .name = "delete",
            .aliases = {"close", "remove"},
            .category = "Canvas",
            .summary = "Delete (close) a canvas.",
            .description = "Removes the specified canvas from the editor. If it was the active "
                           "canvas, another canvas is selected automatically.",
            .scope = CommandScope::Global,
            .params =
                {
                    {.name = "name",
                     .type = ParamType::String,
                     .required = true,
                     .description = "Canvas name or ID to delete"},
                },
            .syntaxExample = "delete mysprite",
        };
        return meta;
    }

protected:
    CommandResult executeGlobal(const CommandArgs& args, EditorContext& ctx) override {
        const std::string& nameOrId = args.getString("name");

        Canvas* canvas = ctx.canvases->resolve(nameOrId);
        if (!canvas) {
            return {false, "Canvas '" + nameOrId + "' not found"};
        }

        uint32_t removedId = canvas->id;
        std::string removedName = canvas->name;
        bool wasActive = (removedId == ctx.commands->getActiveCanvasId());

        if (!ctx.canvases->remove(removedId)) {
            return {false, "Failed to remove canvas '" + removedName + "'"};
        }

        // Reassign active canvas if the deleted one was active.
        if (wasActive) {
            auto ids = ctx.canvases->getIds();
            if (!ids.empty()) {
                ctx.commands->setActiveCanvasId(ids.front());
            } else {
                ctx.commands->setActiveCanvasId(0);
            }
        }

        return {true, "Deleted canvas '" + removedName + "'"};
    }
};

REGISTER_COMMAND(DeleteCanvasCommand)

}  // namespace vde::tools
