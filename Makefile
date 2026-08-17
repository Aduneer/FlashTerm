CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra
SRC_DIR = src
BUILD_DIR = build
TARGET = FlashTerm
TEST_TARGET = $(BUILD_DIR)/run_tests

# Where `make install` puts the binary. DESTDIR is prepended for staged
# installs, which is what distro packaging expects.
PREFIX ?= /usr/local
BINDIR = $(DESTDIR)$(PREFIX)/bin

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

# End-to-end tests: drives the built binary with scripted input and diffs the
# whole transcript. Needs the binary rather than the library, which is why it
# is a separate target from `test` -- and why `test` stays the fast one.
golden: $(TARGET)
	tests/golden/run.sh

check: test golden

install: $(TARGET)
	mkdir -p $(BINDIR)
	install -m 755 $(TARGET) $(BINDIR)/$(TARGET)

uninstall:
	rm -f $(BINDIR)/$(TARGET)

clean:
	rm -rf $(BUILD_DIR) $(TARGET)

-include $(OBJS:.o=.d)

.PHONY: all test golden check install uninstall clean
