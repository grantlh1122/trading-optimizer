# Native engine build. The project intentionally has no application executable
# yet; `make test` compiles and exercises the strategy/backtester boundary.

CXX ?= c++
CPPFLAGS ?= -I.
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Wpedantic -Werror

BUILD_DIR := build
ENGINE_SOURCES := backtest.cpp strategies.cpp
ENGINE_OBJECTS := $(ENGINE_SOURCES:%.cpp=$(BUILD_DIR)/%.o)
TEST_BINARY := $(BUILD_DIR)/backtest_test

.PHONY: all test clean

all: $(ENGINE_OBJECTS)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: %.cpp backtest.hpp strategies.hpp trading_types.hpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(TEST_BINARY): tests/backtest_test.cpp $(ENGINE_OBJECTS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< $(ENGINE_OBJECTS) -o $@

test: $(TEST_BINARY)
	./$(TEST_BINARY)

clean:
	rm -rf $(BUILD_DIR)
