
####### Expanded from @PACKAGE_INIT@ by configure_package_config_file() #######
####### Any changes to this file will be overwritten by the next CMake run ####
####### The input file was MarkupConfig.cmake.in                            ########

get_filename_component(PACKAGE_PREFIX_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../" ABSOLUTE)

macro(set_and_check _var _file)
  set(${_var} "${_file}")
  if(NOT EXISTS "${_file}")
    message(FATAL_ERROR "File or directory ${_file} referenced by variable ${_var} does not exist !")
  endif()
endmacro()

macro(check_required_components _NAME)
  foreach(comp ${${_NAME}_FIND_COMPONENTS})
    if(NOT ${_NAME}_${comp}_FOUND)
      if(${_NAME}_FIND_REQUIRED_${comp})
        set(${_NAME}_FOUND FALSE)
      endif()
    endif()
  endforeach()
endmacro()

####################################################################################

include("${CMAKE_CURRENT_LIST_DIR}/MarkupTargets.cmake")

# Present only in an install built with the deprecated raylib backend. Guarded because
# that file runs find_dependency(raylib REQUIRED): including it unconditionally made every
# consumer need raylib installed, software-only ones included, and pointed
# markup::raylib at a .lib that a raylib-free build never produced.
if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/MarkupImportedRaylib.cmake")
    include("${CMAKE_CURRENT_LIST_DIR}/MarkupImportedRaylib.cmake")
endif()

check_required_components(Markup)
