cmake_minimum_required(VERSION 3.2)

project(tivars_lib_cpp
        VERSION     1.2.0
        LANGUAGES   CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -W -Wall -Wextra -Werror=unused-parameter -Werror=write-strings -Werror=redundant-decls -Werror=format -Werror=format-security -Werror=declaration-after-statement -Werror=implicit-function-declaration -Werror=date-time -Werror=missing-prototypes -Werror=return-type -Werror=pointer-arith -Winit-self ")

file(GLOB HEADER_FILES "src/*.h" "src/TypeHandlers/*.h")
file(GLOB COMMON_SOURCE ${HEADER_FILES} "src/*.cpp" "src/TypeHandlers/*.cpp")

set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address,undefined")
set (CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address,undefined")

add_executable(tivars_lib_cpp_tests ${COMMON_SOURCE} tests.cpp)
add_executable(tivars_lib_cpp_cli   ${COMMON_SOURCE} cli/cli.cpp)
