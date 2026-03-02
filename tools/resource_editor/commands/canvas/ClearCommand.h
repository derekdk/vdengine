#pragma once

/**
 * @file ClearCommand.h
 * @brief Command to clear a canvas to transparent black.
 */

#include "../CommandBase.h"
#include "../CommandRegistry.h"
#include "../EditorContext.h"
#include "../../CanvasRegistry.h"

namespace vde::tools {

/**
 * @brief Clears the entire canvas to transparent black (0,0,0,0).
 *
 * Syntax: clear
 */
class ClearCommand final : public CanvasCommand {
public:
    const CommandMetadata& metadata() const override {
        static const CommandMetadata meta{
            .name = "clear",
            .aliases = {},
            .category = "Edit",
            .summary = "Clear the canvas to transparent black.",
            .description = "Fills every pixel with transparent black (0,0,0,0).",
            .scope = CommandScope::Canvas,
            .params = {},
            .syntaxExample = "clear",
        };
        return meta;
    }

protected:
    CommandResult executeCanvas(Canvas& canvas, const CommandArgs& /*args*/,
                                EditorContext& /*ctx*/) override {
        canvas.document->snapshotForUndo();
        canvas.document->clear();
        return {true, "Canvas cleared"};
    }
};

REGISTER_COMMAND(ClearCommand)

}  // namespace vde::tools
