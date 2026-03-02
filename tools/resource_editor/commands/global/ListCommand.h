#pragma once

/**
 * @file ListCommand.h
 * @brief Command to list all open canvases.
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
 * @brief Lists all open canvases with their ID, name, dimensions, and state.
 *
 * Syntax: list
 */
class ListCommand final : public GlobalCommand {
public:
    const CommandMetadata& metadata() const override {
        static const CommandMetadata meta{
            .name = "list",
            .aliases = {"ls", "canvases"},
            .category = "Canvas",
            .summary = "List all open canvases.",
            .description = "Displays a table of all canvases showing ID, name, dimensions, "
                           "and whether each is the active canvas.",
            .scope = CommandScope::Global,
            .params = {},
            .syntaxExample = "list",
        };
        return meta;
    }

protected:
    CommandResult executeGlobal(const CommandArgs& /*args*/, EditorContext& ctx) override {
        auto ids = ctx.canvases->getIds();
        if (ids.empty()) {
            return {true, "No canvases open."};
        }

        uint32_t activeId = ctx.commands->getActiveCanvasId();

        std::ostringstream os;
        os << "Canvases (" << ids.size() << "):\n";
        os << "  ID  Name                 Size        Active\n";
        os << "  --- -------------------- ----------- ------\n";

        for (uint32_t id : ids) {
            Canvas* c = ctx.canvases->getById(id);
            if (!c) continue;

            uint32_t w = c->document ? c->document->getWidth() : 0;
            uint32_t h = c->document ? c->document->getHeight() : 0;
            bool isActive = (id == activeId);

            // Format each row
            os << "  ";
            // ID (3 chars, left-aligned)
            std::string idStr = std::to_string(id);
            os << idStr;
            for (size_t i = idStr.size(); i < 4; ++i) os << ' ';

            // Name (20 chars, left-aligned)
            std::string nameStr = c->name;
            if (nameStr.size() > 20) nameStr = nameStr.substr(0, 17) + "...";
            os << nameStr;
            for (size_t i = nameStr.size(); i < 21; ++i) os << ' ';

            // Size
            std::string sizeStr = std::to_string(w) + "x" + std::to_string(h);
            os << sizeStr;
            for (size_t i = sizeStr.size(); i < 12; ++i) os << ' ';

            // Active marker
            if (isActive) os << "*";

            os << "\n";
        }

        return {true, os.str()};
    }
};

REGISTER_COMMAND(ListCommand)

}  // namespace vde::tools
