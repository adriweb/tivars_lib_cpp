CC ?= cc
CXXFLAGS := -O3 -std=c++2a -DTH_GDB_SUPPORT=1 -Ivendor/pugixml -W -Wall -Wextra -Werror=write-strings -Werror=redundant-decls -Werror=format -Werror=format-security -Werror=date-time -Werror=return-type -Werror=pointer-arith -Winit-self
CFLAGS := -O3 -std=c2x -W -Wall -Wextra

SOURCES_COMMON := $(wildcard src/*.cpp) $(wildcard src/TypeHandlers/*.cpp) $(wildcard src/TypeHandlers/*.c) vendor/pugixml/pugixml.cpp

SOURCES_TESTS := $(SOURCES_COMMON) tests.cpp
SOURCES_CLI   := $(SOURCES_COMMON) cli/cli.cpp

OBJS_TESTS = $(patsubst %.c, %.o, $(patsubst %.cpp, %.o, $(SOURCES_TESTS)))
OBJS_CLI   = $(patsubst %.c, %.o, $(patsubst %.cpp, %.o, $(SOURCES_CLI)))

OUTPUT := tivars_tests tivars_cli

all: $(OUTPUT)

amalgamated:
	python3 scripts/amalgamate.py

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

tivars_tests: $(OBJS_TESTS)
	$(CXX) $(CXXFLAGS) $(LFLAGS) $^ -o $@

tivars_cli: $(OBJS_CLI)
	$(CXX) $(CXXFLAGS) $(LFLAGS) $^ -o $@

clean:
	$(RM) -f $(OBJS_TESTS) $(OBJS_CLI) $(OUTPUT)

.PHONY: all amalgamated clean
