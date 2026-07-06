# VDE Games CMakeLists.txt
# Games are more involved than examples and are organized into per-game directories.

file(GLOB_RECURSE VDE_GAME_SHADER_FILES CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/shaders/*
)

function(vde_add_game_shader_sync target_name)
    set(shader_stamp ${CMAKE_CURRENT_BINARY_DIR}/${target_name}_shaders.stamp)

    add_custom_command(
        OUTPUT ${shader_stamp}
        COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:${target_name}>/shaders
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/shaders
            $<TARGET_FILE_DIR:${target_name}>/shaders
        COMMAND ${CMAKE_COMMAND} -E touch ${shader_stamp}
        DEPENDS ${VDE_GAME_SHADER_FILES}
        COMMENT "Syncing shaders for ${target_name}..."
        VERBATIM
    )

    add_custom_target(${target_name}_sync_shaders ALL
        DEPENDS ${shader_stamp}
    )

    add_dependencies(${target_name} ${target_name}_sync_shaders)
endfunction()

function(add_vde_game target_name)
    cmake_parse_arguments(ARG "NO_IMGUI" "" "" ${ARGN})
    add_executable(${target_name} ${ARG_UNPARSED_ARGUMENTS})
    if(ARG_NO_IMGUI)
        target_link_libraries(${target_name} PRIVATE vde)
    else()
        target_link_libraries(${target_name} PRIVATE vde imgui_backend)
        target_compile_definitions(${target_name} PRIVATE VDE_GAME_USE_IMGUI)
    endif()
    target_compile_options(${target_name} PRIVATE ${VDE_WARNING_FLAGS})
    vde_add_game_shader_sync(${target_name})
endfunction()

add_subdirectory(fishing_game)
add_subdirectory(level_builder)
add_subdirectory(pong)
