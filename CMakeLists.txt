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

option(MARKUP_FETCH_RAYLIB "Fetch raylib via FetchContent" ON)
option(MARKUP_WITH_SKIA "Build Skia + SDL3 desktop backend and demo_skia" OFF)
option(MARKUP_FETCH_SDL3 "Fetch SDL3 via FetchContent when MARKUP_WITH_SKIA is ON" ON)
option(MARKUP_WITH_BLUR "Build optional blur FX stub library" OFF)
option(MARKUP_WITH_STYLE_FILE "Build optional stylesheet file parser stub" OFF)
option(MARKUP_WITH_EXTRA_WIDGETS "Build optional extra widgets stub library" OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/markup/cmake")

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
elseif(NOT MARKUP_WITH_SKIA)
    find_package(raylib REQUIRED)
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

add_library(markup_input STATIC markup/src/mu_input.c)
target_link_libraries(markup_input PUBLIC markup_core)

add_library(markup_paint STATIC markup/src/mu_paint.c markup/src/mu_image_util.c markup/src/mu_image_decode.c)
target_link_libraries(markup_paint PUBLIC markup_core)
find_package(PNG QUIET)
if(PNG_FOUND)
    target_compile_definitions(markup_paint PRIVATE MU_HAVE_LIBPNG=1)
    target_link_libraries(markup_paint PUBLIC PNG::PNG)
endif()

add_library(markup_raylib STATIC markup/src/mu_raylib.c)
target_link_libraries(markup_raylib PUBLIC markup_core markup_input markup_paint
    PRIVATE markup_widgets_basic raylib)

add_library(markup_widgets_basic STATIC markup/src/mu_widgets_basic.c markup/src/mu_scroll.c markup/src/mu_popup.c markup/src/mu_list.c markup/src/mu_tabs.c markup/src/mu_textinput.c)
target_link_libraries(markup_widgets_basic PUBLIC
    markup_core markup_layout_flex markup_style markup_paint
)

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
add_library(markup::raylib ALIAS markup_raylib)
add_library(markup::widgets_basic ALIAS markup_widgets_basic)

if(MARKUP_WITH_BLUR)
    add_library(markup_fx_blur STATIC markup/src/mu_fx_blur.c)
    target_link_libraries(markup_fx_blur PUBLIC markup_core)
    add_library(markup::fx_blur ALIAS markup_fx_blur)
endif()

if(MARKUP_WITH_STYLE_FILE)
    add_library(markup_style_parse STATIC markup/src/mu_style_parse.c)
    target_link_libraries(markup_style_parse PUBLIC markup_style)
    add_library(markup::style_parse ALIAS markup_style_parse)
endif()

if(MARKUP_WITH_EXTRA_WIDGETS)
    add_library(markup_widgets_extra STATIC markup/src/mu_widgets_extra.c)
    target_link_libraries(markup_widgets_extra PUBLIC markup_widgets_basic)
    add_library(markup::widgets_extra ALIAS markup_widgets_extra)
endif()

set(MARKUP_DEMO_FONT_ABS "${CMAKE_CURRENT_SOURCE_DIR}/apps/demo_minimal/assets/InterVariable.ttf")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/apps/cmake/markup_demo_font_config.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/markup_demo_font_config.h"
    @ONLY)

add_library(markup_demo_font STATIC apps/demo_font.c)
target_include_directories(markup_demo_font PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/apps>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
)
if(TARGET raylib)
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

    add_library(markup_skia STATIC markup/src/mu_skia.cpp)
    set_target_properties(markup_skia PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
    target_link_libraries(markup_skia PUBLIC markup_core markup_input markup_paint ${MARKUP_SKIA_LIBRARIES})
    target_include_directories(markup_skia PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/markup/include>
    )

    add_library(markup_sdl STATIC markup/src/mu_sdl.c)
    target_link_libraries(markup_sdl PUBLIC markup_core markup_input markup_skia ${MARKUP_SDL3_LIB}
        PRIVATE markup_widgets_basic)

    add_library(markup::skia ALIAS markup_skia)
    add_library(markup::sdl ALIAS markup_sdl)

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
install(TARGETS markup_raylib markup_widgets_basic
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
