#pragma once

/**
 * @file SaveCommand.h
 * @brief Command to save the active canvas to disk.
 */

#include <string>

#include "../../CanvasRegistry.h"
#include "../../FileOperations.h"
#include "../../ImageDocument.h"
#include "../CommandBase.h"
#include "../CommandRegistry.h"
#include "../EditorContext.h"

namespace vde::tools {

/**
 * @brief Saves the active canvas to its file path, or prompts for one.
 *
 * Syntax: save [filepath]
 */
class SaveCommand final : public GlobalCommand {
  public:
    const CommandMetadata& metadata() const override {
        static const CommandMetadata meta{
            .name = "save",
            .aliases = {},
            .category = "File",
            .summary = "Save the active canvas to disk.",
            .description = "Saves the active canvas image. Uses the provided filepath, "
                           "the document's existing path, or opens a save dialog.",
            .scope = CommandScope::Global,
            .params =
                {
                    {.name = "filepath",
                     .type = ParamType::QuotedString,
                     .required = false,
                     .description = "Output file path",
                     .defaultValue = "",
                     .enumValues = {}},
                },
            .syntaxExample = R"(save "sprite.png")",
        };
        return meta;
    }

  protected:
    CommandResult executeGlobal(const CommandArgs& args, EditorContext& ctx) override {
        Canvas* canvas = ctx.getActiveCanvas();
        if (!canvas) {
            return {false, "No active canvas"};
        }

        std::string path;
        if (args.has("filepath")) {
            path = args.getString("filepath");
        } else if (!canvas->document->getFilePath().empty()) {
            path = canvas->document->getFilePath();
        } else {
            path = FileOperations::saveImageDialog(canvas->name);
            if (path.empty()) {
                return {false, "No file selected"};
            }
        }

        if (!canvas->document->saveToFile(path)) {
            return {false, "Failed to save to: " + path};
        }

        canvas->document->setFilePath(path);
        return {true, "Saved '" + canvas->name + "' to " + path};
    }
};

REGISTER_COMMAND(SaveCommand)

}  // namespace vde::tools
