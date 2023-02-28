
####### Expanded from @PACKAGE_INIT@ by configure_package_config_file() #######
####### Any changes to this file will be overwritten by the next CMake run ####
####### The input file was xlogConfig.cmake.in                            ########

get_filename_component(PACKAGE_PREFIX_DIR "${CMAKE_CURRENT_LIST_DIR}/../" ABSOLUTE)

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

set(PACKAGE_NAME xlog)
set(${PACKAGE_NAME}_VERSION 1.0.0.0)

set_and_check(${PACKAGE_NAME}_INCLUDE_DIRS /Users/huzh/github/xlog/install/include)
set_and_check(${PACKAGE_NAME}_LINK_DIRS /Users/huzh/github/xlog/install/lib)

file(GLOB_RECURSE LIB_PATHES ${${PACKAGE_NAME}_LINK_DIRS}/*)
get_filename_component(LIBRARIES ${LIB_PATHES} NAME_WE)
list(APPEND ${PACKAGE_NAME}_LIBRARIES ${PACKAGE_NAME}::${LIBRARIES})

#include(CMakeFindDependencyMacro)
#find_dependency(OpenCV REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/${PACKAGE_NAME}Targets.cmake")
check_required_components(${PACKAGE_NAME})
