/**
 * @file InputScript.cpp
 * @brief Implementation of the input script replay system
 */

#include <vde/api/Game.h>
#include <vde/api/InputHandler.h>
#include <vde/api/InputScript.h>
#include <vde/api/KeyCodes.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace vde {

// ============================================================================
// Logging prefix constant — used by Game.cpp for direct logging
// static constexpr const char* LOG_PREFIX = "[VDE:InputScript] ";

// ============================================================================
// String utilities
// ============================================================================

static std::string toLower(const std::string& s) {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

static std::string toUpper(const std::string& s) {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return result;
}

static std::string trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos)
        return "";
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

static std::vector<std::string> splitWhitespace(const std::string& s) {
    std::vector<std::string> tokens;
    std::istringstream iss(s);
    std::string token;
    while (iss >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

static std::vector<std::string> splitOn(const std::string& s, char delimiter) {
    std::vector<std::string> tokens;
    std::istringstream iss(s);
    std::string token;
    while (std::getline(iss, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

static std::string stripComment(const std::string& line) {
    // Strip # comments and // comments
    // Be careful not to strip inside quoted strings (not applicable here)
    std::string result = line;

    auto hashPos = result.find('#');
    if (hashPos != std::string::npos) {
        result = result.substr(0, hashPos);
    }

    auto slashPos = result.find("//");
    if (slashPos != std::string::npos) {
        result = result.substr(0, slashPos);
    }

    return result;
}

// ============================================================================
// Key name resolution
// ============================================================================

static const std::unordered_map<std::string, int>& getKeyNameMap() {
    static const std::unordered_map<std::string, int> map = {
        // Letters
        {"A", KEY_A},
        {"B", KEY_B},
        {"C", KEY_C},
        {"D", KEY_D},
        {"E", KEY_E},
        {"F", KEY_F},
        {"G", KEY_G},
        {"H", KEY_H},
        {"I", KEY_I},
        {"J", KEY_J},
        {"K", KEY_K},
        {"L", KEY_L},
        {"M", KEY_M},
        {"N", KEY_N},
        {"O", KEY_O},
        {"P", KEY_P},
        {"Q", KEY_Q},
        {"R", KEY_R},
        {"S", KEY_S},
        {"T", KEY_T},
        {"U", KEY_U},
        {"V", KEY_V},
        {"W", KEY_W},
        {"X", KEY_X},
        {"Y", KEY_Y},
        {"Z", KEY_Z},

        // Numbers
        {"0", KEY_0},
        {"1", KEY_1},
        {"2", KEY_2},
        {"3", KEY_3},
        {"4", KEY_4},
        {"5", KEY_5},
        {"6", KEY_6},
        {"7", KEY_7},
        {"8", KEY_8},
        {"9", KEY_9},

        // Named keys
        {"SPACE", KEY_SPACE},
        {"ESC", KEY_ESCAPE},
        {"ESCAPE", KEY_ESCAPE},
        {"ENTER", KEY_ENTER},
        {"RETURN", KEY_ENTER},
        {"TAB", KEY_TAB},
        {"BACKSPACE", KEY_BACKSPACE},
        {"DELETE", KEY_DELETE},
        {"INSERT", KEY_INSERT},
        {"HOME", KEY_HOME},
        {"END", KEY_END},
        {"LEFT", KEY_LEFT},
        {"RIGHT", KEY_RIGHT},
        {"UP", KEY_UP},
        {"DOWN", KEY_DOWN},
        {"PGUP", KEY_PAGE_UP},
        {"PAGEUP", KEY_PAGE_UP},
        {"PGDN", KEY_PAGE_DOWN},
        {"PAGEDOWN", KEY_PAGE_DOWN},

        // Function keys
        {"F1", KEY_F1},
        {"F2", KEY_F2},
        {"F3", KEY_F3},
        {"F4", KEY_F4},
        {"F5", KEY_F5},
        {"F6", KEY_F6},
        {"F7", KEY_F7},
        {"F8", KEY_F8},
        {"F9", KEY_F9},
        {"F10", KEY_F10},
        {"F11", KEY_F11},
        {"F12", KEY_F12},

        // Punctuation
        {"COMMA", KEY_COMMA},
        {"PERIOD", KEY_PERIOD},
        {"SLASH", KEY_SLASH},
        {"SEMICOLON", KEY_SEMICOLON},
        {"APOSTROPHE", KEY_APOSTROPHE},
        {"MINUS", KEY_MINUS},
        {"EQUAL", KEY_EQUAL},
        {"LEFTBRACKET", KEY_LEFT_BRACKET},
        {"RIGHTBRACKET", KEY_RIGHT_BRACKET},
        {"BACKSLASH", KEY_BACKSLASH},
        {"GRAVEACCENT", KEY_GRAVE_ACCENT},

        // Modifier keys (as standalone keys)
        {"LSHIFT", KEY_LEFT_SHIFT},
        {"RSHIFT", KEY_RIGHT_SHIFT},
        {"LCTRL", KEY_LEFT_CONTROL},
        {"RCTRL", KEY_RIGHT_CONTROL},
        {"LALT", KEY_LEFT_ALT},
        {"RALT", KEY_RIGHT_ALT},

        // Numpad
        {"KP0", KEY_KP_0},
        {"KP1", KEY_KP_1},
        {"KP2", KEY_KP_2},
        {"KP3", KEY_KP_3},
        {"KP4", KEY_KP_4},
        {"KP5", KEY_KP_5},
        {"KP6", KEY_KP_6},
        {"KP7", KEY_KP_7},
        {"KP8", KEY_KP_8},
        {"KP9", KEY_KP_9},
    };
    return map;
}

int resolveKeyName(const std::string& keyName) {
    std::string upper = toUpper(keyName);

    // Strip optional KEY_ prefix
    if (upper.size() > 4 && upper.substr(0, 4) == "KEY_") {
        upper = upper.substr(4);
    }

    const auto& map = getKeyNameMap();
    auto it = map.find(upper);
    if (it != map.end()) {
        return it->second;
    }

    return -1;  // Unknown key
}

// ============================================================================
// Modifier parsing
// ============================================================================

bool parseKeyWithModifiers(const std::string& keyArg, int& keyCode, int& modifiers,
                           std::string& errorMsg) {
    modifiers = 0;

    auto parts = splitOn(keyArg, '+');
    if (parts.empty()) {
        errorMsg = "empty key argument";
        return false;
    }

    // Last part is the key, everything before is modifiers
    std::string keyPart = parts.back();
    for (size_t i = 0; i + 1 < parts.size(); ++i) {
        std::string mod = toLower(parts[i]);
        if (mod == "ctrl" || mod == "control") {
            modifiers |= INPUT_SCRIPT_MOD_CTRL;
        } else if (mod == "shift") {
            modifiers |= INPUT_SCRIPT_MOD_SHIFT;
        } else if (mod == "alt") {
            modifiers |= INPUT_SCRIPT_MOD_ALT;
        } else {
            errorMsg = "unknown modifier '" + parts[i] + "'";
            return false;
        }
    }

    keyCode = resolveKeyName(keyPart);
    if (keyCode < 0) {
        errorMsg = "unknown key name '" + keyPart + "'";
        return false;
    }

    return true;
}

// ============================================================================
// Line parser
// ============================================================================

bool parseScriptLine(const std::string& line, int lineNumber, ScriptCommand& cmd,
                     std::string& errorMsg) {
    auto tokens = splitWhitespace(line);
    if (tokens.empty()) {
        errorMsg = "empty command";
        return false;
    }

    cmd = ScriptCommand{};
    cmd.lineNumber = lineNumber;

    std::string verb = toLower(tokens[0]);

    // ---- wait ----
    if (verb == "wait") {
        if (tokens.size() < 2) {
            errorMsg = "at line " + std::to_string(lineNumber) + ": 'wait' requires an argument";
            return false;
        }
        std::string arg = toLower(tokens[1]);
        if (arg == "startup") {
            cmd.type = InputCommandType::WaitStartup;
        } else {
            // Parse as milliseconds (with optional 's' suffix for seconds)
            cmd.type = InputCommandType::WaitMs;
            try {
                if (arg.back() == 's' && arg.size() > 1) {
                    // Seconds suffix
                    cmd.waitMs = std::stod(arg.substr(0, arg.size() - 1)) * 1000.0;
                } else {
                    cmd.waitMs = std::stod(arg);
                }
            } catch (...) {
                errorMsg = "at line " + std::to_string(lineNumber) + ": invalid wait duration '" +
                           tokens[1] + "'";
                return false;
            }
        }
    }
    // ---- press ----
    else if (verb == "press") {
        if (tokens.size() < 2) {
            errorMsg =
                "at line " + std::to_string(lineNumber) + ": 'press' requires a key argument";
            return false;
        }
        cmd.type = InputCommandType::Press;
        if (!parseKeyWithModifiers(tokens[1], cmd.keyCode, cmd.modifiers, errorMsg)) {
            errorMsg = "at line " + std::to_string(lineNumber) + ": " + errorMsg;
            return false;
        }
    }
    // ---- keydown ----
    else if (verb == "keydown") {
        if (tokens.size() < 2) {
            errorMsg =
                "at line " + std::to_string(lineNumber) + ": 'keydown' requires a key argument";
            return false;
        }
        cmd.type = InputCommandType::KeyDown;
        if (!parseKeyWithModifiers(tokens[1], cmd.keyCode, cmd.modifiers, errorMsg)) {
            errorMsg = "at line " + std::to_string(lineNumber) + ": " + errorMsg;
            return false;
        }
    }
    // ---- keyup ----
    else if (verb == "keyup") {
        if (tokens.size() < 2) {
            errorMsg =
                "at line " + std::to_string(lineNumber) + ": 'keyup' requires a key argument";
            return false;
        }
        cmd.type = InputCommandType::KeyUp;
        if (!parseKeyWithModifiers(tokens[1], cmd.keyCode, cmd.modifiers, errorMsg)) {
            errorMsg = "at line " + std::to_string(lineNumber) + ": " + errorMsg;
            return false;
        }
    }
    // ---- click ----
    else if (verb == "click") {
        if (tokens.size() < 3) {
            errorMsg = "at line " + std::to_string(lineNumber) +
                       ": 'click' requires coordinates (click X Y or click right X Y)";
            return false;
        }
        // Check if second token is "right"
        if (toLower(tokens[1]) == "right") {
            cmd.type = InputCommandType::ClickRight;
            if (tokens.size() < 4) {
                errorMsg = "at line " + std::to_string(lineNumber) +
                           ": 'click right' requires X Y coordinates";
                return false;
            }
            try {
                cmd.mouseX = std::stod(tokens[2]);
                cmd.mouseY = std::stod(tokens[3]);
            } catch (...) {
                errorMsg = "at line " + std::to_string(lineNumber) + ": invalid mouse coordinates";
                return false;
            }
        } else {
            cmd.type = InputCommandType::Click;
            try {
                cmd.mouseX = std::stod(tokens[1]);
                cmd.mouseY = std::stod(tokens[2]);
            } catch (...) {
                errorMsg = "at line " + std::to_string(lineNumber) + ": invalid mouse coordinates";
                return false;
            }
        }
    }
    // ---- mousedown ----
    else if (verb == "mousedown") {
        if (tokens.size() < 3) {
            errorMsg =
                "at line " + std::to_string(lineNumber) + ": 'mousedown' requires X Y coordinates";
            return false;
        }
        cmd.type = InputCommandType::MouseDown;
        try {
            cmd.mouseX = std::stod(tokens[1]);
            cmd.mouseY = std::stod(tokens[2]);
        } catch (...) {
            errorMsg = "at line " + std::to_string(lineNumber) + ": invalid mouse coordinates";
            return false;
        }
    }
    // ---- mouseup ----
    else if (verb == "mouseup") {
        if (tokens.size() < 3) {
            errorMsg =
                "at line " + std::to_string(lineNumber) + ": 'mouseup' requires X Y coordinates";
            return false;
        }
        cmd.type = InputCommandType::MouseUp;
        try {
            cmd.mouseX = std::stod(tokens[1]);
            cmd.mouseY = std::stod(tokens[2]);
        } catch (...) {
            errorMsg = "at line " + std::to_string(lineNumber) + ": invalid mouse coordinates";
            return false;
        }
    }
    // ---- mousemove ----
    else if (verb == "mousemove") {
        if (tokens.size() < 3) {
            errorMsg =
                "at line " + std::to_string(lineNumber) + ": 'mousemove' requires X Y coordinates";
            return false;
        }
        cmd.type = InputCommandType::MouseMove;
        try {
            cmd.mouseX = std::stod(tokens[1]);
            cmd.mouseY = std::stod(tokens[2]);
        } catch (...) {
            errorMsg = "at line " + std::to_string(lineNumber) + ": invalid mouse coordinates";
            return false;
        }
    }
    // ---- scroll ----
    else if (verb == "scroll") {
        if (tokens.size() < 4) {
            errorMsg =
                "at line " + std::to_string(lineNumber) + ": 'scroll' requires X Y DELTA arguments";
            return false;
        }
        cmd.type = InputCommandType::Scroll;
        try {
            cmd.mouseX = std::stod(tokens[1]);
            cmd.mouseY = std::stod(tokens[2]);
            cmd.scrollDelta = std::stod(tokens[3]);
        } catch (...) {
            errorMsg = "at line " + std::to_string(lineNumber) + ": invalid scroll arguments";
            return false;
        }
    }
    // ---- screenshot ----
    else if (verb == "screenshot") {
        if (tokens.size() < 2) {
            errorMsg = "at line " + std::to_string(lineNumber) +
                       ": 'screenshot' requires a file path argument";
            return false;
        }
        cmd.type = InputCommandType::Screenshot;
        cmd.argument = tokens[1];
    }
    // ---- print ----
    else if (verb == "print") {
        if (tokens.size() < 2) {
            errorMsg =
                "at line " + std::to_string(lineNumber) + ": 'print' requires a message argument";
            return false;
        }
        cmd.type = InputCommandType::Print;
        // Capture everything after "print " as the message (preserves original casing/spacing)
        auto verbEnd = line.find_first_of(" \t");
        if (verbEnd != std::string::npos) {
            auto msgStart = line.find_first_not_of(" \t", verbEnd);
            if (msgStart != std::string::npos) {
                cmd.argument = line.substr(msgStart);
            }
        }
    }
    // ---- label ----
    else if (verb == "label") {
        if (tokens.size() < 2) {
            errorMsg =
                "at line " + std::to_string(lineNumber) + ": 'label' requires a name argument";
            return false;
        }
        cmd.type = InputCommandType::Label;
        cmd.argument = tokens[1];
    }
    // ---- loop ----
    else if (verb == "loop") {
        if (tokens.size() < 3) {
            errorMsg = "at line " + std::to_string(lineNumber) +
                       ": 'loop' requires a label name and count";
            return false;
        }
        cmd.type = InputCommandType::Loop;
        cmd.argument = tokens[1];
        try {
            cmd.loopCount = std::stoi(tokens[2]);
        } catch (...) {
            errorMsg = "at line " + std::to_string(lineNumber) + ": invalid loop count '" +
                       tokens[2] + "'";
            return false;
        }
    }
    // ---- exit / quit ----
    else if (verb == "exit" || verb == "quit") {
        cmd.type = InputCommandType::Exit;
    }
    // ---- unknown ----
    else {
        errorMsg =
            "at line " + std::to_string(lineNumber) + ": unrecognized command '" + tokens[0] + "'";
        return false;
    }

    return true;
}

// ============================================================================
// File parser
// ============================================================================

bool parseInputScript(const std::string& filePath, std::vector<ScriptCommand>& commands,
                      std::unordered_map<std::string, LabelState>& labels, std::string& errorMsg) {
    commands.clear();
    labels.clear();

    std::ifstream file(filePath);
    if (!file.is_open()) {
        errorMsg = "unable to open script file '" + filePath + "'";
        return false;
    }

    std::string rawLine;
    int lineNumber = 0;

    while (std::getline(file, rawLine)) {
        lineNumber++;

        // Strip comments and trim
        std::string line = trim(stripComment(rawLine));

        // Skip blank lines
        if (line.empty()) {
            continue;
        }

        ScriptCommand cmd;
        if (!parseScriptLine(line, lineNumber, cmd, errorMsg)) {
            return false;
        }

        // Track labels
        if (cmd.type == InputCommandType::Label) {
            if (labels.count(cmd.argument)) {
                errorMsg = "at line " + std::to_string(lineNumber) + ": duplicate label '" +
                           cmd.argument + "'";
                return false;
            }
            LabelState ls;
            ls.commandIndex = commands.size();
            ls.remainingIterations = -1;
            labels[cmd.argument] = ls;
        }

        // Validate loop references (no forward references)
        if (cmd.type == InputCommandType::Loop) {
            if (labels.find(cmd.argument) == labels.end()) {
                errorMsg = "at line " + std::to_string(lineNumber) + ": undefined label '" +
                           cmd.argument + "'";
                return false;
            }
        }

        commands.push_back(cmd);
    }

    return true;
}

// ============================================================================
// CLI argument parsing
// ============================================================================

std::string getInputScriptArg(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);
        if (arg == "--input-script" && i + 1 < argc) {
            return std::string(argv[i + 1]);
        }
        // Also support --input-script=path
        if (arg.substr(0, 15) == "--input-script=") {
            return arg.substr(15);
        }
    }
    return "";
}

void configureInputScriptFromArgs(Game& game, int argc, char** argv) {
    std::string scriptPath = getInputScriptArg(argc, argv);
    if (!scriptPath.empty()) {
        // Resolve to absolute path before CWD may change
        std::filesystem::path p(scriptPath);
        if (p.is_relative()) {
            std::error_code ec;
            auto abs = std::filesystem::absolute(p, ec);
            if (!ec) {
                scriptPath = abs.string();
            }
        }
        game.setInputScriptFile(scriptPath);
    }
}

}  // namespace vde
