/**
 * @file CommandRegistry.cpp
 * @brief Implementation of the dynamic command registry.
 */

#include "CommandRegistry.h"

#include <algorithm>
#include <sstream>

namespace vde {
namespace tools {

std::string CommandRegistry::toLower(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

bool CommandRegistry::add(const std::string& name, const std::string& usage,
                          const std::string& description, CommandHandler handler,
                          CompletionCallback completer) {
    std::string key = toLower(name);
    if (m_commands.find(key) != m_commands.end()) {
        return false;
    }

    CommandInfo info;
    info.name = key;
    info.usage = usage;
    info.description = description;
    info.handler = std::move(handler);
    info.completer = std::move(completer);
    info.enabled = true;

    m_commands[key] = std::move(info);
    return true;
}

bool CommandRegistry::remove(const std::string& name) {
    return m_commands.erase(toLower(name)) > 0;
}

bool CommandRegistry::enable(const std::string& name) {
    auto it = m_commands.find(toLower(name));
    if (it == m_commands.end()) {
        return false;
    }
    it->second.enabled = true;
    return true;
}

bool CommandRegistry::disable(const std::string& name) {
    auto it = m_commands.find(toLower(name));
    if (it == m_commands.end()) {
        return false;
    }
    it->second.enabled = false;
    return true;
}

bool CommandRegistry::has(const std::string& name) const {
    return m_commands.find(toLower(name)) != m_commands.end();
}

bool CommandRegistry::isEnabled(const std::string& name) const {
    auto it = m_commands.find(toLower(name));
    if (it == m_commands.end()) {
        return false;
    }
    return it->second.enabled;
}

bool CommandRegistry::execute(const std::string& cmdLine) {
    // Trim leading whitespace
    std::string trimmed = cmdLine;
    trimmed.erase(0, trimmed.find_first_not_of(" \t"));
    if (trimmed.empty()) {
        return true;  // Empty input is not an error
    }

    // Split into command + args
    std::istringstream iss(trimmed);
    std::string cmd;
    iss >> cmd;

    std::string key = toLower(cmd);
    auto it = m_commands.find(key);
    if (it == m_commands.end()) {
        return false;  // Not found
    }

    if (!it->second.enabled) {
        return false;  // Disabled
    }

    // Extract the remainder as the argument string
    std::string args;
    if (iss.tellg() != std::istringstream::pos_type(-1)) {
        args = trimmed.substr(static_cast<size_t>(iss.tellg()));
        // Trim leading whitespace from args
        args.erase(0, args.find_first_not_of(" \t"));
    }

    it->second.handler(args);
    return true;
}

std::vector<std::string> CommandRegistry::getCompletions(const std::string& input) const {
    std::vector<std::string> results;
    if (input.empty()) {
        return results;
    }

    // Tokenize input
    std::vector<std::string> tokens;
    std::istringstream iss(input);
    std::string token;
    while (iss >> token) {
        tokens.push_back(token);
    }

    // Check if there's a trailing space (user is starting a new token)
    bool trailingSpace = !input.empty() && input.back() == ' ';

    if (tokens.empty()) {
        return results;
    }

    if (tokens.size() == 1 && !trailingSpace) {
        // Complete command names
        std::string prefix = toLower(tokens[0]);
        for (const auto& [name, info] : m_commands) {
            if (info.enabled && name.substr(0, prefix.size()) == prefix) {
                results.push_back(name);
            }
        }
    } else {
        // Delegate to the command's completer
        std::string cmdName = toLower(tokens[0]);
        auto it = m_commands.find(cmdName);
        if (it != m_commands.end() && it->second.enabled && it->second.completer) {
            // Determine partial text for the current argument
            std::string partial;
            if (!trailingSpace && tokens.size() > 1) {
                partial = tokens.back();
            }

            results = it->second.completer(partial, tokens);
        }
    }

    return results;
}

std::vector<const CommandInfo*> CommandRegistry::getEnabledCommands() const {
    std::vector<const CommandInfo*> result;
    for (const auto& [name, info] : m_commands) {
        if (info.enabled) {
            result.push_back(&info);
        }
    }
    return result;
}

std::vector<const CommandInfo*> CommandRegistry::getAllCommands() const {
    std::vector<const CommandInfo*> result;
    for (const auto& [name, info] : m_commands) {
        result.push_back(&info);
    }
    return result;
}

const CommandInfo* CommandRegistry::getCommand(const std::string& name) const {
    auto it = m_commands.find(toLower(name));
    if (it == m_commands.end()) {
        return nullptr;
    }
    return &it->second;
}

}  // namespace tools
}  // namespace vde
