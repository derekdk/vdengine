#pragma once

/**
 * @file CommandBase.h
 * @brief Base classes for the Resource Editor command system.
 *
 * Provides CommandResult, ParsedArg, CommandArgs, and the CommandBase /
 * GlobalCommand / CanvasCommand hierarchy.
 */

#include "CommandTypes.h"

#include <map>
#include <stdexcept>
#include <string>

namespace vde::tools {

// Forward declarations — full definitions come in later phases.
struct Canvas;
struct EditorContext;

// =============================================================================
// Command result
// =============================================================================

/**
 * @brief Return value produced by every command execution.
 */
struct CommandResult {
    bool success = true;
    std::string message;
};

// =============================================================================
// Parsed argument
// =============================================================================

class CommandArgParser;  // friend forward

/**
 * @brief A single parsed argument with typed accessors.
 *
 * Populated by the CommandArgParser.  Use asInt(), asFloat(), etc. to
 * retrieve the value in the expected type.
 */
struct ParsedArg {
    std::string raw;
    ParamType type = ParamType::String;

    int asInt() const { return std::stoi(raw); }
    float asFloat() const { return std::stof(raw); }

    bool asBool() const {
        return raw == "true" || raw == "1" || raw == "filled" || raw == "show" || raw == "yes";
    }

    const std::string& asString() const { return raw; }
    RGBAColor asColor() const { return m_color; }
    IntPair asPoint() const { return m_pair; }
    IntPair asSize() const { return m_pair; }
    IntRect asRect() const { return m_rect; }

private:
    friend class CommandArgParser;
    IntPair m_pair;
    IntRect m_rect;
    RGBAColor m_color;
};

// =============================================================================
// CommandArgs — bag of named parsed arguments
// =============================================================================

/**
 * @brief Collection of named, parsed arguments ready for consumption by a command.
 */
class CommandArgs {
public:
    bool has(const std::string& name) const { return m_args.count(name) > 0; }

    const ParsedArg& get(const std::string& name) const {
        auto it = m_args.find(name);
        if (it == m_args.end()) {
            throw std::runtime_error("CommandArgs: missing parameter '" + name + "'");
        }
        return it->second;
    }

    int getInt(const std::string& name) const { return get(name).asInt(); }
    float getFloat(const std::string& name) const { return get(name).asFloat(); }
    bool getBool(const std::string& name) const { return get(name).asBool(); }
    const std::string& getString(const std::string& name) const { return get(name).asString(); }
    RGBAColor getColor(const std::string& name) const { return get(name).asColor(); }
    IntPair getPoint(const std::string& name) const { return get(name).asPoint(); }
    IntPair getSize(const std::string& name) const { return get(name).asSize(); }
    IntRect getRect(const std::string& name) const { return get(name).asRect(); }
    const std::string& remainder() const { return m_remainder; }

private:
    friend class CommandArgParser;
    std::map<std::string, ParsedArg> m_args;
    std::string m_remainder;
};

// =============================================================================
// Command base classes
// =============================================================================

/**
 * @brief Abstract base for all resource editor commands.
 */
class CommandBase {
public:
    virtual ~CommandBase() = default;

    /** @brief Return the command's metadata (name, params, etc.). */
    virtual const CommandMetadata& metadata() const = 0;

    /**
     * @brief Execute the command.
     * @param canvasId Target canvas ID (0 if global).
     * @param args Parsed command arguments.
     * @param ctx Editor context reference.
     */
    virtual CommandResult execute(uint32_t canvasId, const CommandArgs& args,
                                  EditorContext& ctx) = 0;

    /** @brief True if the command handles its own argument parsing. */
    virtual bool usesCustomParsing() const { return false; }
};

/**
 * @brief Base class for commands that operate globally (not on a canvas).
 *
 * Subclasses implement executeGlobal(); the canvasId is ignored.
 */
class GlobalCommand : public CommandBase {
public:
    /// Delegates to executeGlobal().  Defined in CommandBase.cpp.
    CommandResult execute(uint32_t canvasId, const CommandArgs& args,
                          EditorContext& ctx) override;

protected:
    virtual CommandResult executeGlobal(const CommandArgs& args, EditorContext& ctx) = 0;
};

/**
 * @brief Base class for commands that operate on a specific canvas.
 *
 * Subclasses implement executeCanvas(); the canvas is resolved from ctx.
 * Implementation in CommandBase.cpp.
 */
class CanvasCommand : public CommandBase {
public:
    /// Looks up the canvas from EditorContext and delegates to executeCanvas().
    /// Defined in CommandBase.cpp.
    CommandResult execute(uint32_t canvasId, const CommandArgs& args,
                          EditorContext& ctx) override;

protected:
    virtual CommandResult executeCanvas(Canvas& canvas, const CommandArgs& args,
                                        EditorContext& ctx) = 0;
};

}  // namespace vde::tools
