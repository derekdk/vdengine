# VDE Examples CMakeLists.txt

# Create a custom target to copy shaders once (avoids parallel copy conflicts with MSBuild)
add_custom_target(copy_example_shaders ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/Debug/shaders
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/Release/shaders
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/shaders
        ${CMAKE_CURRENT_BINARY_DIR}/Debug/shaders
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/shaders
        ${CMAKE_CURRENT_BINARY_DIR}/Release/shaders
    COMMENT "Copying shader files to examples directory..."
)

# Basic triangle example
add_executable(vde_triangle_example
    triangle/main.cpp
)

target_link_libraries(vde_triangle_example PRIVATE vde)
add_dependencies(vde_triangle_example copy_example_shaders)

# Simple game example using Game API
add_executable(vde_simple_game_example
    simple_game/main.cpp
)

target_link_libraries(vde_simple_game_example PRIVATE vde)
add_dependencies(vde_simple_game_example copy_example_shaders)

# Sprite demo example demonstrating SpriteEntity
add_executable(vde_sprite_demo
    sprite_demo/main.cpp
)

target_link_libraries(vde_sprite_demo PRIVATE vde)
add_dependencies(vde_sprite_demo copy_example_shaders)

# World bounds demo example demonstrating Phase 2.5 features
add_executable(vde_world_bounds_demo
    world_bounds_demo/main.cpp
)

target_link_libraries(vde_world_bounds_demo PRIVATE vde)
add_dependencies(vde_world_bounds_demo copy_example_shaders)

# Materials & Lighting demo example demonstrating Phase 4 features
add_executable(vde_materials_lighting_demo
    materials_lighting_demo/main.cpp
)

target_link_libraries(vde_materials_lighting_demo PRIVATE vde)
add_dependencies(vde_materials_lighting_demo copy_example_shaders)

# Resource Management demo example demonstrating Phase 5 features
add_executable(vde_resource_demo
    resource_demo/main.cpp
)

target_link_libraries(vde_resource_demo PRIVATE vde)
add_dependencies(vde_resource_demo copy_example_shaders)

# Audio demo example demonstrating audio system
add_executable(vde_audio_demo
    audio_demo/main.cpp
)

target_link_libraries(vde_audio_demo PRIVATE vde)
add_dependencies(vde_audio_demo copy_example_shaders)

# Copy audio assets directory
add_custom_command(TARGET vde_audio_demo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/examples/audio_demo/assets
    $<TARGET_FILE_DIR:vde_audio_demo>/assets
    COMMENT "Copying audio assets next to executable..."
)

# 2D Sidescroller example demonstrating platformer game mechanics
add_executable(vde_sidescroller
    sidescroller/main.cpp
)

target_link_libraries(vde_sidescroller PRIVATE vde)
add_dependencies(vde_sidescroller copy_example_shaders)

# Wireframe viewer example demonstrating shape switching, render modes, and interaction
add_executable(vde_wireframe_viewer
    wireframe_viewer/main.cpp
)

target_link_libraries(vde_wireframe_viewer PRIVATE vde)
add_dependencies(vde_wireframe_viewer copy_example_shaders)

# Multi-scene demo example demonstrating scene management, switching, and background simulation
add_executable(vde_multi_scene_demo
    multi_scene_demo/main.cpp
)

target_link_libraries(vde_multi_scene_demo PRIVATE vde)
add_dependencies(vde_multi_scene_demo copy_example_shaders)

# Quad viewport demo example demonstrating four simultaneous scenes in split-screen
add_executable(vde_quad_viewport_demo
    quad_viewport_demo/main.cpp
)

target_link_libraries(vde_quad_viewport_demo PRIVATE vde)
add_dependencies(vde_quad_viewport_demo copy_example_shaders)

