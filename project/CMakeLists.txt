# This CMake file is intended to register project-wide objects.
# This allows for reuse between deployments, or other projects.

# Project configuration must be registered before components so it can override
# F Prime's default OS handle sizes for the Zephyr platform.
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/config")
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/Components")
