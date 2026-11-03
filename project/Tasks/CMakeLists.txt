# One active component per flight task (EPS, ADCS, ...). A task owns a thread
# and calls its subsystem's drivers through ports, so their I2C and GPIO
# transactions run on that thread. The task is wired to its drivers in its
# subsystem's subtopology under project/Subtopologies, not here.
#
# Register each task like a driver:
# add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/EpsManager/")
