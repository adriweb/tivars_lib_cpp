cmake_minimum_required(VERSION 3.2)
project(tivars_lib_cpp)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

file(GLOB SOURCE_FILES "src/*.cpp" "src/TypeHandlers/*.cpp" tests.cpp)

add_executable(tivars_lib_cpp ${SOURCE_FILES})