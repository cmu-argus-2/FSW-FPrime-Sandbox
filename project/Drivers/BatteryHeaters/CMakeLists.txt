####
# F Prime CMakeLists.txt:
#
# SOURCES: list of source files (to be compiled)
# AUTOCODER_INPUTS: list of files to be passed to the autocoders
# DEPENDS: list of libraries that this module depends on
#
# More information in the F´ CMake API documentation:
# https://fprime.jpl.nasa.gov/latest/docs/reference/api/cmake/API/
#
####

register_fprime_library(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/BatteryHeaters.fpp"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/BatteryHeaters.cpp"
    DEPENDS
        Drv_Ports
)

register_fprime_ut(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/BatteryHeaters.fpp"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/test/ut/BatteryHeatersTestMain.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/test/ut/BatteryHeatersTester.cpp"
    DEPENDS
        Drv_Ports
)