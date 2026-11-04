register_fprime_library(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/EpsManager.fpp"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/EpsManager.cpp"
)

register_fprime_ut(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/EpsManager.fpp"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/test/ut/EpsManagerTestMain.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/test/ut/EpsManagerTester.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/test/ut/EpsLogicTest.cpp"
)