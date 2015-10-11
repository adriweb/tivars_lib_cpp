cmake_minimum_required(VERSION 3.2)
project(tivars_lib_cpp)

if(EMSCRIPTEN)
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -Wno-warn-absolute-paths")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-warn-absolute-paths")
    set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} -g4 -s DEMANGLE_SUPPORT=1 -s ASSERTIONS=2")
    set(PLATFORM_PREFIX             "emscripten")
    set(CMAKE_EXECUTABLE_SUFFIX     ".js")
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")

set(SOURCE_FILES
        src/BinaryFile.cpp
        src/TIModels.cpp
        src/TIModel.cpp
        src/TIVarFile.cpp
        src/TIVarType.cpp
        src/TIVarTypes.cpp
        src/utils.cpp
        src/TypeHandlers/TH_0x00.cpp
        src/TypeHandlers/TH_0x05.cpp
        src/TypeHandlers/TH_0x06.h
        src/TypeHandlers/TypeHandlerFuncDispatcher.h
        tests.cpp)

add_executable(tivars_lib_cpp ${SOURCE_FILES})