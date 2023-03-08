CXXFLAGS := -O3 -std=c++2a -W -Wall -Wextra -Werror=write-strings -Werror=redundant-decls -Werror=format -Werror=format-security -Werror=date-time -Werror=return-type -Werror=pointer-arith -Winit-self

SOURCES_COMMON := $(wildcard src/*.cpp) $(wildcard src/TypeHandlers/*.cpp)

SOURCES_TESTS := $(SOURCES_COMMON) tests.cpp
SOURCES_CLI   := $(SOURCES_COMMON) cli/cli.cpp

OBJS_TESTS = $(patsubst %.cpp, %.o, $(SOURCES_TESTS))
OBJS_CLI   = $(patsubst %.cpp, %.o, $(SOURCES_CLI))

OUTPUT := tivars_tests tivars_cli

all: $(OUTPUT)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

tivars_tests: $(OBJS_TESTS)
	$(CXX) $(CXXFLAGS) $(LFLAGS) $^ -o $@

tivars_cli: $(OBJS_CLI)
	$(CXX) $(CXXFLAGS) $(LFLAGS) $^ -o $@

clean:
	$(RM) -f $(OBJS_TESTS) $(OBJS_CLI) $(OUTPUT)

.PHONY: all clean
