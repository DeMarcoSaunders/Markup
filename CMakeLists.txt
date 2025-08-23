cmake_minimum_required(VERSION 3.15)
project(MarkUp-UI VERSION 1.0.0 LANGUAGES C)

# Set C standard
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Options
option(BUILD_EXAMPLES "Build example applications" ON)
option(BUILD_TESTS "Build test applications" OFF)
option(ENABLE_DEBUG "Enable debug mode" OFF)

# Set build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

if(ENABLE_DEBUG)
    add_compile_definitions(DEBUG)
    if(MSVC)
        add_compile_options(/Od /Zi)
    else()
        add_compile_options(-g -O0)
    endif()
else()
    if(MSVC)
        add_compile_options(/O2)
    else()
        add_compile_options(-O2)
    endif()
endif()

# Find raylib
find_package(raylib QUIET)
if(NOT raylib_FOUND)
    # Try to find raylib in common locations
    find_library(RAYLIB_LIBRARY
        NAMES raylib
        PATHS
        /usr/lib
        /usr/local/lib
        /opt/homebrew/lib
        ${CMAKE_CURRENT_SOURCE_DIR}/external/raylib/lib
        ${CMAKE_CURRENT_SOURCE_DIR}/external/raylib/src
    )
    
    find_path(RAYLIB_INCLUDE_DIR
        NAMES raylib.h
        PATHS
        /usr/include
        /usr/local/include
        /opt/homebrew/include
        ${CMAKE_CURRENT_SOURCE_DIR}/external/raylib/include
        ${CMAKE_CURRENT_SOURCE_DIR}/external/raylib/src
    )
    
    if(RAYLIB_LIBRARY AND RAYLIB_INCLUDE_DIR)
        set(raylib_LIBRARIES ${RAYLIB_LIBRARY})
        set(raylib_INCLUDE_DIRS ${RAYLIB_INCLUDE_DIR})
        set(raylib_FOUND TRUE)
    else()
        # If raylib is not found, try to build it from source
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/raylib/CMakeLists.txt")
            message(STATUS "Building raylib from source...")
            add_subdirectory(external/raylib)
            set(raylib_LIBRARIES raylib)
            set(raylib_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/external/raylib/src)
            set(raylib_FOUND TRUE)
        else()
            message(FATAL_ERROR "raylib not found. Please install raylib or ensure it's in external/raylib/")
        endif()
    endif()
endif()

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${raylib_INCLUDE_DIRS})

# Source files
set(MARKUP_SOURCES
    components/error_handling.c
    components/button.c
    components/panel.c
    components/sidebar.c
    components/slider.c
    components/dropdown.c
    components/text_input.c
    components/text_area.c
    components/checkbox.c
    components/radio_button.c
    components/modal.c
    components/imagebox.c
    components/separator.c
    components/backdrop.c
    components/blur_effect.c
    components/shadow.c
    components/loading_bar.c
    components/spinner.c
    components/background_image.c
    # Add new components here as they are implemented
    # components/tabs.c
    # components/toast.c
    # components/card.c
)

# Header files
set(MARKUP_HEADERS
    markup.h
    theme.h
    components/error_handling.h
    components/component_base.h
    components/style_system.h
    components/layout_engine.h
    components/event_system.h
    components/button.h
    components/panel.h
    components/sidebar.h
    components/slider.h
    components/dropdown.h
    components/text_input.h
    components/text_area.h
    components/checkbox.h
    components/radio_button.h
    components/modal.h
    components/imagebox.h
    components/separator.h
    components/backdrop.h
    components/blur_effect.h
    components/shadow.h
    components/loading_bar.h
    components/spinner.h
    components/tabs.h
    components/toast.h
    components/card.h
    components/background_image.h
)

# Create static library
add_library(markup-ui STATIC ${MARKUP_SOURCES} ${MARKUP_HEADERS})
target_link_libraries(markup-ui ${raylib_LIBRARIES})

# Set library properties
set_target_properties(markup-ui PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    PUBLIC_HEADER "${MARKUP_HEADERS}"
)

# Install rules
install(TARGETS markup-ui
    EXPORT MarkUpUITargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    PUBLIC_HEADER DESTINATION include/markup-ui
)

# Install headers
install(FILES ${MARKUP_HEADERS}
    DESTINATION include/markup-ui
)

# Export targets
install(EXPORT MarkUpUITargets
    FILE MarkUpUITargets.cmake
    NAMESPACE MarkUpUI::
    DESTINATION lib/cmake/MarkUpUI
)

# Create config file
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    MarkUpUIConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/MarkUpUIConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/MarkUpUIConfig.cmake"
    INSTALL_DESTINATION lib/cmake/MarkUpUI
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/MarkUpUIConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/MarkUpUIConfigVersion.cmake"
    DESTINATION lib/cmake/MarkUpUI
)

# Examples
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Tests
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Print configuration summary
message(STATUS "MarkUp UI Configuration:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Build examples: ${BUILD_EXAMPLES}")
message(STATUS "  Build tests: ${BUILD_TESTS}")
message(STATUS "  Debug mode: ${ENABLE_DEBUG}")
message(STATUS "  raylib found: ${raylib_FOUND}")
if(raylib_FOUND)
    message(STATUS "  raylib version: ${raylib_VERSION}")
endif()
