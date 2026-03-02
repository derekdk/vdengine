# VDE Smoke Tests

This directory contains automated smoke test resources for VDE examples and tools.

## Structure

```
smoketests/
├── scripts/               # Input automation scripts (.vdescript files)
│   ├── smoke_quick.vdescript          # Default fast smoke test (1s)
│   ├── smoke_test.vdescript           # Standard smoke test (3s)
│   ├── smoke_<name>.vdescript         # Example/tool-specific scripts
│   └── ...
└── verification_images/   # Reference images for visual regression testing
    └── (future use - screenshot comparison baselines)
```

## Smoke Test Scripts

Smoke test scripts (`.vdescript` files) automate input to verify that examples and tools launch, render, and exit cleanly. They use the VDE input scripting system to simulate user interaction without human intervention.

### Script Naming Convention

- `smoke_quick.vdescript` - Fast test for CI (1 second wait + exit)
- `smoke_test.vdescript` - Standard test (3 second wait + exit)
- `smoke_<example_name>.vdescript` - Custom script for specific example/tool

### Running Smoke Tests

See the **smoke-testing** skill in `.github/skills/smoke-testing/SKILL.md` for detailed usage.

Quick reference:
```powershell
# Run all smoke tests
.\scripts\smoke-test.ps1

# Run examples only
.\scripts\smoke-test.ps1 -Category Examples

# Run specific test
.\scripts\smoke-test.ps1 -Filter "*physics*"
```

## Verification Images

The `verification_images/` directory is reserved for future visual regression testing capabilities. When implemented, this will contain reference screenshots for automated visual comparison of rendered output.

## Adding New Smoke Tests

When creating a new example or tool:

1. Create a smoke script: `smoketests/scripts/smoke_<name>.vdescript`
2. Add the mapping in `scripts/smoke-test.ps1`:
   ```powershell
   $smokeScriptMap = @{
       ...
       'vde_my_new_tool.exe' = 'smoke_my_new_tool.vdescript'
   }
   ```

See the **writing-examples** or **writing-tools** skills for detailed guidance.
