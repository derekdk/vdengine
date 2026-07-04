add_vde_game(vde_level_builder
    main.cpp
    LevelBuilderScene.cpp
    PlayerController.cpp
    TileMapSession.cpp
)

file(GLOB_RECURSE VDE_LEVEL_BUILDER_ASSET_FILES CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/assets/tiled/*
)

set(VDE_LEVEL_BUILDER_ASSET_STAMP
    ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/vde_level_builder_assets.stamp
)

add_custom_command(
    OUTPUT ${VDE_LEVEL_BUILDER_ASSET_STAMP}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}
    COMMAND ${CMAKE_COMMAND} -E rm -r -f $<TARGET_FILE_DIR:vde_level_builder>/assets/tiled
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:vde_level_builder>/assets/tiled
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets/tiled
        $<TARGET_FILE_DIR:vde_level_builder>/assets/tiled
    COMMAND ${CMAKE_COMMAND} -E touch ${VDE_LEVEL_BUILDER_ASSET_STAMP}
    DEPENDS ${VDE_LEVEL_BUILDER_ASSET_FILES}
    COMMENT "Syncing level builder Tiled assets..."
    VERBATIM
)

add_custom_target(vde_level_builder_sync_assets
    DEPENDS ${VDE_LEVEL_BUILDER_ASSET_STAMP}
)

add_dependencies(vde_level_builder vde_level_builder_sync_assets)