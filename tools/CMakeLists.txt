# VDE Tools CMakeLists.txt
# Tools are for asset creation and are more complex than examples.
# They can run in interactive (GUI) mode or script (batch) mode.

# Track shader files globally for all tool targets
file(GLOB_RECURSE VDE_SHADER_FILES CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/shaders/*
)

# Register shader sync for a tool executable.
# This ensures shader updates trigger copy even if the executable itself doesn't relink.
function(vde_add_shader_sync target_name)
    set(shader_stamp ${CMAKE_CURRENT_BINARY_DIR}/${target_name}_shaders.stamp)

    add_custom_command(
        OUTPUT ${shader_stamp}
        COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:${target_name}>/shaders
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/shaders
            $<TARGET_FILE_DIR:${target_name}>/shaders
        COMMAND ${CMAKE_COMMAND} -E touch ${shader_stamp}
        DEPENDS ${VDE_SHADER_FILES}
        COMMENT "Syncing shaders for ${target_name}..."
        VERBATIM
    )

    add_custom_target(${target_name}_sync_shaders ALL
        DEPENDS ${shader_stamp}
    )

    add_dependencies(${target_name} ${target_name}_sync_shaders)
endfunction()

# Add tool subdirectories
add_subdirectory(geometry_repl)
add_subdirectory(resource_editor)
add_subdirectory(vlauncher)
