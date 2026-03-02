# VLauncher Tool
#
# Interactive launcher for VDE examples and tools with executable freshness checks.

add_executable(vde_vlauncher
    main.cpp
    VLauncherScene.cpp
    VLauncherScene.h
    ExecutableScanner.cpp
    ExecutableScanner.h
    GitUtils.cpp
    GitUtils.h
    ProcessLauncher.cpp
    ProcessLauncher.h
)

target_link_libraries(vde_vlauncher PRIVATE
    vde
    imgui_backend
)

if(WIN32)
    target_link_libraries(vde_vlauncher PRIVATE ole32 uuid)
endif()

add_dependencies(vde_vlauncher copy_tool_shaders)

add_custom_command(TARGET vde_vlauncher POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/shaders
        $<TARGET_FILE_DIR:vde_vlauncher>/shaders
    COMMENT "Copying shaders to vlauncher executable directory..."
)
