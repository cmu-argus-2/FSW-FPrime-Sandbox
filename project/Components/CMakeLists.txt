# Include project-wide components here

# add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/MyComponent/")
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/HiComponent/")
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/OPT4003/")

# Components are registered once, above, and unconditionally. They depend only
# on portable Drv/Svc ports, so they build both for the board and for the
# native unit tests, and each component's own CMakeLists.txt decides what to do
# under BUILD_TESTING (registering its unit tests there). Registering a
# component in both places instead is a CMake configure error: "is already used
# to build a source directory".
#
# The deployment registers itself with register_fprime_zephyr_deployment, which
# only exists when the Zephyr library is loaded, and its topology pulls in
# Zephyr-only drivers. Unit tests build against F Prime's native OS layer, so
# the deployment is left out of a BUILD_TESTING configure.
if (NOT BUILD_TESTING)
    add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/FirstDeployment/")
endif()
