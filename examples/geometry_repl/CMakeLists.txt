# Geometry REPL Tool Example
#
# Interactive tool for creating and exporting custom geometry using ImGui REPL interface.

# The example executable
add_executable(vde_geometry_repl
    main.cpp
)

target_link_libraries(vde_geometry_repl PRIVATE
    vde
    imgui_backend
)

add_dependencies(vde_geometry_repl copy_example_shaders)

# Copy shaders to the geometry_repl output directory
add_custom_command(TARGET vde_geometry_repl POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/shaders
        $<TARGET_FILE_DIR:vde_geometry_repl>/shaders
    COMMENT "Copying shaders to geometry_repl executable directory..."
)
