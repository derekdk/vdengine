#pragma once

/**
 * @file ExportCommand.h
 * @brief Command to export the active canvas to a file.
 */

#include "../CommandBase.h"
#include "../CommandRegistry.h"
#include "../EditorContext.h"
#include "../../CanvasRegistry.h"
#include "../../FileOperations.h"
#include "../../ImageDocument.h"

#include <string>

namespace vde::tools {

/**
 * @brief Exports the active canvas to a file (format determined by extension).
 *
 * Syntax: export [filepath]
 */
class ExportCommand final : public GlobalCommand {
public:
    const CommandMetadata& metadata() const override {
        static const CommandMetadata meta{
            .name = "export",
            .aliases = {},
            .category = "File",
            .summary = "Export the active canvas to a file.",
            .description = "Exports the active canvas image to the given path. The output "
                           "format is determined by the file extension (.png, .bmp, .tga). "
                           "Opens an export dialog if no path is given.",
            .scope = CommandScope::Global,
            .params = {},
            .syntaxExample = R"(export "output.png")",
        };
        return meta;
    }

    bool usesCustomParsing() const override { return true; }

protected:
    CommandResult executeGlobal(const CommandArgs& args, EditorContext& ctx) override {
        Canvas* canvas = ctx.getActiveCanvas();
        if (!canvas) {
            return {false, "No active canvas"};
        }

        std::string remainder = args.remainder();
        std::string path;

        if (!remainder.empty()) {
            path = stripQuotes(remainder);
        } else {
            path = FileOperations::exportImageDialog(canvas->name);
            if (path.empty()) {
                return {false, "No file selected"};
            }
        }

        if (!canvas->document->exportToFile(path)) {
            return {false, "Failed to export to: " + path};
        }

        return {true, "Exported '" + canvas->name + "' to " + path};
    }

private:
    static std::string stripQuotes(const std::string& s) {
        std::string trimmed = s;
        // Trim leading/trailing whitespace
        while (!trimmed.empty() && trimmed.front() == ' ') trimmed.erase(trimmed.begin());
        while (!trimmed.empty() && trimmed.back() == ' ') trimmed.pop_back();
        // Strip quotes
        if (trimmed.size() >= 2 && trimmed.front() == '"' && trimmed.back() == '"') {
            return trimmed.substr(1, trimmed.size() - 2);
        }
        return trimmed;
    }
};

REGISTER_COMMAND(ExportCommand)

}  // namespace vde::tools
