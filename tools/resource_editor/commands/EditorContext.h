#pragma once

/**
 * @file EditorContext.h
 * @brief Runtime context shared across all Resource Editor commands.
 *
 * Aggregates pointers to the major subsystems (canvas registry, command
 * system, tool palette) so commands can interact with the editor without
 * tight coupling.
 */

#include <map>
#include <string>

#include "CommandTypes.h"

namespace vde {
class Game;
}  // namespace vde

namespace vde::tools {

class CanvasRegistry;
class CommandSystem;
class ToolPalette;
struct Canvas;

/**
 * @brief Shared context passed to every command during execution.
 */
struct EditorContext {
    CanvasRegistry* canvases = nullptr;
    CommandSystem* commands = nullptr;
    ToolPalette* palette = nullptr;
    std::map<std::string, RGBAColor> namedColors;

    vde::Game* game = nullptr;

    /**
     * @brief Retrieve the currently-active canvas.
     * @return Pointer to the active Canvas, or nullptr if none.
     */
    Canvas* getActiveCanvas();

    /**
     * @brief Resolve a color token (hex string or named color) to an RGBAColor.
     * @param token Color string (e.g. "#FF0000" or "red").
     * @param out   Receives the resolved color on success.
     * @return true if resolution succeeded.
     */
    bool resolveColor(const std::string& token, RGBAColor& out) const {
        // Try named colors first
        auto it = namedColors.find(token);
        if (it != namedColors.end()) {
            out = it->second;
            return true;
        }
        // Fall back to hex parsing
        return RGBAColor::fromHex(token, out);
    }
};

}  // namespace vde::tools
