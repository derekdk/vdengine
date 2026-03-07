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
    RunLogStorage.cpp
    RunLogStorage.h
)

target_link_libraries(vde_vlauncher PRIVATE
    vde
    imgui_backend
    tomlplusplus::tomlplusplus
)

if(WIN32)
    target_link_libraries(vde_vlauncher PRIVATE ole32 uuid)
endif()

vde_add_shader_sync(vde_vlauncher)
