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
