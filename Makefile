SOURCES=$(wildcard *.cpp)
HEADERS=$(SOURCES:.cpp=.hpp)
FLAGS=-DDEBUG -g

all: main

main: $(SOURCES) $(HEADERS)
	mpic++ $(SOURCES) $(FLAGS) -o main

clean:
	rm main

