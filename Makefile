CXXFLAGS := -O3 -std=c++11 -W -Wall -Wextra -Werror=unused-parameter -Werror=write-strings -Werror=redundant-decls -Werror=format -Werror=format-security -Werror=declaration-after-statement -Werror=implicit-function-declaration -Werror=date-time -Werror=missing-prototypes -Werror=return-type -Werror=pointer-arith -Winit-self

SOURCES_TESTS := $(wildcard src/*.cpp) $(wildcard src/TypeHandlers/*.cpp) tests.cpp
SOURCES_CLI := $(wildcard src/*.cpp) $(wildcard src/TypeHandlers/*.cpp) cli.cpp

OBJS_TESTS = $(patsubst %.cpp, %.o, $(SOURCES_TESTS))
OBJS_CLI = $(patsubst %.cpp, %.o, $(SOURCES_CLI))

OUTPUT := tivars_test tivars_cli

all: $(OUTPUT)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

tivars_test: $(OBJS_TESTS)
	$(CXX) $(CXXFLAGS) $(LFLAGS) $^ -o $@

tivars_cli: $(OBJS_CLI)
	$(CXX) $(CXXFLAGS) $(LFLAGS) $^ -o $@

clean:
	$(RM) -f $(OBJS_TESTS) $(OBJS_CLI $(OUTPUT)

.PHONY: all clean