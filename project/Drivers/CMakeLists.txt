# One passive component per chip. A driver only talks through Drv ports (I2C,
# GPIO) and has no idea which bus or pin it is on: that is decided where it is
# instantiated, in its subsystem's subtopology.
#
# Drivers depend only on portable Drv/Svc ports, so they build both for the
# board and for the native unit tests, and each driver's own CMakeLists.txt
# registers its unit tests.
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/ADM1176/")
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/BatteryHeaters/")
