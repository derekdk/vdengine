#pragma once

/**
 * @file InputScript.h
 * @brief Input script replay system for VDE
 *
 * Provides scripted input replay for smoke testing and automation.
 * Scripts use a verb-arg command syntax with .vdescript file extension.
 *
 * Supports keyboard, mouse, timing, loop control, and screenshot commands.
 * Scripts can be loaded via API call, CLI argument, or environment variable.
 *
 * Priority order: API call > CLI arg (--input-script) > env var (VDE_INPUT_SCRIPT)
 */

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace vde {

class Game;  // forward

// ============================================================================
// Command types and structures
// ============================================================================

/**
 * @brief Types of commands that can appear in an input script.
 */
enum class InputCommandType {
    WaitStartup,  ///< wait startup — wait for first frame render
    WaitMs,       ///< wait 500 — wait N milliseconds
    Press,        ///< press A — keydown + keyup (with optional modifiers)
    KeyDown,      ///< keydown W — hold key (with optional modifiers)
    KeyUp,        ///< keyup W — release key (with optional modifiers)
    Click,        ///< click 400 300 — left-click at position
    ClickRight,   ///< click right 400 300 — right-click at position
    MouseDown,    ///< mousedown 400 300 — press left button
    MouseUp,      ///< mouseup 400 300 — release left button
    MouseMove,    ///< mousemove 640 360 — move cursor
    Scroll,       ///< scroll 400 300 -3 — scroll at position
    Screenshot,   ///< screenshot path.png — save frame to PNG
    Print,        ///< print message — output text to console
    Label,        ///< label loop_start — define jump target
    Loop,         ///< loop loop_start 5 — jump back to label
    Exit,         ///< exit — quit the application

    // Track A extensions
    WaitFrames,        ///< wait_frames 10 — wait N rendered frames
    AssertSceneCount,  ///< assert rendered_scene_count == N
    AssertScene,       ///< assert scene "name" <field> <op> <value>
    Compare,           ///< compare actual.png golden.png 0.02 — image comparison
    Set                ///< set VAR value — define a script variable
};

/**
 * @brief Modifier key bitmask constants for input scripts.
 *
 * These match the GLFW modifier definitions used in KeyCodes.h.
 */
constexpr int INPUT_SCRIPT_MOD_CTRL = 0x0002;
constexpr int INPUT_SCRIPT_MOD_SHIFT = 0x0001;
constexpr int INPUT_SCRIPT_MOD_ALT = 0x0004;

/**
 * @brief A single parsed command from an input script.
 */
/**
 * @brief Comparison operators for assert commands.
 */
enum class CompareOp { Eq, Ne, Lt, Le, Gt, Ge };

/**
 * @brief A single parsed command from an input script.
 */
struct ScriptCommand {
    InputCommandType type;
    int keyCode = 0;           ///< For Press/KeyDown/KeyUp
    int modifiers = 0;         ///< Bitmask: MOD_CTRL | MOD_SHIFT | MOD_ALT
    double waitMs = 0.0;       ///< For WaitMs
    double mouseX = 0.0;       ///< For Click/MouseDown/MouseUp/MouseMove/Scroll
    double mouseY = 0.0;       ///< For Click/MouseDown/MouseUp/MouseMove/Scroll
    double scrollDelta = 0.0;  ///< For Scroll
    std::string argument;      ///< For Screenshot path, Print message, Label name, Loop target
    int loopCount = 0;         ///< For Loop (0 = infinite)
    int lineNumber = 0;        ///< Source line for error messages

    // A3: wait_frames
    int waitFrames = 0;  ///< For WaitFrames — number of frames to wait

    // A1: assertion fields
    std::string assertSceneName;         ///< For AssertScene — scene name to check
    std::string assertField;             ///< "was_rendered", "draw_calls", etc.
    CompareOp assertOp = CompareOp::Eq;  ///< Comparison operator
    double assertValue = 0.0;            ///< RHS of comparison

    // A4: compare fields
    std::string comparePath;        ///< Golden reference image path
    double compareThreshold = 0.0;  ///< RMSE threshold for Compare

    // A5: set fields
    std::string setVarName;    ///< Variable name for Set command
    double setVarValue = 0.0;  ///< Variable value for Set command
};

/**
 * @brief Label state tracking for loop execution.
 */
struct LabelState {
    size_t commandIndex = 0;       ///< Index of the label command
    int remainingIterations = -1;  ///< -1 = not yet entered, 0 = infinite
};

/**
 * @brief Opaque state for a running input script.
 */
struct InputScriptState {
    std::vector<ScriptCommand> commands;
    size_t currentCommand = 0;
    double waitAccumulator = 0.0;
    bool startupComplete = false;
    bool finished = false;
    std::string scriptPath;
    uint64_t frameNumber = 0;
    std::unordered_map<std::string, LabelState> labels;

    /// Pending mouse button release (for click commands that span 2 frames)
    bool pendingMouseRelease = false;
    int pendingMouseButton = 0;
    double pendingMouseX = 0.0;
    double pendingMouseY = 0.0;

    /// A3: Frame-wait counter (decremented each frame until zero)
    int frameWaitCounter = 0;

    /// A5: Script variables (name -> value)
    std::unordered_map<std::string, double> variables;

    /// A1: Track whether any assertion has failed
    bool assertionFailed = false;
};

// ============================================================================
// Public API
// ============================================================================

/**
 * @brief Parse a script file into a list of commands.
 * @param filePath Path to the .vdescript file
 * @param[out] commands Parsed commands output
 * @param[out] labels Label state map output
 * @param[out] errorMsg Error message if parsing fails
 * @return true if parsing succeeded
 */
bool parseInputScript(const std::string& filePath, std::vector<ScriptCommand>& commands,
                      std::unordered_map<std::string, LabelState>& labels, std::string& errorMsg);

/**
 * @brief Parse a single line of script text into a command.
 * @param line The line text (comments and whitespace already stripped)
 * @param lineNumber The source line number for error reporting
 * @param[out] cmd The parsed command
 * @param[out] errorMsg Error message if parsing fails
 * @return true if parsing succeeded
 */
bool parseScriptLine(const std::string& line, int lineNumber, ScriptCommand& cmd,
                     std::string& errorMsg);

/**
 * @brief Resolve a key name string to a VDE key code.
 * @param keyName The key name (e.g., "A", "SPACE", "ESCAPE", "F1")
 * @return The key code, or -1 if not recognized
 */
int resolveKeyName(const std::string& keyName);

/**
 * @brief Parse modifiers and key from a combined key argument.
 *
 * Splits "ctrl+shift+A" into modifiers bitmask and key name "A".
 *
 * @param keyArg The key argument (e.g., "ctrl+S", "shift+W", "A")
 * @param[out] keyCode The resolved key code
 * @param[out] modifiers The modifier bitmask
 * @param[out] errorMsg Error message if parsing fails
 * @return true if parsing succeeded
 */
bool parseKeyWithModifiers(const std::string& keyArg, int& keyCode, int& modifiers,
                           std::string& errorMsg);

/**
 * @brief Parse a comparison operator string to CompareOp enum.
 * @param opStr The operator string ("==", "!=", "<", "<=", ">", ">=")
 * @param[out] op The parsed CompareOp
 * @param[out] errorMsg Error message if parsing fails
 * @return true if parsing succeeded
 */
bool parseCompareOp(const std::string& opStr, CompareOp& op, std::string& errorMsg);

/**
 * @brief Evaluate a comparison between two values.
 * @param lhs Left-hand side value
 * @param op Comparison operator
 * @param rhs Right-hand side value
 * @return true if the comparison is satisfied
 */
bool evaluateComparison(double lhs, CompareOp op, double rhs);

/**
 * @brief Get the string representation of a CompareOp.
 * @param op The comparison operator
 * @return String representation ("==", "!=", "<", "<=", ">", ">=")
 */
const char* compareOpToString(CompareOp op);

/**
 * @brief Parse --input-script from command-line arguments.
 * @param argc Argument count
 * @param argv Argument values
 * @return The script path, or empty string if not found
 */
std::string getInputScriptArg(int argc, char** argv);

/**
 * @brief Configure game with input script from CLI args.
 * @param game The game instance
 * @param argc Argument count
 * @param argv Argument values
 */
void configureInputScriptFromArgs(Game& game, int argc, char** argv);

}  // namespace vde
