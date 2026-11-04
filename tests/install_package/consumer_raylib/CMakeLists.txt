cmake_minimum_required(VERSION 3.16)
project(markup_consumer_raylib LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Standalone, built by tests/install/run.cmake against an installed prefix. Only runs when
# the install was made with the deprecated -DMARKUP_WITH_RAYLIB=ON; delete this directory
# when that backend goes.
find_package(Markup CONFIG REQUIRED)

# markup::raylib and nothing else. Everything it needs -- markup::core, markup::input,
# markup::paint, the LINK_ONLY markup::widgets_basic, raylib itself and (on Windows) the
# winmm that raylib's own installed config forgets -- has to arrive through its link
# interface, or this fails to link. Several of those were missing.
add_executable(consumer_raylib main.c)
target_link_libraries(consumer_raylib PRIVATE markup::raylib)

set(_consumer_out "${CMAKE_BINARY_DIR}/bin")
set_target_properties(consumer_raylib PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${_consumer_out}")
foreach(_cfg IN LISTS CMAKE_CONFIGURATION_TYPES)
    string(TOUPPER "${_cfg}" _CFG)
    set_target_properties(consumer_raylib PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_${_CFG} "${_consumer_out}")
endforeach()
