cmake_minimum_required(VERSION 3.2)

project(tivars_lib_cpp
        VERSION     1.3.0
        LANGUAGES   CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /utf-8 ")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -W -Wall -Wextra -Werror=write-strings -Werror=redundant-decls -Werror=format -Werror=format-security -Werror=date-time -Werror=return-type -Werror=pointer-arith -Winit-self ")
endif()

file(GLOB HEADER_FILES "src/*.h" "src/TypeHandlers/*.h")
file(GLOB COMMON_SOURCE ${HEADER_FILES} "src/*.cpp" "src/TypeHandlers/*.cpp")

if(NOT WIN32)
    set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address,undefined")
    set (CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address,undefined")
endif()

add_executable(tivars_lib_cpp_tests ${COMMON_SOURCE} tests.cpp)
add_executable(tivars_lib_cpp_cli   ${COMMON_SOURCE} cli/cli.cpp)
