/**
 * @file CommandSystem.cpp
 * @brief Implementation of the CommandSystem command dispatch.
 */

#include "CommandSystem.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace vde {
namespace tools {

// =============================================================================
// Command registration
// =============================================================================

void CommandSystem::registerGlobalCommand(const std::string& name, const std::string& help,
                                          GlobalHandler handler) {
    m_globalHandlers[name] = std::move(handler);
    m_helpText[name] = help;
}

void CommandSystem::registerCanvasCommand(const std::string& name, const std::string& help,
                                          CanvasHandler handler) {
    m_canvasHandlers[name] = std::move(handler);
    m_helpText[name] = help;
}

// =============================================================================
// Command resolution
// =============================================================================

ResolvedCommand CommandSystem::resolveCommand(const std::string& commandLine) const {
    ResolvedCommand resolved;

    std::string trimmed = commandLine;
    // Trim leading whitespace
    size_t start = trimmed.find_first_not_of(" \t");
    if (start == std::string::npos) {
        return resolved;
    }
    trimmed = trimmed.substr(start);

    // Check for @canvas prefix
    if (!trimmed.empty() && trimmed[0] == '@') {
        size_t spacePos = trimmed.find_first_of(" \t", 1);
        std::string canvasRef;
        std::string rest;

        if (spacePos == std::string::npos) {
            canvasRef = trimmed.substr(1);
        } else {
            canvasRef = trimmed.substr(1, spacePos - 1);
            rest = trimmed.substr(spacePos);
            // Trim leading whitespace from rest
            size_t restStart = rest.find_first_not_of(" \t");
            if (restStart != std::string::npos) {
                rest = rest.substr(restStart);
            } else {
                rest.clear();
            }
        }

        resolved.hasExplicitCanvas = true;

        // Resolve the canvas reference
        if (m_registry) {
            Canvas* canvas = m_registry->resolve(canvasRef);
            if (canvas) {
                resolved.canvasId = canvas->id;
                resolved.canvasName = canvas->name;
            }
        }

        trimmed = rest;
    } else {
        // No explicit canvas — use active canvas
        resolved.canvasId = m_activeCanvasId;
        if (m_registry && m_activeCanvasId > 0) {
            Canvas* canvas = m_registry->getById(m_activeCanvasId);
            if (canvas) {
                resolved.canvasName = canvas->name;
            }
        }
    }

    // Parse command name and args from remaining text
    if (!trimmed.empty()) {
        size_t spacePos = trimmed.find_first_of(" \t");
        if (spacePos == std::string::npos) {
            resolved.commandName = trimmed;
        } else {
            resolved.commandName = trimmed.substr(0, spacePos);
            std::string args = trimmed.substr(spacePos);
            size_t argsStart = args.find_first_not_of(" \t");
            if (argsStart != std::string::npos) {
                resolved.argsString = args.substr(argsStart);
            }
        }
    }

    // Lowercase command name
    std::transform(resolved.commandName.begin(), resolved.commandName.end(),
                   resolved.commandName.begin(), ::tolower);

    return resolved;
}

// =============================================================================
// Command execution
// =============================================================================

bool CommandSystem::execute(const std::string& commandLine) {
    // Trim
    std::string trimmed = commandLine;
    size_t start = trimmed.find_first_not_of(" \t\r\n");
    if (start == std::string::npos)
        return true;  // Empty line, skip
    trimmed = trimmed.substr(start);
    size_t end = trimmed.find_last_not_of(" \t\r\n");
    if (end != std::string::npos)
        trimmed = trimmed.substr(0, end + 1);

    // Skip comments
    if (trimmed.empty() || trimmed[0] == '#')
        return true;

    ResolvedCommand resolved = resolveCommand(trimmed);

    if (resolved.commandName.empty()) {
        return true;  // Nothing to execute
    }

    // Create log entry
    CommandLogEntry entry;
    entry.index = m_log.size();
    entry.timestamp = getCurrentTimestamp();
    entry.commandLine = commandLine;
    entry.canvasId = resolved.canvasId;
    entry.canvasName = resolved.canvasName;

    // Try global handler first
    auto globalIt = m_globalHandlers.find(resolved.commandName);
    if (globalIt != m_globalHandlers.end()) {
        if (resolved.hasExplicitCanvas) {
            entry.result =
                "Warning: Global command '" + resolved.commandName + "' ignores @canvas prefix";
        }
        // Add log entry before executing (so setLastResult can update it)
        m_log.push_back(std::move(entry));

        globalIt->second(resolved.argsString);
        return m_log.back().success;
    }

    // Try canvas handler
    auto canvasIt = m_canvasHandlers.find(resolved.commandName);
    if (canvasIt != m_canvasHandlers.end()) {
        if (resolved.canvasId == 0) {
            entry.result = "Error: No active canvas for command '" + resolved.commandName + "'";
            entry.success = false;
            m_log.push_back(std::move(entry));
            return false;
        }

        m_log.push_back(std::move(entry));
        canvasIt->second(resolved.canvasId, resolved.argsString);
        return m_log.back().success;
    }

    // Unknown command
    entry.result = "Error: Unknown command '" + resolved.commandName + "'";
    entry.success = false;
    m_log.push_back(std::move(entry));
    return false;
}

void CommandSystem::setLastResult(const std::string& result, bool success) {
    if (!m_log.empty()) {
        m_log.back().result = result;
        m_log.back().success = success;
    }
}

bool CommandSystem::executeScript(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        return false;
    }

    std::string line;
    bool allSuccess = true;

    while (std::getline(file, line)) {
        // Strip BOM if present
        if (line.size() >= 3 && static_cast<unsigned char>(line[0]) == 0xEF &&
            static_cast<unsigned char>(line[1]) == 0xBB &&
            static_cast<unsigned char>(line[2]) == 0xBF) {
            line = line.substr(3);
        }

        if (!execute(line)) {
            allSuccess = false;
        }
    }

    return allSuccess;
}

// =============================================================================
// Log I/O
// =============================================================================

bool CommandSystem::saveFullLog(const std::string& filePath) const {
    return saveLogRange(0, m_log.size(), filePath);
}

bool CommandSystem::saveLogRange(size_t startIdx, size_t endIdx,
                                 const std::string& filePath) const {
    std::ofstream file(filePath);
    if (!file.is_open())
        return false;

    endIdx = std::min(endIdx, m_log.size());

    for (size_t i = startIdx; i < endIdx; ++i) {
        const auto& entry = m_log[i];

        // For canvas-targeted commands without explicit @prefix, normalize with @name
        std::string line = entry.commandLine;

        // Trim for output
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start != std::string::npos && start > 0) {
            line = line.substr(start);
        }

        // Skip comments and empty lines
        if (line.empty() || line[0] == '#')
            continue;

        file << line << "\n";
    }

    return true;
}

// =============================================================================
// Accessors
// =============================================================================

std::vector<std::string> CommandSystem::getCommandNames() const {
    std::vector<std::string> names;
    for (const auto& [name, handler] : m_globalHandlers) {
        names.push_back(name);
    }
    for (const auto& [name, handler] : m_canvasHandlers) {
        names.push_back(name);
    }
    std::sort(names.begin(), names.end());
    return names;
}

std::string CommandSystem::getHelpText(const std::string& command) const {
    auto it = m_helpText.find(command);
    if (it != m_helpText.end())
        return it->second;
    return "";
}

// =============================================================================
// Utilities
// =============================================================================

std::string CommandSystem::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto timeT = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    std::tm localTime{};
#ifdef _WIN32
    localtime_s(&localTime, &timeT);
#else
    localtime_r(&timeT, &localTime);
#endif

    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(2) << localTime.tm_hour << ":" << std::setw(2)
        << localTime.tm_min << ":" << std::setw(2) << localTime.tm_sec << "." << std::setw(3)
        << ms.count();
    return oss.str();
}

}  // namespace tools
}  // namespace vde
