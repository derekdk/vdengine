#pragma once

/**
 * @file RedoCommand.h
 * @brief Command to redo a previously undone edit operation on a canvas.
 */

#include "../../CanvasRegistry.h"
#include "../CommandBase.h"
#include "../CommandRegistry.h"
#include "../EditorContext.h"

namespace vde::tools {

/**
 * @brief Redoes a previously undone edit operation on the active canvas.
 *
 * Syntax: redo
 */
class RedoCommand final : public CanvasCommand {
  public:
    const CommandMetadata& metadata() const override {
        static const CommandMetadata meta{
            .name = "redo",
            .aliases = {},
            .category = "Edit",
            .summary = "Redo the last undone operation.",
            .description = "Re-applies the most recently undone edit on the canvas.",
            .scope = CommandScope::Canvas,
            .params = {},
            .syntaxExample = "redo",
        };
        return meta;
    }

  protected:
    CommandResult executeCanvas(Canvas& canvas, const CommandArgs& /*args*/,
                                EditorContext& /*ctx*/) override {
        if (canvas.document->getRedoCount() == 0) {
            return {false, "Nothing to redo"};
        }

        canvas.document->redo();
        return {true, "Redone (" + std::to_string(canvas.document->getRedoCount()) + " remaining)"};
    }
};

REGISTER_COMMAND(RedoCommand)

}  // namespace vde::tools
