/**
 * @file CommandRegistry.cpp
 * @brief Implementation of the command registry singleton.
 */

#include "CommandRegistry.h"

#include <algorithm>
#include <iostream>

namespace vde::tools {

// =============================================================================
// Singleton
// =============================================================================

CommandRegistry& CommandRegistry::instance() {
    static CommandRegistry s_instance;
    return s_instance;
}

// =============================================================================
// registerCommand
// =============================================================================

void CommandRegistry::registerCommand(std::unique_ptr<CommandBase> cmd) {
    CommandBase* raw = cmd.get();
    const auto& meta = raw->metadata();

    // Index by primary name
    m_nameIndex[meta.name] = raw;

    // Index by aliases
    for (const auto& alias : meta.aliases) {
        m_aliasIndex[alias] = raw;
    }

    m_commands.push_back(std::move(cmd));
}

// =============================================================================
// find — longest-match on compound names
// =============================================================================

CommandBase* CommandRegistry::find(const std::string& name) const {
    // Try the full string first
    {
        auto it = m_nameIndex.find(name);
        if (it != m_nameIndex.end())
            return it->second;
    }
    {
        auto it = m_aliasIndex.find(name);
        if (it != m_aliasIndex.end())
            return it->second;
    }

    // Progressively try shorter prefixes by splitting on the last space
    std::string candidate = name;
    while (true) {
        auto pos = candidate.rfind(' ');
        if (pos == std::string::npos)
            break;
        candidate = candidate.substr(0, pos);

        auto it = m_nameIndex.find(candidate);
        if (it != m_nameIndex.end())
            return it->second;

        auto ait = m_aliasIndex.find(candidate);
        if (ait != m_aliasIndex.end())
            return ait->second;
    }

    return nullptr;
}

// =============================================================================
// Metadata queries
// =============================================================================

std::vector<const CommandMetadata*> CommandRegistry::getAllMetadata() const {
    std::vector<const CommandMetadata*> result;
    result.reserve(m_commands.size());
    for (const auto& cmd : m_commands) {
        result.push_back(&cmd->metadata());
    }
    return result;
}

std::vector<const CommandMetadata*>
CommandRegistry::getByCategory(const std::string& category) const {
    std::vector<const CommandMetadata*> result;
    for (const auto& cmd : m_commands) {
        if (cmd->metadata().category == category) {
            result.push_back(&cmd->metadata());
        }
    }
    return result;
}

}  // namespace vde::tools
