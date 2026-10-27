register_fprime_library(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/ADM1176.fpp"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/ADM1176.cpp"
)

register_fprime_ut(
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/test/ut/ADM1176MathTest.cpp"
)