# VDE Examples CMakeLists.txt

# Basic triangle example
add_executable(vde_triangle_example
    triangle/main.cpp
)

target_link_libraries(vde_triangle_example PRIVATE vde)

# Copy shader files from main shaders directory to build directory
add_custom_command(TARGET vde_triangle_example POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/shaders
    $<TARGET_FILE_DIR:vde_triangle_example>/shaders
    COMMENT "Copying shader files..."
)

# Simple game example using Game API
add_executable(vde_simple_game_example
    simple_game/main.cpp
)

target_link_libraries(vde_simple_game_example PRIVATE vde)

# Copy shader files for simple_game example
add_custom_command(TARGET vde_simple_game_example POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/shaders
    $<TARGET_FILE_DIR:vde_simple_game_example>/shaders
    COMMENT "Copying shader files..."
)

# Sprite demo example demonstrating SpriteEntity
add_executable(vde_sprite_demo
    sprite_demo/main.cpp
)

target_link_libraries(vde_sprite_demo PRIVATE vde)

# Copy shader files for sprite_demo example
add_custom_command(TARGET vde_sprite_demo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/shaders
    $<TARGET_FILE_DIR:vde_sprite_demo>/shaders
    COMMENT "Copying shader files..."
)

# World bounds demo example demonstrating Phase 2.5 features
add_executable(vde_world_bounds_demo
    world_bounds_demo/main.cpp
)

target_link_libraries(vde_world_bounds_demo PRIVATE vde)

# Copy shader files for world_bounds_demo example
add_custom_command(TARGET vde_world_bounds_demo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/shaders
    $<TARGET_FILE_DIR:vde_world_bounds_demo>/shaders
    COMMENT "Copying shader files..."
)

# Materials & Lighting demo example demonstrating Phase 4 features
add_executable(vde_materials_lighting_demo
    materials_lighting_demo/main.cpp
)

target_link_libraries(vde_materials_lighting_demo PRIVATE vde)

# Copy shader files for materials_lighting_demo example
add_custom_command(TARGET vde_materials_lighting_demo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/shaders
    $<TARGET_FILE_DIR:vde_materials_lighting_demo>/shaders
    COMMENT "Copying shader files..."
)

# Resource Management demo example demonstrating Phase 5 features
add_executable(vde_resource_demo
    resource_demo/main.cpp
)

target_link_libraries(vde_resource_demo PRIVATE vde)

# Copy shader files for resource_demo example
add_custom_command(TARGET vde_resource_demo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/shaders
    $<TARGET_FILE_DIR:vde_resource_demo>/shaders
    COMMENT "Copying shader files..."
)
