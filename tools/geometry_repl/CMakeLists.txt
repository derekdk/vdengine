# Geometry REPL Tool
#
# Interactive and scriptable tool for creating custom geometry and exporting to OBJ format.
#
# Usage:
#   vde_geometry_repl              - Interactive mode  
#   vde_geometry_repl script.txt   - Script mode

# The tool executable
add_executable(vde_geometry_repl
    main.cpp
    CommandRegistry.cpp
    CommandRegistry.h
    ReplConsole.cpp
    ReplConsole.h
    FileDialog.cpp
    FileDialog.h
    GeometryReplScene.cpp
    GeometryReplScene.h
    GeometryObject.h
)

target_link_libraries(vde_geometry_repl PRIVATE
    vde
    imgui_backend
)

# Windows COM libraries for native file dialogs
if(WIN32)
    target_link_libraries(vde_geometry_repl PRIVATE ole32 uuid)
endif()

target_compile_options(vde_geometry_repl PRIVATE ${VDE_WARNING_FLAGS})

vde_add_shader_sync(vde_geometry_repl)
