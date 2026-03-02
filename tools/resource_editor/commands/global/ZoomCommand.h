#pragma once

/**
 * @file ZoomCommand.h
 * @brief Command to adjust the viewport zoom level of the active canvas.
 */

#include <algorithm>
#include <string>

#include "../../CanvasRegistry.h"
#include "../../ImageDocument.h"
#include "../CommandBase.h"
#include "../CommandRegistry.h"
#include "../EditorContext.h"

namespace vde::tools {

/**
 * @brief Adjusts the zoom level of the active canvas viewport.
 *
 * Syntax: zoom <level|in|out>
 */
class ZoomCommand final : public GlobalCommand {
  public:
    const CommandMetadata& metadata() const override {
        static const CommandMetadata meta{
            .name = "zoom",
            .aliases = {},
            .category = "View",
            .summary = "Set or adjust viewport zoom.",
            .description = "Sets the zoom level to a specific value, or steps in/out by 2x. "
                           "Zoom is clamped between 1 and 64.",
            .scope = CommandScope::Global,
            .params = {},
            .syntaxExample = "zoom in",
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
        // Trim whitespace
        while (!remainder.empty() && remainder.front() == ' ')
            remainder.erase(remainder.begin());
        while (!remainder.empty() && remainder.back() == ' ')
            remainder.pop_back();

        if (remainder.empty()) {
            return {true,
                    "Current zoom: " + std::to_string(static_cast<int>(canvas->zoomLevel)) + "x"};
        }

        float newZoom = canvas->zoomLevel;

        if (remainder == "in") {
            newZoom = canvas->zoomLevel * 2.0f;
        } else if (remainder == "out") {
            newZoom = canvas->zoomLevel / 2.0f;
        } else {
            try {
                newZoom = std::stof(remainder);
            } catch (...) {
                return {false, "Invalid zoom value: " + remainder};
            }
        }

        newZoom = std::clamp(newZoom, 1.0f, 64.0f);
        canvas->zoomLevel = newZoom;

        return {true, "Zoom set to " + std::to_string(static_cast<int>(newZoom)) + "x"};
    }
};

REGISTER_COMMAND(ZoomCommand)

}  // namespace vde::tools
