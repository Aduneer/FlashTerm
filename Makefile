CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra
SRC_DIR = src
BUILD_DIR = build
TARGET = FlashTerm
TEST_TARGET = $(BUILD_DIR)/run_tests

# Everything except main.cpp, so the tests can link against it.
LIB_SRCS = $(filter-out $(SRC_DIR)/main.cpp,$(wildcard $(SRC_DIR)/*.cpp))
SRCS = $(LIB_SRCS) $(SRC_DIR)/main.cpp
LIB_OBJS = $(LIB_SRCS:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)
OBJS = $(SRCS:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -I$(SRC_DIR) -MMD -MP -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

test: $(TEST_TARGET)
	./$(TEST_TARGET)

$(TEST_TARGET): tests/tests.cpp $(LIB_OBJS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -I$(SRC_DIR) -o $@ tests/tests.cpp $(LIB_OBJS)

clean:
	rm -rf $(BUILD_DIR) $(TARGET)

-include $(OBJS:.o=.d)

.PHONY: all test clean
