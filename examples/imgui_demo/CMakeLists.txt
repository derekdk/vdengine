# ImGui integration demo
#
# Fetches Dear ImGui (with docking branch) via FetchContent
# and builds the GLFW + Vulkan backends alongside the example.

include(FetchContent)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.91.8-docking
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(imgui)

# Build ImGui as a small static library so it can be reused if
# more imgui-based examples are added later.
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

# The example executable
add_executable(vde_imgui_demo
    main.cpp
)

target_link_libraries(vde_imgui_demo PRIVATE
    vde
    imgui_backend
)

add_dependencies(vde_imgui_demo copy_example_shaders)
