#pragma once

/**
 * @file ListColorsCommand.h
 * @brief Command to list all named colors in the editor context.
 */

#include "../../CanvasRegistry.h"
#include "../../ImageDocument.h"
#include "../../ToolPalette.h"
#include "../CommandBase.h"
#include "../CommandRegistry.h"
#include "../EditorContext.h"

namespace vde::tools {

class ListColorsCommand final : public GlobalCommand {
  public:
    const CommandMetadata& metadata() const override {
        static const CommandMetadata meta{
            .name = "list colors",
            .aliases = {},
            .category = "Color",
            .summary = "List all named colors.",
            .description =
                "Lists all named colors defined in the editor context in alphabetical order.",
            .scope = CommandScope::Global,
            .params = {},
            .syntaxExample = "list colors",
        };
        return meta;
    }

  protected:
    CommandResult executeGlobal(const CommandArgs& /*args*/, EditorContext& ctx) override {
        if (ctx.namedColors.empty()) {
            return {true, "No named colors defined."};
        }
        std::string result = "Named colors:";
        for (const auto& [name, color] : ctx.namedColors) {
            result += "\n  " + name + ": " + color.toHex();
        }
        return {true, result};
    }
};

REGISTER_COMMAND(ListColorsCommand)

}  // namespace vde::tools
