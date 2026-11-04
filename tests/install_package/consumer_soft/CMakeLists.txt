cmake_minimum_required(VERSION 3.16)
project(markup_consumer_soft LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# A standalone project on purpose: it is configured and built by tests/install/run.cmake
# against an installed prefix, never added to the main build. Nothing here may refer to
# the Markup source tree.
#
# Deliberately no find_package(raylib): a software-only consumer must not need it, which
# is what MarkupConfig.cmake's guard around MarkupImportedRaylib.cmake buys.
find_package(Markup CONFIG REQUIRED)

add_executable(consumer_soft main.c)

# Every exported library, so a missing install() or a wrong link interface fails here.
target_link_libraries(consumer_soft PRIVATE
    markup::compose
    markup::widgets_basic
    markup::layout_flex
    markup::style
    markup::paint
    markup::soft
    markup::input
    markup::anim
    markup::physics
    markup::physics_node
    markup::core
)

# One predictable path for the driver to run, under single- and multi-config generators
# alike (the latter would otherwise bury it in a per-config subdirectory).
set(_consumer_out "${CMAKE_BINARY_DIR}/bin")
set_target_properties(consumer_soft PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${_consumer_out}")
foreach(_cfg IN LISTS CMAKE_CONFIGURATION_TYPES)
    string(TOUPPER "${_cfg}" _CFG)
    set_target_properties(consumer_soft PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_${_CFG} "${_consumer_out}")
endforeach()
