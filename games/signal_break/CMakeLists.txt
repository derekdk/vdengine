add_vde_game(vde_signal_break NO_IMGUI
    main.cpp
    SignalBreakScene.cpp
    Input.cpp
    Level.cpp
    Gameplay.cpp
    Artwork.cpp
    Presentation.cpp
)

file(GLOB_RECURSE VDE_SIGNAL_BREAK_ASSET_FILES CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/assets/signal_break/*
)

set(VDE_SIGNAL_BREAK_ASSET_STAMP
    ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/vde_signal_break_assets.stamp
)

add_custom_command(
    OUTPUT ${VDE_SIGNAL_BREAK_ASSET_STAMP}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}
    COMMAND ${CMAKE_COMMAND} -E rm -r -f $<TARGET_FILE_DIR:vde_signal_break>/assets/signal_break
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:vde_signal_break>/assets/signal_break
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets/signal_break
        $<TARGET_FILE_DIR:vde_signal_break>/assets/signal_break
    COMMAND ${CMAKE_COMMAND} -E touch ${VDE_SIGNAL_BREAK_ASSET_STAMP}
    DEPENDS ${VDE_SIGNAL_BREAK_ASSET_FILES}
    COMMENT "Syncing Signal Break art assets..."
    VERBATIM
)

add_custom_target(vde_signal_break_sync_assets
    DEPENDS ${VDE_SIGNAL_BREAK_ASSET_STAMP}
)

add_dependencies(vde_signal_break vde_signal_break_sync_assets)
