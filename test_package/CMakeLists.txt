cmake_minimum_required(VERSION 3.20)
project(test_package CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(curvepress CONFIG REQUIRED)

add_executable(example example.cpp)
target_link_libraries(example PRIVATE curvepress::curvepress)
