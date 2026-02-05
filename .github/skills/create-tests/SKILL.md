---
name: create-tests
description: Guide on how to create tests in this project. Use this when asked to create unit-tests.
---

# Creating Unit-Tests

This skill helps you create unit tests for this project.

## When to use this skill

- Add coverage for new or changed public APIs in include/vde/ or src/.
- Capture regressions with focused, reproducible cases before fixing a bug.
- Add guardrails around math/geometry logic (GLM usage), hashing, resource descriptions, and other pure code that can run without a GPU.
- Prefer unit-level tests over integration (window/device creation) unless you truly need OS/Vulkan interaction.


## Creating Tests

1. Ensure tests are enabled (VDE_BUILD_TESTS is ON by default in CMake). The tests live under tests/ and build into the vde_tests target.
2. Add a new source file named <Name>_test.cpp under tests/. Follow existing examples such as [tests/Camera_test.cpp](tests/Camera_test.cpp) for layout.
3. Structure the file with a brief Doxygen-style header, then includes in this order: `<gtest/gtest.h>`, `<vde/...>` headers under test, then third-party (e.g., GLM) or standard headers. Place tests inside `namespace vde::test` and use fixtures deriving from `::testing::Test` when setup/teardown is shared.
4. Register the new file in [tests/CMakeLists.txt](tests/CMakeLists.txt) by adding it to the `VDE_TEST_SOURCES` list so it is compiled into vde_tests and discovered by CTest.
5. Prefer deterministic, fast checks (math, hashing, data structures). Avoid GLFW window creation or Vulkan device work unless absolutely required; if you must, clearly comment the integration dependency so it can be skipped or isolated.
6. Use expressive test and fixture names, keep assertions narrow, and use `EXPECT_NEAR`/`EXPECT_FLOAT_EQ` for floating point comparisons. Reuse engine helpers and RAII types rather than duplicating logic.


## Running Tests


```powershell
./scripts/build-and-test.ps1 [-BuildConfig Debug|Release] [-NoBuild] [-NoTest] [-Filter <gtest-pattern>]
```

This builds vde_tests and executes it (default config Debug) using GoogleTest filters if provided.

- Manual CMake flow (if you are using your own build directory):

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug --parallel
ctest --test-dir build --build-config Debug --output-on-failure
# Or run the binary directly (MSVC multi-config example):
build/tests/Debug/vde_tests.exe --gtest_filter=NamePattern
```


## Best practices

- Keep tests small, isolated, and free of external resources (no network, no file system writes unless using temporary in-memory data).
- Match naming and namespaces to the production code; keep symbols under `vde::test` to avoid collisions.
- Prefer `EXPECT_*` over `ASSERT_*` so later checks still run unless a hard failure makes continuing unsafe.
- Use GLM helpers for matrix/vector expectations and clamp floating-point tolerances to meaningful thresholds.
- If a test needs real graphics resources (GLFW window, Vulkan device), document the requirement in comments and consider guarding it so headless CI can still pass.