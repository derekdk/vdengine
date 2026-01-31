# VDE Tests CMakeLists.txt

# Fetch GoogleTest
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Test sources
set(VDE_TEST_SOURCES
    main_test.cpp
    Window_test.cpp
    Camera_test.cpp
    HexGeometry_test.cpp
    ShaderCache_test.cpp
    Types_test.cpp
    Mesh_test.cpp
)

# Create test executable
add_executable(vde_tests ${VDE_TEST_SOURCES})

target_link_libraries(vde_tests PRIVATE
    vde
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(vde_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

# Register tests with CTest
include(GoogleTest)
gtest_discover_tests(vde_tests)
