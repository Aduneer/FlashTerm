CXX = g++
CXXFLAGS = -std=c++17 -Wall
SRCS = src/main.cpp src/flashcard.cpp src/utils.cpp
OBJS = $(SRCS:.cpp=.o)
TARGET = cpp-flashcards

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRCS)

clean:
	rm -f $(TARGET) $(OBJS)
