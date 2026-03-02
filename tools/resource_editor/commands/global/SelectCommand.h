#pragma once

/**
 * @file SelectCommand.h
 * @brief Command to select (activate) a canvas by name or ID.
 */

#include "../CommandBase.h"
#include "../CommandRegistry.h"
#include "../EditorContext.h"
#include "../../CanvasRegistry.h"
#include "../../ImageDocument.h"

#include <sstream>
#include <string>

namespace vde::tools {

/**
 * @brief Selects a canvas as the active canvas.
 *
 * Syntax: select [canvas] <name>
 *
 * Uses custom parsing so both `select canvas mysprite` and
 * `select mysprite` are accepted.
 */
class SelectCommand final : public GlobalCommand {
public:
    const CommandMetadata& metadata() const override {
        static const CommandMetadata meta{
            .name = "select",
            .aliases = {},
            .category = "Canvas",
            .summary = "Select a canvas as the active canvas.",
            .description = "Sets the given canvas as active. The optional 'canvas' keyword "
                           "can be included for clarity but is not required.",
            .scope = CommandScope::Global,
            .params =
                {
                    {.name = "name",
                     .type = ParamType::String,
                     .required = true,
                     .description = "Canvas name or ID to select"},
                },
            .syntaxExample = "select canvas mysprite",
        };
        return meta;
    }

    bool usesCustomParsing() const override { return true; }

protected:
    CommandResult executeGlobal(const CommandArgs& args, EditorContext& ctx) override {
        std::string remainder = args.remainder();
        std::istringstream iss(remainder);
        std::string first;
        iss >> first;

        std::string canvasName;
        if (first == "canvas") {
            iss >> canvasName;
        } else {
            canvasName = first;
        }

        if (canvasName.empty()) {
            return {false, "Usage: select [canvas] <name>"};
        }

        Canvas* canvas = ctx.canvases->resolve(canvasName);
        if (!canvas) {
            return {false, "Canvas '" + canvasName + "' not found"};
        }

        ctx.commands->setActiveCanvasId(canvas->id);
        return {true, "Active canvas set to '" + canvas->name + "'"};
    }
};

REGISTER_COMMAND(SelectCommand)

}  // namespace vde::tools
