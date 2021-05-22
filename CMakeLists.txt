cmake_minimum_required(VERSION 3.2)

project(tivars_lib_cpp
        VERSION     1.2.0
        LANGUAGES   CXX)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -W -Wall -Wextra -Werror=unused-parameter -Werror=write-strings -Werror=redundant-decls -Werror=format -Werror=format-security -Werror=declaration-after-statement -Werror=implicit-function-declaration -Werror=date-time -Werror=missing-prototypes -Werror=return-type -Werror=pointer-arith -Winit-self ")

file(GLOB SOURCE_FILES "src/*.cpp" "src/*.h" "src/TypeHandlers/*.cpp" "src/TypeHandlers/*.h" tests.cpp)

set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address,undefined")
set (CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address,undefined")

add_executable(tivars_lib_cpp ${SOURCE_FILES})
