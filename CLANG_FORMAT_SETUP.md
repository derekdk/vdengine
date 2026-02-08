# clang-format Setup Complete ✓

## What Was Configured

### 1. `.clang-format` Configuration File
Created at project root with VDE-specific formatting rules:
- **Indentation:** 4 spaces (matching project standard)
- **Column limit:** 100 characters
- **Braces:** Attach style (K&R style)
- **Naming:** Preserves VDE conventions (m_ prefix, camelCase, etc.)
- **Include ordering:** Matches VDE conventions (corresponding header → VDE headers → third-party → std)
- **Pointer alignment:** Left (`int* ptr` not `int *ptr`)

### 2. VSCode Settings (`.vscode/settings.json`)
Configured VSCode to use clang-format:
- Format-on-save enabled for C/C++ files
- Uses the `.clang-format` file automatically
- Tab size: 4 spaces
- Insert spaces instead of tabs

### 3. Format Script (`scripts/format.ps1`)
PowerShell script for batch formatting:
```powershell
.\scripts\format.ps1           # Format all C++ files
.\scripts\format.ps1 -Check    # Check formatting without changes
.\scripts\format.ps1 -Help     # Show detailed help
```

Formats all:
- `include/vde/**/*.h`
- `src/**/*.cpp`
- `examples/**/*.cpp`
- `tests/**/*.cpp`

### 4. Documentation Updates
- Updated `scripts/README.md` with format.ps1 documentation
- Updated main `README.md` with code formatting section
- Added formatting workflow examples

## How to Use

### In VSCode (Automatic)
- Files automatically format on save
- Manual format: Right-click → "Format Document" or `Alt+Shift+F`

### From Command Line
```powershell
# Format all code
.\scripts\format.ps1

# Check if formatting is needed (good for CI)
.\scripts\format.ps1 -Check
```

### Pre-Commit Workflow
```powershell
# Before committing
.\scripts\format.ps1 -Check

# If formatting needed
.\scripts\format.ps1

# Then commit
git add .
git commit -m "Your message"
```

## Requirements

**clang-format must be installed:**

**Option 1: Visual Studio (Recommended for Windows)**
1. Open Visual Studio Installer
2. Modify your installation
3. Select "C++ Clang tools for Windows"
4. Install

**Option 2: LLVM Distribution**
1. Download from https://releases.llvm.org/
2. Install and add to PATH
3. Verify: `clang-format --version`

**Check if installed:**
```powershell
Get-Command clang-format
```

## Configuration Details

The `.clang-format` file is based on VDE's coding conventions from `.github/skills/writing-code/SKILL.md`:

- **Namespaces:** vde::, vde::api::
- **Classes:** PascalCase
- **Functions:** camelCase
- **Members:** m_camelCase
- **Constants:** UPPER_SNAKE_CASE or kPascalCase
- **Include order:** Corresponding → VDE → Third-party → Standard

## Next Steps

1. **Install clang-format** (if not already installed)
2. **Test it:** `.\scripts\format.ps1 -Check`
3. **Consider:** Add to CI/CD pipeline to enforce formatting
4. **Optional:** Run format on existing code: `.\scripts\format.ps1`

## Troubleshooting

**"clang-format not found"**
- Install via Visual Studio or LLVM (see above)
- Ensure it's in your PATH
- Restart VSCode/terminal after installation

**VSCode not formatting on save**
- Check `.vscode/settings.json` exists
- Verify C/C++ extension is installed (ms-vscode.cpptools)
- Check "Format On Save" is enabled in VSCode settings

**Format script fails**
- Ensure you're in project root or scripts directory
- Check PowerShell execution policy: `Set-ExecutionPolicy RemoteSigned -Scope CurrentUser`

## Example Formatting

**Before:**
```cpp
class   MyClass {
    int   m_value;
public:
void   myMethod(  int x  , int  y){
if(x>0){
std::cout<<"Hello"<<std::endl;
}
}
};
```

**After:**
```cpp
class MyClass {
    int m_value;

public:
    void myMethod(int x, int y) {
        if (x > 0) {
            std::cout << "Hello" << std::endl;
        }
    }
};
```

---

For more information, see:
- [.clang-format](.clang-format) - Configuration file
- [scripts/README.md](scripts/README.md) - Script documentation
- [.github/skills/writing-code/SKILL.md](.github/skills/writing-code/SKILL.md) - Coding conventions
