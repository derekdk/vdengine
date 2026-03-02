#pragma once

/**
 * @file CommandTypes.h
 * @brief Foundational types for the Resource Editor command system.
 *
 * Defines colors, geometry primitives, parameter descriptors, and
 * command metadata used throughout the command infrastructure.
 */

#include <cstdint>
#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

namespace vde::tools {

// =============================================================================
// Color type
// =============================================================================

/**
 * @brief RGBA color with 8-bit-per-channel components.
 */
struct RGBAColor {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t a = 255;

    /** @brief Pack into a single 32-bit value (R << 24 | G << 16 | B << 8 | A). */
    uint32_t toPackedRGBA() const {
        return (static_cast<uint32_t>(r) << 24) | (static_cast<uint32_t>(g) << 16) |
               (static_cast<uint32_t>(b) << 8) | static_cast<uint32_t>(a);
    }

    /** @brief Create from a packed 32-bit RGBA value. */
    static RGBAColor fromPacked(uint32_t packed) {
        RGBAColor c;
        c.r = static_cast<uint8_t>((packed >> 24) & 0xFF);
        c.g = static_cast<uint8_t>((packed >> 16) & 0xFF);
        c.b = static_cast<uint8_t>((packed >> 8) & 0xFF);
        c.a = static_cast<uint8_t>(packed & 0xFF);
        return c;
    }

    /**
     * @brief Parse a hex color string into an RGBAColor.
     * @param hex String in "#RRGGBB" (6-digit) or "#RRGGBBAA" (8-digit) format.
     * @param out Receives the parsed color on success.
     * @return true if parsing succeeded.
     */
    static bool fromHex(const std::string& hex, RGBAColor& out) {
        if (hex.empty() || hex[0] != '#') {
            return false;
        }
        std::string digits = hex.substr(1);
        if (digits.size() != 6 && digits.size() != 8) {
            return false;
        }
        // Validate all hex digits
        for (char ch : digits) {
            if (!((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') ||
                  (ch >= 'A' && ch <= 'F'))) {
                return false;
            }
        }
        unsigned long val = std::stoul(digits, nullptr, 16);
        if (digits.size() == 6) {
            out.r = static_cast<uint8_t>((val >> 16) & 0xFF);
            out.g = static_cast<uint8_t>((val >> 8) & 0xFF);
            out.b = static_cast<uint8_t>(val & 0xFF);
            out.a = 255;
        } else {
            out.r = static_cast<uint8_t>((val >> 24) & 0xFF);
            out.g = static_cast<uint8_t>((val >> 16) & 0xFF);
            out.b = static_cast<uint8_t>((val >> 8) & 0xFF);
            out.a = static_cast<uint8_t>(val & 0xFF);
        }
        return true;
    }

    /** @brief Convert to "#RRGGBBAA" hex string. */
    std::string toHex() const {
        char buf[12];
        std::snprintf(buf, sizeof(buf), "#%02X%02X%02X%02X", r, g, b, a);
        return std::string(buf);
    }

    bool operator==(const RGBAColor& o) const {
        return r == o.r && g == o.g && b == o.b && a == o.a;
    }

    bool operator!=(const RGBAColor& o) const { return !(*this == o); }
};

// =============================================================================
// Geometry primitives
// =============================================================================

struct IntPair {
    int x = 0;
    int y = 0;
};

struct IntRect {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
};

// =============================================================================
// Parameter descriptors
// =============================================================================

/** @brief Types that a command parameter can take. */
enum class ParamType { Int, Float, String, QuotedString, Color, Bool, Keyword, Enum, Point, Size, Rect };

/**
 * @brief Describes a single parameter that a command accepts.
 */
struct ParamDescriptor {
    std::string name;
    ParamType type = ParamType::String;
    bool required = true;
    std::string description;
    std::string defaultValue;
    std::vector<std::string> enumValues;
};

// =============================================================================
// Command metadata
// =============================================================================

/** @brief Whether a command operates globally or on a specific canvas. */
enum class CommandScope { Global, Canvas };

/**
 * @brief Full metadata for a registered command, used for help text and parsing.
 */
struct CommandMetadata {
    std::string name;
    std::vector<std::string> aliases;
    std::string category;
    std::string summary;
    std::string description;
    CommandScope scope = CommandScope::Global;
    std::vector<ParamDescriptor> params;
    std::string syntaxExample;

    /**
     * @brief Format a short usage line: "name <param1> <param2> ..."
     */
    std::string formatUsage() const {
        std::ostringstream os;
        os << name;
        for (const auto& p : params) {
            if (p.required) {
                os << " <" << p.name << ">";
            } else {
                os << " [" << p.name << "]";
            }
        }
        return os.str();
    }

    /**
     * @brief Format detailed help text for this command.
     */
    std::string formatHelp() const {
        std::ostringstream os;
        os << name;
        if (!aliases.empty()) {
            os << " (aliases:";
            for (const auto& a : aliases) {
                os << " " << a;
            }
            os << ")";
        }
        os << "\n";

        if (!summary.empty()) {
            os << "  " << summary << "\n";
        }
        if (!category.empty()) {
            os << "  Category: " << category << "\n";
        }
        os << "  Scope: " << (scope == CommandScope::Global ? "global" : "canvas") << "\n";

        os << "  Usage: " << formatUsage() << "\n";

        if (!syntaxExample.empty()) {
            os << "  Example: " << syntaxExample << "\n";
        }

        if (!params.empty()) {
            os << "  Parameters:\n";
            for (const auto& p : params) {
                os << "    " << p.name << " (";
                switch (p.type) {
                case ParamType::Int:         os << "int";          break;
                case ParamType::Float:       os << "float";        break;
                case ParamType::String:      os << "string";       break;
                case ParamType::QuotedString:os << "quoted string"; break;
                case ParamType::Color:       os << "color";        break;
                case ParamType::Bool:        os << "bool";         break;
                case ParamType::Keyword:     os << "keyword";      break;
                case ParamType::Enum:        os << "enum";         break;
                case ParamType::Point:       os << "point";        break;
                case ParamType::Size:        os << "size";         break;
                case ParamType::Rect:        os << "rect";         break;
                }
                os << ")";
                if (!p.required) {
                    os << " [optional";
                    if (!p.defaultValue.empty()) {
                        os << ", default=" << p.defaultValue;
                    }
                    os << "]";
                }
                if (!p.description.empty()) {
                    os << " - " << p.description;
                }
                if (!p.enumValues.empty()) {
                    os << " {";
                    for (size_t i = 0; i < p.enumValues.size(); ++i) {
                        if (i > 0) os << ", ";
                        os << p.enumValues[i];
                    }
                    os << "}";
                }
                os << "\n";
            }
        }

        if (!description.empty()) {
            os << "  Description: " << description << "\n";
        }
        return os.str();
    }
};

}  // namespace vde::tools
