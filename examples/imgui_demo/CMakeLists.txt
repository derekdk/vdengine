# ImGui integration demo
#
# ImGui is now fetched at the parent level (examples/CMakeLists.txt)
# so all examples can optionally use it.

# The example executable
add_executable(vde_imgui_demo
    main.cpp
)

target_link_libraries(vde_imgui_demo PRIVATE
    vde
    imgui_backend
)

add_dependencies(vde_imgui_demo copy_example_shaders)

# Copy shaders to the imgui_demo output directory
add_custom_command(TARGET vde_imgui_demo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/shaders
        $<TARGET_FILE_DIR:vde_imgui_demo>/shaders
    COMMENT "Copying shaders to imgui_demo executable directory..."
)
