# This CMake file is intended to register project-wide objects.
# This allows for reuse between deployments, or other projects.
#
# Layout (see README.md, "Repository layout"):
#   config/         F Prime configuration overrides for the RP2350
#   Drivers/        one passive component per chip
#   Tasks/          one active component per flight task
#   Subtopologies/  instances and wiring, one per subsystem plus Base and Board
#   Deployments/    flight image (Argus) and per-subsystem benches

# Project configuration must be registered before components so it can override
# F Prime's default OS handle sizes for the Zephyr platform.
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/config")
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/Drivers")
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/Tasks")

# fprime-zephyr allows one deployment per build cache: a second call to
# register_fprime_zephyr_deployment asserts. Pick it at generate time, in its
# own build cache so the flight build is left alone:
#
#   fprime-util generate zephyr --build-cache build-fprime-zephyr-EpsBench -DARGUS_DEPLOYMENT=EpsBench
set(ARGUS_DEPLOYMENT "Argus" CACHE STRING "Deployment under project/Deployments to build")

# Subtopologies instantiate Zephyr-only drivers, and deployments register with
# register_fprime_zephyr_deployment, which only exists when the Zephyr library
# is loaded. Unit tests build against F Prime's native OS layer, so both are
# left out of a BUILD_TESTING configure.
if (NOT BUILD_TESTING)
    if (NOT EXISTS "${CMAKE_CURRENT_LIST_DIR}/Deployments/${ARGUS_DEPLOYMENT}/CMakeLists.txt")
        message(FATAL_ERROR "ARGUS_DEPLOYMENT=${ARGUS_DEPLOYMENT} is not a directory under project/Deployments")
    endif()
    add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/Subtopologies")
    add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/Deployments/${ARGUS_DEPLOYMENT}")
endif()
