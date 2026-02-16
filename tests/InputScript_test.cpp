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
// Line parser tests — Print
// ============================================================================

TEST(InputScriptParseLine, ParsesPrintSimple) {
    ScriptCommand cmd;
    std::string error;
    EXPECT_TRUE(parseScriptLine("print Hello", 1, cmd, error));
    EXPECT_EQ(cmd.type, InputCommandType::Print);
    EXPECT_EQ(cmd.argument, "Hello");
}

TEST(InputScriptParseLine, ParsesPrintMultiWord) {
    ScriptCommand cmd;
    std::string error;
    EXPECT_TRUE(parseScriptLine("print Hello World", 1, cmd, error));
    EXPECT_EQ(cmd.type, InputCommandType::Print);
    EXPECT_EQ(cmd.argument, "Hello World");
}

TEST(InputScriptParseLine, ParsesPrintPreservesCase) {
    ScriptCommand cmd;
    std::string error;
    EXPECT_TRUE(parseScriptLine("print Starting Phase 2", 1, cmd, error));
    EXPECT_EQ(cmd.type, InputCommandType::Print);
    EXPECT_EQ(cmd.argument, "Starting Phase 2");
}

TEST(InputScriptParseLine, ParsesPrintCaseInsensitiveVerb) {
    ScriptCommand cmd;
    std::string error;
    EXPECT_TRUE(parseScriptLine("PRINT message text", 1, cmd, error));
    EXPECT_EQ(cmd.type, InputCommandType::Print);
    EXPECT_EQ(cmd.argument, "message text");
}

TEST(InputScriptParseLine, PrintMissingMessageReportsError) {
    ScriptCommand cmd;
    std::string error;
    EXPECT_FALSE(parseScriptLine("print", 3, cmd, error));
    EXPECT_FALSE(error.empty());
    EXPECT_NE(error.find("3"), std::string::npos);
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
    EXPECT_FALSE(parseScriptLine("print", 7, cmd, error));
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

TEST_F(InputScriptFileTest, ParsesScriptWithPrint) {
    writeScript("wait startup\n"
                "print Test started\n"
                "press A\n"
                "wait 100\n"
                "print Phase 2 beginning\n"
                "press B\n"
                "print All done!\n"
                "exit\n");

    std::vector<ScriptCommand> commands;
    std::unordered_map<std::string, LabelState> labels;
    std::string error;

    EXPECT_TRUE(parseInputScript(m_tempFile, commands, labels, error)) << error;
    EXPECT_EQ(commands.size(), 8u);
    EXPECT_EQ(commands[0].type, InputCommandType::WaitStartup);
    EXPECT_EQ(commands[1].type, InputCommandType::Print);
    EXPECT_EQ(commands[1].argument, "Test started");
    EXPECT_EQ(commands[4].type, InputCommandType::Print);
    EXPECT_EQ(commands[4].argument, "Phase 2 beginning");
    EXPECT_EQ(commands[6].type, InputCommandType::Print);
    EXPECT_EQ(commands[6].argument, "All done!");
    EXPECT_EQ(commands[7].type, InputCommandType::Exit);
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

// ============================================================================
// A3: wait_frames command tests
// ============================================================================

TEST(InputScriptParseLine, ParsesWaitFrames) {
    ScriptCommand cmd;
    std::string error;
    EXPECT_TRUE(parseScriptLine("wait_frames 10", 1, cmd, error));
    EXPECT_EQ(cmd.type, InputCommandType::WaitFrames);
    EXPECT_EQ(cmd.waitFrames, 10);
}

TEST(InputScriptParseLine, ParsesWaitFramesSingle) {
    ScriptCommand cmd;
    std::string error;
    EXPECT_TRUE(parseScriptLine("wait_frames 1", 1, cmd, error));
    EXPECT_EQ(cmd.type, InputCommandType::WaitFrames);
    EXPECT_EQ(cmd.waitFrames, 1);
}

TEST(InputScriptParseLine, WaitFramesZeroIsError) {
    ScriptCommand cmd;
    std::string error;
    EXPECT_FALSE(parseScriptLine("wait_frames 0", 1, cmd, error));
    EXPECT_FALSE(error.empty());
}

TEST(InputScriptParseLine, WaitFramesNegativeIsError) {
    ScriptCommand cmd;
    std::string error;
    EXPECT_FALSE(parseScriptLine("wait_frames -5", 1, cmd, error));
    EXPECT_FALSE(error.empty());
}

TEST(InputScriptParseLine, WaitFramesMissingArgIsError) {
    ScriptCommand cmd;
    std::string error;
    EXPECT_FALSE(parseScriptLine("wait_frames", 1, cmd, error));
    EXPECT_FALSE(error.empty());
}

TEST(InputScriptParseLine, WaitFramesInvalidArgIsError) {
    ScriptCommand cmd;
    std::string error;
    EXPECT_FALSE(parseScriptLine("wait_frames abc", 1, cmd, error));
    EXPECT_FALSE(error.empty());
}

// ============================================================================
// A1: assert command tests — rendered_scene_count
// ============================================================================

TEST(InputScriptParseLine, ParsesAssertSceneCountEq) {
    ScriptCommand cmd;
    std::string error;
    EXPECT_TRUE(parseScriptLine("assert rendered_scene_count == 4", 1, cmd, error)) << error;
    EXPECT_EQ(cmd.type, InputCommandType::AssertSceneCount);
    EXPECT_EQ(cmd.assertOp, CompareOp::Eq);
    EXPECT_DOUBLE_EQ(cmd.assertValue, 4.0);
}

TEST(InputScriptParseLine, ParsesAssertSceneCountGt) {
    ScriptCommand cmd;
    std::string error;
    EXPECT_TRUE(parseScriptLine("assert rendered_scene_count > 0", 1, cmd, error)) << error;
    EXPECT_EQ(cmd.type, InputCommandType::AssertSceneCount);
    EXPECT_EQ(cmd.assertOp, CompareOp::Gt);
    EXPECT_DOUBLE_EQ(cmd.assertValue, 0.0);
}

TEST(InputScriptParseLine, ParsesAssertSceneCountLe) {
    ScriptCommand cmd;
    std::string error;
    EXPECT_TRUE(parseScriptLine("assert rendered_scene_count <= 10", 1, cmd, error)) << error;
    EXPECT_EQ(cmd.type, InputCommandType::AssertSceneCount);
    EXPECT_EQ(cmd.assertOp, CompareOp::Le);
    EXPECT_DOUBLE_EQ(cmd.assertValue, 10.0);
}

TEST(InputScriptParseLine, ParsesAssertSceneCountNe) {
    ScriptCommand cmd;
    std::string error;
    EXPECT_TRUE(parseScriptLine("assert rendered_scene_count != 0", 1, cmd, error)) << error;
    EXPECT_EQ(cmd.type, InputCommandType::AssertSceneCount);
    EXPECT_EQ(cmd.assertOp, CompareOp::Ne);
    EXPECT_DOUBLE_EQ(cmd.assertValue, 0.0);
}

TEST(InputScriptParseLine, AssertSceneCountMissingOpIsError) {
    ScriptCommand cmd;
    std::string error;
    EXPECT_FALSE(parseScriptLine("assert rendered_scene_count", 3, cmd, error));
    EXPECT_FALSE(error.empty());
}

TEST(InputScriptParseLine, AssertSceneCountInvalidOpIsError) {
    ScriptCommand cmd;
    std::string error;
    EXPECT_FALSE(parseScriptLine("assert rendered_scene_count ~= 4", 3, cmd, error));
    EXPECT_FALSE(error.empty());
}

// ============================================================================
// A1: assert command tests — scene field checks
// ============================================================================

TEST(InputScriptParseLine, ParsesAssertSceneWasRendered) {
    ScriptCommand cmd;
    std::string error;
    EXPECT_TRUE(parseScriptLine("assert scene \"crystal\" was_rendered == true", 1, cmd, error))
        << error;
    EXPECT_EQ(cmd.type, InputCommandType::AssertScene);
    EXPECT_EQ(cmd.assertSceneName, "crystal");
    EXPECT_EQ(cmd.assertField, "was_rendered");
    EXPECT_EQ(cmd.assertOp, CompareOp::Eq);
    EXPECT_DOUBLE_EQ(cmd.assertValue, 1.0);  // true -> 1.0
}

TEST(InputScriptParseLine, ParsesAssertSceneDrawCalls) {
    ScriptCommand cmd;
    std::string error;
    EXPECT_TRUE(parseScriptLine("assert scene \"metropolis\" draw_calls > 0", 1, cmd, error))
        << error;
    EXPECT_EQ(cmd.type, InputCommandType::AssertScene);
    EXPECT_EQ(cmd.assertSceneName, "metropolis");
    EXPECT_EQ(cmd.assertField, "draw_calls");
    EXPECT_EQ(cmd.assertOp, CompareOp::Gt);
    EXPECT_DOUBLE_EQ(cmd.assertValue, 0.0);
}

TEST(InputScriptParseLine, ParsesAssertSceneEntitiesDrawn) {
    ScriptCommand cmd;
    std::string error;
    EXPECT_TRUE(parseScriptLine("assert scene \"nature\" entities_drawn >= 3", 1, cmd, error))
        << error;
    EXPECT_EQ(cmd.type, InputCommandType::AssertScene);
    EXPECT_EQ(cmd.assertSceneName, "nature");
    EXPECT_EQ(cmd.assertField, "entities_drawn");
    EXPECT_EQ(cmd.assertOp, CompareOp::Ge);
    EXPECT_DOUBLE_EQ(cmd.assertValue, 3.0);
}

TEST(InputScriptParseLine, ParsesAssertSceneViewportWidth) {
    ScriptCommand cmd;
    std::string error;
    EXPECT_TRUE(parseScriptLine("assert scene \"main\" viewport_width > 0", 1, cmd, error))
        << error;
    EXPECT_EQ(cmd.type, InputCommandType::AssertScene);
    EXPECT_EQ(cmd.assertSceneName, "main");
    EXPECT_EQ(cmd.assertField, "viewport_width");
    EXPECT_EQ(cmd.assertOp, CompareOp::Gt);
    EXPECT_DOUBLE_EQ(cmd.assertValue, 0.0);
}

TEST(InputScriptParseLine, ParsesAssertSceneNotBlank) {
    ScriptCommand cmd;
    std::string error;
    EXPECT_TRUE(parseScriptLine("assert scene \"cosmos\" not_blank", 1, cmd, error)) << error;
    EXPECT_EQ(cmd.type, InputCommandType::AssertScene);
    EXPECT_EQ(cmd.assertSceneName, "cosmos");
    EXPECT_EQ(cmd.assertField, "not_blank");
}

TEST(InputScriptParseLine, ParsesAssertSceneWasRenderedFalse) {
    ScriptCommand cmd;
    std::string error;
    EXPECT_TRUE(parseScriptLine("assert scene \"hidden\" was_rendered == false", 1, cmd, error))
        << error;
    EXPECT_EQ(cmd.type, InputCommandType::AssertScene);
    EXPECT_EQ(cmd.assertSceneName, "hidden");
    EXPECT_EQ(cmd.assertField, "was_rendered");
    EXPECT_EQ(cmd.assertOp, CompareOp::Eq);
    EXPECT_DOUBLE_EQ(cmd.assertValue, 0.0);  // false -> 0.0
}

TEST(InputScriptParseLine, AssertSceneMissingQuoteIsError) {
    ScriptCommand cmd;
    std::string error;
    EXPECT_FALSE(parseScriptLine("assert scene crystal was_rendered == true", 1, cmd, error));
    EXPECT_FALSE(error.empty());
}

TEST(InputScriptParseLine, AssertSceneUnterminatedQuoteIsError) {
    ScriptCommand cmd;
    std::string error;
    EXPECT_FALSE(parseScriptLine("assert scene \"crystal was_rendered == true", 1, cmd, error));
    EXPECT_FALSE(error.empty());
}

TEST(InputScriptParseLine, AssertSceneUnknownFieldIsError) {
    ScriptCommand cmd;
    std::string error;
    EXPECT_FALSE(parseScriptLine("assert scene \"x\" unknown_field == 0", 1, cmd, error));
    EXPECT_FALSE(error.empty());
    EXPECT_NE(error.find("unknown assert field"), std::string::npos);
}

TEST(InputScriptParseLine, AssertSceneMissingFieldIsError) {
    ScriptCommand cmd;
    std::string error;
    EXPECT_FALSE(parseScriptLine("assert scene \"x\"", 5, cmd, error));
    EXPECT_FALSE(error.empty());
}

TEST(InputScriptParseLine, AssertUnknownTypeIsError) {
    ScriptCommand cmd;
    std::string error;
    EXPECT_FALSE(parseScriptLine("assert foobar == 4", 5, cmd, error));
    EXPECT_FALSE(error.empty());
    EXPECT_NE(error.find("unknown assert type"), std::string::npos);
}

// ============================================================================
// A4: compare command tests
// ============================================================================

TEST(InputScriptParseLine, ParsesCompare) {
    ScriptCommand cmd;
    std::string error;
    EXPECT_TRUE(parseScriptLine("compare actual.png golden.png 0.02", 1, cmd, error)) << error;
    EXPECT_EQ(cmd.type, InputCommandType::Compare);
    EXPECT_EQ(cmd.argument, "actual.png");
    EXPECT_EQ(cmd.comparePath, "golden.png");
    EXPECT_DOUBLE_EQ(cmd.compareThreshold, 0.02);
}

TEST(InputScriptParseLine, ParsesCompareZeroThreshold) {
    ScriptCommand cmd;
    std::string error;
    EXPECT_TRUE(parseScriptLine("compare a.png b.png 0", 1, cmd, error)) << error;
    EXPECT_EQ(cmd.type, InputCommandType::Compare);
    EXPECT_DOUBLE_EQ(cmd.compareThreshold, 0.0);
}

TEST(InputScriptParseLine, ParsesCompareMaxThreshold) {
    ScriptCommand cmd;
    std::string error;
    EXPECT_TRUE(parseScriptLine("compare a.png b.png 1.0", 1, cmd, error)) << error;
    EXPECT_EQ(cmd.type, InputCommandType::Compare);
    EXPECT_DOUBLE_EQ(cmd.compareThreshold, 1.0);
}

TEST(InputScriptParseLine, CompareMissingArgsIsError) {
    ScriptCommand cmd;
    std::string error;
    EXPECT_FALSE(parseScriptLine("compare actual.png golden.png", 1, cmd, error));
    EXPECT_FALSE(error.empty());
}

TEST(InputScriptParseLine, CompareThresholdOutOfRangeIsError) {
    ScriptCommand cmd;
    std::string error;
    EXPECT_FALSE(parseScriptLine("compare a.png b.png 1.5", 1, cmd, error));
    EXPECT_FALSE(error.empty());
}

TEST(InputScriptParseLine, CompareNegativeThresholdIsError) {
    ScriptCommand cmd;
    std::string error;
    EXPECT_FALSE(parseScriptLine("compare a.png b.png -0.1", 1, cmd, error));
    EXPECT_FALSE(error.empty());
}

TEST(InputScriptParseLine, CompareInvalidThresholdIsError) {
    ScriptCommand cmd;
    std::string error;
    EXPECT_FALSE(parseScriptLine("compare a.png b.png abc", 1, cmd, error));
    EXPECT_FALSE(error.empty());
}

// ============================================================================
// A5: set command tests
// ============================================================================

TEST(InputScriptParseLine, ParsesSet) {
    ScriptCommand cmd;
    std::string error;
    EXPECT_TRUE(parseScriptLine("set VAR 42", 1, cmd, error)) << error;
    EXPECT_EQ(cmd.type, InputCommandType::Set);
    EXPECT_EQ(cmd.setVarName, "VAR");
    EXPECT_DOUBLE_EQ(cmd.setVarValue, 42.0);
}

TEST(InputScriptParseLine, ParsesSetFloat) {
    ScriptCommand cmd;
    std::string error;
    EXPECT_TRUE(parseScriptLine("set PI 3.14", 1, cmd, error)) << error;
    EXPECT_EQ(cmd.type, InputCommandType::Set);
    EXPECT_EQ(cmd.setVarName, "PI");
    EXPECT_NEAR(cmd.setVarValue, 3.14, 0.001);
}

TEST(InputScriptParseLine, ParsesSetNegative) {
    ScriptCommand cmd;
    std::string error;
    EXPECT_TRUE(parseScriptLine("set OFFSET -10", 1, cmd, error)) << error;
    EXPECT_EQ(cmd.type, InputCommandType::Set);
    EXPECT_EQ(cmd.setVarName, "OFFSET");
    EXPECT_DOUBLE_EQ(cmd.setVarValue, -10.0);
}

TEST(InputScriptParseLine, SetMissingArgsIsError) {
    ScriptCommand cmd;
    std::string error;
    EXPECT_FALSE(parseScriptLine("set VAR", 1, cmd, error));
    EXPECT_FALSE(error.empty());
}

TEST(InputScriptParseLine, SetInvalidValueIsError) {
    ScriptCommand cmd;
    std::string error;
    EXPECT_FALSE(parseScriptLine("set VAR abc", 1, cmd, error));
    EXPECT_FALSE(error.empty());
}

// ============================================================================
// CompareOp helper tests
// ============================================================================

TEST(InputScriptCompareOp, ParsesAllOps) {
    CompareOp op;
    std::string error;

    EXPECT_TRUE(parseCompareOp("==", op, error));
    EXPECT_EQ(op, CompareOp::Eq);

    EXPECT_TRUE(parseCompareOp("!=", op, error));
    EXPECT_EQ(op, CompareOp::Ne);

    EXPECT_TRUE(parseCompareOp("<", op, error));
    EXPECT_EQ(op, CompareOp::Lt);

    EXPECT_TRUE(parseCompareOp("<=", op, error));
    EXPECT_EQ(op, CompareOp::Le);

    EXPECT_TRUE(parseCompareOp(">", op, error));
    EXPECT_EQ(op, CompareOp::Gt);

    EXPECT_TRUE(parseCompareOp(">=", op, error));
    EXPECT_EQ(op, CompareOp::Ge);
}

TEST(InputScriptCompareOp, InvalidOpReportsError) {
    CompareOp op;
    std::string error;
    EXPECT_FALSE(parseCompareOp("~=", op, error));
    EXPECT_FALSE(error.empty());
}

TEST(InputScriptCompareOp, EvaluatesCorrectly) {
    EXPECT_TRUE(evaluateComparison(4.0, CompareOp::Eq, 4.0));
    EXPECT_FALSE(evaluateComparison(4.0, CompareOp::Eq, 5.0));

    EXPECT_TRUE(evaluateComparison(4.0, CompareOp::Ne, 5.0));
    EXPECT_FALSE(evaluateComparison(4.0, CompareOp::Ne, 4.0));

    EXPECT_TRUE(evaluateComparison(3.0, CompareOp::Lt, 4.0));
    EXPECT_FALSE(evaluateComparison(4.0, CompareOp::Lt, 4.0));

    EXPECT_TRUE(evaluateComparison(4.0, CompareOp::Le, 4.0));
    EXPECT_TRUE(evaluateComparison(3.0, CompareOp::Le, 4.0));
    EXPECT_FALSE(evaluateComparison(5.0, CompareOp::Le, 4.0));

    EXPECT_TRUE(evaluateComparison(5.0, CompareOp::Gt, 4.0));
    EXPECT_FALSE(evaluateComparison(4.0, CompareOp::Gt, 4.0));

    EXPECT_TRUE(evaluateComparison(4.0, CompareOp::Ge, 4.0));
    EXPECT_TRUE(evaluateComparison(5.0, CompareOp::Ge, 4.0));
    EXPECT_FALSE(evaluateComparison(3.0, CompareOp::Ge, 4.0));
}

TEST(InputScriptCompareOp, OpToString) {
    EXPECT_STREQ(compareOpToString(CompareOp::Eq), "==");
    EXPECT_STREQ(compareOpToString(CompareOp::Ne), "!=");
    EXPECT_STREQ(compareOpToString(CompareOp::Lt), "<");
    EXPECT_STREQ(compareOpToString(CompareOp::Le), "<=");
    EXPECT_STREQ(compareOpToString(CompareOp::Gt), ">");
    EXPECT_STREQ(compareOpToString(CompareOp::Ge), ">=");
}

// ============================================================================
// File parser tests — new commands
// ============================================================================

TEST_F(InputScriptFileTest, ParsesScriptWithWaitFrames) {
    writeScript("wait startup\n"
                "wait_frames 10\n"
                "print after 10 frames\n"
                "exit\n");

    std::vector<ScriptCommand> commands;
    std::unordered_map<std::string, LabelState> labels;
    std::string error;

    EXPECT_TRUE(parseInputScript(m_tempFile, commands, labels, error)) << error;
    EXPECT_EQ(commands.size(), 4u);
    EXPECT_EQ(commands[1].type, InputCommandType::WaitFrames);
    EXPECT_EQ(commands[1].waitFrames, 10);
}

TEST_F(InputScriptFileTest, ParsesScriptWithAsserts) {
    writeScript("wait startup\n"
                "wait_frames 5\n"
                "assert rendered_scene_count == 4\n"
                "assert scene \"crystal\" was_rendered == true\n"
                "assert scene \"crystal\" draw_calls > 0\n"
                "print PASS\n"
                "exit\n");

    std::vector<ScriptCommand> commands;
    std::unordered_map<std::string, LabelState> labels;
    std::string error;

    EXPECT_TRUE(parseInputScript(m_tempFile, commands, labels, error)) << error;
    EXPECT_EQ(commands.size(), 7u);
    EXPECT_EQ(commands[2].type, InputCommandType::AssertSceneCount);
    EXPECT_EQ(commands[3].type, InputCommandType::AssertScene);
    EXPECT_EQ(commands[3].assertSceneName, "crystal");
    EXPECT_EQ(commands[3].assertField, "was_rendered");
    EXPECT_EQ(commands[4].type, InputCommandType::AssertScene);
    EXPECT_EQ(commands[4].assertSceneName, "crystal");
    EXPECT_EQ(commands[4].assertField, "draw_calls");
}

TEST_F(InputScriptFileTest, ParsesScriptWithCompare) {
    writeScript("screenshot output.png\n"
                "compare output_frame_1.png golden.png 0.05\n"
                "exit\n");

    std::vector<ScriptCommand> commands;
    std::unordered_map<std::string, LabelState> labels;
    std::string error;

    EXPECT_TRUE(parseInputScript(m_tempFile, commands, labels, error)) << error;
    EXPECT_EQ(commands.size(), 3u);
    EXPECT_EQ(commands[1].type, InputCommandType::Compare);
    EXPECT_EQ(commands[1].argument, "output_frame_1.png");
    EXPECT_EQ(commands[1].comparePath, "golden.png");
    EXPECT_DOUBLE_EQ(commands[1].compareThreshold, 0.05);
}

TEST_F(InputScriptFileTest, ParsesScriptWithSet) {
    writeScript("set X 100\n"
                "set Y 200\n"
                "exit\n");

    std::vector<ScriptCommand> commands;
    std::unordered_map<std::string, LabelState> labels;
    std::string error;

    EXPECT_TRUE(parseInputScript(m_tempFile, commands, labels, error)) << error;
    EXPECT_EQ(commands.size(), 3u);
    EXPECT_EQ(commands[0].type, InputCommandType::Set);
    EXPECT_EQ(commands[0].setVarName, "X");
    EXPECT_DOUBLE_EQ(commands[0].setVarValue, 100.0);
    EXPECT_EQ(commands[1].type, InputCommandType::Set);
    EXPECT_EQ(commands[1].setVarName, "Y");
    EXPECT_DOUBLE_EQ(commands[1].setVarValue, 200.0);
}

TEST_F(InputScriptFileTest, ParsesFullValidationScript) {
    writeScript("# Full validation script test\n"
                "wait startup\n"
                "wait_frames 10\n"
                "\n"
                "# Scene count check\n"
                "assert rendered_scene_count == 4\n"
                "\n"
                "# Per-scene checks\n"
                "assert scene \"crystal\" was_rendered == true\n"
                "assert scene \"crystal\" draw_calls > 0\n"
                "assert scene \"metropolis\" was_rendered == true\n"
                "assert scene \"nature\" entities_drawn >= 3\n"
                "assert scene \"cosmos\" not_blank\n"
                "\n"
                "set THRESHOLD 0.02\n"
                "print PASS: all assertions passed\n"
                "exit\n");

    std::vector<ScriptCommand> commands;
    std::unordered_map<std::string, LabelState> labels;
    std::string error;

    EXPECT_TRUE(parseInputScript(m_tempFile, commands, labels, error)) << error;
    EXPECT_EQ(commands.size(), 11u);
}

}  // namespace test
}  // namespace vde
