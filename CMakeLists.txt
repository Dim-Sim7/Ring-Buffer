cmake_minimum_required(VERSION 3.20)
project(RingBuffer)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(-O2 -march=native)

add_executable(bench benchmarkbuffer.cpp)
add_executable(benchburst benchmarkburst.cpp)