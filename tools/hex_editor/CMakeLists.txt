# Hex Editor
#
# Interactive tool for opening, inspecting, and comparing binary files.
# Displays both hex and ASCII representations side-by-side.
# Multiple files can be open at once and compared in a split-panel view.
#
# Usage:
#   vde_hex_editor              - Interactive mode
#   vde_hex_editor script.txt   - Script mode

add_executable(vde_hex_editor
    main.cpp
    HexEditorScene.cpp
    HexEditorScene.h
    ../geometry_repl/FileDialog.cpp
    ../geometry_repl/FileDialog.h
)

target_link_libraries(vde_hex_editor PRIVATE
    vde
    imgui_backend
)

# Windows COM libraries for native file dialogs
if(WIN32)
    target_link_libraries(vde_hex_editor PRIVATE ole32 uuid)
endif()

target_compile_options(vde_hex_editor PRIVATE ${VDE_WARNING_FLAGS})

vde_add_shader_sync(vde_hex_editor)
