# Resource Editor Tool
#
# Interactive and scriptable pixel art editor with multi-canvas support.
#
# Usage:
#   vde_resource_editor              - Interactive mode
#   vde_resource_editor script.txt   - Script mode

add_executable(vde_resource_editor
    main.cpp
    ResourceEditorScene.cpp
    ResourceEditorScene.h
    ImageDocument.cpp
    ImageDocument.h
    CanvasRegistry.cpp
    CanvasRegistry.h
    CommandSystem.cpp
    CommandSystem.h
    FileOperations.cpp
    FileOperations.h
    ToolPalette.cpp
    ToolPalette.h
    EditorPanels.cpp
    EditorPanels.h
)

target_link_libraries(vde_resource_editor PRIVATE
    vde
    imgui_backend
)

# stb headers for image I/O (private to vde, so we include explicitly)
target_include_directories(vde_resource_editor PRIVATE
    ${CMAKE_SOURCE_DIR}/third_party/stb
)

# Windows COM libraries for native file dialogs
if(WIN32)
    target_link_libraries(vde_resource_editor PRIVATE ole32 uuid)
endif()

add_dependencies(vde_resource_editor copy_tool_shaders)

# Copy shaders to the resource_editor output directory
add_custom_command(TARGET vde_resource_editor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/shaders
        $<TARGET_FILE_DIR:vde_resource_editor>/shaders
    COMMENT "Copying shaders to resource editor executable directory..."
)
