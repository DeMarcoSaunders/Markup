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
else()
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

add_library(markup_style STATIC markup/src/mu_style.c)
target_link_libraries(markup_style PUBLIC markup_core)

add_library(markup_input STATIC markup/src/mu_input.c)
target_link_libraries(markup_input PUBLIC markup_core)

add_library(markup_raylib STATIC markup/src/mu_raylib.c)
target_link_libraries(markup_raylib PUBLIC markup_core markup_input)
target_link_libraries(markup_raylib PRIVATE raylib)

add_library(markup_widgets_basic STATIC markup/src/mu_widgets_basic.c)
target_link_libraries(markup_widgets_basic PUBLIC
    markup_core markup_layout_flex markup_style markup_raylib
)
target_link_libraries(markup_widgets_basic PRIVATE raylib)

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
target_link_libraries(markup_demo_font PRIVATE raylib markup_raylib)

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
    markup_input markup_core raylib
)
set_target_properties(demo_retheme PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:demo_retheme>")
add_custom_command(TARGET demo_retheme POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/apps/demo_minimal/assets"
    "$<TARGET_FILE_DIR:demo_retheme>/assets"
)

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
