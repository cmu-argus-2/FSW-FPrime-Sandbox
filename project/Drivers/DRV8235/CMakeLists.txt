register_fprime_library(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/DRV8235.fpp"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/DRV8235.cpp"
)

register_fprime_ut(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/DRV8235.fpp"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/test/ut/DRV8235TestMain.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/test/ut/DRV8235Tester.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/test/ut/DRV8235MathTest.cpp"
)
