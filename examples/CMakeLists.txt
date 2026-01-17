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
