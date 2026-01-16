CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2 -g
INCLUDES = -Iinclude
SRC_DIR = src
TEST_DIR = tests
BUILD_DIR = build

# Source files
SOURCES = $(SRC_DIR)/mos6510.c $(SRC_DIR)/opcodes.c $(SRC_DIR)/illegal_opcodes.c $(SRC_DIR)/opcode_table.c
TEST_SOURCES = $(TEST_DIR)/test_opcodes.c

# Object files
OBJECTS = $(SOURCES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
TEST_OBJECTS = $(TEST_SOURCES:$(TEST_DIR)/%.c=$(BUILD_DIR)/%.o)

# Executables
LIB_NAME = $(BUILD_DIR)/libmos6510.a
TEST_EXEC = $(BUILD_DIR)/test_opcodes
EXAMPLE_EXEC = $(BUILD_DIR)/example

.PHONY: all clean test run-test example run-example docs

all: $(LIB_NAME) $(TEST_EXEC) $(EXAMPLE_EXEC)

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Compile source files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Compile test files
$(BUILD_DIR)/%.o: $(TEST_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Create static library
$(LIB_NAME): $(OBJECTS) | $(BUILD_DIR)
	ar rcs $@ $(OBJECTS)

# Build test executable
$(TEST_EXEC): $(OBJECTS) $(TEST_OBJECTS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $^ -o $@

# Build example executable
$(EXAMPLE_EXEC): $(OBJECTS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $(SRC_DIR)/example.c $^ -o $@

# Run tests
test: $(TEST_EXEC)
	./$(TEST_EXEC)

run-test: test

# Run example
example: $(EXAMPLE_EXEC)
	./$(EXAMPLE_EXEC)

run-example: example

# Install (optional)
install: $(LIB_NAME)
	mkdir -p /usr/local/lib
	mkdir -p /usr/local/include
	cp $(LIB_NAME) /usr/local/lib/
	cp include/mos6510.h /usr/local/include/

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR)

# Generate documentation (if doxygen is available)
docs:
	@if command -v doxygen >/dev/null 2>&1; then \
		doxygen Doxyfile; \
	else \
		echo "Doxygen not found. Install doxygen to generate documentation."; \
	fi

# Show help
help:
	@echo "Available targets:"
	@echo "  all        - Build library, tests, and examples"
	@echo "  test       - Run the test suite"
	@echo "  example    - Build and run example programs"
	@echo "  clean      - Remove build artifacts"
	@echo "  install    - Install library to system"
	@echo "  docs       - Generate documentation"
	@echo "  help       - Show this help message"

# Debug build
debug: CFLAGS += -DDEBUG -O0
debug: all

# Release build
release: CFLAGS += -DNDEBUG -O3
release: clean all