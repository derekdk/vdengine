cmake_minimum_required(VERSION 3.20)
project(VulkanDisplayEngine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Options
option(VDE_BUILD_EXAMPLES "Build example applications" ON)
option(VDE_BUILD_TESTS "Build unit tests" ON)
option(VDE_SHARED_LIBS "Build as shared library" OFF)

# Compiler options
if(MSVC)
    add_compile_options(/W4 /FS)
    add_compile_definitions(
        _CRT_SECURE_NO_WARNINGS
        NOMINMAX
        WIN32_LEAN_AND_MEAN
    )
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Enable testing support
add_compile_definitions(ENABLE_TESTING)

# Find Vulkan SDK
find_package(Vulkan REQUIRED)
message(STATUS "Vulkan SDK found: ${Vulkan_INCLUDE_DIRS}")

# GLFW
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

# Check if GLFW already exists (when used as subdirectory of another project)
if(NOT TARGET glfw)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/glfw/CMakeLists.txt")
        add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../third_party/glfw ${CMAKE_CURRENT_BINARY_DIR}/glfw)
    else()
        # Fetch GLFW if not available locally
        include(FetchContent)
        FetchContent_Declare(
            glfw
            GIT_REPOSITORY https://github.com/glfw/glfw.git
            GIT_TAG 3.4
        )
        FetchContent_MakeAvailable(glfw)
    endif()
endif()

# GLM
if(NOT TARGET glm)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/glm/CMakeLists.txt")
        add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../third_party/glm ${CMAKE_CURRENT_BINARY_DIR}/glm)
    else()
        # Fetch GLM if not available locally
        FetchContent_Declare(
            glm
            GIT_REPOSITORY https://github.com/g-truc/glm.git
            GIT_TAG 1.0.1
        )
        FetchContent_MakeAvailable(glm)
    endif()
endif()

# glslang for runtime shader compilation
include(FetchContent)
FetchContent_Declare(
    glslang
    GIT_REPOSITORY https://github.com/KhronosGroup/glslang.git
    GIT_TAG 14.3.0
)

# Configure glslang options before fetching
set(ENABLE_GLSLANG_BINARIES OFF CACHE BOOL "" FORCE)
set(ENABLE_SPVREMAPPER OFF CACHE BOOL "" FORCE)
set(ENABLE_HLSL OFF CACHE BOOL "" FORCE)
set(ENABLE_OPT OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(BUILD_EXTERNAL OFF CACHE BOOL "" FORCE)

# Only fetch if not already available
if(NOT TARGET glslang)
    FetchContent_MakeAvailable(glslang)
endif()

# Library sources
set(VDE_SOURCES
    src/Window.cpp
    src/VulkanContext.cpp
    src/Camera.cpp
    src/Texture.cpp
    src/ShaderCompiler.cpp
    src/ShaderCache.cpp
    src/ShaderHash.cpp
    src/BufferUtils.cpp
    src/UniformBuffer.cpp
    src/DescriptorManager.cpp
    src/ImageLoader.cpp
    src/stb_impl.cpp
    src/HexGeometry.cpp
    src/HexPrismMesh.cpp
    # Game API
    src/api/Entity.cpp
    src/api/Game.cpp
    src/api/GameCamera.cpp
    src/api/GameTypes.cpp
    src/api/LightBox.cpp
    src/api/Scene.cpp
    src/api/Mesh.cpp
)

# Public headers
set(VDE_PUBLIC_HEADERS
    include/vde/Core.h
    include/vde/Window.h
    include/vde/VulkanContext.h
    include/vde/Camera.h
    include/vde/Texture.h
    include/vde/ShaderCompiler.h
    include/vde/ShaderCache.h
    include/vde/ShaderStage.h
    include/vde/BufferUtils.h
    include/vde/UniformBuffer.h
    include/vde/DescriptorManager.h
    include/vde/ImageLoader.h
    include/vde/Types.h
    include/vde/HexGeometry.h
    include/vde/HexPrismMesh.h
    include/vde/QueueFamilyIndices.h
    include/vde/SwapChainSupportDetails.h
    # Game API headers
    include/vde/api/GameAPI.h
    include/vde/api/Game.h
    include/vde/api/GameSettings.h
    include/vde/api/GameTypes.h
    include/vde/api/GameCamera.h
    include/vde/api/Entity.h
    include/vde/api/Scene.h
    include/vde/api/LightBox.h
    include/vde/api/Mesh.h
    include/vde/api/Resource.h
    include/vde/api/InputHandler.h
    include/vde/api/KeyCodes.h
)

# Create library
if(VDE_SHARED_LIBS)
    add_library(vde SHARED ${VDE_SOURCES} ${VDE_PUBLIC_HEADERS})
    target_compile_definitions(vde PUBLIC VDE_SHARED)
    target_compile_definitions(vde PRIVATE VDE_EXPORTS)
else()
    add_library(vde STATIC ${VDE_SOURCES} ${VDE_PUBLIC_HEADERS})
endif()

# Add alias for namespaced target
add_library(vde::vde ALIAS vde)

# Public include directories
target_include_directories(vde PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Private include directories
target_include_directories(vde PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb
)

# Link dependencies
target_link_libraries(vde PUBLIC
    Vulkan::Vulkan
    glfw
    glm::glm
)

target_link_libraries(vde PRIVATE
    glslang::glslang
    glslang::glslang-default-resource-limits
    glslang::SPIRV
)

# Shader directory configuration
target_compile_definitions(vde PRIVATE
    VDE_SHADER_DIR="${CMAKE_CURRENT_SOURCE_DIR}/shaders"
)

# Install rules (simplified - without export since we depend on non-exported targets)
include(GNUInstallDirs)

install(TARGETS vde
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY include/vde
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Note: Full CMake package config is not provided since dependencies (glfw, glm, glslang)
# are not exported. For proper package installation, use FetchContent or add_subdirectory
# to include VDE in your project.

# Examples
if(VDE_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Tests
if(VDE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Print configuration summary
message(STATUS "===========================================")
message(STATUS "Vulkan Display Engine Configuration:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Shared library: ${VDE_SHARED_LIBS}")
message(STATUS "  Build examples: ${VDE_BUILD_EXAMPLES}")
message(STATUS "  Build tests: ${VDE_BUILD_TESTS}")
message(STATUS "===========================================")
