#####
# 'AdcsBench' Deployment:
#
# The ADCS subsystem on its own, for bringing up ADCS drivers on the flatsat and
# measuring the ADCS task in isolation. Selected with
# -DARGUS_DEPLOYMENT=AdcsBench; see project/CMakeLists.txt.
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
