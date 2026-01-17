# Contributing to VDE

Thank you for your interest in contributing to the Vulkan Display Engine!

## Getting Started

1. Fork the repository
2. Clone your fork
3. Create a feature branch: `git checkout -b feature/my-feature`
4. Make your changes
5. Run tests: `.\scripts\build-and-test.ps1`
6. Commit with descriptive messages
7. Push and create a pull request

## Development Setup

### Prerequisites
- CMake 3.20+
- Vulkan SDK 1.3+
- C++20 compatible compiler
- Git

### Building for Development

```powershell
cd vdengine
mkdir build && cd build
cmake ..
cmake --build . --config Debug
```

### Running Tests

```powershell
# All tests
.\scripts\build-and-test.ps1

# Specific test
.\build\tests\Debug\vde_tests.exe --gtest_filter=CameraTest.*
```

## Code Style

### General Guidelines
- Use C++20 features where appropriate
- Follow existing naming conventions (see below)
- Keep functions focused and small
- Prefer composition over inheritance
- Document public APIs with Doxygen comments

### Naming Conventions
- **Classes**: PascalCase (`VulkanContext`)
- **Functions**: camelCase (`createBuffer`)
- **Constants**: UPPER_SNAKE_CASE or k-prefix (`MAX_SIZE`, `kDefaultValue`)
- **Member variables**: m_ prefix (`m_device`)
- **Local variables**: camelCase (`localBuffer`)

### File Organization
- One class per file
- Header in `include/vde/ClassName.h`
- Implementation in `src/ClassName.cpp`
- Tests in `tests/ClassName_test.cpp`

### Header Format

```cpp
#pragma once

/**
 * @file ClassName.h
 * @brief One-line description
 * 
 * Detailed description if needed.
 */

#include <vde/Dependency.h>
#include <vulkan/vulkan.h>
#include <vector>

namespace vde {

/**
 * @brief Class description
 */
class ClassName {
public:
    /**
     * @brief Method description
     * @param param Parameter description
     * @return Return value description
     */
    ReturnType methodName(ParamType param);

private:
    Type m_memberVariable;
};

} // namespace vde
```

## Testing

### Writing Tests
- Use Google Test framework
- Name test files `ClassName_test.cpp`
- Group related tests in test fixtures
- Use descriptive test names

```cpp
class ClassNameTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup code
    }
};

TEST_F(ClassNameTest, MethodDoesExpectedBehavior) {
    // Arrange
    ClassName obj;
    
    // Act
    auto result = obj.method();
    
    // Assert
    EXPECT_EQ(expected, result);
}
```

### Test Coverage
- Aim for high coverage on core logic
- Mock external dependencies where feasible
- Test both success and failure paths
- Include edge cases

## Pull Request Process

1. **Title**: Clear, concise description of the change
2. **Description**: Explain what and why
3. **Tests**: Include new tests for new functionality
4. **Documentation**: Update docs if API changes
5. **Single Focus**: One feature/fix per PR

### PR Checklist
- [ ] Code follows style guidelines
- [ ] Tests pass locally
- [ ] New tests added for new features
- [ ] Documentation updated
- [ ] No unrelated changes

## Reporting Issues

### Bug Reports
Include:
- VDE version
- Operating system and GPU
- Steps to reproduce
- Expected vs actual behavior
- Relevant logs or screenshots

### Feature Requests
Include:
- Use case description
- Proposed API (if applicable)
- Alternatives considered

## Questions?

Open an issue with the "question" label for any questions about contributing.

## License

By contributing, you agree that your contributions will be licensed under the same license as the project.
