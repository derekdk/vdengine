/**
 * @file CommandArgParser.cpp
 * @brief Implementation of the command argument tokenizer and parser.
 */

#include "CommandArgParser.h"

#include "EditorContext.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <stdexcept>

namespace vde::tools {

// =============================================================================
// tokenize — parenthesis- and quote-aware splitting
// =============================================================================

std::vector<std::string> CommandArgParser::tokenize(const std::string& input) {
    std::vector<std::string> tokens;
    std::string current;
    int parenDepth = 0;
    bool inQuote = false;

    for (size_t i = 0; i < input.size(); ++i) {
        char ch = input[i];

        // Quoted string handling
        if (ch == '"' && parenDepth == 0) {
            if (!inQuote) {
                inQuote = true;
                current += ch;
                continue;
            } else {
                inQuote = false;
                current += ch;
                tokens.push_back(current);
                current.clear();
                continue;
            }
        }

        if (inQuote) {
            current += ch;
            continue;
        }

        // Parenthesis grouping
        if (ch == '(') {
            ++parenDepth;
            current += ch;
            continue;
        }
        if (ch == ')') {
            if (parenDepth > 0) {
                --parenDepth;
            }
            current += ch;
            if (parenDepth == 0) {
                tokens.push_back(current);
                current.clear();
            }
            continue;
        }

        if (parenDepth > 0) {
            current += ch;
            continue;
        }

        // Whitespace separation
        if (std::isspace(static_cast<unsigned char>(ch))) {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
            continue;
        }

        current += ch;
    }

    if (!current.empty()) {
        tokens.push_back(current);
    }
    return tokens;
}

// =============================================================================
// validateEnum
// =============================================================================

bool CommandArgParser::validateEnum(const std::string& value,
                                    const std::vector<std::string>& allowed) {
    for (const auto& s : allowed) {
        // Case-insensitive comparison
        if (s.size() == value.size()) {
            bool match = true;
            for (size_t i = 0; i < s.size(); ++i) {
                if (std::tolower(static_cast<unsigned char>(s[i])) !=
                    std::tolower(static_cast<unsigned char>(value[i]))) {
                    match = false;
                    break;
                }
            }
            if (match) return true;
        }
    }
    return false;
}

// =============================================================================
// parseTuple — "(a, b)" → IntPair,  "(a, b, c, d)" or "((a,b),(c,d))" → IntRect
// =============================================================================

bool CommandArgParser::parseTuple(const std::string& token, ParsedArg& out, ParamType expected) {
    // Strip outer parentheses
    std::string inner = token;
    if (inner.size() >= 2 && inner.front() == '(' && inner.back() == ')') {
        inner = inner.substr(1, inner.size() - 2);
    } else {
        return false;
    }

    // Check for nested-pair rect: "(a,b),(c,d)"
    if (expected == ParamType::Rect && inner.find("(") != std::string::npos) {
        // Parse "((a,b),(c,d))" → already stripped one layer: "(a,b),(c,d)"
        // Strip another layer if present
        if (inner.size() >= 2 && inner.front() == '(' && inner.back() == ')') {
            inner = inner.substr(1, inner.size() - 2);
        }
        // Now we have "a,b),(c,d" or similar — find the split
        auto mid = inner.find("),(");
        if (mid != std::string::npos) {
            std::string first = inner.substr(0, mid);
            std::string second = inner.substr(mid + 3);
            // Parse "a,b" and "c,d"
            auto comma1 = first.find(',');
            auto comma2 = second.find(',');
            if (comma1 == std::string::npos || comma2 == std::string::npos) return false;
            try {
                out.m_rect.x = std::stoi(first.substr(0, comma1));
                out.m_rect.y = std::stoi(first.substr(comma1 + 1));
                out.m_rect.w = std::stoi(second.substr(0, comma2));
                out.m_rect.h = std::stoi(second.substr(comma2 + 1));
            } catch (...) {
                return false;
            }
            out.raw = token;
            return true;
        }
    }

    // Split by comma
    std::vector<int> values;
    std::istringstream ss(inner);
    std::string part;
    while (std::getline(ss, part, ',')) {
        // Trim whitespace
        auto start = part.find_first_not_of(" \t");
        auto end = part.find_last_not_of(" \t");
        if (start == std::string::npos) return false;
        part = part.substr(start, end - start + 1);
        try {
            values.push_back(std::stoi(part));
        } catch (...) {
            return false;
        }
    }

    if ((expected == ParamType::Point || expected == ParamType::Size) && values.size() == 2) {
        out.m_pair.x = values[0];
        out.m_pair.y = values[1];
        out.raw = token;
        return true;
    }
    if (expected == ParamType::Rect && values.size() == 4) {
        out.m_rect.x = values[0];
        out.m_rect.y = values[1];
        out.m_rect.w = values[2];
        out.m_rect.h = values[3];
        out.raw = token;
        return true;
    }
    return false;
}

// =============================================================================
// parseToken — single-token dispatch by ParamType
// =============================================================================

bool CommandArgParser::parseToken(const std::string& token, ParamType type, ParsedArg& out,
                                  const EditorContext& ctx) {
    out.raw = token;
    out.type = type;

    switch (type) {
    case ParamType::Int:
        try {
            (void)std::stoi(token);
        } catch (...) {
            return false;
        }
        return true;

    case ParamType::Float:
        try {
            (void)std::stof(token);
        } catch (...) {
            return false;
        }
        return true;

    case ParamType::String:
    case ParamType::Keyword:
        return true;

    case ParamType::QuotedString:
        if (token.size() >= 2 && token.front() == '"' && token.back() == '"') {
            out.raw = token.substr(1, token.size() - 2);
        }
        return true;

    case ParamType::Bool: {
        std::string lower = token;
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        if (lower == "true" || lower == "false" || lower == "1" || lower == "0" ||
            lower == "filled" || lower == "outline" || lower == "show" || lower == "hide" ||
            lower == "yes" || lower == "no") {
            return true;
        }
        return false;
    }

    case ParamType::Color: {
        if (!token.empty() && token[0] == '#') {
            return RGBAColor::fromHex(token, out.m_color);
        }
        // Try named color resolution from editor context
        if (ctx.resolveColor(token, out.m_color)) {
            return true;
        }
        return false;
    }

    case ParamType::Enum:
        // Validation happens at the parse() level with enumValues.
        return true;

    case ParamType::Point:
    case ParamType::Size:
        return parseTuple(token, out, type);

    case ParamType::Rect:
        return parseTuple(token, out, type);
    }
    return false;
}

// =============================================================================
// parse — walk ParamDescriptors against token list
// =============================================================================

CommandArgParser::ParseResult CommandArgParser::parse(const std::string& argsString,
                                                      const std::vector<ParamDescriptor>& params,
                                                      const EditorContext& ctx) {
    ParseResult result;
    auto tokens = tokenize(argsString);
    size_t tokenIdx = 0;

    for (const auto& param : params) {
        // --- Keyword parameter ---
        if (param.type == ParamType::Keyword) {
            if (tokenIdx < tokens.size()) {
                // Case-insensitive keyword match
                std::string tok = tokens[tokenIdx];
                std::string expected = param.name;
                std::transform(tok.begin(), tok.end(), tok.begin(),
                               [](unsigned char c) { return std::tolower(c); });
                std::transform(expected.begin(), expected.end(), expected.begin(),
                               [](unsigned char c) { return std::tolower(c); });
                if (tok == expected) {
                    ParsedArg arg;
                    arg.raw = tokens[tokenIdx];
                    arg.type = ParamType::Keyword;
                    result.args.m_args[param.name] = arg;
                    ++tokenIdx;
                    continue;
                }
            }
            // Optional keyword not matched — skip
            if (!param.required) {
                continue;
            }
            result.error = "Expected keyword '" + param.name + "'";
            return result;
        }

        // --- Point / Size — may consume 1 tuple token or 2 bare tokens ---
        if (param.type == ParamType::Point || param.type == ParamType::Size) {
            if (tokenIdx >= tokens.size()) {
                if (param.required) {
                    result.error = "Missing required parameter '" + param.name + "'";
                    return result;
                }
                continue;
            }
            ParsedArg arg;
            if (tokens[tokenIdx].front() == '(') {
                if (!parseTuple(tokens[tokenIdx], arg, param.type)) {
                    result.error =
                        "Invalid " + param.name + " tuple: '" + tokens[tokenIdx] + "'";
                    return result;
                }
                arg.type = param.type;
                result.args.m_args[param.name] = arg;
                ++tokenIdx;
            } else {
                // Bare-pair fallback: consume two tokens as x y
                if (tokenIdx + 1 >= tokens.size()) {
                    if (param.required) {
                        result.error = "Not enough tokens for parameter '" + param.name + "'";
                        return result;
                    }
                    continue;
                }
                try {
                    arg.m_pair.x = std::stoi(tokens[tokenIdx]);
                    arg.m_pair.y = std::stoi(tokens[tokenIdx + 1]);
                } catch (...) {
                    result.error = "Invalid integers for parameter '" + param.name + "'";
                    return result;
                }
                arg.raw = tokens[tokenIdx] + " " + tokens[tokenIdx + 1];
                arg.type = param.type;
                result.args.m_args[param.name] = arg;
                tokenIdx += 2;
            }
            continue;
        }

        // --- Rect — may consume 1 tuple or 4 bare tokens ---
        if (param.type == ParamType::Rect) {
            if (tokenIdx >= tokens.size()) {
                if (param.required) {
                    result.error = "Missing required parameter '" + param.name + "'";
                    return result;
                }
                continue;
            }
            ParsedArg arg;
            if (tokens[tokenIdx].front() == '(') {
                if (!parseTuple(tokens[tokenIdx], arg, ParamType::Rect)) {
                    result.error =
                        "Invalid " + param.name + " rect: '" + tokens[tokenIdx] + "'";
                    return result;
                }
                arg.type = ParamType::Rect;
                result.args.m_args[param.name] = arg;
                ++tokenIdx;
            } else {
                // Flat 4-int fallback
                if (tokenIdx + 3 >= tokens.size()) {
                    if (param.required) {
                        result.error = "Not enough tokens for rect parameter '" + param.name + "'";
                        return result;
                    }
                    continue;
                }
                try {
                    arg.m_rect.x = std::stoi(tokens[tokenIdx]);
                    arg.m_rect.y = std::stoi(tokens[tokenIdx + 1]);
                    arg.m_rect.w = std::stoi(tokens[tokenIdx + 2]);
                    arg.m_rect.h = std::stoi(tokens[tokenIdx + 3]);
                } catch (...) {
                    result.error = "Invalid integers for rect parameter '" + param.name + "'";
                    return result;
                }
                arg.raw = tokens[tokenIdx] + " " + tokens[tokenIdx + 1] + " " +
                          tokens[tokenIdx + 2] + " " + tokens[tokenIdx + 3];
                arg.type = ParamType::Rect;
                result.args.m_args[param.name] = arg;
                tokenIdx += 4;
            }
            continue;
        }

        // --- All other types: consume a single token ---
        if (tokenIdx >= tokens.size()) {
            if (param.required) {
                result.error = "Missing required parameter '" + param.name + "'";
                return result;
            }
            // Apply default if available
            if (!param.defaultValue.empty()) {
                ParsedArg arg;
                arg.raw = param.defaultValue;
                arg.type = param.type;
                result.args.m_args[param.name] = arg;
            }
            continue;
        }

        ParsedArg arg;
        if (!parseToken(tokens[tokenIdx], param.type, arg, ctx)) {
            if (!param.required) {
                if (!param.defaultValue.empty()) {
                    arg.raw = param.defaultValue;
                    arg.type = param.type;
                    result.args.m_args[param.name] = arg;
                }
                continue;
            }
            result.error = "Invalid value for '" + param.name + "': '" + tokens[tokenIdx] + "'";
            return result;
        }

        // Enum validation
        if (param.type == ParamType::Enum && !param.enumValues.empty()) {
            if (!validateEnum(arg.raw, param.enumValues)) {
                result.error = "Invalid enum value for '" + param.name + "': '" + arg.raw + "'";
                return result;
            }
        }

        result.args.m_args[param.name] = arg;
        ++tokenIdx;
    }

    // Collect remaining tokens as the remainder
    if (tokenIdx < tokens.size()) {
        std::ostringstream rem;
        for (size_t i = tokenIdx; i < tokens.size(); ++i) {
            if (i > tokenIdx) rem << " ";
            rem << tokens[i];
        }
        result.args.m_remainder = rem.str();
    }

    result.success = true;
    return result;
}

}  // namespace vde::tools
