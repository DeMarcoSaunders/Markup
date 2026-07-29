# Used after install: defines markup::raylib / markup::widgets_basic as STATIC IMPORTED
# targets without putting them through install(EXPORT), so CMake does not recurse into
# raylib's GLFW (etc.) linkage graph for export validation.
#
# Installed beside MarkupTargets.cmake under lib/cmake/Markup/ → lib/ is ../../ from here.

include_guard(GLOBAL)

if(NOT TARGET raylib)
    include(CMakeFindDependencyMacro)
    find_dependency(raylib REQUIRED)
endif()

get_filename_component(_markup_libdir "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

set(_m_markup_rl_lib "${CMAKE_STATIC_LIBRARY_PREFIX}markup_raylib${CMAKE_STATIC_LIBRARY_SUFFIX}")
set(_m_markup_wb_lib "${CMAKE_STATIC_LIBRARY_PREFIX}markup_widgets_basic${CMAKE_STATIC_LIBRARY_SUFFIX}")

if(NOT TARGET markup::raylib)
    add_library(markup::raylib STATIC IMPORTED GLOBAL)
endif()
# Mirrors target_compile_definitions(markup_raylib PUBLIC ...) from the build tree, so installed
# consumers get the same automatic backend selection in mu_backend.h. Without this, including
# mu.h / mu_backend.h after find_package(Markup) fails with "no rendering backend selected".
set_target_properties(markup::raylib PROPERTIES
    IMPORTED_LOCATION "${_markup_libdir}/${_m_markup_rl_lib}"
    INTERFACE_LINK_LIBRARIES "markup::core;markup::input;$<LINK_ONLY:raylib>"
    INTERFACE_COMPILE_DEFINITIONS "MU_BACKEND_RAYLIB=1"
)

if(NOT TARGET markup::widgets_basic)
    add_library(markup::widgets_basic STATIC IMPORTED GLOBAL)
endif()
set_target_properties(markup::widgets_basic PROPERTIES
    IMPORTED_LOCATION "${_markup_libdir}/${_m_markup_wb_lib}"
    INTERFACE_LINK_LIBRARIES "markup::core;markup::layout_flex;markup::style;markup::raylib;$<LINK_ONLY:raylib>"
)
