# VDE Examples CMakeLists.txt

# Fetch ImGui for all examples that want to use it
include(FetchContent)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.91.8-docking
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(imgui)

# Build ImGui as a static library for use in examples
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

# Create a custom target to copy shaders once (avoids parallel copy conflicts with MSBuild)
add_custom_target(copy_example_shaders ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/shaders
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/Debug/shaders
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/Release/shaders
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/shaders
        ${CMAKE_CURRENT_BINARY_DIR}/shaders
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/shaders
        ${CMAKE_CURRENT_BINARY_DIR}/Debug/shaders
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/shaders
        ${CMAKE_CURRENT_BINARY_DIR}/Release/shaders
    COMMENT "Copying shader files to examples directory..."
)

# Helper function to add examples with ImGui support enabled
function(add_vde_example target_name source_files)
    add_executable(${target_name} ${source_files})
    target_link_libraries(${target_name} PRIVATE vde imgui_backend)
    target_compile_definitions(${target_name} PRIVATE VDE_EXAMPLE_USE_IMGUI)
    add_dependencies(${target_name} copy_example_shaders)
endfunction()

# Basic triangle example (low-level, no ImGui)
add_executable(vde_triangle_example
    triangle/main.cpp
)

target_link_libraries(vde_triangle_example PRIVATE vde)
add_dependencies(vde_triangle_example copy_example_shaders)

# Simple game example using Game API
add_vde_example(vde_simple_game_example "simple_game/main.cpp")

# Sprite demo example demonstrating SpriteEntity
add_vde_example(vde_sprite_demo "sprite_demo/main.cpp")

# Breakout demo - simple breakout clone demonstrating 2D gameplay and sprite collisions
add_vde_example(vde_breakout_demo "breakout_demo/main.cpp")

# Asteroids demo - classic asteroids clone demonstrating spaceship control, collision detection, and game logic
add_vde_example(vde_asteroids_demo "asteroids_demo/main.cpp")

# Asteroids demo (Physics) - Modern asteroids clone using physics-based gameplay with forces, collision callbacks, and entity management
add_vde_example(vde_asteroids_physics_demo "asteroids_demo/main_new.cpp")

# World bounds demo example demonstrating Phase 2.5 features
add_vde_example(vde_world_bounds_demo "world_bounds_demo/main.cpp")

# Materials & Lighting demo example demonstrating Phase 4 features
add_vde_example(vde_materials_lighting_demo "materials_lighting_demo/main.cpp")

# Hex prism stacks demo example demonstrating mouse pick-and-drag interaction
add_vde_example(vde_hex_prism_stacks_demo "hex_prism_stacks_demo/main.cpp")

# Resource Management demo example demonstrating Phase 5 features
add_vde_example(vde_resource_demo "resource_demo/main.cpp")

# Audio demo example demonstrating audio system
add_vde_example(vde_audio_demo "audio_demo/main.cpp")

# Copy audio assets directory
add_custom_command(TARGET vde_audio_demo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/examples/audio_demo/assets
    $<TARGET_FILE_DIR:vde_audio_demo>/assets
    COMMENT "Copying audio assets next to executable..."
)

# 2D Sidescroller example demonstrating platformer game mechanics
add_vde_example(vde_sidescroller "sidescroller/main.cpp")

# Wireframe viewer example demonstrating shape switching, render modes, and interaction
add_vde_example(vde_wireframe_viewer "wireframe_viewer/main.cpp")

# Multi-scene demo example demonstrating scene management, switching, and background simulation
add_vde_example(vde_multi_scene_demo "multi_scene_demo/main.cpp")

# Quad viewport demo example demonstrating four simultaneous scenes in split-screen
add_vde_example(vde_quad_viewport_demo "quad_viewport_demo/main.cpp")

# Four Scene 3D demo - four simultaneous 3D scenes with focus indicator
add_vde_example(vde_four_scene_3d_demo "four_scene_3d_demo/main.cpp")

# Physics demo example demonstrating PhysicsScene simulation
add_vde_example(vde_physics_demo "physics_demo/main.cpp")

# Parallel physics demo demonstrating thread pool integration
add_vde_example(vde_parallel_physics_demo "parallel_physics_demo/main.cpp")

# Physics + Audio demo demonstrating collision -> game logic -> audio pipeline
add_vde_example(vde_physics_audio_demo "physics_audio_demo/main.cpp")

# Dear ImGui integration demo - demonstrates ImGui overlay on VDE scenes
add_vde_example(vde_imgui_demo "imgui_demo/main.cpp")

