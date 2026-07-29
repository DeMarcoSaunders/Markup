# Locate Skia for markup_skia.
# Prefer vcpkg: vcpkg install skia sdl3

set(MARKUP_SKIA_FOUND FALSE)

if(TARGET skia)
    set(MARKUP_SKIA_FOUND TRUE)
    set(MARKUP_SKIA_LIBRARIES skia)
elseif(TARGET Skia::skia)
    set(MARKUP_SKIA_FOUND TRUE)
    set(MARKUP_SKIA_LIBRARIES Skia::skia)
else()
    find_package(skia CONFIG QUIET)
    if(skia_FOUND)
        set(MARKUP_SKIA_FOUND TRUE)
        if(TARGET skia)
            set(MARKUP_SKIA_LIBRARIES skia)
        elseif(TARGET Skia::skia)
            set(MARKUP_SKIA_LIBRARIES Skia::skia)
        endif()
    endif()
endif()

if(NOT MARKUP_SKIA_FOUND)
    if(SKIA_ROOT)
        list(APPEND CMAKE_PREFIX_PATH "${SKIA_ROOT}")
    endif()
    find_path(SKIA_INCLUDE_DIR NAMES core/SkCanvas.h PATH_SUFFIXES include/skia include)
    find_library(SKIA_LIBRARY NAMES skia libskia PATH_SUFFIXES lib)
    include(FindPackageHandleStandardArgs)
    find_package_handle_standardArgs(MarkupSkia DEFAULT_MSG SKIA_INCLUDE_DIR SKIA_LIBRARY)
    if(MarkupSkia_FOUND)
        set(MARKUP_SKIA_FOUND TRUE)
        if(NOT TARGET markup_skia_lib)
            add_library(markup_skia_lib UNKNOWN IMPORTED)
            set_target_properties(markup_skia_lib PROPERTIES
                IMPORTED_LOCATION "${SKIA_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${SKIA_INCLUDE_DIR}"
            )
        endif()
        set(MARKUP_SKIA_LIBRARIES markup_skia_lib)
    endif()
endif()
