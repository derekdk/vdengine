---
name: writing-code
description: Comprehensive guide for writing code in the VDE engine, including coding conventions, file organization, CMake integration, and best practices.
---

# Writing Code for VDE

This skill covers all aspects of writing code for the Vulkan Display Engine, from coding conventions to CMake integration.

## When to use this skill

- Adding new classes or functions to the engine
- Understanding project coding standards
- Adding source files and updating CMakeLists.txt
- Organizing headers and implementation files
- Writing API documentation
- Following include patterns

## Coding Conventions

### Naming Conventions

**Namespaces:**
- All code must be in the `vde::` namespace
- Nested namespaces for API categories: `vde::api::`

**Classes and Types:**
- PascalCase for classes, structs, and enums
- Examples: `Window`, `VulkanContext`, `GameCamera`, `EntityHandle`

**Functions and Methods:**
- camelCase for functions and methods
- Examples: `shouldClose()`, `pollEvents()`, `createEntity()`

**Constants:**
- UPPER_SNAKE_CASE for global constants
- kPrefix for class-level constants
- Examples: `MAX_FRAMES_IN_FLIGHT`, `kDefaultResolution`

**Member Variables:**
- `m_` prefix for private/protected members
- camelCase after prefix
- Examples: `m_width`, `m_window`, `m_vulkanContext`

**Parameters and Local Variables:**
- camelCase without prefix
- Examples: `width`, `height`, `entityHandle`

### File Organization

**Public Headers:**
- Location: `include/vde/` for core engine, `include/vde/api/` for game API
- One class per header file
- Filename matches class name: `Window.h`, `GameCamera.h`
- Use `#pragma once` for header guards
- Must include complete documentation

**Implementation Files:**
- Location: `src/` for core engine, `src/api/` for game API
- One implementation per source file
- Filename matches header: `Window.cpp`, `GameCamera.cpp`
- Include corresponding header first

**Example Structure:**
```
include/vde/
    Core.h              # Core types and macros
    Window.h            # Public header
    VulkanContext.h
    api/
        Game.h
        Entity.h
        
src/
    Window.cpp          # Implementation
    VulkanContext.cpp
    api/
        Game.cpp
        Entity.cpp
```

### Include Order

In `.cpp` files, organize includes in this exact order:

1. **Corresponding header** (e.g., `#include <vde/Window.h>` in `Window.cpp`)
2. **Other VDE headers** (`#include <vde/...>`)
3. **Third-party headers** (Vulkan, GLFW, GLM, etc.)
4. **Standard library** (`<vector>`, `<memory>`, etc.)

**Example:**
```cpp
#include <vde/Window.h>          // 1. Corresponding header

#include <vde/VulkanContext.h>   // 2. Other vde headers
#include <vde/api/Entity.h>

#include <GLFW/glfw3.h>          // 3. Third-party
#include <glm/glm.hpp>

#include <stdexcept>             // 4. Standard library
#include <vector>
```

In `.h` files:
- Include only what's needed in the header
- Forward declare when possible
- Use `<vde/...>` for all VDE includes

### Documentation

Use Doxygen-style comments for all public API:

**File Headers:**
```cpp
#pragma once

/**
 * @file Window.h
 * @brief Window management for Vulkan Display Engine
 * 
 * Provides platform-independent window creation and input handling
 * using GLFW as the underlying implementation.
 */
```

**Class Documentation:**
```cpp
/**
 * @brief Window class for managing a GLFW window with Vulkan support
 * 
 * Provides functionality for:
 * - Window creation and destruction
 * - Resolution changes and fullscreen toggle
 * - Event polling
 * - Resize callbacks
 */
class Window {
```

**Function Documentation:**
```cpp
/**
 * @brief Construct a window with specified dimensions
 * @param width Window width in pixels
 * @param height Window height in pixels
 * @param title Window title
 * @throws std::runtime_error if window creation fails
 */
Window(uint32_t width, uint32_t height, const char* title);
```

**Required Tags:**
- `@brief` - Brief description
- `@param` - For each parameter
- `@return` - For non-void functions
- `@throws` - For functions that can throw exceptions

## Adding New Source Files

### Step 1: Create the Files

**For Core Engine Components:**
```
include/vde/MyNewClass.h
src/MyNewClass.cpp
```

**For Game API Components:**
```
include/vde/api/MyNewClass.h
src/api/MyNewClass.cpp
```

### Step 2: Update CMakeLists.txt

Open `CMakeLists.txt` in the project root and add your files to the appropriate lists.

**Add Implementation File:**

Find the `VDE_SOURCES` section and add your `.cpp` file:

```cmake
set(VDE_SOURCES
    src/Window.cpp
    src/VulkanContext.cpp
    src/Camera.cpp
    # ... existing files ...
    src/MyNewClass.cpp        # Add your implementation here
    # Game API
    src/api/Entity.cpp
    src/api/MyNewAPIClass.cpp # Or here for API files
)
```

**Add Public Header:**

Find the `VDE_PUBLIC_HEADERS` section and add your `.h` file:

```cmake
set(VDE_PUBLIC_HEADERS
    include/vde/Core.h
    include/vde/Window.h
    # ... existing headers ...
    include/vde/MyNewClass.h   # Add your header here
    # Game API headers
    include/vde/api/GameAPI.h
    include/vde/api/MyNewAPIClass.h  # Or here for API headers
)
```

**Important Notes:**
- Keep files organized by category (core engine vs. game API)
- Use forward slashes (`/`) in paths, even on Windows
- Paths are relative to the project root
- Headers must be in `VDE_PUBLIC_HEADERS` to be installed
- Source files must be in `VDE_SOURCES` to be compiled

### Step 3: Rebuild the Project

After updating CMakeLists.txt, rebuild:

```powershell
# Visual Studio build
cmake --build build --config Debug --clean-first

# Or Ninja build
cmake --build build_ninja --clean-first
```

CMake will automatically detect the changes and include your new files.

## Adding Headers-Only Components

For header-only utilities or templates:

1. Create the header in `include/vde/` or `include/vde/api/`
2. Add it to `VDE_PUBLIC_HEADERS` in CMakeLists.txt
3. Do NOT add a `.cpp` file or entry in `VDE_SOURCES`

**Example:**
```cmake
set(VDE_PUBLIC_HEADERS
    # ... existing headers ...
    include/vde/MathUtils.h    # Header-only utility
)
```

## Code Style Guidelines

### Formatting

- **Indentation:** 4 spaces (no tabs)
- **Braces:** Opening brace on same line for functions, control structures
- **Line length:** Prefer 100 characters max, but not strict

**Example:**
```cpp
void MyClass::myFunction(int param) {
    if (param > 0) {
        // Do something
    } else {
        // Do something else
    }
}
```

### Modern C++ Practices

- **Use C++20 features** where appropriate
- **Prefer RAII** for resource management
- **Use smart pointers** (`std::unique_ptr`, `std::shared_ptr`) over raw pointers
- **Use `const`** wherever possible
- **Avoid macros** except for header guards (use `#pragma once`)

**Example:**
```cpp
class ResourceManager {
public:
    std::unique_ptr<Texture> createTexture(const std::string& path);
    
private:
    std::vector<std::unique_ptr<Buffer>> m_buffers;
};
```

### Error Handling

- **Use exceptions** for error handling in public API
- **Throw `std::runtime_error`** or derived exceptions
- **Document exceptions** with `@throws` in Doxygen comments

**Example:**
```cpp
/**
 * @brief Load texture from file
 * @param path File path to texture
 * @return Loaded texture
 * @throws std::runtime_error if file cannot be loaded
 */
Texture loadTexture(const std::string& path) {
    if (!fileExists(path)) {
        throw std::runtime_error("Texture file not found: " + path);
    }
    // Load texture...
}
```

## CMake Integration Details

### Understanding the Build Structure

The VDE project uses CMake with these key components:

**Library Target:**
```cmake
# Creates the 'vde' library target
add_library(vde STATIC ${VDE_SOURCES} ${VDE_PUBLIC_HEADERS})
```

**Include Directories:**
```cmake
# Public includes (available to library users)
target_include_directories(vde PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Private includes (only for library implementation)
target_include_directories(vde PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb
)
```

**Dependencies:**
```cmake
# Public dependencies (exposed to library users)
target_link_libraries(vde PUBLIC
    Vulkan::Vulkan
    glfw
    glm::glm
)

# Private dependencies (internal use only)
target_link_libraries(vde PRIVATE
    glslang::glslang
    glslang::SPIRV
)
```

### When to Modify CMakeLists.txt

**You MUST update CMakeLists.txt when:**
- Adding new `.cpp` source files → update `VDE_SOURCES`
- Adding new public `.h` headers → update `VDE_PUBLIC_HEADERS`
- Adding new external dependencies → update `target_link_libraries`

**You do NOT need to update CMakeLists.txt when:**
- Modifying existing files
- Adding private headers in `src/` (not public API)
- Changing function implementations

### Adding External Dependencies

If you need to add a new external library:

1. **Add FetchContent declaration:**
```cmake
FetchContent_Declare(
    mylibrary
    GIT_REPOSITORY https://github.com/user/mylibrary.git
    GIT_TAG v1.0.0
)
FetchContent_MakeAvailable(mylibrary)
```

2. **Link the library:**
```cmake
target_link_libraries(vde PRIVATE  # or PUBLIC if exposed in headers
    mylibrary::mylibrary
)
```

## Quick Reference Checklist

When adding a new class to VDE:

- [ ] Create `include/vde/ClassName.h` (or `include/vde/api/ClassName.h`)
- [ ] Create `src/ClassName.cpp` (or `src/api/ClassName.cpp`)
- [ ] Add `#pragma once` to header
- [ ] Add file and class Doxygen documentation
- [ ] Use `vde::` namespace (or `vde::api::`)
- [ ] Follow naming conventions (PascalCase class, camelCase methods, m_ members)
- [ ] Add header to `VDE_PUBLIC_HEADERS` in CMakeLists.txt
- [ ] Add source to `VDE_SOURCES` in CMakeLists.txt
- [ ] Include corresponding header first in .cpp file
- [ ] Follow include order in implementation
- [ ] Document public methods with @brief, @param, @return, @throws
- [ ] Rebuild project with `cmake --build build --config Debug`
- [ ] Run tests to verify: `./scripts/build-and-test.ps1`

## Common Patterns

### Creating a New Engine Component

```cpp
// include/vde/MyComponent.h
#pragma once

/**
 * @file MyComponent.h
 * @brief Brief description of component
 */

#include <vde/Core.h>
#include <memory>

namespace vde {

/**
 * @brief Component description
 */
class MyComponent {
public:
    MyComponent();
    ~MyComponent();
    
    /**
     * @brief Do something
     * @param value Input parameter
     * @return Result value
     */
    int doSomething(int value);
    
private:
    int m_value;
};

} // namespace vde
```

```cpp
// src/MyComponent.cpp
#include <vde/MyComponent.h>

#include <stdexcept>

namespace vde {

MyComponent::MyComponent()
    : m_value(0) {
}

MyComponent::~MyComponent() = default;

int MyComponent::doSomething(int value) {
    if (value < 0) {
        throw std::runtime_error("Value must be non-negative");
    }
    m_value = value;
    return m_value * 2;
}

} // namespace vde
```

### Creating a New Game API Component

```cpp
// include/vde/api/MyFeature.h
#pragma once

/**
 * @file MyFeature.h
 * @brief Feature for game developers
 */

#include <vde/api/GameTypes.h>

namespace vde::api {

/**
 * @brief Feature description
 */
class MyFeature {
public:
    /**
     * @brief Initialize feature
     * @param settings Configuration settings
     */
    void initialize(const Settings& settings);
    
private:
    bool m_initialized = false;
};

} // namespace vde::api
```

## See Also

- **add-component** - Guide for adding complex engine systems
- **build-tool-workflows** - Build and test commands
- **create-tests** - Writing unit tests for your code
- **vulkan-patterns** - Vulkan-specific coding patterns
- **architecture** - Project structure and organization
