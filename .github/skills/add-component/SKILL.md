---
name: add-component
description: Guide for adding new components to the VDE engine. Use this when creating new classes, systems, or modules in the engine.
---

# Adding a New Component

This skill guides you through adding a new component (class, system, or module) to the Vulkan Display Engine.

## When to use this skill

- Creating a new engine system or subsystem
- Adding a new public API class
- Implementing a new renderer, manager, or utility class
- Extending the engine with new functionality
- Adding internal implementation classes

## Component Structure

VDE follows a clear structure for code organization:

- **Public API headers:** `include/vde/` - Headers exposed to users
- **Implementation files:** `src/` - Core implementation code
- **API implementation:** `src/api/` - Game-facing API implementations
- **Tests:** `tests/` - Unit tests for components

## Steps to Add a Component

### 1. Create the Header File

Add your header to `include/vde/ComponentName.h`:

```cpp
#pragma once

#include <vulkan/vulkan.h>
// Other necessary includes

namespace vde {

/**
 * @brief Brief description of the component
 * 
 * Detailed description explaining the purpose,
 * usage patterns, and key responsibilities.
 */
class ComponentName {
public:
    ComponentName();
    ~ComponentName();
    
    // Public methods with Doxygen documentation
    
private:
    // Private members with m_ prefix
};

} // namespace vde
```

### 2. Create the Implementation File

Add your implementation to `src/ComponentName.cpp`:

```cpp
#include <vde/ComponentName.h>

// Include order:
// 1. Corresponding header (already included)
// 2. Other vde headers: <vde/...>
// 3. Third-party headers
// 4. Standard library headers

namespace vde {

ComponentName::ComponentName() {
    // Implementation
}

ComponentName::~ComponentName() {
    // Cleanup
}

} // namespace vde
```

### 3. Register in CMake

Add both files to the root `CMakeLists.txt`:

```cmake
# Add to VDE_PUBLIC_HEADERS
set(VDE_PUBLIC_HEADERS
    # ... existing headers ...
    include/vde/ComponentName.h
)

# Add to VDE_SOURCES
set(VDE_SOURCES
    # ... existing sources ...
    src/ComponentName.cpp
)
```

### 4. Make it Accessible

If the component is part of the public API, add it to `include/vde/Core.h`:

```cpp
// Core VDE includes
#include <vde/ComponentName.h>
```

This allows users to include everything with `#include <vde/Core.h>`.

### 5. Create Unit Tests

Add test coverage in `tests/ComponentName_test.cpp`:

```cpp
/**
 * @file ComponentName_test.cpp
 * @brief Unit tests for ComponentName
 */

#include <gtest/gtest.h>
#include <vde/ComponentName.h>
// Other necessary includes

namespace vde::test {

class ComponentNameTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Test setup
    }
    
    void TearDown() override {
        // Test cleanup
    }
};

TEST_F(ComponentNameTest, DescriptiveTestName) {
    // Test implementation
    EXPECT_TRUE(true);
}

} // namespace vde::test
```

See the [create-tests](../create-tests/SKILL.md) skill for detailed testing guidelines.

### 6. Register Tests in CMake

Add the test file to `tests/CMakeLists.txt`:

```cmake
set(VDE_TEST_SOURCES
    # ... existing tests ...
    ComponentName_test.cpp
)
```

## Naming Conventions

Follow these conventions from [conventions.md](../../conventions.md):

- **Classes:** PascalCase (e.g., `ComponentName`, `RenderManager`)
- **Functions/Methods:** camelCase (e.g., `initialize()`, `createBuffer()`)
- **Constants:** UPPER_SNAKE_CASE or kPrefix (e.g., `MAX_FRAMES`, `kDefaultSize`)
- **Member variables:** m_ prefix (e.g., `m_device`, `m_bufferSize`)
- **Namespace:** `vde::` for all engine code

## Best Practices

- **One class per file:** Keep headers and implementations focused
- **RAII for resources:** Use constructors/destructors for Vulkan resource management
- **Document public APIs:** Use Doxygen-style comments with @brief, @param, @return, @throws
- **Error handling:** Throw `std::runtime_error` for unrecoverable issues; return values for recoverable cases
- **Include guards:** Use `#pragma once` in all headers
- **Test early:** Create basic tests alongside the component to verify functionality
- **Follow conventions:** Match the style and patterns of existing engine code

## Example: Adding a BufferManager

**Header** (`include/vde/BufferManager.h`):
```cpp
#pragma once

#include <vulkan/vulkan.h>
#include <memory>
#include <vector>

namespace vde {

/**
 * @brief Manages Vulkan buffer allocation and lifetime
 * 
 * Provides RAII-based buffer management with automatic
 * cleanup and tracking of buffer resources.
 */
class BufferManager {
public:
    BufferManager(VkDevice device, VkPhysicalDevice physicalDevice);
    ~BufferManager();
    
    /**
     * @brief Creates a new buffer with the specified properties
     * @param size Size of the buffer in bytes
     * @param usage Buffer usage flags
     * @param properties Memory property flags
     * @return Unique pointer to the created buffer
     * @throws std::runtime_error if buffer creation fails
     */
    std::unique_ptr<Buffer> createBuffer(
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags properties
    );
    
private:
    VkDevice m_device;
    VkPhysicalDevice m_physicalDevice;
    std::vector<VkBuffer> m_buffers;
    std::vector<VkDeviceMemory> m_memories;
};

} // namespace vde
```

**Implementation** (`src/BufferManager.cpp`):
```cpp
#include <vde/BufferManager.h>
#include <vde/BufferUtils.h>

#include <stdexcept>

namespace vde {

BufferManager::BufferManager(VkDevice device, VkPhysicalDevice physicalDevice)
    : m_device(device)
    , m_physicalDevice(physicalDevice) {
}

BufferManager::~BufferManager() {
    // RAII cleanup
    for (size_t i = 0; i < m_buffers.size(); ++i) {
        vkDestroyBuffer(m_device, m_buffers[i], nullptr);
        vkFreeMemory(m_device, m_memories[i], nullptr);
    }
}

std::unique_ptr<Buffer> BufferManager::createBuffer(
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags properties
) {
    VkBuffer buffer;
    VkDeviceMemory memory;
    
    BufferUtils::createBuffer(size, usage, properties, buffer, memory);
    
    m_buffers.push_back(buffer);
    m_memories.push_back(memory);
    
    return std::make_unique<Buffer>(buffer, memory, size);
}

} // namespace vde
```

**Tests** (`tests/BufferManager_test.cpp`):
```cpp
#include <gtest/gtest.h>
#include <vde/BufferManager.h>

namespace vde::test {

TEST(BufferManagerTest, CreateBufferSucceeds) {
    // Test implementation
    EXPECT_TRUE(true);
}

} // namespace vde::test
```

## Verification

After adding a component:

1. **Build succeeds:** `cmake --build build --config Debug`
2. **Tests pass:** `./scripts/build-and-test.ps1 -NoBuild`
3. **No warnings:** Check compiler output for warnings
4. **Documentation clear:** Review Doxygen comments for clarity
5. **Follows conventions:** Verify naming and style match project standards

See [build-tool-workflows](../build-tool-workflows/SKILL.md) for build and test commands.
