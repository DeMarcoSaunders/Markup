cmake_minimum_required(VERSION 3.16)
project(Markup VERSION 0.2.0 LANGUAGES C)

# CMake 4+ no longer honors ancient minimums inside dependencies (e.g. FetchContent raylib 5.0
# declares 3.0). This floor restores compatibility until we pin a newer raylib tag.
if(CMAKE_VERSION VERSION_GREATER_EQUAL "4.0")
    set(CMAKE_POLICY_VERSION_MINIMUM "3.5" CACHE STRING
        "CMake 4: allow fetched deps that still declare cmake_minimum_required below 3.5")
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(MARKUP_WITH_RAYLIB "Build the raylib backend and its demos" ON)
option(MARKUP_FETCH_RAYLIB "Fetch raylib via FetchContent instead of find_package" ON)
option(MARKUP_WITH_SKIA "Build the Skia backend and demo_skia (implies MARKUP_WITH_SDL)" OFF)
option(MARKUP_WITH_SDL "Build the SDL3 window harness and demo_soft (software backend, no GPU)" OFF)
option(MARKUP_FETCH_SDL3 "Fetch SDL3 via FetchContent when SDL is enabled" ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/markup/cmake")

# raylib is only acquired when its backend is wanted. A software-only build (-DMARKUP_WITH_RAYLIB=OFF)
# must not need a GPU windowing library, which is the whole premise of markup_soft.
if(MARKUP_WITH_RAYLIB)
    if(MARKUP_FETCH_RAYLIB)
        include(FetchContent)
        FetchContent_Declare(
            raylib
            GIT_REPOSITORY https://github.com/raysan5/raylib.git
            GIT_TAG 5.0
            GIT_SHALLOW TRUE
        )
        set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(raylib)
    else()
        find_package(raylib REQUIRED)
    endif()
endif()

add_library(markup_core STATIC
    markup/src/mu_core.c
    markup/src/mu_arena.c
)
target_include_directories(markup_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/markup/include>
    $<INSTALL_INTERFACE:include>
)

add_library(markup_layout_flex STATIC markup/src/mu_layout_flex.c)
target_link_libraries(markup_layout_flex PUBLIC markup_core)

add_library(markup_style STATIC
    markup/src/mu_style.c
    markup/src/mu_text.c
    markup/src/mu_text_layout.c
)
target_link_libraries(markup_style PUBLIC markup_core)
if(MSVC)
    # mu_text_layout.c calls snprintf. Consumers that also link raylib pull the UCRT shim in
    # transitively; standalone ones (the tests) do not, and fail with unresolved snprintf.
    target_link_libraries(markup_style PUBLIC legacy_stdio_definitions)
endif()

add_library(markup_input STATIC markup/src/mu_input.c)
target_link_libraries(markup_input PUBLIC markup_core)

add_library(markup_paint STATIC markup/src/mu_paint.c markup/src/mu_image_util.c markup/src/mu_image_decode.c)
target_link_libraries(markup_paint PUBLIC markup_core)
# Decoding is stb_image (vendored), so there is no libpng to find. That dependency used
# to be optional, which meant image loading silently did nothing wherever it was absent.

# Software rasterizer backend. No external dependencies by design — this is the path to
# bare-metal and non-desktop targets, so it always builds.
add_library(markup_soft STATIC markup/src/mu_soft.c markup/src/mu_soft_text.c markup/src/mu_soft_blur.c)
# markup_style supplies mu_text_style_init and the backend-neutral wrapping built on
# mu_text_measure; the two reference each other, which static libs resolve fine.
target_link_libraries(markup_soft PUBLIC markup_core markup_paint markup_style)
target_include_directories(markup_soft PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/markup/include>
    $<INSTALL_INTERFACE:include>
)
# PUBLIC so consumers select the matching struct MuRenderContext layout by linking (see mu_backend.h).
target_compile_definitions(markup_soft PUBLIC MU_BACKEND_SOFT=1)
add_library(markup::soft ALIAS markup_soft)

# Only when raylib is actually available: a software-only build (the bare-metal path)
# must not be forced to acquire a GPU windowing library it will never link.
if(TARGET raylib)
    add_library(markup_raylib STATIC markup/src/mu_raylib.c)
    target_link_libraries(markup_raylib PUBLIC markup_core markup_input markup_paint
        PRIVATE markup_widgets_basic raylib)
    # PUBLIC so consumers select the matching struct MuRenderContext layout by linking (see mu_backend.h).
    target_compile_definitions(markup_raylib PUBLIC MU_BACKEND_RAYLIB=1)
    add_library(markup::raylib ALIAS markup_raylib)
endif()

add_library(markup_widgets_basic STATIC markup/src/mu_widgets_basic.c markup/src/mu_scroll.c markup/src/mu_popup.c markup/src/mu_list.c markup/src/mu_tabs.c markup/src/mu_textinput.c)
target_link_libraries(markup_widgets_basic PUBLIC
    markup_core markup_layout_flex markup_style markup_paint
)

# 2D rigid body physics. Depends only on markup_core for MuVec2/MuRect, so it can be
# built, tested and debugged with no renderer and no window.
add_library(markup_physics STATIC markup/src/mu_physics.c markup/src/mu_physics_collide.c)
target_link_libraries(markup_physics PUBLIC markup_core)
target_include_directories(markup_physics PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/markup/include>
    $<INSTALL_INTERFACE:include>
)
add_library(markup::physics ALIAS markup_physics)

# The node-tree binding, kept out of markup_physics so that library stays usable with no
# context and no renderer. Needs only core: it moves nodes, it does not draw them.
add_library(markup_physics_node STATIC markup/src/mu_physics_node.c)
target_link_libraries(markup_physics_node PUBLIC markup_physics markup_core)
target_include_directories(markup_physics_node PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/markup/include>
    $<INSTALL_INTERFACE:include>
)
add_library(markup::physics_node ALIAS markup_physics_node)

# Spring animation. Depends only on core: it moves nodes, it does not draw them.
add_library(markup_anim STATIC markup/src/mu_anim.c)
target_link_libraries(markup_anim PUBLIC markup_core)
target_include_directories(markup_anim PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/markup/include>
    $<INSTALL_INTERFACE:include>
)
add_library(markup::anim ALIAS markup_anim)

add_library(markup_compose STATIC markup/src/mu_compose.c)
target_link_libraries(markup_compose PUBLIC markup_widgets_basic markup_layout_flex)
target_include_directories(markup_compose PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/markup/include>
)

add_library(markup::compose ALIAS markup_compose)

add_library(markup::paint ALIAS markup_paint)

add_library(markup::core ALIAS markup_core)
add_library(markup::layout_flex ALIAS markup_layout_flex)
add_library(markup::style ALIAS markup_style)
add_library(markup::input ALIAS markup_input)
add_library(markup::widgets_basic ALIAS markup_widgets_basic)

set(MARKUP_DEMO_FONT_ABS "${CMAKE_CURRENT_SOURCE_DIR}/apps/demo_minimal/assets/InterVariable.ttf")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/apps/cmake/markup_demo_font_config.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/markup_demo_font_config.h"
    @ONLY)

# apps/demo_font.c includes markup/mu_raylib.h, so it cannot compile in a raylib-free build.
if(TARGET raylib)
    add_library(markup_demo_font STATIC apps/demo_font.c)
    target_include_directories(markup_demo_font PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/apps>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
    )
    target_link_libraries(markup_demo_font PRIVATE raylib markup_raylib)
endif()

if(TARGET raylib)
add_executable(demo_minimal apps/demo_minimal/main.c)
target_link_libraries(demo_minimal PRIVATE
    markup_demo_font markup_widgets_basic markup_raylib markup_style markup_layout_flex
    markup_input markup_core raylib
)
set_target_properties(demo_minimal PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:demo_minimal>")
add_custom_command(TARGET demo_minimal POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/apps/demo_minimal/assets"
    "$<TARGET_FILE_DIR:demo_minimal>/assets"
)

add_executable(demo_retheme apps/demo_retheme/main.c)
target_link_libraries(demo_retheme PRIVATE
    markup_demo_font markup_widgets_basic markup_raylib markup_style markup_layout_flex
    markup_input markup_core markup_paint raylib
)
set_target_properties(demo_retheme PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:demo_retheme>")
add_custom_command(TARGET demo_retheme POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/apps/demo_minimal/assets"
    "$<TARGET_FILE_DIR:demo_retheme>/assets"
)
endif()

# ------------------------------------------------------------------------------
# SDL3 window harness. Backend-neutral — mu_sdl.c presents through
# mu_present_pixel_data(), so it is shared by the Skia and software backends and the
# final executable supplies whichever one it wants.
# ------------------------------------------------------------------------------
if(MARKUP_WITH_SDL OR MARKUP_WITH_SKIA)
    if(MARKUP_FETCH_SDL3)
        include(FetchContent)
        FetchContent_Declare(
            SDL3
            GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
            GIT_TAG release-3.2.8
            GIT_SHALLOW TRUE
        )
        set(SDL_SHARED OFF CACHE BOOL "" FORCE)
        set(SDL_STATIC ON CACHE BOOL "" FORCE)
        set(SDL_TEST OFF CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(SDL3)
    else()
        find_package(SDL3 REQUIRED CONFIG)
    endif()

    if(TARGET SDL3::SDL3-static)
        set(MARKUP_SDL3_LIB SDL3::SDL3-static)
    else()
        set(MARKUP_SDL3_LIB SDL3::SDL3)
    endif()

    add_library(markup_sdl STATIC markup/src/mu_sdl.c)
    target_link_libraries(markup_sdl PUBLIC markup_core markup_input ${MARKUP_SDL3_LIB}
        PRIVATE markup_widgets_basic)
    add_library(markup::sdl ALIAS markup_sdl)
endif()

# Software-rendered demo: no GPU, no Skia. Also the visual reference for the raster path.
if(MARKUP_WITH_SDL)
    add_executable(demo_soft apps/demo_soft/main.c)
    target_link_libraries(demo_soft PRIVATE
        markup_soft markup_sdl markup_compose markup_widgets_basic markup_style
        markup_layout_flex markup_input markup_paint markup_core markup_anim ${MARKUP_SDL3_LIB}
    )
    # For markup_demo_font_config.h (MARKUP_DEMO_FONT_FILE_ABS), the fallback font path.
    target_include_directories(demo_soft PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>)
    set_target_properties(demo_soft PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:demo_soft>")
    add_custom_command(TARGET demo_soft POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/apps/demo_minimal/assets"
        "$<TARGET_FILE_DIR:demo_soft>/assets"
    )

    # Rigid bodies driving the node tree, on the same software path.
    add_executable(demo_physics apps/demo_physics/main.c)
    target_link_libraries(demo_physics PRIVATE
        markup_soft markup_sdl markup_physics_node markup_widgets_basic markup_style
        markup_layout_flex markup_input markup_paint markup_core ${MARKUP_SDL3_LIB}
    )
    target_include_directories(demo_physics PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>)
    set_target_properties(demo_physics PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:demo_physics>")
    add_custom_command(TARGET demo_physics POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/apps/demo_minimal/assets"
        "$<TARGET_FILE_DIR:demo_physics>/assets"
    )
endif()

if(MARKUP_WITH_SKIA)
    enable_language(CXX)
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/markup/cmake")
    include(FindSkia)
    if(NOT MARKUP_SKIA_FOUND)
        find_package(skia CONFIG REQUIRED)
        set(MARKUP_SKIA_FOUND TRUE)
        if(TARGET skia)
            set(MARKUP_SKIA_LIBRARIES skia)
        elseif(TARGET Skia::skia)
            set(MARKUP_SKIA_LIBRARIES Skia::skia)
        endif()
    endif()

    add_library(markup_skia STATIC markup/src/mu_skia.cpp)
    set_target_properties(markup_skia PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
    target_link_libraries(markup_skia PUBLIC markup_core markup_input markup_paint ${MARKUP_SKIA_LIBRARIES})
    target_include_directories(markup_skia PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/markup/include>
    )
    # PUBLIC so consumers select the matching struct MuRenderContext layout by linking (see mu_backend.h).
    target_compile_definitions(markup_skia PUBLIC MU_BACKEND_SKIA=1)

    add_library(markup::skia ALIAS markup_skia)

    add_library(markup_demo_font_skia STATIC apps/demo_font_skia.c)
    target_include_directories(markup_demo_font_skia PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/apps>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
    )
    target_link_libraries(markup_demo_font_skia PRIVATE markup_skia)

    add_executable(demo_skia apps/demo_skia/main.c)
    target_link_libraries(demo_skia PRIVATE
        markup_compose markup_demo_font_skia markup_widgets_basic markup_sdl markup_skia
        markup_style markup_layout_flex markup_input markup_core markup_paint
        ${MARKUP_SKIA_LIBRARIES} ${MARKUP_SDL3_LIB}
    )
    set_target_properties(demo_skia PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:demo_skia>")
    add_custom_command(TARGET demo_skia POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/apps/demo_minimal/assets"
        "$<TARGET_FILE_DIR:demo_skia>/assets"
    )
endif()

option(MARKUP_BUILD_TESTS "Build Markup test suite" ON)
if(MARKUP_BUILD_TESTS)
    enable_testing()
    add_executable(test_raster tests/test_raster.c)
    target_link_libraries(test_raster PRIVATE markup_soft)
    add_test(NAME raster COMMAND test_raster)

    # Drives real widgets headlessly; markup_soft supplies text metrics so no window is needed.
    add_executable(test_textinput tests/test_textinput.c)
    target_link_libraries(test_textinput PRIVATE markup_widgets_basic markup_input markup_soft)
    add_test(NAME textinput COMMAND test_textinput)

    # Needs a real TTF; the path is passed in so the test does not depend on the cwd.
    add_executable(test_text tests/test_text.c)
    target_link_libraries(test_text PRIVATE markup_soft)
    add_test(NAME text COMMAND test_text
        "${CMAKE_CURRENT_SOURCE_DIR}/apps/demo_minimal/assets/InterVariable.ttf")

    # Blit tests build their images in memory; the trailing path also exercises real
    # file decoding, which is the part that silently failed when libpng was absent.
    add_executable(test_image tests/test_image.c)
    target_link_libraries(test_image PRIVATE markup_soft)
    add_test(NAME image COMMAND test_image
        "${CMAKE_CURRENT_SOURCE_DIR}/apps/demo_minimal/assets/icon_settings.png")

    add_executable(test_damage tests/test_damage.c)
    target_link_libraries(test_damage PRIVATE markup_widgets_basic markup_input markup_soft)
    add_test(NAME damage COMMAND test_damage)

    # No renderer needed: physics is independent of the UI stack.
    add_executable(test_physics tests/test_physics.c)
    target_link_libraries(test_physics PRIVATE markup_physics)
    add_test(NAME physics COMMAND test_physics)

    # The node binding does need one, since it asserts on damage as well as on motion.
    add_executable(test_physics_node tests/test_physics_node.c)
    target_link_libraries(test_physics_node PRIVATE
        markup_physics_node markup_widgets_basic markup_input markup_soft)
    add_test(NAME physics_node COMMAND test_physics_node)

    add_executable(test_anim tests/test_anim.c)
    target_link_libraries(test_anim PRIVATE
        markup_anim markup_widgets_basic markup_input markup_soft)
    add_test(NAME anim COMMAND test_anim)

    add_executable(test_blur tests/test_blur.c)
    target_link_libraries(test_blur PRIVATE markup_soft)
    add_test(NAME blur COMMAND test_blur)
endif()

include(GNUInstallDirs)
# install(EXPORT) on STATIC libs folds private deps (raylib) into the exported interface.
# CMake then requires raylib (and GLFW) in the same export — not viable. Export only raylib-free
# targets via MarkupTargets.cmake; markup::raylib / markup::widgets_basic are STATIC IMPORTED in
# MarkupImportedRaylib.cmake after find_dependency(raylib).
install(TARGETS markup_core markup_layout_flex markup_style markup_input
    EXPORT MarkupTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
set(MARKUP_INSTALL_EXTRA markup_widgets_basic markup_soft)
if(TARGET markup_raylib)
    list(APPEND MARKUP_INSTALL_EXTRA markup_raylib)
endif()
install(TARGETS ${MARKUP_INSTALL_EXTRA}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(DIRECTORY markup/include/markup DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT MarkupTargets
    FILE MarkupTargets.cmake
    NAMESPACE markup::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Markup
)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/MarkupConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/markup/cmake/MarkupConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/MarkupConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Markup
)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/MarkupConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/MarkupConfigVersion.cmake"
    "${CMAKE_CURRENT_SOURCE_DIR}/markup/cmake/MarkupImportedRaylib.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Markup
)
