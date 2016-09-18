# Quick and dirty Makefile for Desktop + Emscripten build

EMCC := em++
GCC := g++
CPPFLAGS := -O3 -std=c++11 -Wall
SOURCES := tests.cpp src/*.cpp src/TypeHandlers/*.cpp
OUTPUT := tivars_test

lib:
	$(GCC) $(CPPFLAGS) -o $(OUTPUT) $(SOURCES)

js:
	$(EMCC) -s DISABLE_EXCEPTION_CATCHING=1 $(CPPFLAGS) $(SOURCES) -o $(OUTPUT).html --preload-file assets

clean:
	rm -rf *.o cmake_install.cmake CMakeCache.txt CMakeFiles $(OUTPUT).html $(OUTPUT).js $(OUTPUT).data $(OUTPUT).html.mem
