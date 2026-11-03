#####
# 'EpsBench' Deployment:
#
# The EPS subsystem on its own, for bringing up EPS drivers on the flatsat and
# measuring the EPS task in isolation. Selected with
# -DARGUS_DEPLOYMENT=EpsBench; see project/CMakeLists.txt.
#
#####

###
# Topology and Components
###

add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/Top/")

register_fprime_zephyr_deployment(
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/Main.cpp"
    DEPENDS
        ${FPRIME_CURRENT_MODULE}_Top
)
