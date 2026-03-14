# VDE Tests CMakeLists.txt

# Fetch GoogleTest
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Test sources
set(VDE_TEST_SOURCES
    main_test.cpp
    Window_test.cpp
    Camera_test.cpp
    HexGeometry_test.cpp
    ShaderCache_test.cpp
    Types_test.cpp
    Mesh_test.cpp
    SpriteEntity_test.cpp
    # Phase 1 tests
    Entity_test.cpp
    Scene_test.cpp
    GameCamera_test.cpp
    LightBox_test.cpp
    # Phase 2.5 tests
    WorldUnits_test.cpp
    WorldBounds_test.cpp
    CameraBounds_test.cpp
    # Phase 4 tests
    Material_test.cpp
    # Phase 5 tests
    Texture_test.cpp
    ResourceManager_test.cpp
    # Scheduler tests
    Scheduler_test.cpp
    # Phase 2 multi-scene tests
    SceneGroup_test.cpp
    # Phase 3 viewport tests
    ViewportRect_test.cpp
    # Phase 4 scene phase callback & audio event tests
    AudioEvent_test.cpp
    # Phase 5 physics tests
    PhysicsScene_test.cpp
    # Phase 6 physics entity tests
    PhysicsEntity_test.cpp
    # Phase 7 thread pool tests
    ThreadPool_test.cpp
    # Joystick/gamepad tests
    Joystick_test.cpp
    # Input script tests
    InputScript_test.cpp
    # Storage tests
    StorageManager_test.cpp
    # Offscreen render target tests
    OffscreenRenderTarget_test.cpp
    # Transition tests
    Transition_test.cpp
    TransitionManager_test.cpp
    VLauncherRunLogStorage_test.cpp
    VLauncherUtilities_test.cpp
    # Gameplay utility tests
    Math2D_test.cpp
    Timing_test.cpp
    Random_test.cpp

    # VLauncher storage utilities
    ${CMAKE_SOURCE_DIR}/tools/vlauncher/RunLogStorage.cpp
    ${CMAKE_SOURCE_DIR}/tools/vlauncher/ExecutableScanner.cpp
    ${CMAKE_SOURCE_DIR}/tools/vlauncher/GitUtils.cpp
    ${CMAKE_SOURCE_DIR}/tools/vlauncher/ProcessLauncher.cpp
)

# Create test executable
add_executable(vde_tests ${VDE_TEST_SOURCES})

target_link_libraries(vde_tests PRIVATE
    vde
    GTest::gtest
    GTest::gtest_main
    tomlplusplus::tomlplusplus
)

target_include_directories(vde_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/stb
)

# Register tests with CTest
include(GoogleTest)
gtest_discover_tests(vde_tests)

# =============================================================================
# Resource Editor Tests
#
# Separate target that compiles the resource editor domain code (no GPU/ImGui)
# and links AllCommands.cpp to fire every REGISTER_COMMAND static initializer.
# =============================================================================

set(VDE_RESOURCE_EDITOR_SRC ${CMAKE_SOURCE_DIR}/tools/resource_editor)

add_executable(vde_resource_editor_tests
    CommandSystem_test.cpp
    ToolPalette_test.cpp
    Phase2Commands_test.cpp
    Phase2Commands_test.cpp

    # Domain sources (main.cpp and EditorPanels excluded — no GPU/window needed)
    ${VDE_RESOURCE_EDITOR_SRC}/CommandSystem.cpp
    ${VDE_RESOURCE_EDITOR_SRC}/CanvasRegistry.cpp
    ${VDE_RESOURCE_EDITOR_SRC}/ImageDocument.cpp
    ${VDE_RESOURCE_EDITOR_SRC}/ToolPalette.cpp
    ${VDE_RESOURCE_EDITOR_SRC}/FileOperations.cpp
    ${VDE_RESOURCE_EDITOR_SRC}/commands/CommandBase.cpp
    ${VDE_RESOURCE_EDITOR_SRC}/commands/CommandArgParser.cpp
    ${VDE_RESOURCE_EDITOR_SRC}/commands/CommandRegistry.cpp
    ${VDE_RESOURCE_EDITOR_SRC}/commands/EditorContext.cpp
    ${VDE_RESOURCE_EDITOR_SRC}/commands/AllCommands.cpp
)

target_include_directories(vde_resource_editor_tests PRIVATE
    ${VDE_RESOURCE_EDITOR_SRC}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/third_party/stb
)

target_link_libraries(vde_resource_editor_tests PRIVATE
    vde
    GTest::gtest
    GTest::gtest_main
)

if(WIN32)
    target_link_libraries(vde_resource_editor_tests PRIVATE ole32 uuid)
endif()

gtest_discover_tests(vde_resource_editor_tests)
