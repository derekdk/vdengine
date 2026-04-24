# VDE New Game Scaffold Script
# Creates the skeleton of a new game: source files, CMakeLists.txt entry, vde.toml,
# smoke test script, and README.
#
# Usage:
#   .\scripts\new-game.ps1 -Name my_game
#   .\scripts\new-game.ps1 -Name my_game -Title "My Game" -Sections entity,input,physics

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true, HelpMessage = "Snake_case name for the game (e.g. my_game)")]
    [string]$Name,

    [Parameter(HelpMessage = "Human-readable title shown in the window. Defaults to a prettified version of Name.")]
    [string]$Title = "",

    [Parameter(HelpMessage = "Comma-separated canonical smoke-test section identifiers (e.g. entity,input). Default: entity,input")]
    [string[]]$Sections = @("entity", "input")
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
    Write-Err "Name '$Name' is invalid. Use snake_case lowercase letters, digits, and underscores only (e.g. my_game)."
    exit 1
}

$GameDir = Join-Path $RepoRoot "games\$Name"
if (Test-Path $GameDir) {
    Write-Err "Game directory already exists: $GameDir"
    exit 1
}

# Derive display title from name if not supplied
if ([string]::IsNullOrWhiteSpace($Title)) {
    $Title = (Get-Culture).TextInfo.ToTitleCase($Name.Replace("_", " "))
}

# Derive PascalCase class prefix from snake_case name (e.g. my_game -> MyGame)
$PascalName = ($Name -split '_' | ForEach-Object { (Get-Culture).TextInfo.ToTitleCase($_) }) -join ''

# Derive a short namespace (lowercase, no underscores, e.g. mygame)
$Namespace = $Name -replace '_', ''

# Target executable name (VDE convention: vde_<name>)
$TargetName = "vde_$Name"

# Class names
$SceneClass = "${PascalName}Scene"
$InputClass = "${PascalName}Input"

# Canonical sections list (joined for toml)
$SectionsToml = '["' + ($Sections -join '", "') + '"]'

Write-Host ""
Write-Host "=======================================" -ForegroundColor Green
Write-Host "   VDE Game Scaffold" -ForegroundColor Green
Write-Host "=======================================" -ForegroundColor Green
Write-Host "  Name      : $Name"
Write-Host "  Title     : $Title"
Write-Host "  Target    : $TargetName"
Write-Host "  Namespace : $Namespace"
Write-Host "  Classes   : $InputClass, $SceneClass"
Write-Host "  Sections  : $($Sections -join ', ')"

# ---------------------------------------------------------------------------
# 1. Create game source directory and files
# ---------------------------------------------------------------------------

Write-Header "Creating game source files..."

New-Item -ItemType Directory -Path $GameDir -Force | Out-Null

# --- Input.h ---

$InputH = @"
#pragma once

#include "../GameBase.h"

namespace $Namespace {

class $InputClass : public vde::games::BaseGameInputHandler {
  public:
    $InputClass() {
        // Bind keys here. Examples:
        //   keys.bindHeld(vde::KEY_LEFT,  "left");
        //   keys.bindHeld(vde::KEY_RIGHT, "right");
        //   keys.bindOneShot(vde::KEY_SPACE, "action");
    }

    void onKeyPress(int key) override {
        BaseGameInputHandler::onKeyPress(key);
        keys.handlePress(key);
    }

    void onKeyRelease(int key) override { keys.handleRelease(key); }

    vde::KeyStateTracker keys;
};

}  // namespace $Namespace
"@

Set-Content -Path (Join-Path $GameDir "Input.h") -Value $InputH -Encoding UTF8
Write-Ok "games/$Name/Input.h"

# --- Scene header ---

$SceneH = @"
#pragma once

#include <string>
#include <vector>

#include "../GameBase.h"

namespace $Namespace {

class $InputClass;

class $SceneClass : public vde::games::BaseGameScene {
  public:
    $SceneClass();

    void onEnter() override;
    void update(float deltaTime) override;

    // Optional: uncomment if you add ImGui debug panels
    // void drawDebugUI() override;

  protected:
    std::string getGameName() const override;
    std::vector<std::string> getGameplaySummary() const override;
    std::vector<std::string> getGoals() const override;
    std::vector<std::string> getControls() const override;

  private:
    // TODO: Add member variables for game state here.
};

}  // namespace $Namespace
"@

Set-Content -Path (Join-Path $GameDir "${SceneClass}.h") -Value $SceneH -Encoding UTF8
Write-Ok "games/$Name/${SceneClass}.h"

# --- Scene implementation ---

$SceneCpp = @"
#include "${SceneClass}.h"

#include "Input.h"

namespace $Namespace {

$SceneClass::$SceneClass() = default;

void $SceneClass::onEnter() {
    printGameHeader();

    // TODO: Set up camera, entities, lighting, etc.
    // Examples:
    //   auto* camera = new vde::OrbitCamera(vde::Position(0, 0, 0), 8.0f, 25.0f, 45.0f);
    //   setCamera(camera);
    //   setBackgroundColor(vde::Color::fromHex(0x1a1a2e));
}

void $SceneClass::update(float deltaTime) {
    BaseGameScene::update(deltaTime);  // handles ESC, F1, F11

    auto* input = dynamic_cast<$InputClass*>(getInputHandler());
    if (!input) return;

    // TODO: Query input and update game state.
    // Example:
    //   if (input->keys.consume("action")) { /* ... */ }
}

std::string $SceneClass::getGameName() const { return "$Title"; }

std::vector<std::string> $SceneClass::getGameplaySummary() const {
    return {
        "TODO: describe how the game is played",
    };
}

std::vector<std::string> $SceneClass::getGoals() const {
    return {
        "TODO: describe the win/loss condition",
    };
}

std::vector<std::string> $SceneClass::getControls() const {
    return {
        "ESC - Exit",
        "F1  - Toggle UI",
        // "SPACE - TODO: describe action",
    };
}

}  // namespace $Namespace
"@

Set-Content -Path (Join-Path $GameDir "${SceneClass}.cpp") -Value $SceneCpp -Encoding UTF8
Write-Ok "games/$Name/${SceneClass}.cpp"

# --- main.cpp ---

$MainCpp = @"
#include "../GameBase.h"
#include "${SceneClass}.h"
#include "Input.h"

class ${PascalName}Game : public vde::games::BaseGame<${Namespace}::$InputClass, ${Namespace}::$SceneClass> {
  public:
    ${PascalName}Game() = default;
};

int main(int argc, char** argv) {
    ${PascalName}Game game;
    return vde::games::runGame(game, "VDE $Title", 1280, 720, argc, argv);
}
"@

Set-Content -Path (Join-Path $GameDir "main.cpp") -Value $MainCpp -Encoding UTF8
Write-Ok "games/$Name/main.cpp"

# --- Per-game CMakeLists.txt ---

$GameCmake = @"
add_vde_game($TargetName
    main.cpp
    ${SceneClass}.cpp
)
"@

Set-Content -Path (Join-Path $GameDir "CMakeLists.txt") -Value $GameCmake -Encoding UTF8
Write-Ok "games/$Name/CMakeLists.txt"

# --- vde.toml ---

$TomlContent = @"
[smoke]
scripts = ["smoke_${Name}.vdescript"]
priority = 2
sections = $SectionsToml
"@

Set-Content -Path (Join-Path $GameDir "vde.toml") -Value $TomlContent -Encoding UTF8
Write-Ok "games/$Name/vde.toml"

# --- README.md ---

$Readme = @"
# $Title

TODO: One-paragraph description of the game.

## How to play

TODO: Describe gameplay, objective, and controls.

## Controls

| Key | Action |
|-----|--------|
| ESC | Exit |
| F1  | Toggle debug UI |

## Building

``````
.\scripts\build.ps1
``````

## Running

``````
.\scripts\run-vlauncher.ps1
``````

Select **$Title** in the launcher, or run `$TargetName` directly from the build output directory.
"@

Set-Content -Path (Join-Path $GameDir "README.md") -Value $Readme -Encoding UTF8
Write-Ok "games/$Name/README.md"

# ---------------------------------------------------------------------------
# 2. Create smoke test script
# ---------------------------------------------------------------------------

Write-Header "Creating smoke test script..."

$SmokeDir = Join-Path $RepoRoot "smoketests\scripts"
New-Item -ItemType Directory -Path $SmokeDir -Force | Out-Null

$SmokeScript = @"
# Smoke test: $Title
# Verify the game starts and runs without crashing.
wait startup
wait 2s

# TODO: Add game-specific interactions here, e.g.:
#   press SPACE
#   wait 500
#   keydown LEFT
#   wait 1000
#   keyup LEFT

exit
"@

Set-Content -Path (Join-Path $SmokeDir "smoke_${Name}.vdescript") -Value $SmokeScript -Encoding UTF8
Write-Ok "smoketests/scripts/smoke_${Name}.vdescript"

# ---------------------------------------------------------------------------
# 3. Register in games/CMakeLists.txt
# ---------------------------------------------------------------------------

Write-Header "Updating games/CMakeLists.txt..."

$GamesCmakePath = Join-Path $RepoRoot "games\CMakeLists.txt"
$CmakeAppend    = "`nadd_subdirectory($Name)"

Add-Content -Path $GamesCmakePath -Value $CmakeAppend -Encoding UTF8
Write-Ok "games/CMakeLists.txt (appended add_subdirectory($Name))"

# ---------------------------------------------------------------------------
# Done
# ---------------------------------------------------------------------------

Write-Host ""
Write-Host "=======================================" -ForegroundColor Green
Write-Host "   Scaffold complete!" -ForegroundColor Green
Write-Host "=======================================" -ForegroundColor Green
Write-Host ""
Write-Host "Next steps:" -ForegroundColor Yellow
Write-Host "  1. Implement your game in:" -ForegroundColor White
Write-Host "       games/$Name/${SceneClass}.cpp" -ForegroundColor Cyan
Write-Host "       games/$Name/${SceneClass}.h" -ForegroundColor Cyan
Write-Host "       games/$Name/Input.h" -ForegroundColor Cyan
Write-Host "  2. Build and verify the game:" -ForegroundColor White
Write-Host "       .\scripts\build.ps1" -ForegroundColor Cyan
Write-Host "  3. Run smoke tests:" -ForegroundColor White
Write-Host "       .\scripts\smoke-test.ps1" -ForegroundColor Cyan
Write-Host ""
