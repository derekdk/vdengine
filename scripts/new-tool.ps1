# VDE New Tool Scaffold Script
# Creates the skeleton of a new tool: source files, CMakeLists.txt entry, vde.toml,
# smoke test script, and registers the tool in smoke-test.ps1.
#
# Usage:
#   .\scripts\new-tool.ps1 -Name my_tool
#   .\scripts\new-tool.ps1 -Name my_tool -Title "My Tool" -Sections entity,input

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true, HelpMessage = "Snake_case name for the tool (e.g. my_tool)")]
    [string]$Name,

    [Parameter(HelpMessage = "Human-readable title shown in the window and console header. Defaults to a prettified version of Name.")]
    [string]$Title = "",

    [Parameter(HelpMessage = "Comma-separated canonical smoke-test section identifiers (e.g. entity,input). Default: entity")]
    [string[]]$Sections = @("entity")
)

$ErrorActionPreference = "Stop"

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

function Write-Header { param([string]$msg) Write-Host "`n$msg" -ForegroundColor Cyan }
function Write-Ok     { param([string]$msg) Write-Host "  [+] $msg" -ForegroundColor Green }
function Write-Skip   { param([string]$msg) Write-Host "  [~] $msg" -ForegroundColor DarkGray }
function Write-Err    { param([string]$msg) Write-Host "  [!] $msg" -ForegroundColor Red }

# Resolve the repo root from the script's own location
$RepoRoot = Split-Path -Parent $PSScriptRoot

# ---------------------------------------------------------------------------
# Validation
# ---------------------------------------------------------------------------

if ($Name -notmatch '^[a-z][a-z0-9_]*$') {
    Write-Err "Name '$Name' is invalid. Use snake_case lowercase letters, digits, and underscores only (e.g. my_tool)."
    exit 1
}

$ToolDir = Join-Path $RepoRoot "tools\$Name"
if (Test-Path $ToolDir) {
    Write-Err "Tool directory already exists: $ToolDir"
    exit 1
}

# Derive display title from name if not supplied
if ([string]::IsNullOrWhiteSpace($Title)) {
    $Title = (Get-Culture).TextInfo.ToTitleCase($Name.Replace("_", " "))
}

# Derive PascalCase class prefix from snake_case name (e.g. my_tool -> MyTool)
$PascalName = ($Name -split '_' | ForEach-Object { (Get-Culture).TextInfo.ToTitleCase($_) }) -join ''

# Target executable name (VDE convention: vde_<name>)
$TargetName = "vde_$Name"

# Scene class name
$SceneClass = "${PascalName}Scene"

# Canonical sections list (joined for toml)
$SectionsToml = '["' + ($Sections -join '", "') + '"]'

Write-Host ""
Write-Host "=======================================" -ForegroundColor Green
Write-Host "   VDE Tool Scaffold" -ForegroundColor Green
Write-Host "=======================================" -ForegroundColor Green
Write-Host "  Name      : $Name"
Write-Host "  Title     : $Title"
Write-Host "  Target    : $TargetName"
Write-Host "  Class     : $PascalName"
Write-Host "  Sections  : $($Sections -join ', ')"

# ---------------------------------------------------------------------------
# 1. Create tool source directory and files
# ---------------------------------------------------------------------------

Write-Header "Creating tool source files..."

New-Item -ItemType Directory -Path $ToolDir -Force | Out-Null

# --- main.cpp ---

$MainCpp = @"
#include <vde/Window.h>

#include <iostream>
#include <string>

#include "${SceneClass}.h"

using namespace vde::tools;

// =============================================================================
// Tool Game class
// =============================================================================

class ${PascalName}Tool : public BaseToolGame<BaseToolInputHandler, ${SceneClass}> {
  public:
    ${PascalName}Tool(ToolMode mode, const std::string& scriptFile = "")
        : BaseToolGame(mode), m_scriptFile(scriptFile) {}

    void onStart() override {
        BaseToolGame::onStart();

        if (m_toolMode == ToolMode::SCRIPT && !m_scriptFile.empty()) {
            auto* scene = getToolScene();
            if (scene && !scene->processScriptFile(m_scriptFile)) {
                std::cerr << "Failed to process script file: " << m_scriptFile << "\n";
                m_exitCode = 1;
            }
            quit();
        }
    }

  private:
    std::string m_scriptFile;
};

// =============================================================================
// Main
// =============================================================================

int main(int argc, char** argv) {
    ToolMode mode = ToolMode::INTERACTIVE;
    std::string scriptFile;

    if (argc > 1 && std::string(argv[1]) != "--input-script") {
        scriptFile = argv[1];
        mode = ToolMode::SCRIPT;

        std::cout << "====================================================\n";
        std::cout << "VDE $Title - Script Mode\n";
        std::cout << "====================================================\n";
        std::cout << "Processing script: " << scriptFile << "\n";
        std::cout << "====================================================\n\n";
    }

    ${PascalName}Tool tool(mode, scriptFile);

    if (mode == ToolMode::INTERACTIVE) {
        float dpiScale = vde::Window::getPrimaryMonitorDPIScale();
        uint32_t width  = static_cast<uint32_t>(1400 * dpiScale);
        uint32_t height = static_cast<uint32_t>(800  * dpiScale);
        return runTool(tool, "VDE $Title", width, height, argc, argv);
    } else {
        vde::configureInputScriptFromArgs(tool, argc, argv);

        vde::GameSettings settings;
        settings.gameName = "VDE $Title (Script Mode)";
        settings.display.windowWidth  = 800;
        settings.display.windowHeight = 600;
        settings.debug.enableValidation = false;

        if (!tool.initialize(settings)) {
            std::cerr << "Failed to initialize tool\n";
            return 1;
        }

        tool.run();
        return tool.getExitCode();
    }
}
"@

Set-Content -Path (Join-Path $ToolDir "main.cpp") -Value $MainCpp -Encoding UTF8
Write-Ok "tools/$Name/main.cpp"

# --- Scene header ---

$SceneH = @"
#pragma once

#include <sstream>
#include <string>

#include "../ToolBase.h"

class ${SceneClass} : public vde::tools::BaseToolScene {
  public:
    explicit ${SceneClass}(vde::tools::ToolMode mode = vde::tools::ToolMode::INTERACTIVE);

    void onEnter() override;

    // REQUIRED: Execute a single command line typed by the user or read from script.
    void executeCommand(const std::string& cmdLine) override;

    // REQUIRED: Tool metadata
    std::string getToolName() const override;
    std::string getToolDescription() const override;

    // OPTIONAL: Custom ImGui UI panels
    void drawDebugUI() override;

  private:
    void cmdHelp();
    // TODO: add more command handler declarations here
};
"@

Set-Content -Path (Join-Path $ToolDir "${SceneClass}.h") -Value $SceneH -Encoding UTF8
Write-Ok "tools/$Name/${SceneClass}.h"

# --- Scene implementation ---

$SceneCpp = @"
#include "${SceneClass}.h"

#include <imgui.h>

#include <sstream>

using namespace vde::tools;

${SceneClass}::${SceneClass}(ToolMode mode) : BaseToolScene(mode) {}

void ${SceneClass}::onEnter() {
    // TODO: Set up camera, lighting, and initial scene objects.
    // Example:
    //   setCamera(new vde::OrbitCamera(vde::Position(0, 0, 0), 8.0f, 25.0f, 45.0f));

    addConsoleMessage("Welcome to VDE $Title");
    addConsoleMessage("Type 'help' for available commands.");
}

void ${SceneClass}::executeCommand(const std::string& cmdLine) {
    std::istringstream iss(cmdLine);
    std::string cmd;
    iss >> cmd;

    if (cmd == "help") {
        cmdHelp();
    } else {
        addConsoleMessage("Unknown command: " + cmd + "  (type 'help')");
    }
}

std::string ${SceneClass}::getToolName() const {
    return "$Title";
}

std::string ${SceneClass}::getToolDescription() const {
    return "TODO: one-line description of what this tool does";
}

void ${SceneClass}::drawDebugUI() {
    // TODO: Add custom ImGui windows/panels here.
    // See tools/geometry_repl/GeometryReplScene.cpp for a full console + property panel example.
}

// ---------------------------------------------------------------------------
// Private command handlers
// ---------------------------------------------------------------------------

void ${SceneClass}::cmdHelp() {
    addConsoleMessage("COMMANDS:");
    addConsoleMessage("  help - Show this message");
    // TODO: list additional commands
}
"@

Set-Content -Path (Join-Path $ToolDir "${SceneClass}.cpp") -Value $SceneCpp -Encoding UTF8
Write-Ok "tools/$Name/${SceneClass}.cpp"

# --- Per-tool CMakeLists.txt ---

$ToolCmake = @"
# $Title
#
# Interactive and scriptable tool for TODO: describe tool purpose.
#
# Usage:
#   $TargetName              - Interactive mode
#   $TargetName script.txt   - Script mode

add_executable($TargetName
    main.cpp
    ${SceneClass}.cpp
    ${SceneClass}.h
)

target_link_libraries($TargetName PRIVATE
    vde
    imgui_backend
)

target_compile_options($TargetName PRIVATE `${VDE_WARNING_FLAGS})

vde_add_shader_sync($TargetName)
"@

Set-Content -Path (Join-Path $ToolDir "CMakeLists.txt") -Value $ToolCmake -Encoding UTF8
Write-Ok "tools/$Name/CMakeLists.txt"

# --- vde.toml ---

$TomlContent = @"
[smoke]
scripts = ["smoke_${Name}.vdescript"]
sections = $SectionsToml
"@

Set-Content -Path (Join-Path $ToolDir "vde.toml") -Value $TomlContent -Encoding UTF8
Write-Ok "tools/$Name/vde.toml"

# --- README.md ---

$Readme = @"
# $Title

TODO: Describe what this tool does and what assets it produces.

## Usage

### Interactive mode

``````
$TargetName
``````

### Script mode

``````
$TargetName path/to/script.txt
``````

## Commands

| Command | Description |
|---------|-------------|
| help    | Show available commands |

## Controls

| Key | Action |
|-----|--------|
| ESC | Exit |
| F1  | Toggle UI |
| F11 | Toggle fullscreen |
| Mouse drag | Rotate camera |
| Mouse wheel | Zoom camera |
"@

Set-Content -Path (Join-Path $ToolDir "README.md") -Value $Readme -Encoding UTF8
Write-Ok "tools/$Name/README.md"

# ---------------------------------------------------------------------------
# 2. Create smoke test script
# ---------------------------------------------------------------------------

Write-Header "Creating smoke test script..."

$SmokeDir = Join-Path $RepoRoot "smoketests\scripts"
New-Item -ItemType Directory -Path $SmokeDir -Force | Out-Null

$SmokeScript = @"
# Smoke test: $Title
# Verify the tool starts and runs without crashing.
wait startup
wait 2s

# TODO: Add tool-specific interactions here, e.g.:
#   type help
#   wait 500

exit
"@

Set-Content -Path (Join-Path $SmokeDir "smoke_${Name}.vdescript") -Value $SmokeScript -Encoding UTF8
Write-Ok "smoketests/scripts/smoke_${Name}.vdescript"

# ---------------------------------------------------------------------------
# 3. Register in tools/CMakeLists.txt
# ---------------------------------------------------------------------------

Write-Header "Updating tools/CMakeLists.txt..."

$ToolsCmakePath = Join-Path $RepoRoot "tools\CMakeLists.txt"
$CmakeAppend    = "`nadd_subdirectory($Name)"

Add-Content -Path $ToolsCmakePath -Value $CmakeAppend -Encoding UTF8
Write-Ok "tools/CMakeLists.txt (appended add_subdirectory($Name))"

# ---------------------------------------------------------------------------
# 4. Register in smoke-test.ps1 tool map
# ---------------------------------------------------------------------------

Write-Header "Registering in scripts/smoke-test.ps1..."

$SmokeTestPath = Join-Path $RepoRoot "scripts\smoke-test.ps1"
$SmokeContent  = Get-Content $SmokeTestPath -Raw -Encoding UTF8

# Compute column-aligned entry (match existing tool map formatting)
$KeyStr     = "    '$TargetName.exe'"
$Padding    = ' ' * [Math]::Max(1, 44 - $KeyStr.Length)
$NewEntry   = "${KeyStr}${Padding}= 'smoke_${Name}.vdescript'"

# Insert new entry before the closing brace of the tool smoke map
$Anchor = '}'   # closing brace of $toolSmokeScriptMap
if ($SmokeContent.Contains('$toolSmokeScriptMap')) {
    # Find the toolSmokeScriptMap block and insert before its closing brace
    $BlockStart = $SmokeContent.IndexOf('$toolSmokeScriptMap')
    $BlockEnd   = $SmokeContent.IndexOf($Anchor, $BlockStart)
    if ($BlockEnd -ge 0) {
        $Updated = $SmokeContent.Substring(0, $BlockEnd) + $NewEntry + [System.Environment]::NewLine + $SmokeContent.Substring($BlockEnd)
        Set-Content -Path $SmokeTestPath -Value $Updated -Encoding UTF8 -NoNewline
        Write-Ok "scripts/smoke-test.ps1 (added $TargetName.exe to tool map)"
    } else {
        Write-Skip "scripts/smoke-test.ps1 - could not locate map closing brace; add manually:"
        Write-Skip "    $NewEntry"
    }
} else {
    Write-Skip "scripts/smoke-test.ps1 - toolSmokeScriptMap not found; add manually:"
    Write-Skip "    $NewEntry"
}

# ---------------------------------------------------------------------------
# Done
# ---------------------------------------------------------------------------

Write-Host ""
Write-Host "=======================================" -ForegroundColor Green
Write-Host "   Scaffold complete!" -ForegroundColor Green
Write-Host "=======================================" -ForegroundColor Green
Write-Host ""
Write-Host "Next steps:" -ForegroundColor Yellow
Write-Host "  1. Implement your tool in:" -ForegroundColor White
Write-Host "       tools/$Name/${SceneClass}.cpp" -ForegroundColor Cyan
Write-Host "       tools/$Name/${SceneClass}.h" -ForegroundColor Cyan
Write-Host "  2. Build and verify the tool:" -ForegroundColor White
Write-Host "       .\scripts\build.ps1" -ForegroundColor Cyan
Write-Host "  3. Run smoke tests:" -ForegroundColor White
Write-Host "       .\scripts\smoke-test.ps1" -ForegroundColor Cyan
Write-Host ""
