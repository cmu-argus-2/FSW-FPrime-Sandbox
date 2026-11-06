register_fprime_library(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/ADM1176.fpp"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/ADM1176.cpp"
)

register_fprime_ut(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/ADM1176.fpp"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/test/ut/ADM1176TestMain.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/test/ut/ADM1176Tester.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/test/ut/ADM1176MathTest.cpp"
)