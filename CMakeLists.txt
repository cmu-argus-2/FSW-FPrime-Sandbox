####
# This sets up the build system for the 'project' project, including
# components and deployments from project.cmake. In addition, it imports the core F Prime components.
####

cmake_minimum_required(VERSION 3.24.2)

if (NOT BUILD_TESTING)
    # A deployment can adjust the build in its own deployment.cmake: add
    # Kconfig on top of prj.conf (EXTRA_CONF_FILE), or replace an F Prime
    # config header (see project/config/CMakeLists.txt). It is included
    # first because Zephyr reads EXTRA_CONF_FILE in find_package().
    # ARGUS_DEPLOYMENT is declared in project/CMakeLists.txt, but is already
    # in the cache when it was passed with -D.
    if (DEFINED ARGUS_DEPLOYMENT AND EXISTS "${CMAKE_CURRENT_LIST_DIR}/project/Deployments/${ARGUS_DEPLOYMENT}/deployment.cmake")
        include("${CMAKE_CURRENT_LIST_DIR}/project/Deployments/${ARGUS_DEPLOYMENT}/deployment.cmake")
    endif()
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
