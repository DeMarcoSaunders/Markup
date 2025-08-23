# Examples CMakeLists.txt

# Example: Basic usage
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/example_basic.c")
    add_executable(example_basic example_basic.c)
    target_link_libraries(example_basic markup-ui)
    target_include_directories(example_basic PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
endif()

# Example: Loading components
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/example_loading_components.c")
    add_executable(example_loading_components example_loading_components.c)
    target_link_libraries(example_loading_components markup-ui)
    target_include_directories(example_loading_components PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
endif()

# Example: Background images
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/example_background_images.c")
    add_executable(example_background_images example_background_images.c)
    target_link_libraries(example_background_images markup-ui)
    target_include_directories(example_background_images PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
endif()

# Example: Robust usage
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/example_robust_usage.c")
    add_executable(example_robust_usage example_robust_usage.c)
    target_link_libraries(example_robust_usage markup-ui)
    target_include_directories(example_robust_usage PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
endif()

# Copy example files to build directory for easy access
file(GLOB EXAMPLE_FILES "*.c")
foreach(EXAMPLE_FILE ${EXAMPLE_FILES})
    configure_file(${EXAMPLE_FILE} ${CMAKE_CURRENT_BINARY_DIR}/${EXAMPLE_FILE} COPYONLY)
endforeach()

