# VDE Smoke Tests

This directory contains automated smoke test resources for VDE examples, games, and tools.

## Structure

```
smoketests/
├── scripts/               # Input automation scripts (.vdescript files)
│   ├── smoke_quick.vdescript          # Default fast smoke test (1s)
│   ├── smoke_test.vdescript           # Standard smoke test (3s)
│   ├── smoke_<name>.vdescript         # Example/game/tool-specific scripts
│   └── ...
└── verification_images/   # Reference images for visual regression testing
    └── (future use - screenshot comparison baselines)
```

## Smoke Test Scripts

Smoke test scripts (`.vdescript` files) automate input to verify that examples, games, and tools launch, render, and exit cleanly. They use the VDE input scripting system to simulate user interaction without human intervention.

### Script Naming Convention

- `smoke_quick.vdescript` - Fast test for CI (1 second wait + exit)
- `smoke_test.vdescript` - Standard test (3 second wait + exit)
- `smoke_<name>.vdescript` - Custom script for a specific example, game, or tool

### Running Smoke Tests

See the **smoke-testing** skill in `.github/skills/smoke-testing/SKILL.md` for detailed usage.

Quick reference:
```powershell
# Run all smoke tests
.\scripts\smoke-test.ps1

# Run examples only
.\scripts\smoke-test.ps1 -Category Examples

# Run games only
.\scripts\smoke-test.ps1 -Category Games

# Run specific test
.\scripts\smoke-test.ps1 -Filter "*physics*"
```

## Verification Images

The `verification_images/` directory is reserved for future visual regression testing capabilities. When implemented, this will contain reference screenshots for automated visual comparison of rendered output.

## Adding New Smoke Tests

When creating a new example, game, or tool:

1. Create a smoke script: `smoketests/scripts/smoke_<name>.vdescript`
2. Add metadata or a mapping in `scripts/smoke-test.ps1`:
   ```powershell
   $smokeScriptMap = @{
       ...
       'vde_my_new_tool.exe' = 'smoke_my_new_tool.vdescript'
   }
   ```

Examples and games should prefer `vde.toml` smoke metadata next to the source. Tools still use an explicit mapping.

See the **writing-examples**, **writing-games**, or **writing-tools** skills for detailed guidance.
