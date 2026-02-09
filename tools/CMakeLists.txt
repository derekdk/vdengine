# VDE Tools CMakeLists.txt
# Tools are for asset creation and are more complex than examples.
# They can run in interactive (GUI) mode or script (batch) mode.

# Fetch ImGui for tool UI
include(FetchContent)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.91.8-docking
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(imgui)

# Build ImGui as a static library for tools (only if not already created by examples)
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
endif()

# Create a custom target to copy shaders for tools
add_custom_target(copy_tool_shaders ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/Debug/shaders
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/Release/shaders
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/shaders
        ${CMAKE_CURRENT_BINARY_DIR}/Debug/shaders
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/shaders
        ${CMAKE_CURRENT_BINARY_DIR}/Release/shaders
    COMMENT "Copying shader files to tools directory..."
)

# Add tool subdirectories
add_subdirectory(geometry_repl)
