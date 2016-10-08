cmake_minimum_required(VERSION 3.2)
project(tivars_lib_cpp)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -W -Wall -Werror=unused-parameter -Werror=write-strings -Werror=redundant-decls -Werror=format -Werror=format-security -Werror=declaration-after-statement -Werror=implicit-function-declaration -Werror=date-time -Werror=missing-prototypes -Werror=return-type -Werror=pointer-arith -Winit-self ")

file(GLOB SOURCE_FILES "src/*.cpp" "src/TypeHandlers/*.cpp" tests.cpp)

add_executable(tivars_lib_cpp ${SOURCE_FILES})