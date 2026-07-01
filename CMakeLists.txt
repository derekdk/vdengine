cmake_minimum_required(VERSION 3.20)
project(VulkanDisplayEngine VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Options
option(VDE_BUILD_EXAMPLES "Build example applications" ON)
option(VDE_BUILD_GAMES "Build game applications" ON)
option(VDE_BUILD_TESTS "Build unit tests" ON)
option(VDE_BUILD_TOOLS "Build asset creation tools" ON)
option(VDE_SHARED_LIBS "Build as shared library" OFF)
option(VDE_TIMING "Enable per-TU compiler timing flags (MSVC: /Bt, GCC/Clang: -ftime-report)" OFF)

# Compiler options
if(MSVC)
    add_compile_definitions(
        _CRT_SECURE_NO_WARNINGS
        NOMINMAX
        WIN32_LEAN_AND_MEAN
    )
    if(VDE_TIMING)
        # /Bt  - print per-TU "time(file)=Xs (Ys front end, Zs back end)" to stderr
        add_compile_options(/Bt)
        message(STATUS "VDE: compiler timing enabled (/Bt)")
    endif()
    set(VDE_WARNING_FLAGS /W4 /WX /FS)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
    if(VDE_TIMING)
        add_compile_options(-ftime-report)
        message(STATUS "VDE: compiler timing enabled (-ftime-report)")
    endif()
    set(VDE_WARNING_FLAGS -Wall -Wextra -Wpedantic -Werror)
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
        # Suppress cmake_minimum_required deprecation from glm 1.0.1
        set(_prev_warn_deprecated ${CMAKE_WARN_DEPRECATED})
        set(CMAKE_WARN_DEPRECATED OFF CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(glm)
        set(CMAKE_WARN_DEPRECATED ${_prev_warn_deprecated} CACHE BOOL "" FORCE)
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
set(ENABLE_RTTI ON CACHE BOOL "" FORCE)
set(ENABLE_OPT OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(BUILD_EXTERNAL OFF CACHE BOOL "" FORCE)

# Only fetch if not already available
if(NOT TARGET glslang)
    FetchContent_MakeAvailable(glslang)
endif()

# Suppress warnings from third-party glslang targets
if(TARGET MachineIndependent)
    foreach(_glslang_target
        glslang MachineIndependent OSDependent GenericCodeGen SPIRV
        glslang-default-resource-limits glslang-standalone)
        if(TARGET ${_glslang_target})
            target_compile_options(${_glslang_target} PRIVATE
                $<$<CXX_COMPILER_ID:MSVC>:/W0>
                $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-w>
            )
        endif()
    endforeach()
endif()

# miniaudio for cross-platform audio
FetchContent_Declare(
    miniaudio
    GIT_REPOSITORY https://github.com/mackron/miniaudio.git
    GIT_TAG 0.11.21
)
FetchContent_MakeAvailable(miniaudio)

# SQLite3 for persistent key-value storage
FetchContent_Declare(
    sqlite3
    URL https://www.sqlite.org/2025/sqlite-amalgamation-3490100.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

# toml++ for TOML configuration parsing (header-only)
set(TOMLPLUSPLUS_VERSION_TAG "v3.4.0")
set(TOMLPLUSPLUS_COMMIT "30172438cee64926dc41fdd9c11fb3ba5b2ba9de")
set(TOMLPLUSPLUS_ARCHIVE_SHA256 "0BADB114FA26325320CFBCCE28B5953F85942D2A0BB1E4FDD99197497AFD2ED5")

# Use the immutable commit archive for the v3.4.0 release and verify its contents.
FetchContent_Declare(
    tomlplusplus
    URL https://github.com/marzer/tomlplusplus/archive/${TOMLPLUSPLUS_COMMIT}.zip
    URL_HASH SHA256=${TOMLPLUSPLUS_ARCHIVE_SHA256}
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

# nlohmann/json for metadata import helpers (header-only)
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE
)
FetchContent_GetProperties(sqlite3)
if(NOT sqlite3_POPULATED)
    # Policy CMP0169: allow direct FetchContent_Populate for content with no CMakeLists
    cmake_policy(SET CMP0169 OLD)
    FetchContent_Populate(sqlite3)
    add_library(sqlite3 STATIC ${sqlite3_SOURCE_DIR}/sqlite3.c)
    target_include_directories(sqlite3 PUBLIC ${sqlite3_SOURCE_DIR})
    if(MSVC)
        target_compile_options(sqlite3 PRIVATE /W0)
    else()
        target_compile_options(sqlite3 PRIVATE -w)
    endif()
endif()

# Make toml++ available
FetchContent_MakeAvailable(tomlplusplus)
FetchContent_MakeAvailable(nlohmann_json)

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
    src/OffscreenRenderTarget.cpp
    src/stb_impl.cpp
    src/HexGeometry.cpp
    src/HexPrismMesh.cpp
    src/PlaybackClock.cpp
    # Game API
    src/api/Entity.cpp
    src/api/Game.cpp
    src/api/GameCamera.cpp
    src/api/GameTypes.cpp
    src/api/LightBox.cpp
    src/api/Scene.cpp
    src/api/ResourceManager.cpp
    src/api/Mesh.cpp
    src/api/CameraBounds.cpp
    src/api/Material.cpp
    src/api/Scheduler.cpp
    src/api/PhysicsScene.cpp
    src/api/PhysicsEntity.cpp
    src/api/ThreadPool.cpp
    src/api/TimedEvents.cpp
    src/api/Animator.cpp
    # Transitions
    src/api/Transition.cpp
    src/api/FadeTransition.cpp
    src/api/WipeTransition.cpp
    src/api/CircleRevealTransition.cpp
    src/api/BlockFallTransition.cpp
    src/api/TransitionManager.cpp
    # Audio
    src/miniaudio_impl.cpp
    src/api/AudioClip.cpp
    src/api/AudioManager.cpp
    src/api/AudioSource.cpp
    # Input
    src/api/InputActionMap.cpp
    src/api/KeyStateTracker.cpp
    # Input script
    src/api/InputScript.cpp
    src/api/InputScriptExecutor.cpp
    # Storage
    src/api/StorageManager.cpp
    # SpriteSheet
    src/api/SpriteSheet.cpp
    src/api/TileMap.cpp
    src/api/SpriteAnimation.cpp
    src/api/SpriteAnimationImport.cpp
    src/api/AnimatedSpriteEntity.cpp
    # Text rendering
    src/api/BitmapFont.cpp
    src/api/TextRenderer.cpp
    src/api/TrueTypeFont.cpp
    src/api/TextEntity.cpp
    src/api/EmojiFont.cpp
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
    include/vde/OffscreenRenderTarget.h
    include/vde/Types.h
    include/vde/HexGeometry.h
    include/vde/HexPrismMesh.h
    include/vde/QueueFamilyIndices.h
    include/vde/SwapChainSupportDetails.h
    include/vde/PlaybackClock.h
    include/vde/Easing.h
    include/vde/Tween.h
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
    include/vde/api/Material.h
    include/vde/api/Resource.h
    include/vde/api/InputHandler.h
    include/vde/api/KeyCodes.h
    include/vde/api/WorldUnits.h
    include/vde/api/WorldBounds.h
    include/vde/api/CameraBounds.h
    include/vde/api/Scheduler.h
    include/vde/api/SceneGroup.h
    include/vde/api/ViewportRect.h
    include/vde/api/PhysicsTypes.h
    include/vde/api/PhysicsScene.h
    include/vde/api/PhysicsEntity.h
    include/vde/api/ThreadPool.h
    include/vde/api/TimedEvents.h
    # Animator
    include/vde/api/Animator.h
    include/vde/api/AnimatorImpl.h
    # Transition headers
    include/vde/api/Transition.h
    include/vde/api/FadeTransition.h
    include/vde/api/WipeTransition.h
    include/vde/api/CircleRevealTransition.h
    include/vde/api/BlockFallTransition.h
    include/vde/api/TransitionManager.h
    # Audio headers
    include/vde/api/AudioClip.h
    include/vde/api/AudioEvent.h
    include/vde/api/AudioManager.h
    include/vde/api/AudioSource.h
    # Input
    include/vde/api/InputActionMap.h
    include/vde/api/KeyStateTracker.h
    # Input script
    include/vde/api/InputScript.h
    include/vde/api/InputScriptExecutor.h
    include/vde/api/ScriptEnvironment.h
    include/vde/api/SpriteAnimation.h
    include/vde/api/SpriteAnimationImport.h
    include/vde/api/AnimatedSpriteEntity.h
    # Storage
    include/vde/api/StorageManager.h
    # SpriteSheet
    include/vde/api/SpriteSheet.h
    include/vde/api/TileMap.h
    # Default constants
    include/vde/api/Defaults.h
    # Gameplay utility headers
    include/vde/api/Math2D.h
    include/vde/api/Timing.h
    include/vde/api/Random.h
    # Text rendering headers
    include/vde/api/BitmapFont.h
    include/vde/api/TextRenderer.h
    include/vde/api/TrueTypeFont.h
    include/vde/api/TextEntity.h
    include/vde/api/EmojiFont.h
    include/vde/api/Utf8.h
)

# Create library
if(VDE_SHARED_LIBS)
    add_library(vde SHARED ${VDE_SOURCES} ${VDE_PUBLIC_HEADERS})
    target_compile_definitions(vde PUBLIC VDE_SHARED)
    target_compile_definitions(vde PRIVATE VDE_EXPORTS)
else()
    add_library(vde STATIC ${VDE_SOURCES} ${VDE_PUBLIC_HEADERS})
endif()

# Strict warnings on VDE sources only (not third-party dependencies)
target_compile_options(vde PRIVATE ${VDE_WARNING_FLAGS})

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

# Third-party includes as SYSTEM so their warnings are suppressed by /external:W0 (MSVC) or -isystem (GCC/Clang)
target_include_directories(vde SYSTEM PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/flip
    ${miniaudio_SOURCE_DIR}
)

# Link dependencies
target_link_libraries(vde PUBLIC
    Vulkan::Vulkan
    glfw
    glm::glm
    glslang::glslang
    MachineIndependent
    OSDependent
    glslang::glslang-default-resource-limits
    glslang::SPIRV
)

target_link_libraries(vde PRIVATE
    sqlite3
    nlohmann_json::nlohmann_json
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

# ImGui for examples, games, and tools (single declaration shared by all app categories)
if(VDE_BUILD_EXAMPLES OR VDE_BUILD_GAMES OR VDE_BUILD_TOOLS)
    if(NOT TARGET imgui)
        FetchContent_Declare(
            imgui
            GIT_REPOSITORY https://github.com/ocornut/imgui.git
            GIT_TAG        v1.91.8-docking
            GIT_SHALLOW    TRUE
        )
        FetchContent_MakeAvailable(imgui)
    endif()

    if(NOT TARGET imgui_backend)
        add_library(imgui_backend STATIC
            ${imgui_SOURCE_DIR}/imgui.cpp
            ${imgui_SOURCE_DIR}/imgui_demo.cpp
            ${imgui_SOURCE_DIR}/imgui_draw.cpp
            ${imgui_SOURCE_DIR}/imgui_tables.cpp
            ${imgui_SOURCE_DIR}/imgui_widgets.cpp
            ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
            ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
        )

        target_include_directories(imgui_backend PUBLIC
            ${imgui_SOURCE_DIR}
            ${imgui_SOURCE_DIR}/backends
        )

        target_link_libraries(imgui_backend PUBLIC
            Vulkan::Vulkan
            glfw
        )

        # Enable 32-bit ImWchar for full Unicode/emoji support in ImGui
        target_compile_definitions(imgui_backend PUBLIC IMGUI_USE_WCHAR32)
    endif()
endif()

# Examples
if(VDE_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Games
if(VDE_BUILD_GAMES)
    add_subdirectory(games)
endif()

# Tools
if(VDE_BUILD_TOOLS)
    add_subdirectory(tools)
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
message(STATUS "  Build games: ${VDE_BUILD_GAMES}")
message(STATUS "  Build tools: ${VDE_BUILD_TOOLS}")
message(STATUS "  Build tests: ${VDE_BUILD_TESTS}")
message(STATUS "  Compiler timing: ${VDE_TIMING}")
message(STATUS "===========================================")
