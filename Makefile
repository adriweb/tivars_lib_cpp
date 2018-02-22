CXXFLAGS := -O3 -std=c++11 -W -Wall -Wextra -Werror=unused-parameter -Werror=write-strings -Werror=redundant-decls -Werror=format -Werror=format-security -Werror=declaration-after-statement -Werror=implicit-function-declaration -Werror=date-time -Werror=missing-prototypes -Werror=return-type -Werror=pointer-arith -Winit-self

SOURCES := $(wildcard src/*.cpp) $(wildcard src/TypeHandlers/*.cpp) tests.cpp

OUTPUT := tivars_test

OBJS = $(patsubst %.cpp, %.o, $(SOURCES))

OUTPUT := tivars_test

all: $(OUTPUT)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OUTPUT): $(OBJS)
	$(CXX) $(CXXFLAGS) $(LFLAGS) $^ -o $@

clean:
	$(RM) -f $(OBJS) $(OUTPUT)

.PHONY: all clean

