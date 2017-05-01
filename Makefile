CC := g++

CXXFLAGS := -O3 -std=c++11 -W -Wall -Wextra

SOURCES := $(wildcard src/*.cpp) $(wildcard src/TypeHandlers/*.cpp) tests.cpp

OUTPUT := tivars_test

OBJS = $(patsubst %.cpp, %.o, $(SOURCES))

OUTPUT := tivars_test

all: $(OUTPUT)

%.o: %.cpp
	$(CC) $(CXXFLAGS) -c $< -o $@

$(OUTPUT): $(OBJS)
	$(CC) $(CXXFLAGS) $(LFLAGS) $^ -o $@

clean:
	$(RM) -f $(OBJS) $(OUTPUT)

.PHONY: all clean

