#####
# 'Argus' Deployment:
#
# The flight image. Selected with -DARGUS_DEPLOYMENT=Argus (the default); see
# project/CMakeLists.txt.
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
