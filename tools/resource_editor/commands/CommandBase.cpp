/**
 * @file CommandBase.cpp
 * @brief Implementations of GlobalCommand::execute() and CanvasCommand::execute().
 */

#include "CommandBase.h"

#include "../CanvasRegistry.h"
#include "../CommandSystem.h"
#include "EditorContext.h"

namespace vde::tools {

CommandResult GlobalCommand::execute(uint32_t /*canvasId*/, const CommandArgs& args,
                                     EditorContext& ctx) {
    return executeGlobal(args, ctx);
}

CommandResult CanvasCommand::execute(uint32_t canvasId, const CommandArgs& args,
                                     EditorContext& ctx) {
    // Resolve the target canvas ID — use the explicit ID if provided,
    // otherwise fall back to the active canvas.
    uint32_t targetId = canvasId;
    if (targetId == 0 && ctx.commands) {
        targetId = ctx.commands->getActiveCanvasId();
    }

    if (targetId == 0) {
        return {false, "No canvas selected. Use 'create' or 'select' first."};
    }

    Canvas* canvas = ctx.canvases ? ctx.canvases->getById(targetId) : nullptr;
    if (!canvas) {
        return {false, "Canvas #" + std::to_string(targetId) + " not found."};
    }

    return executeCanvas(*canvas, args, ctx);
}

}  // namespace vde::tools
