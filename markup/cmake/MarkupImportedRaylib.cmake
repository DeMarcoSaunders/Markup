# Used after install: defines markup::raylib as a STATIC IMPORTED target without putting
# it through install(EXPORT), so CMake does not recurse into raylib's GLFW (etc.) linkage
# graph for export validation.
#
# Only markup::raylib. Every other Markup library has a Markup-internal link interface and
# goes through install(EXPORT) in MarkupTargets.cmake, which gets transitive linkage and
# include directories right on its own. This file used to declare markup::widgets_basic
# too, with raylib hardcoded into its link interface — which the build tree never did.
#
# DEPRECATED along with the backend itself; delete this file when markup_raylib goes.
#
# Installed beside MarkupTargets.cmake under lib/cmake/Markup/ → lib/ is ../../ from here.

include_guard(GLOBAL)

if(NOT TARGET raylib)
    include(CMakeFindDependencyMacro)
    find_dependency(raylib REQUIRED)
endif()

# raylib 5.0's *installed* raylib-config.cmake builds its link interface out of
# pkg-config's LDFLAGS, which comes back empty under MSVC -- so the Windows system
# libraries it needs are simply absent. Its in-tree target has them, which is why the
# demos link here and only a find_package consumer fails, on timeBeginPeriod /
# timeEndPeriod from rcore.obj. Upstream's gap, patched here because this shim is already
# the compatibility layer for an installed raylib backend.
if(WIN32 AND NOT TARGET markup::_raylib_platform_libs)
    add_library(markup::_raylib_platform_libs INTERFACE IMPORTED)
    set_target_properties(markup::_raylib_platform_libs PROPERTIES
        INTERFACE_LINK_LIBRARIES "winmm"
    )
endif()

get_filename_component(_markup_libdir "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

set(_m_markup_rl_lib "${CMAKE_STATIC_LIBRARY_PREFIX}markup_raylib${CMAKE_STATIC_LIBRARY_SUFFIX}")

if(NOT TARGET markup::raylib)
    add_library(markup::raylib STATIC IMPORTED GLOBAL)
endif()
# Mirrors the build tree exactly:
#   target_link_libraries(markup_raylib PUBLIC  markup_core markup_input markup_paint
#                                       PRIVATE markup_widgets_basic raylib)
#   target_compile_definitions(markup_raylib PUBLIC MU_BACKEND_RAYLIB=1)
#
# The PRIVATE deps of a STATIC library are still needed at final link, hence
# $<LINK_ONLY:...>. markup::paint and markup::widgets_basic were both missing here before,
# so a consumer linking markup::raylib got undefined references to the paint and widget
# symbols. The compile definition is what gives installed consumers the same automatic
# backend selection in mu_backend.h; without it, including mu.h / mu_backend.h after
# find_package(Markup) fails with "no rendering backend selected".
set_target_properties(markup::raylib PROPERTIES
    IMPORTED_LOCATION "${_markup_libdir}/${_m_markup_rl_lib}"
    INTERFACE_LINK_LIBRARIES "markup::core;markup::input;markup::paint;$<LINK_ONLY:markup::widgets_basic>;$<LINK_ONLY:raylib>;$<$<BOOL:${WIN32}>:$<LINK_ONLY:markup::_raylib_platform_libs>>"
    INTERFACE_COMPILE_DEFINITIONS "MU_BACKEND_RAYLIB=1"
)
