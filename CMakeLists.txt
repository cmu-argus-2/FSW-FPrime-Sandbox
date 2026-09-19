####
# This sets up the build system for the 'project' project, including
# components and deployments from project.cmake. In addition, it imports the core F Prime components.
####

cmake_minimum_required(VERSION 3.24.2)

if (NOT BUILD_TESTING)
    find_package(Zephyr HINTS "${CMAKE_CURRENT_LIST_DIR}/lib/zephyr-workspace")
endif()

project(project C CXX)

###
# F' Core Setup
# This includes all of the F prime core components, and imports the make-system.
###
include("${CMAKE_CURRENT_LIST_DIR}/lib/fprime/cmake/FPrime.cmake")

# # The Zephyr F Prime library is only valid for board builds. Native unit tests
# # use F Prime's native OS implementation and must not include Zephyr headers.
# if (NOT BUILD_TESTING)
#     list(APPEND FPRIME_LIBRARY_LOCATIONS "${CMAKE_CURRENT_LIST_DIR}/lib/fprime-zephyr")
#     set(FPRIME_LIBRARY_LOCATIONS "${FPRIME_LIBRARY_LOCATIONS}" CACHE STRING "F Prime library locations" FORCE)
# endif()

# NOTE: register custom targets between these two lines
fprime_setup_included_code()


# This includes project-wide objects
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/project")
