#pragma once

/**
 * @file CommandArgParser.h
 * @brief Tokenizer and argument parser for the Resource Editor command system.
 *
 * Converts a raw argument string into a typed CommandArgs bag according to
 * the parameter descriptors declared in CommandMetadata.
 */

#include "CommandBase.h"

#include <string>
#include <vector>

namespace vde::tools {

struct EditorContext;  // forward — needed for named-color resolution

/**
 * @brief Stateless parser that tokenizes an argument string and produces
 *        a CommandArgs bag from a set of ParamDescriptors.
 */
class CommandArgParser {
public:
    /** @brief Result of a parse attempt. */
    struct ParseResult {
        bool success = false;
        CommandArgs args;
        std::string error;
    };

    /**
     * @brief Parse an argument string against the given parameter descriptors.
     * @param argsString  Raw argument text (everything after the command name).
     * @param params      Ordered parameter descriptors from CommandMetadata.
     * @param ctx         Editor context (used for named-color resolution).
     * @return ParseResult with success flag, args, or error message.
     */
    static ParseResult parse(const std::string& argsString,
                             const std::vector<ParamDescriptor>& params,
                             const EditorContext& ctx);

    /**
     * @brief Tokenize an input string, respecting parentheses and quoted strings.
     * @param input Raw input string.
     * @return Vector of tokens.
     */
    static std::vector<std::string> tokenize(const std::string& input);

private:
    static bool parseToken(const std::string& token, ParamType type, ParsedArg& out,
                           const EditorContext& ctx);
    static bool parseTuple(const std::string& token, ParsedArg& out, ParamType expected);
    static bool validateEnum(const std::string& value, const std::vector<std::string>& allowed);
};

}  // namespace vde::tools
