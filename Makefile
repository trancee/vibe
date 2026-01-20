CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2 -g
INCLUDES = -Iinclude
SRC_DIR = src
TEST_DIR = tests
BUILD_DIR = build

# Source files
SOURCES = $(SRC_DIR)/mos6510.c $(SRC_DIR)/opcodes.c $(SRC_DIR)/illegal_opcodes.c $(SRC_DIR)/opcode_table.c $(SRC_DIR)/vic.c $(SRC_DIR)/cia6526.c
TEST_SOURCES = $(TEST_DIR)/test_opcodes.c
TEST_DORMANN_SRC = $(TEST_DIR)/test_dormann.c
TEST_LORENZ_SRC = $(TEST_DIR)/test_lorenz.c
TEST_NESTEST_SRC = $(TEST_DIR)/test_nestest.c
TEST_VIC_SRC = $(TEST_DIR)/test_vic.c
TEST_CIA1_SRC = $(TEST_DIR)/test_cia1.c
TEST_CIA2_SRC = $(TEST_DIR)/test_cia2.c

# Object files
OBJECTS = $(SOURCES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
TEST_OBJECTS = $(TEST_SOURCES:$(TEST_DIR)/%.c=$(BUILD_DIR)/%.o)
TEST_DORMANN_OBJ = $(TEST_DORMANN_SRC:$(TEST_DIR)/%.c=$(BUILD_DIR)/%.o)
TEST_LORENZ_OBJ = $(TEST_LORENZ_SRC:$(TEST_DIR)/%.c=$(BUILD_DIR)/%.o)
TEST_NESTEST_OBJ = $(TEST_NESTEST_SRC:$(TEST_DIR)/%.c=$(BUILD_DIR)/%.o)
TEST_VIC_OBJ = $(TEST_VIC_SRC:$(TEST_DIR)/%.c=$(BUILD_DIR)/%.o)
TEST_CIA1_OBJ = $(TEST_CIA1_SRC:$(TEST_DIR)/%.c=$(BUILD_DIR)/%.o)
TEST_CIA2_OBJ = $(TEST_CIA2_SRC:$(TEST_DIR)/%.c=$(BUILD_DIR)/%.o)

# Executables
LIB_NAME = $(BUILD_DIR)/libmos6510.a
TEST_EXEC = $(BUILD_DIR)/test_opcodes
TEST_DORMANN_BIN = $(BUILD_DIR)/test_dormann
TEST_LORENZ_BIN = $(BUILD_DIR)/test_lorenz
TEST_NESTEST_BIN = $(BUILD_DIR)/test_nestest
TEST_VIC_BIN = $(BUILD_DIR)/test_vic
TEST_CIA1_BIN = $(BUILD_DIR)/test_cia1
TEST_CIA2_BIN = $(BUILD_DIR)/test_cia2
EXAMPLE_BIN = $(BUILD_DIR)/example
C64_BIN = $(BUILD_DIR)/c64

.PHONY: all clean test run-test example run-example c64 run-c64 docs

all: $(LIB_NAME) $(TEST_EXEC) $(TEST_DORMANN_BIN) $(TEST_LORENZ_BIN) $(TEST_NESTEST_BIN) $(TEST_VIC_BIN) $(TEST_CIA1_BIN) $(TEST_CIA2_BIN) $(EXAMPLE_BIN) $(C64_BIN)

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
$(TEST_DORMANN_BIN): $(OBJECTS) $(TEST_DORMANN_OBJ) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $^ -o $@
$(TEST_LORENZ_BIN): $(OBJECTS) $(TEST_LORENZ_OBJ) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $^ -o $@
$(TEST_NESTEST_BIN): $(OBJECTS) $(TEST_NESTEST_OBJ) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $^ -o $@

# Build VIC-II test executable
$(TEST_VIC_BIN): $(OBJECTS) $(TEST_VIC_OBJ) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $^ -o $@
# Build CIA1 test executable
$(TEST_CIA1_BIN): $(OBJECTS) $(TEST_CIA1_OBJ) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $^ -o $@
# Build CIA2 test executable
$(TEST_CIA2_BIN): $(OBJECTS) $(TEST_CIA2_OBJ) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $^ -o $@

# Build example executable
$(EXAMPLE_BIN): $(OBJECTS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $(SRC_DIR)/example.c $^ -o $@

# Build C64 executable
$(C64_BIN): $(OBJECTS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $(SRC_DIR)/c64.c $^ -o $@

# Run tests
test: $(TEST_EXEC)
	./$(TEST_EXEC)
test-vic: $(TEST_VIC_BIN)
	./$(TEST_VIC_BIN)
test-cia1: $(TEST_CIA1_BIN)
	./$(TEST_CIA1_BIN)
test-cia2: $(TEST_CIA2_BIN)
	./$(TEST_CIA2_BIN)

run-test: test

# Run Dormann tests
test-dormann: $(TEST_DORMANN_BIN)
	./$(TEST_DORMANN_BIN)

run-test-dormann: test-dormann

# Run Lorenz tests
test-lorenz: $(TEST_LORENZ_BIN)
	./$(TEST_LORENZ_BIN)

run-test-lorenz: test-lorenz

# Run NESTEST tests
test-nestest: $(TEST_NESTEST_BIN)
	./$(TEST_NESTEST_BIN)

run-test-nestest: test-nestest

# Run example
example: $(EXAMPLE_BIN)
	./$(EXAMPLE_BIN)

run-example: example

# Run C64
c64: $(C64_BIN)
	./$(C64_BIN)

run-c64: c64

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
	@echo "  all           - Build library, tests, and examples"
	@echo "  test          - Run test suite"
	@echo "  test-dormann  - Run Dormann test suite"
	@echo "  test-lorenz   - Run Lorenz test suite"
	@echo "  test-nestest  - Run NESTEST test suite"
	@echo "  test-vic      - Run VIC test suite"
	@echo "  test-cia1     - Run CIA1 test suite"
	@echo "  test-cia2     - Run CIA2 test suite"
	@echo "  example       - Build and run example programs"
	@echo "  c64           - Build and run c64 programs"
	@echo "  clean         - Remove build artifacts"
	@echo "  install       - Install library to system"
	@echo "  docs          - Generate documentation"
	@echo "  help          - Show this help message"

# Debug build
debug: CFLAGS += -DDEBUG -O0
debug: all

# Release build
release: CFLAGS += -DNDEBUG -O3
release: clean all