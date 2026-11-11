# Bench and bring-up tools: components for testing hardware and other
# components on the flatsat, never part of the flight image. They are
# instantiated in Subtopologies/BenchTools, which only the bench deployments
# import.
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/I2cScanner/")
