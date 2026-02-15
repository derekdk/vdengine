/**
 * @file InputScript_test.cpp
 * @brief Unit tests for VDE InputScript parser
 *
 * Tests the script parser logic against ScriptCommand output.
 * Pure CPU tests — no window or Vulkan context needed.
 */

#include <vde/api/InputScript.h>
#include <vde/api/KeyCodes.h>

#include <fstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace vde {
namespace test {

// ============================================================================
// Key name resolution tests
// ============================================================================

TEST(InputScriptKeyResolve, ResolvesLetterKeys) {
    EXPECT_EQ(resolveKeyName("A"), KEY_A);
    EXPECT_EQ(resolveKeyName("Z"), KEY_Z);
    EXPECT_EQ(resolveKeyName("a"), KEY_A);  // case insensitive
    EXPECT_EQ(resolveKeyName("m"), KEY_M);
}

TEST(InputScriptKeyResolve, ResolvesNumberKeys) {
    EXPECT_EQ(resolveKeyName("0"), KEY_0);
    EXPECT_EQ(resolveKeyName("9"), KEY_9);
    EXPECT_EQ(resolveKeyName("5"), KEY_5);
}

TEST(InputScriptKeyResolve, ResolvesNamedKeys) {
    EXPECT_EQ(resolveKeyName("SPACE"), KEY_SPACE);
    EXPECT_EQ(resolveKeyName("ESC"), KEY_ESCAPE);
    EXPECT_EQ(resolveKeyName("ESCAPE"), KEY_ESCAPE);
    EXPECT_EQ(resolveKeyName("ENTER"), KEY_ENTER);
    EXPECT_EQ(resolveKeyName("RETURN"), KEY_ENTER);
    EXPECT_EQ(resolveKeyName("TAB"), KEY_TAB);
    EXPECT_EQ(resolveKeyName("BACKSPACE"), KEY_BACKSPACE);
    EXPECT_EQ(resolveKeyName("DELETE"), KEY_DELETE);
    EXPECT_EQ(resolveKeyName("INSERT"), KEY_INSERT);
    EXPECT_EQ(resolveKeyName("HOME"), KEY_HOME);
    EXPECT_EQ(resolveKeyName("END"), KEY_END);
    EXPECT_EQ(resolveKeyName("LEFT"), KEY_LEFT);
    EXPECT_EQ(resolveKeyName("RIGHT"), KEY_RIGHT);
    EXPECT_EQ(resolveKeyName("UP"), KEY_UP);
    EXPECT_EQ(resolveKeyName("DOWN"), KEY_DOWN);
    EXPECT_EQ(resolveKeyName("PGUP"), KEY_PAGE_UP);
    EXPECT_EQ(resolveKeyName("PAGEUP"), KEY_PAGE_UP);
    EXPECT_EQ(resolveKeyName("PGDN"), KEY_PAGE_DOWN);
    EXPECT_EQ(resolveKeyName("PAGEDOWN"), KEY_PAGE_DOWN);
}

TEST(InputScriptKeyResolve, ResolvesFunctionKeys) {
    EXPECT_EQ(resolveKeyName("F1"), KEY_F1);
    EXPECT_EQ(resolveKeyName("F12"), KEY_F12);
    EXPECT_EQ(resolveKeyName("F5"), KEY_F5);
}

TEST(InputScriptKeyResolve, ResolvesWithKeyPrefix) {
    EXPECT_EQ(resolveKeyName("KEY_SPACE"), KEY_SPACE);
    EXPECT_EQ(resolveKeyName("KEY_A"), KEY_A);
    EXPECT_EQ(resolveKeyName("key_escape"), KEY_ESCAPE);
}

TEST(InputScriptKeyResolve, ReturnsNegativeForUnknown) {
    EXPECT_EQ(resolveKeyName("FOOBAR"), -1);
    EXPECT_EQ(resolveKeyName("XYZ"), -1);
    EXPECT_EQ(resolveKeyName(""), -1);
}

TEST(InputScriptKeyResolve, CaseInsensitive) {
    EXPECT_EQ(resolveKeyName("space"), KEY_SPACE);
    EXPECT_EQ(resolveKeyName("Space"), KEY_SPACE);
    EXPECT_EQ(resolveKeyName("SPACE"), KEY_SPACE);
    EXPECT_EQ(resolveKeyName("esc"), KEY_ESCAPE);
    EXPECT_EQ(resolveKeyName("Escape"), KEY_ESCAPE);
}

// ============================================================================
// Modifier parsing tests
// ============================================================================

TEST(InputScriptModifiers, ParsesCtrlModifier) {
    int keyCode = 0, modifiers = 0;
    std::string error;
    EXPECT_TRUE(parseKeyWithModifiers("ctrl+A", keyCode, modifiers, error));
    EXPECT_EQ(keyCode, KEY_A);
    EXPECT_EQ(modifiers, INPUT_SCRIPT_MOD_CTRL);
}

TEST(InputScriptModifiers, ParsesShiftModifier) {
    int keyCode = 0, modifiers = 0;
    std::string error;
    EXPECT_TRUE(parseKeyWithModifiers("shift+W", keyCode, modifiers, error));
    EXPECT_EQ(keyCode, KEY_W);
    EXPECT_EQ(modifiers, INPUT_SCRIPT_MOD_SHIFT);
}

TEST(InputScriptModifiers, ParsesAltModifier) {
    int keyCode = 0, modifiers = 0;
    std::string error;
    EXPECT_TRUE(parseKeyWithModifiers("alt+F4", keyCode, modifiers, error));
    EXPECT_EQ(keyCode, KEY_F4);
    EXPECT_EQ(modifiers, INPUT_SCRIPT_MOD_ALT);
}

TEST(InputScriptModifiers, ParsesMultipleModifiers) {
    int keyCode = 0, modifiers = 0;
    std::string error;
    EXPECT_TRUE(parseKeyWithModifiers("ctrl+shift+Z", keyCode, modifiers, error));
    EXPECT_EQ(keyCode, KEY_Z);
    EXPECT_EQ(modifiers, INPUT_SCRIPT_MOD_CTRL | INPUT_SCRIPT_MOD_SHIFT);
}

TEST(InputScriptModifiers, ModifierOrderDoesNotMatter) {
    int keyCode1 = 0, modifiers1 = 0;
    int keyCode2 = 0, modifiers2 = 0;
    std::string error;

    EXPECT_TRUE(parseKeyWithModifiers("ctrl+shift+A", keyCode1, modifiers1, error));
    EXPECT_TRUE(parseKeyWithModifiers("shift+ctrl+A", keyCode2, modifiers2, error));

    EXPECT_EQ(keyCode1, keyCode2);
    EXPECT_EQ(modifiers1, modifiers2);
}

TEST(InputScriptModifiers, UnknownModifierReportsError) {
    int keyCode = 0, modifiers = 0;
    std::string error;
    EXPECT_FALSE(parseKeyWithModifiers("super+A", keyCode, modifiers, error));
    EXPECT_FALSE(error.empty());
}

TEST(InputScriptModifiers, BareKeyHasNoModifiers) {
    int keyCode = 0, modifiers = 0;
    std::string error;
    EXPECT_TRUE(parseKeyWithModifiers("A", keyCode, modifiers, error));
    EXPECT_EQ(keyCode, KEY_A);
    EXPECT_EQ(modifiers, 0);
}

TEST(InputScriptModifiers, UnknownKeyInModifierExpression) {
    int keyCode = 0, modifiers = 0;
    std::string error;
    EXPECT_FALSE(parseKeyWithModifiers("ctrl+UNKNOWNKEY", keyCode, modifiers, error));
    EXPECT_FALSE(error.empty());
}

// ============================================================================
// Line parser tests — Timing
// ============================================================================

TEST(InputScriptParseLine, ParsesWaitStartup) {
    ScriptCommand cmd;
    std::string error;
    EXPECT_TRUE(parseScriptLine("wait startup", 1, cmd, error));
    EXPECT_EQ(cmd.type, InputCommandType::WaitStartup);
    EXPECT_EQ(cmd.lineNumber, 1);
}

TEST(InputScriptParseLine, ParsesWaitMs) {
    ScriptCommand cmd;
    std::string error;
    EXPECT_TRUE(parseScriptLine("wait 500", 2, cmd, error));
    EXPECT_EQ(cmd.type, InputCommandType::WaitMs);
    EXPECT_DOUBLE_EQ(cmd.waitMs, 500.0);
}

TEST(InputScriptParseLine, ParsesWaitSecondsSuffix) {
    ScriptCommand cmd;
    std::string error;
    EXPECT_TRUE(parseScriptLine("wait 2s", 3, cmd, error));
    EXPECT_EQ(cmd.type, InputCommandType::WaitMs);
    EXPECT_DOUBLE_EQ(cmd.waitMs, 2000.0);
}

TEST(InputScriptParseLine, ParsesWaitFractionalSeconds) {
    ScriptCommand cmd;
    std::string error;
    EXPECT_TRUE(parseScriptLine("wait 1.5s", 4, cmd, error));
    EXPECT_EQ(cmd.type, InputCommandType::WaitMs);
    EXPECT_DOUBLE_EQ(cmd.waitMs, 1500.0);
}

// ============================================================================
// Line parser tests — Keyboard
// ============================================================================

TEST(InputScriptParseLine, ParsesPressCharacterKey) {
    ScriptCommand cmd;
    std::string error;
    EXPECT_TRUE(parseScriptLine("press A", 1, cmd, error));
    EXPECT_EQ(cmd.type, InputCommandType::Press);
    EXPECT_EQ(cmd.keyCode, KEY_A);
    EXPECT_EQ(cmd.modifiers, 0);
}

TEST(InputScriptParseLine, ParsesPressNamedKey) {
    ScriptCommand cmd;
    std::string error;
    EXPECT_TRUE(parseScriptLine("press SPACE", 1, cmd, error));
    EXPECT_EQ(cmd.type, InputCommandType::Press);
    EXPECT_EQ(cmd.keyCode, KEY_SPACE);
}

TEST(InputScriptParseLine, ParsesPressWithModifiers) {
    ScriptCommand cmd;
    std::string error;
    EXPECT_TRUE(parseScriptLine("press ctrl+S", 1, cmd, error));
    EXPECT_EQ(cmd.type, InputCommandType::Press);
    EXPECT_EQ(cmd.keyCode, KEY_S);
    EXPECT_EQ(cmd.modifiers, INPUT_SCRIPT_MOD_CTRL);
}

TEST(InputScriptParseLine, ParsesPressCtrlShift) {
    ScriptCommand cmd;
    std::string error;
    EXPECT_TRUE(parseScriptLine("press ctrl+shift+Z", 1, cmd, error));
    EXPECT_EQ(cmd.type, InputCommandType::Press);
    EXPECT_EQ(cmd.keyCode, KEY_Z);
    EXPECT_EQ(cmd.modifiers, INPUT_SCRIPT_MOD_CTRL | INPUT_SCRIPT_MOD_SHIFT);
}

TEST(InputScriptParseLine, ParsesKeyDown) {
    ScriptCommand cmd;
    std::string error;
    EXPECT_TRUE(parseScriptLine("keydown W", 1, cmd, error));
    EXPECT_EQ(cmd.type, InputCommandType::KeyDown);
    EXPECT_EQ(cmd.keyCode, KEY_W);
}

TEST(InputScriptParseLine, ParsesKeyUp) {
    ScriptCommand cmd;
    std::string error;
    EXPECT_TRUE(parseScriptLine("keyup W", 1, cmd, error));
    EXPECT_EQ(cmd.type, InputCommandType::KeyUp);
    EXPECT_EQ(cmd.keyCode, KEY_W);
}

TEST(InputScriptParseLine, ParsesKeyDownWithModifiers) {
    ScriptCommand cmd;
    std::string error;
    EXPECT_TRUE(parseScriptLine("keydown shift+W", 1, cmd, error));
    EXPECT_EQ(cmd.type, InputCommandType::KeyDown);
    EXPECT_EQ(cmd.keyCode, KEY_W);
    EXPECT_EQ(cmd.modifiers, INPUT_SCRIPT_MOD_SHIFT);
}

// ============================================================================
// Line parser tests — Mouse
// ============================================================================

TEST(InputScriptParseLine, ParsesClick) {
    ScriptCommand cmd;
    std::string error;
    EXPECT_TRUE(parseScriptLine("click 400 300", 1, cmd, error));
    EXPECT_EQ(cmd.type, InputCommandType::Click);
    EXPECT_DOUBLE_EQ(cmd.mouseX, 400.0);
    EXPECT_DOUBLE_EQ(cmd.mouseY, 300.0);
}

TEST(InputScriptParseLine, ParsesClickRight) {
    ScriptCommand cmd;
    std::string error;
    EXPECT_TRUE(parseScriptLine("click right 400 300", 1, cmd, error));
    EXPECT_EQ(cmd.type, InputCommandType::ClickRight);
    EXPECT_DOUBLE_EQ(cmd.mouseX, 400.0);
    EXPECT_DOUBLE_EQ(cmd.mouseY, 300.0);
}

TEST(InputScriptParseLine, ParsesMouseDown) {
    ScriptCommand cmd;
    std::string error;
    EXPECT_TRUE(parseScriptLine("mousedown 100 200", 1, cmd, error));
    EXPECT_EQ(cmd.type, InputCommandType::MouseDown);
    EXPECT_DOUBLE_EQ(cmd.mouseX, 100.0);
    EXPECT_DOUBLE_EQ(cmd.mouseY, 200.0);
}

TEST(InputScriptParseLine, ParsesMouseUp) {
    ScriptCommand cmd;
    std::string error;
    EXPECT_TRUE(parseScriptLine("mouseup 500 300", 1, cmd, error));
    EXPECT_EQ(cmd.type, InputCommandType::MouseUp);
    EXPECT_DOUBLE_EQ(cmd.mouseX, 500.0);
    EXPECT_DOUBLE_EQ(cmd.mouseY, 300.0);
}

TEST(InputScriptParseLine, ParsesMouseMove) {
    ScriptCommand cmd;
    std::string error;
    EXPECT_TRUE(parseScriptLine("mousemove 640 360", 1, cmd, error));
    EXPECT_EQ(cmd.type, InputCommandType::MouseMove);
    EXPECT_DOUBLE_EQ(cmd.mouseX, 640.0);
    EXPECT_DOUBLE_EQ(cmd.mouseY, 360.0);
}

TEST(InputScriptParseLine, ParsesScroll) {
    ScriptCommand cmd;
    std::string error;
    EXPECT_TRUE(parseScriptLine("scroll 400 300 -3", 1, cmd, error));
    EXPECT_EQ(cmd.type, InputCommandType::Scroll);
    EXPECT_DOUBLE_EQ(cmd.mouseX, 400.0);
    EXPECT_DOUBLE_EQ(cmd.mouseY, 300.0);
    EXPECT_DOUBLE_EQ(cmd.scrollDelta, -3.0);
}

TEST(InputScriptParseLine, ParsesScrollPositive) {
    ScriptCommand cmd;
    std::string error;
    EXPECT_TRUE(parseScriptLine("scroll 100 200 5", 1, cmd, error));
    EXPECT_EQ(cmd.type, InputCommandType::Scroll);
    EXPECT_DOUBLE_EQ(cmd.scrollDelta, 5.0);
}

TEST(InputScriptParseLine, InvalidMouseCoordsReportsError) {
    ScriptCommand cmd;
    std::string error;
    EXPECT_FALSE(parseScriptLine("click abc def", 5, cmd, error));
    EXPECT_FALSE(error.empty());
}

// ============================================================================
// Line parser tests — Loops
// ============================================================================

TEST(InputScriptParseLine, ParsesLabel) {
    ScriptCommand cmd;
    std::string error;
    EXPECT_TRUE(parseScriptLine("label my_loop", 1, cmd, error));
    EXPECT_EQ(cmd.type, InputCommandType::Label);
    EXPECT_EQ(cmd.argument, "my_loop");
}

TEST(InputScriptParseLine, ParsesLoop) {
    ScriptCommand cmd;
    std::string error;
    EXPECT_TRUE(parseScriptLine("loop my_loop 5", 1, cmd, error));
    EXPECT_EQ(cmd.type, InputCommandType::Loop);
    EXPECT_EQ(cmd.argument, "my_loop");
    EXPECT_EQ(cmd.loopCount, 5);
}

TEST(InputScriptParseLine, ParsesLoopInfinite) {
    ScriptCommand cmd;
    std::string error;
    EXPECT_TRUE(parseScriptLine("loop my_loop 0", 1, cmd, error));
    EXPECT_EQ(cmd.type, InputCommandType::Loop);
    EXPECT_EQ(cmd.argument, "my_loop");
    EXPECT_EQ(cmd.loopCount, 0);
}

// ============================================================================
// Line parser tests — Control
// ============================================================================

TEST(InputScriptParseLine, ParsesExit) {
    ScriptCommand cmd;
    std::string error;
    EXPECT_TRUE(parseScriptLine("exit", 1, cmd, error));
    EXPECT_EQ(cmd.type, InputCommandType::Exit);
}

TEST(InputScriptParseLine, ParsesQuit) {
    ScriptCommand cmd;
    std::string error;
    EXPECT_TRUE(parseScriptLine("quit", 1, cmd, error));
    EXPECT_EQ(cmd.type, InputCommandType::Exit);
}

TEST(InputScriptParseLine, ParsesScreenshot) {
    ScriptCommand cmd;
    std::string error;
    EXPECT_TRUE(parseScriptLine("screenshot output.png", 1, cmd, error));
    EXPECT_EQ(cmd.type, InputCommandType::Screenshot);
    EXPECT_EQ(cmd.argument, "output.png");
}

// ============================================================================
// Line parser tests — Syntax
// ============================================================================

TEST(InputScriptParseLine, CaseInsensitiveVerb) {
    ScriptCommand cmd;
    std::string error;
    EXPECT_TRUE(parseScriptLine("PRESS A", 1, cmd, error));
    EXPECT_EQ(cmd.type, InputCommandType::Press);

    EXPECT_TRUE(parseScriptLine("Wait 100", 2, cmd, error));
    EXPECT_EQ(cmd.type, InputCommandType::WaitMs);

    EXPECT_TRUE(parseScriptLine("EXIT", 3, cmd, error));
    EXPECT_EQ(cmd.type, InputCommandType::Exit);
}

TEST(InputScriptParseLine, InvalidVerbReportsError) {
    ScriptCommand cmd;
    std::string error;
    EXPECT_FALSE(parseScriptLine("foobar", 5, cmd, error));
    EXPECT_FALSE(error.empty());
    // Error should mention line number
    EXPECT_NE(error.find("5"), std::string::npos);
}

TEST(InputScriptParseLine, MissingArgsReportsError) {
    ScriptCommand cmd;
    std::string error;

    EXPECT_FALSE(parseScriptLine("wait", 1, cmd, error));
    EXPECT_FALSE(parseScriptLine("press", 2, cmd, error));
    EXPECT_FALSE(parseScriptLine("click", 3, cmd, error));
    EXPECT_FALSE(parseScriptLine("label", 4, cmd, error));
    EXPECT_FALSE(parseScriptLine("loop", 5, cmd, error));
    EXPECT_FALSE(parseScriptLine("screenshot", 6, cmd, error));
}

// ============================================================================
// File parser tests
// ============================================================================

class InputScriptFileTest : public ::testing::Test {
  protected:
    std::string m_tempFile;

    void SetUp() override { m_tempFile = "test_script_temp.vdescript"; }

    void TearDown() override { std::remove(m_tempFile.c_str()); }

    void writeScript(const std::string& content) {
        std::ofstream f(m_tempFile);
        f << content;
        f.close();
    }
};

TEST_F(InputScriptFileTest, ParsesSimpleScript) {
    writeScript("wait startup\n"
                "wait 500\n"
                "press A\n"
                "exit\n");

    std::vector<ScriptCommand> commands;
    std::unordered_map<std::string, LabelState> labels;
    std::string error;

    EXPECT_TRUE(parseInputScript(m_tempFile, commands, labels, error)) << error;
    EXPECT_EQ(commands.size(), 4u);
    EXPECT_EQ(commands[0].type, InputCommandType::WaitStartup);
    EXPECT_EQ(commands[1].type, InputCommandType::WaitMs);
    EXPECT_EQ(commands[2].type, InputCommandType::Press);
    EXPECT_EQ(commands[3].type, InputCommandType::Exit);
}

TEST_F(InputScriptFileTest, IgnoresComments) {
    writeScript("# This is a comment\n"
                "wait startup\n"
                "// Another comment\n"
                "exit\n");

    std::vector<ScriptCommand> commands;
    std::unordered_map<std::string, LabelState> labels;
    std::string error;

    EXPECT_TRUE(parseInputScript(m_tempFile, commands, labels, error)) << error;
    EXPECT_EQ(commands.size(), 2u);
}

TEST_F(InputScriptFileTest, IgnoresBlankLines) {
    writeScript("\n"
                "wait startup\n"
                "\n"
                "\n"
                "exit\n"
                "\n");

    std::vector<ScriptCommand> commands;
    std::unordered_map<std::string, LabelState> labels;
    std::string error;

    EXPECT_TRUE(parseInputScript(m_tempFile, commands, labels, error)) << error;
    EXPECT_EQ(commands.size(), 2u);
}

TEST_F(InputScriptFileTest, TracksLabels) {
    writeScript("label my_loop\n"
                "press A\n"
                "wait 100\n"
                "loop my_loop 3\n");

    std::vector<ScriptCommand> commands;
    std::unordered_map<std::string, LabelState> labels;
    std::string error;

    EXPECT_TRUE(parseInputScript(m_tempFile, commands, labels, error)) << error;
    EXPECT_EQ(commands.size(), 4u);
    EXPECT_EQ(labels.count("my_loop"), 1u);
    EXPECT_EQ(labels["my_loop"].commandIndex, 0u);
}

TEST_F(InputScriptFileTest, UndefinedLabelReportsError) {
    writeScript("press A\n"
                "loop undefined_label 3\n");

    std::vector<ScriptCommand> commands;
    std::unordered_map<std::string, LabelState> labels;
    std::string error;

    EXPECT_FALSE(parseInputScript(m_tempFile, commands, labels, error));
    EXPECT_FALSE(error.empty());
    EXPECT_NE(error.find("undefined_label"), std::string::npos);
}

TEST_F(InputScriptFileTest, DuplicateLabelReportsError) {
    writeScript("label test\n"
                "press A\n"
                "label test\n"
                "press B\n");

    std::vector<ScriptCommand> commands;
    std::unordered_map<std::string, LabelState> labels;
    std::string error;

    EXPECT_FALSE(parseInputScript(m_tempFile, commands, labels, error));
    EXPECT_FALSE(error.empty());
    EXPECT_NE(error.find("duplicate"), std::string::npos);
}

TEST_F(InputScriptFileTest, FileNotFoundReportsError) {
    std::vector<ScriptCommand> commands;
    std::unordered_map<std::string, LabelState> labels;
    std::string error;

    EXPECT_FALSE(parseInputScript("nonexistent.vdescript", commands, labels, error));
    EXPECT_FALSE(error.empty());
    EXPECT_NE(error.find("unable to open"), std::string::npos);
}

TEST_F(InputScriptFileTest, InvalidCommandReportsLineNumber) {
    writeScript("wait startup\n"
                "press A\n"
                "foobar\n"
                "exit\n");

    std::vector<ScriptCommand> commands;
    std::unordered_map<std::string, LabelState> labels;
    std::string error;

    EXPECT_FALSE(parseInputScript(m_tempFile, commands, labels, error));
    EXPECT_NE(error.find("line 3"), std::string::npos);
    EXPECT_NE(error.find("foobar"), std::string::npos);
}

TEST_F(InputScriptFileTest, ParsesComplexScript) {
    writeScript("# Complex test script\n"
                "wait startup\n"
                "wait 500\n"
                "\n"
                "# Click and interact\n"
                "click 640 360\n"
                "wait 200\n"
                "press ctrl+S\n"
                "wait 500\n"
                "\n"
                "# Drag sequence\n"
                "mousedown 100 100\n"
                "wait 100\n"
                "mousemove 300 300\n"
                "wait 100\n"
                "mouseup 300 300\n"
                "wait 200\n"
                "\n"
                "# Repeat keys\n"
                "label key_loop\n"
                "press A\n"
                "wait 200\n"
                "press B\n"
                "wait 200\n"
                "loop key_loop 3\n"
                "\n"
                "wait 1000\n"
                "exit\n");

    std::vector<ScriptCommand> commands;
    std::unordered_map<std::string, LabelState> labels;
    std::string error;

    EXPECT_TRUE(parseInputScript(m_tempFile, commands, labels, error)) << error;
    EXPECT_EQ(commands.size(), 20u);
    EXPECT_EQ(labels.count("key_loop"), 1u);
}

TEST_F(InputScriptFileTest, ParsesNestedLoops) {
    writeScript("label outer\n"
                "  label inner\n"
                "  press A\n"
                "  wait 100\n"
                "  loop inner 3\n"
                "press B\n"
                "wait 200\n"
                "loop outer 2\n");

    std::vector<ScriptCommand> commands;
    std::unordered_map<std::string, LabelState> labels;
    std::string error;

    EXPECT_TRUE(parseInputScript(m_tempFile, commands, labels, error)) << error;
    EXPECT_EQ(labels.count("outer"), 1u);
    EXPECT_EQ(labels.count("inner"), 1u);
}

TEST_F(InputScriptFileTest, ParsesModifierKeys) {
    writeScript("press ctrl+A\n"
                "press shift+B\n"
                "press alt+F4\n"
                "press ctrl+shift+Z\n"
                "keydown shift+W\n"
                "keyup shift+W\n");

    std::vector<ScriptCommand> commands;
    std::unordered_map<std::string, LabelState> labels;
    std::string error;

    EXPECT_TRUE(parseInputScript(m_tempFile, commands, labels, error)) << error;
    EXPECT_EQ(commands.size(), 6u);

    EXPECT_EQ(commands[0].modifiers, INPUT_SCRIPT_MOD_CTRL);
    EXPECT_EQ(commands[1].modifiers, INPUT_SCRIPT_MOD_SHIFT);
    EXPECT_EQ(commands[2].modifiers, INPUT_SCRIPT_MOD_ALT);
    EXPECT_EQ(commands[3].modifiers, INPUT_SCRIPT_MOD_CTRL | INPUT_SCRIPT_MOD_SHIFT);
    EXPECT_EQ(commands[4].modifiers, INPUT_SCRIPT_MOD_SHIFT);
    EXPECT_EQ(commands[5].modifiers, INPUT_SCRIPT_MOD_SHIFT);
}

TEST_F(InputScriptFileTest, ParsesMouseCommands) {
    writeScript("click 400 300\n"
                "click right 400 300\n"
                "mousedown 100 200\n"
                "mouseup 500 300\n"
                "mousemove 640 360\n"
                "scroll 400 300 -3\n"
                "scroll 400 300 5\n");

    std::vector<ScriptCommand> commands;
    std::unordered_map<std::string, LabelState> labels;
    std::string error;

    EXPECT_TRUE(parseInputScript(m_tempFile, commands, labels, error)) << error;
    EXPECT_EQ(commands.size(), 7u);

    EXPECT_EQ(commands[0].type, InputCommandType::Click);
    EXPECT_EQ(commands[1].type, InputCommandType::ClickRight);
    EXPECT_EQ(commands[2].type, InputCommandType::MouseDown);
    EXPECT_EQ(commands[3].type, InputCommandType::MouseUp);
    EXPECT_EQ(commands[4].type, InputCommandType::MouseMove);
    EXPECT_EQ(commands[5].type, InputCommandType::Scroll);
    EXPECT_DOUBLE_EQ(commands[5].scrollDelta, -3.0);
    EXPECT_EQ(commands[6].type, InputCommandType::Scroll);
    EXPECT_DOUBLE_EQ(commands[6].scrollDelta, 5.0);
}

// ============================================================================
// CLI argument parsing tests
// ============================================================================

TEST(InputScriptCLI, ParsesInputScriptArg) {
    const char* argv[] = {"program", "--input-script", "test.vdescript"};
    std::string result = getInputScriptArg(3, const_cast<char**>(argv));
    EXPECT_EQ(result, "test.vdescript");
}

TEST(InputScriptCLI, ParsesInputScriptArgEquals) {
    const char* argv[] = {"program", "--input-script=test.vdescript"};
    std::string result = getInputScriptArg(2, const_cast<char**>(argv));
    EXPECT_EQ(result, "test.vdescript");
}

TEST(InputScriptCLI, ReturnsEmptyWhenNoArg) {
    const char* argv[] = {"program", "--other-flag"};
    std::string result = getInputScriptArg(2, const_cast<char**>(argv));
    EXPECT_EQ(result, "");
}

TEST(InputScriptCLI, ReturnsEmptyWhenNoArgs) {
    const char* argv[] = {"program"};
    std::string result = getInputScriptArg(1, const_cast<char**>(argv));
    EXPECT_EQ(result, "");
}

TEST(InputScriptCLI, ReturnsEmptyWhenArgMissesValue) {
    const char* argv[] = {"program", "--input-script"};
    std::string result = getInputScriptArg(2, const_cast<char**>(argv));
    EXPECT_EQ(result, "");
}

}  // namespace test
}  // namespace vde
