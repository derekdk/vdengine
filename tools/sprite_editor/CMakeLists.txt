# Sprite Editor Tool
#
# Visual tool for creating, previewing, and exporting spritesheet assets.
#
# Usage:
#   vde_sprite_editor              - Interactive mode
#   vde_sprite_editor script.txt   - Script mode

add_executable(vde_sprite_editor
    main.cpp
    SpriteEditorScene.cpp
    SpriteEditorScene.h
    SpriteDocument.cpp
    SpriteDocument.h
)

target_link_libraries(vde_sprite_editor PRIVATE
    vde
    imgui_backend
    tomlplusplus::tomlplusplus
)

# Sync shaders so the tool can find them at runtime
vde_add_shader_sync(vde_sprite_editor)
