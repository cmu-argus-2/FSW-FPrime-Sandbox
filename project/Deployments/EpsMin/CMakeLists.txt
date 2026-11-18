#####
# 'EpsMin' Deployment:
#
# The EPS task and drivers on Base alone (no Ground), for comparing RAM and
# flash with other ports' EPS-only builds. Selected with
# -DARGUS_DEPLOYMENT=EpsMin; see project/CMakeLists.txt. Its build settings
# (thread pool, telemetry packetizer limits) are in deployment.cmake.
#
#####

add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/Top/")

register_fprime_zephyr_deployment(
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/Main.cpp"
    DEPENDS
        ${FPRIME_CURRENT_MODULE}_Top
)
