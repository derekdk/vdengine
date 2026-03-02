#include "CommandSystem.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "CanvasRegistry.h"
#include "FileOperations.h"

namespace vde::tools {

// =============================================================================
// Initialization
// =============================================================================

void CommandSystem::initialize(EditorContext& ctx) {
    m_ctx = &ctx;
}

// =============================================================================
// Execute a single command line
// =============================================================================

bool CommandSystem::execute(const std::string& commandLine) {
    // Trim whitespace
    std::string line = commandLine;
    line.erase(0, line.find_first_not_of(" \t\r\n"));
    line.erase(line.find_last_not_of(" \t\r\n") + 1);

    // Skip empty lines and comments
    if (line.empty() || line[0] == '#' || (line.size() >= 2 && line[0] == '/' && line[1] == '/')) {
        return true;
    }

    // Resolve the command (handles @canvas prefix, longest-match lookup)
    ResolvedCommand resolved = resolveCommand(line);

    if (!resolved.command) {
        std::string errorMsg = "Unknown command: " + line;
        addLogEntry(line, errorMsg, false);
        std::cerr << errorMsg << "\n";
        return false;
    }

    // Parse arguments
    CommandArgs args;
    if (resolved.command->usesCustomParsing()) {
        // Custom-parsing commands receive the raw argument string as remainder
        // Set the remainder directly via the friend relationship through CommandArgParser
        // We do a trivial parse that puts everything in remainder
        auto parseResult = CommandArgParser::parse(resolved.rawArgs, {}, *m_ctx);
        args = std::move(parseResult.args);
    } else {
        auto parseResult =
            CommandArgParser::parse(resolved.rawArgs, resolved.command->metadata().params, *m_ctx);
        if (!parseResult.success) {
            std::string errorMsg = "Parse error: " + parseResult.error;
            addLogEntry(line, errorMsg, false);
            std::cerr << errorMsg << "\n";
            return false;
        }
        args = std::move(parseResult.args);
    }

    // Determine target canvas: @prefix overrides, else use active canvas
    uint32_t targetCanvasId =
        (resolved.targetCanvasId != 0) ? resolved.targetCanvasId : m_activeCanvasId;

    // Execute
    CommandResult result = resolved.command->execute(targetCanvasId, args, *m_ctx);

    // Determine canvas name for the log
    std::string canvasName;
    if (m_ctx && m_ctx->canvases) {
        Canvas* c = m_ctx->canvases->getById(targetCanvasId);
        if (c) {
            canvasName = c->name;
        }
    }

    addLogEntry(line, result.message, result.success, canvasName);

    if (!result.success) {
        std::cerr << "Command failed: " << result.message << "\n";
    }

    return result.success;
}

// =============================================================================
// Resolve command line → ResolvedCommand
// =============================================================================

CommandSystem::ResolvedCommand CommandSystem::resolveCommand(const std::string& commandLine) {
    ResolvedCommand resolved;
    std::string remaining = commandLine;

    // Check for @canvasName prefix
    if (!remaining.empty() && remaining[0] == '@') {
        // Extract the token after @
        size_t spacePos = remaining.find_first_of(" \t", 1);
        std::string canvasToken;
        if (spacePos == std::string::npos) {
            canvasToken = remaining.substr(1);
            remaining.clear();
        } else {
            canvasToken = remaining.substr(1, spacePos - 1);
            remaining = remaining.substr(spacePos + 1);
            // Trim leading whitespace from remaining
            remaining.erase(0, remaining.find_first_not_of(" \t"));
        }

        // Resolve canvas name/id
        if (m_ctx && m_ctx->canvases) {
            Canvas* canvas = m_ctx->canvases->resolve(canvasToken);
            if (canvas) {
                resolved.targetCanvasId = canvas->id;
            }
        }
    }

    if (remaining.empty()) {
        return resolved;
    }

    // Tokenize remaining line for longest-match command lookup
    std::istringstream iss(remaining);
    std::vector<std::string> tokens;
    std::string token;
    while (iss >> token) {
        tokens.push_back(token);
    }

    // Try longest match first: two words, then one word
    auto& registry = CommandRegistry::instance();

    if (tokens.size() >= 2) {
        std::string twoWord = tokens[0] + " " + tokens[1];
        CommandBase* cmd = registry.find(twoWord);
        if (cmd) {
            resolved.command = cmd;
            resolved.commandName = twoWord;
            // Remaining args = everything after the first two tokens
            std::istringstream argsStream(remaining);
            std::string skip;
            argsStream >> skip >> skip;  // skip command tokens
            std::getline(argsStream, resolved.rawArgs);
            // Trim leading whitespace
            resolved.rawArgs.erase(0, resolved.rawArgs.find_first_not_of(" \t"));
            return resolved;
        }
    }

    // Single-word lookup
    if (!tokens.empty()) {
        CommandBase* cmd = registry.find(tokens[0]);
        if (cmd) {
            resolved.command = cmd;
            resolved.commandName = tokens[0];
            // Remaining args = everything after the first token
            std::istringstream argsStream(remaining);
            std::string skip;
            argsStream >> skip;
            std::getline(argsStream, resolved.rawArgs);
            resolved.rawArgs.erase(0, resolved.rawArgs.find_first_not_of(" \t"));
            return resolved;
        }
    }

    return resolved;
}

// =============================================================================
// Script execution
// =============================================================================

bool CommandSystem::executeScript(const std::string& filePath) {
    auto lines = FileOperations::readScriptFile(filePath);
    if (lines.empty()) {
        addLogEntry("executeScript " + filePath, "No commands found or file not readable", false);
        return false;
    }

    addLogEntry("executeScript " + filePath,
                "Running script (" + std::to_string(lines.size()) + " commands)", true);

    bool allOk = true;
    for (const auto& line : lines) {
        if (!execute(line)) {
            allOk = false;
        }
    }

    addLogEntry("executeScript " + filePath, allOk ? "Script complete" : "Script had errors",
                allOk);
    return allOk;
}

// =============================================================================
// Log persistence
// =============================================================================

bool CommandSystem::saveLogRange(size_t start, size_t end, const std::string& filePath) {
    if (start >= m_log.size()) {
        return false;
    }
    end = std::min(end, m_log.size());

    std::ofstream out(filePath);
    if (!out.is_open()) {
        return false;
    }

    for (size_t i = start; i < end; ++i) {
        const auto& entry = m_log[i];
        out << "[" << entry.timestamp << "] ";
        if (!entry.canvasName.empty()) {
            out << "@" << entry.canvasName << " ";
        }
        out << entry.commandLine;
        if (!entry.result.empty()) {
            out << " -> " << entry.result;
        }
        if (!entry.success) {
            out << " [FAILED]";
        }
        out << "\n";
    }

    return true;
}

bool CommandSystem::saveFullLog(const std::string& filePath) {
    return saveLogRange(0, m_log.size(), filePath);
}

// =============================================================================
// Log accessors
// =============================================================================

const std::vector<CommandLogEntry>& CommandSystem::getLog() const {
    return m_log;
}

void CommandSystem::addLogEntry(const std::string& line, const std::string& result, bool success,
                                const std::string& canvasName) {
    CommandLogEntry entry;
    entry.timestamp = getCurrentTimestamp();
    entry.commandLine = line;
    entry.canvasName = canvasName;
    entry.result = result;
    entry.success = success;
    m_log.push_back(std::move(entry));

    // Limit log size
    if (m_log.size() > 10000) {
        m_log.erase(m_log.begin());
    }
}

void CommandSystem::clear() {
    m_log.clear();
    m_activeCanvasId = 0;
}

void CommandSystem::setActiveCanvasId(uint32_t id) {
    m_activeCanvasId = id;
}

uint32_t CommandSystem::getActiveCanvasId() const {
    return m_activeCanvasId;
}

// =============================================================================
// Timestamp helper
// =============================================================================

std::string CommandSystem::getCurrentTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);

    std::tm localTime{};
#ifdef _WIN32
    localtime_s(&localTime, &time);
#else
    localtime_r(&time, &localTime);
#endif

    std::ostringstream oss;
    oss << std::setw(2) << std::setfill('0') << localTime.tm_hour << ":" << std::setw(2)
        << std::setfill('0') << localTime.tm_min << ":" << std::setw(2) << std::setfill('0')
        << localTime.tm_sec;
    return oss.str();
}

}  // namespace vde::tools
