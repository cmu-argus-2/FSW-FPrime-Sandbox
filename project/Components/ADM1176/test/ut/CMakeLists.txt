cmake_minimum_required(VERSION 3.16)
project(ADM1176MathTest LANGUAGES CXX)

enable_testing()
add_executable(ADM1176MathTest ADM1176MathTest.cpp)
target_include_directories(ADM1176MathTest PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../../../..")
target_compile_features(ADM1176MathTest PRIVATE cxx_std_14)
add_test(NAME ADM1176MathTest COMMAND ADM1176MathTest)
