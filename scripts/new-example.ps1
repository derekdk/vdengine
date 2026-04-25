# VDE New Example Scaffold Script
# Creates the skeleton of a new example: source files, vde.toml, smoke/render test scripts,
# and a CMakeLists.txt entry.
#
# Usage:
#   .\scripts\new-example.ps1 -Name my_feature_demo
#   .\scripts\new-example.ps1 -Name my_feature_demo -Title "My Feature Demo" -Sections entity,input
#   .\scripts\new-example.ps1 -Name my_feature_demo -NoRenderVerify

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true, HelpMessage = "Snake_case name for the example (e.g. my_feature_demo)")]
    [string]$Name,

    [Parameter(HelpMessage = "Human-readable title shown in the window and console header. Defaults to a prettified version of Name.")]
    [string]$Title = "",

    [Parameter(HelpMessage = "Comma-separated canonical smoke-test section identifiers (e.g. entity,input,physics). Default: entity")]
    [string[]]$Sections = @("entity"),

    [Parameter(HelpMessage = "Skip generating render-verify (capture/verify) scripts and vde.toml render_verify block")]
    [switch]$NoRenderVerify
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
    Write-Err "Name '$Name' is invalid. Use snake_case lowercase letters, digits, and underscores only (e.g. my_feature_demo)."
    exit 1
}

$ExampleDir = Join-Path $RepoRoot "examples\$Name"
if (Test-Path $ExampleDir) {
    Write-Err "Example directory already exists: $ExampleDir"
    exit 1
}

# Derive display title from name if not supplied
if ([string]::IsNullOrWhiteSpace($Title)) {
    $Title = (Get-Culture).TextInfo.ToTitleCase($Name.Replace("_", " "))
}

# Target executable name (VDE convention: vde_<name>)
$TargetName = "vde_$Name"

# Golden image filename
$GoldenImage = "${Name}.png"

# Canonical sections list (joined for toml)
$SectionsToml = '["' + ($Sections -join '", "') + '"]'

Write-Host ""
Write-Host "=======================================" -ForegroundColor Green
Write-Host "   VDE Example Scaffold" -ForegroundColor Green
Write-Host "=======================================" -ForegroundColor Green
Write-Host "  Name    : $Name"
Write-Host "  Title   : $Title"
Write-Host "  Target  : $TargetName"
Write-Host "  Sections: $($Sections -join ', ')"
Write-Host "  Render  : $(!$NoRenderVerify)"

# ---------------------------------------------------------------------------
# 1. Create example source directory and main.cpp
# ---------------------------------------------------------------------------

Write-Header "Creating example source files..."

New-Item -ItemType Directory -Path $ExampleDir -Force | Out-Null

$MainCpp = @"
#include <vde/api/GameAPI.h>

#include <iostream>

#include "../ExampleBase.h"

// ---------------------------------------------------------------------------
// Input handler
// ---------------------------------------------------------------------------

class ${Name}_InputHandler : public vde::examples::BaseExampleInputHandler {
  public:
    // Add key bindings here if needed, for example:
    //   vde::KeyStateTracker keys;
    //   ${Name}_InputHandler() { keys.bindOneShot(vde::KEY_SPACE, "action"); }
    //   void onKeyPress(int key) override { BaseExampleInputHandler::onKeyPress(key); keys.handlePress(key); }
    //   void onKeyRelease(int key) override { keys.handleRelease(key); }
};

// ---------------------------------------------------------------------------
// Scene
// ---------------------------------------------------------------------------

class ${Name}_Scene : public vde::examples::BaseExampleScene {
  public:
    // Change the timeout (seconds) as appropriate for your demo.
    ${Name}_Scene() : BaseExampleScene(15.0f) {}

    void onEnter() override {
        printExampleHeader();

        // TODO: Set up camera, entities, lighting, etc.
        // Example:
        //   auto* camera = new vde::OrbitCamera(vde::Position(0, 0, 0), 8.0f, 25.0f, 45.0f);
        //   setCamera(camera);
        //   setBackgroundColor(vde::Color::fromHex(0x1a1a2e));
    }

    void update(float deltaTime) override {
        BaseExampleScene::update(deltaTime);  // handles ESC, F, auto-terminate

        // TODO: Add per-frame logic here.
        // Example input query:
        //   auto* input = dynamic_cast<${Name}_InputHandler*>(getInputHandler());
        //   if (!input) return;
        //   if (input->keys.consume("action")) { /* ... */ }
    }

  protected:
    std::string getExampleName() const override { return "$Title"; }

    std::vector<std::string> getFeatures() const override {
        return {
            "TODO: describe feature 1",
            "TODO: describe feature 2",
        };
    }

    std::vector<std::string> getExpectedVisuals() const override {
        return {
            "TODO: describe what should be visible on screen",
        };
    }

    std::vector<std::string> getControls() const override {
        return {
            "ESC - Exit",
            "F   - Report failure",
            // "SPACE - TODO: describe action",
        };
    }
};

// ---------------------------------------------------------------------------
// Game class
// ---------------------------------------------------------------------------

class ${Name}_Game : public vde::examples::BaseExampleGame<${Name}_InputHandler, ${Name}_Scene> {};

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

int main(int argc, char** argv) {
    ${Name}_Game demo;
    return vde::examples::runExample(demo, "VDE $Title", 1280, 720, argc, argv);
}
"@

Set-Content -Path (Join-Path $ExampleDir "main.cpp") -Value $MainCpp -Encoding UTF8
Write-Ok "examples/$Name/main.cpp"

# ---------------------------------------------------------------------------
# 2. Create vde.toml
# ---------------------------------------------------------------------------

if ($NoRenderVerify) {
    $TomlContent = @"
[smoke]
scripts = ["smoke_${Name}.vdescript"]
sections = $SectionsToml
"@
} else {
    $TomlContent = @"
[smoke]
scripts = ["smoke_${Name}.vdescript"]
sections = $SectionsToml

[render_verify]
scripts = ["verify_${Name}.vdescript"]
capture_script = "capture_${Name}.vdescript"
priority = 2
golden = "$GoldenImage"
threshold = 0.05
"@
}

Set-Content -Path (Join-Path $ExampleDir "vde.toml") -Value $TomlContent -Encoding UTF8
Write-Ok "examples/$Name/vde.toml"

# ---------------------------------------------------------------------------
# 3. Create smoke test script
# ---------------------------------------------------------------------------

$SmokeDir = Join-Path $RepoRoot "smoketests\scripts"
New-Item -ItemType Directory -Path $SmokeDir -Force | Out-Null

$SmokeScript = @"
# Smoke test: $Title
# Verify the example starts and runs without crashing.
wait startup
wait 2s

# TODO: Add interactions specific to this example, e.g.:
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
# 4. Create render-verify scripts (unless suppressed)
# ---------------------------------------------------------------------------

if (-not $NoRenderVerify) {
    $CaptureScript = @"
# Golden image capture: $Title
# Captures the initial visual state for render verification.
wait startup
wait_frames 20

screenshot render_verify_output/${GoldenImage}
wait 100

exit
"@

    $VerifyScript = @"
# Render verification: $Title
# Compares current output against the golden image.
wait startup
wait_frames 20

screenshot render_verify_output/${GoldenImage}
wait 100
compare render_verify_output/${GoldenImage} ../../smoketests/golden/${GoldenImage} 0.05

exit
"@

    Set-Content -Path (Join-Path $SmokeDir "capture_${Name}.vdescript")  -Value $CaptureScript  -Encoding UTF8
    Set-Content -Path (Join-Path $SmokeDir "verify_${Name}.vdescript")   -Value $VerifyScript   -Encoding UTF8
    Write-Ok "smoketests/scripts/capture_${Name}.vdescript"
    Write-Ok "smoketests/scripts/verify_${Name}.vdescript"
}

# ---------------------------------------------------------------------------
# 5. Append CMakeLists.txt entry
# ---------------------------------------------------------------------------

Write-Header "Updating examples/CMakeLists.txt..."

$CmakePath   = Join-Path $RepoRoot "examples\CMakeLists.txt"
$CmakeAppend = @"

# $Title example
add_vde_example($TargetName "${Name}/main.cpp")
"@

Add-Content -Path $CmakePath -Value $CmakeAppend -Encoding UTF8
Write-Ok "examples/CMakeLists.txt (appended $TargetName)"

# ---------------------------------------------------------------------------
# Done
# ---------------------------------------------------------------------------

Write-Host ""
Write-Host "=======================================" -ForegroundColor Green
Write-Host "   Scaffold complete!" -ForegroundColor Green
Write-Host "=======================================" -ForegroundColor Green
Write-Host ""
Write-Host "Next steps:" -ForegroundColor Yellow
Write-Host "  1. Implement your demo in:" -ForegroundColor White
Write-Host "       examples/$Name/main.cpp" -ForegroundColor Cyan
Write-Host "  2. Build and verify the example:" -ForegroundColor White
Write-Host "       .\scripts\build.ps1" -ForegroundColor Cyan
Write-Host "  3. Run smoke tests:" -ForegroundColor White
Write-Host "       .\scripts\smoke-test.ps1" -ForegroundColor Cyan
if (-not $NoRenderVerify) {
    Write-Host "  4. Capture a golden image once visuals look correct:" -ForegroundColor White
    Write-Host "       .\scripts\render-verify.ps1 -Capture $Name" -ForegroundColor Cyan
    Write-Host "  5. Run render verification:" -ForegroundColor White
    Write-Host "       .\scripts\render-verify.ps1" -ForegroundColor Cyan
}
Write-Host ""
