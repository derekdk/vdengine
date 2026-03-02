#pragma once

/**
 * @file SetToolCommand.h
 * @brief Command to switch the active drawing tool on the palette.
 */

#include "../../CanvasRegistry.h"
#include "../../ImageDocument.h"
#include "../../ToolPalette.h"
#include "../CommandBase.h"
#include "../CommandRegistry.h"
#include "../EditorContext.h"

namespace vde::tools {

/**
 * @brief Switches the active drawing tool on the palette.
 *
 * Syntax: settool <toolname>
 */
class SetToolCommand final : public GlobalCommand {
  public:
    const CommandMetadata& metadata() const override {
        static const CommandMetadata meta{
            .name = "settool",
            .aliases = {"tool"},
            .category = "Tool",
            .summary = "Set the active drawing tool.",
            .description = "Switches the palette to the specified tool (brush, eraser, "
                           "colorpicker, fill, line, rect, circle).",
            .scope = CommandScope::Global,
            .params =
                {
                    {.name = "toolname",
                     .type = ParamType::String,
                     .required = true,
                     .description = "Tool name"},
                },
            .syntaxExample = "settool brush",
        };
        return meta;
    }

  protected:
    CommandResult executeGlobal(const CommandArgs& args, EditorContext& ctx) override {
        const std::string& toolName = args.getString("toolname");

        if (!ctx.palette) {
            return {false, "Tool palette not available"};
        }

        EditorTool tool{};
        if (!ToolPalette::stringToTool(toolName, tool)) {
            return {false, "Unknown tool: " + toolName};
        }

        ctx.palette->setTool(tool);
        return {true, "Tool set to " + ToolPalette::toolToString(tool)};
    }
};

REGISTER_COMMAND(SetToolCommand)

}  // namespace vde::tools
