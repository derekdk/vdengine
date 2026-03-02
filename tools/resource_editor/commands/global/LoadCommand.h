#pragma once

/**
 * @file LoadCommand.h
 * @brief Command to load an image from disk into a new or existing canvas.
 */

#include "../CommandBase.h"
#include "../CommandRegistry.h"
#include "../EditorContext.h"
#include "../../CanvasRegistry.h"
#include "../../FileOperations.h"
#include "../../ImageDocument.h"

#include <string>
#include <vector>

namespace vde::tools {

/**
 * @brief Loads an image file into a canvas.
 *
 * Syntax: load [canvas] "<filepath>" [name]
 *
 * If no filepath is given, opens a file dialog.
 * If a canvas name is given with a filepath, loads the image as a named resource.
 * If just a filepath is given, creates a new canvas from the image.
 */
class LoadCommand final : public GlobalCommand {
public:
    const CommandMetadata& metadata() const override {
        static const CommandMetadata meta{
            .name = "load",
            .aliases = {"open"},
            .category = "File",
            .summary = "Load an image from disk.",
            .description = "Loads an image file into a new canvas, or as a named resource "
                           "into an existing canvas. Opens a file dialog if no path is given.",
            .scope = CommandScope::Global,
            .params = {},
            .syntaxExample = R"(load "sprite.png")",
        };
        return meta;
    }

    bool usesCustomParsing() const override { return true; }

protected:
    CommandResult executeGlobal(const CommandArgs& args, EditorContext& ctx) override {
        std::string remainder = args.remainder();

        std::string filepath;
        std::string canvasName;
        std::string resourceName;

        if (remainder.empty()) {
            filepath = FileOperations::openImageDialog();
            if (filepath.empty()) {
                return {false, "No file selected"};
            }
        } else {
            auto tokens = parseLoadArgs(remainder);
            if (tokens.size() == 1) {
                filepath = stripQuotes(tokens[0]);
            } else if (tokens.size() == 2) {
                canvasName = tokens[0];
                filepath = stripQuotes(tokens[1]);
            } else if (tokens.size() >= 3) {
                canvasName = tokens[0];
                filepath = stripQuotes(tokens[1]);
                resourceName = tokens[2];
            }
        }

        auto doc = ImageDocument::loadFromFile(filepath);
        if (!doc) {
            return {false, "Failed to load: " + filepath};
        }

        if (canvasName.empty()) {
            canvasName = FileOperations::filenameStem(filepath);
        }
        if (resourceName.empty()) {
            resourceName = FileOperations::filenameStem(filepath);
        }

        // If target canvas already exists, load as a named resource
        Canvas* existing = ctx.canvases->getByName(canvasName);
        if (existing) {
            existing->resources[resourceName] = std::move(doc);
            return {true, "Loaded '" + resourceName + "' into canvas '" + canvasName + "'"};
        }

        // Create a new canvas from the loaded image
        Canvas* canvas = ctx.canvases->create(canvasName, std::move(doc));
        if (!canvas) {
            return {false, "Failed to create canvas"};
        }
        canvas->document->setFilePath(filepath);
        ctx.commands->setActiveCanvasId(canvas->id);
        return {true, "Loaded '" + canvasName + "' (" +
                           std::to_string(canvas->document->getWidth()) + "x" +
                           std::to_string(canvas->document->getHeight()) + ")"};
    }

private:
    /**
     * @brief Split a load argument string into tokens, respecting quoted strings.
     */
    static std::vector<std::string> parseLoadArgs(const std::string& input) {
        std::vector<std::string> tokens;
        size_t i = 0;
        while (i < input.size()) {
            // Skip whitespace
            while (i < input.size() && input[i] == ' ') ++i;
            if (i >= input.size()) break;

            if (input[i] == '"') {
                // Quoted token — find closing quote
                size_t start = i;
                ++i;
                while (i < input.size() && input[i] != '"') ++i;
                if (i < input.size()) ++i;  // skip closing quote
                tokens.push_back(input.substr(start, i - start));
            } else {
                // Unquoted token
                size_t start = i;
                while (i < input.size() && input[i] != ' ') ++i;
                tokens.push_back(input.substr(start, i - start));
            }
        }
        return tokens;
    }

    /**
     * @brief Strip surrounding double quotes from a string.
     */
    static std::string stripQuotes(const std::string& s) {
        if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
            return s.substr(1, s.size() - 2);
        }
        return s;
    }
};

REGISTER_COMMAND(LoadCommand)

}  // namespace vde::tools
