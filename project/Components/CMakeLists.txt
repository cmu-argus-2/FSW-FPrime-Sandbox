# Include project-wide components here

# add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/MyComponent/")
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/HiComponent/")
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/MAX17205/")

# Components are registered once, above, and unconditionally. They depend only
# on portable Drv/Svc ports, so they build both for the board and for the
# native unit tests, and each component's own CMakeLists.txt decides what to do
# under BUILD_TESTING (MAX17205 registers its unit tests there). Registering a
# component in both places instead is a CMake configure error: "is already used
# to build a source directory".
#
# The deployment is the one thing that genuinely cannot be built both ways. It
# registers itself with register_fprime_zephyr_deployment, which only exists
# once the Zephyr library is loaded, and its topology pulls in Zephyr-only
# drivers. Unit tests build against F Prime's native OS layer, so the
# deployment is left out of a BUILD_TESTING configure entirely.
if (NOT BUILD_TESTING)
    add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/FirstDeployment/")
endif()
