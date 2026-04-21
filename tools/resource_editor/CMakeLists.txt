# Resource Editor Tool
#
# 2D pixel art editor with metadata-driven command system, ImGui UI,
# REPL console, and scriptable batch mode.
#
# Usage:
#   vde_resource_editor                  - Interactive mode
#   vde_resource_editor <script.txt>     - Script mode (batch)

add_executable(vde_resource_editor
    # Entry point
    main.cpp

    # Core subsystems
    ResourceEditorScene.h
    ResourceEditorScene.cpp
    CommandSystem.h
    CommandSystem.cpp
    ImageDocument.h
    ImageDocument.cpp
    CanvasRegistry.h
    CanvasRegistry.cpp
    ToolPalette.h
    ToolPalette.cpp
    EditorPanels.h
    EditorPanels.cpp
    FileOperations.h
    FileOperations.cpp

    # Command infrastructure
    commands/CommandTypes.h
    commands/CommandBase.h
    commands/CommandBase.cpp
    commands/CommandArgParser.h
    commands/CommandArgParser.cpp
    commands/CommandRegistry.h
    commands/CommandRegistry.cpp
    commands/EditorContext.h
    commands/EditorContext.cpp
    commands/AllCommands.cpp

    # Global commands
    commands/global/CreateCanvasCommand.h
    commands/global/SelectCommand.h
    commands/global/ListCommand.h
    commands/global/DeleteCanvasCommand.h
    commands/global/RenameCanvasCommand.h
    commands/global/LoadCommand.h
    commands/global/SaveCommand.h
    commands/global/ExportCommand.h
    commands/global/HelpCommand.h
    commands/global/HistoryCommand.h
    commands/global/ZoomCommand.h
    commands/global/GridCommand.h
    commands/global/ExitCommand.h
    commands/global/SetColorCommand.h
    commands/global/SetToolCommand.h
    commands/global/SetSizeCommand.h    commands/global/DefineColorCommand.h
    commands/global/ListColorsCommand.h
    commands/global/UndefineColorCommand.h
    commands/global/CopyHostCommand.h
    commands/global/RehostCommand.h
    # Canvas commands
    commands/canvas/FillCommand.h
    commands/canvas/SetCommand.h
    commands/canvas/DrawLineCommand.h
    commands/canvas/DrawRectCommand.h
    commands/canvas/DrawCircleCommand.h
    commands/canvas/DrawEllipseCommand.h
    commands/canvas/FloodFillCommand.h
    commands/canvas/UndoCommand.h
    commands/canvas/RedoCommand.h
    commands/canvas/ResizeCommand.h
    commands/canvas/CropCommand.h
    commands/canvas/FlipCommand.h
    commands/canvas/RotateCommand.h
    commands/canvas/ClearCommand.h
    commands/canvas/DrawArcCommand.h
    commands/canvas/DrawBezierCommand.h
    commands/canvas/DrawImageCommand.h
)

target_include_directories(vde_resource_editor PRIVATE
    ${CMAKE_SOURCE_DIR}/third_party/stb
)

target_link_libraries(vde_resource_editor PRIVATE
    vde
    imgui_backend
)

# Windows COM libraries for native file dialogs
if(WIN32)
    target_link_libraries(vde_resource_editor PRIVATE ole32 uuid)
endif()

target_compile_options(vde_resource_editor PRIVATE ${VDE_WARNING_FLAGS})

vde_add_shader_sync(vde_resource_editor)
