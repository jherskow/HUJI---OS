cmake_minimum_required(VERSION 3.8)
project(OS_Ex1)

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES osm.cpp stopwatch.cpp)
add_executable(OS_Ex1 ${SOURCE_FILES})