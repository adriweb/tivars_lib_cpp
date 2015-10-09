cmake_minimum_required(VERSION 3.2)
project(tivars_lib_cpp)

if(EMSCRIPTEN)
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -Wno-warn-absolute-paths")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-warn-absolute-paths")
    set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} -g4 -s DEMANGLE_SUPPORT=1 -s ASSERTIONS=2")
    set(PLATFORM_PREFIX             "emscripten")
    set(CMAKE_EXECUTABLE_SUFFIX     ".js")
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(SOURCE_FILES src/utils.cpp src/TypeHandlers/TH_0x00.cpp src/TypeHandlers/TH_0x05.cpp tests.cpp)
add_executable(tivars_lib_cpp ${SOURCE_FILES})