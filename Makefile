# Quick and dirty Makefile for Desktop + Emscripten build

EMCC := em++
GCC := g++
CPPFLAGS := -O2 -std=c++14 -Wall
SOURCES := tests.cpp src/*.cpp src/TypeHandlers/*.cpp
OUTPUT := tivars_test

lib:
	$(GCC) $(CPPFLAGS) -o $(OUTPUT) $(SOURCES)

js:
	$(EMCC) -s DISABLE_EXCEPTION_CATCHING=0 $(CPPFLAGS) $(SOURCES) -o $(OUTPUT).html --preload-file assets

clean:
	rm -rf *.o cmake_install.cmake CMakeCache.txt CMakeFiles $(OUTPUT).html $(OUTPUT).js $(OUTPUT).data $(OUTPUT).html.mem
