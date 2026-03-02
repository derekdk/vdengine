#pragma once

/**
 * @file UndoCommand.h
 * @brief Command to undo the last edit operation on a canvas.
 */

#include "../CommandBase.h"
#include "../CommandRegistry.h"
#include "../EditorContext.h"
#include "../../CanvasRegistry.h"

namespace vde::tools {

/**
 * @brief Undoes the last edit operation on the active canvas.
 *
 * Syntax: undo
 */
class UndoCommand final : public CanvasCommand {
public:
    const CommandMetadata& metadata() const override {
        static const CommandMetadata meta{
            .name = "undo",
            .aliases = {},
            .category = "Edit",
            .summary = "Undo the last edit operation.",
            .description = "Restores the canvas to its state before the most recent edit.",
            .scope = CommandScope::Canvas,
            .params = {},
            .syntaxExample = "undo",
        };
        return meta;
    }

protected:
    CommandResult executeCanvas(Canvas& canvas, const CommandArgs& /*args*/,
                                EditorContext& /*ctx*/) override {
        if (canvas.document->getUndoCount() == 0) {
            return {false, "Nothing to undo"};
        }

        canvas.document->undo();
        return {true, "Undone (" + std::to_string(canvas.document->getUndoCount()) +
                           " remaining)"};
    }
};

REGISTER_COMMAND(UndoCommand)

}  // namespace vde::tools
