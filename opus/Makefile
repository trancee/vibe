# C64 Emulator Makefile

CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O2 -g
LDFLAGS = -lm

# Source files
SRCS = src/c64.c \
       src/cpu.c \
       src/memory.c \
       src/vic.c \
       src/cia.c \
       src/sid.c

MAIN_SRCS = src/main.c \
            src/ansi.c

TEST_DIR = tests

TEST_DORMANN_SRC = $(TEST_DIR)/test_dormann.c
TEST_LORENZ_SRC = $(TEST_DIR)/test_lorenz.c

# Object files
OBJDIR = obj
OBJS = $(patsubst src/%.c,$(OBJDIR)/%.o,$(SRCS))
MAIN_OBJS = $(patsubst src/%.c,$(OBJDIR)/%.o,$(MAIN_SRCS))

TEST_DORMANN_OBJ = $(TEST_DORMANN_SRC:src/%.c=$(OBJDIR)/%.o)
TEST_LORENZ_OBJ = $(TEST_LORENZ_SRC:src/%.c=$(OBJDIR)/%.o)

# Target executable
TARGET = c64emu

TEST_DORMANN_BIN = $(TEST_DIR)/test_dormann
TEST_LORENZ_BIN = $(TEST_DIR)/test_lorenz

# Default target
all: $(TARGET) $(TEST_DORMANN_BIN) $(TEST_LORENZ_BIN)

# Create object directory
$(OBJDIR):
	mkdir -p $(OBJDIR)

# Compile source files
$(OBJDIR)/%.o: src/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Link executable
$(TARGET): $(OBJS) $(MAIN_OBJS) 
	$(CC) $(OBJS) $(MAIN_OBJS) -o $@ $(LDFLAGS)

$(TEST_DORMANN_BIN): $(OBJS) $(TEST_DORMANN_OBJ) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -DTEST_DORMANN $^ -o $@
$(TEST_LORENZ_BIN): $(OBJS) $(TEST_LORENZ_OBJ) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -DTEST_LORENZ $^ -o $@

# Clean build artifacts
clean:
	rm -rf $(OBJDIR) $(TARGET) $(TEST_DORMANN_BIN) $(TEST_LORENZ_BIN)

# Run the emulator
run: $(TARGET)
	./$(TARGET) -r roms

# Run with debug output
debug: $(TARGET)
	./$(TARGET) -r roms -d

# Run for limited frames (for quick test)
quicktest: $(TARGET)
	./$(TARGET) -r roms -f 100

# ============================================================================
# Unit Tests
# ============================================================================

# Test source files
TEST_SRCS = tests/test_main.c \
            tests/test_cpu.c \
            tests/test_cpu_doc.c \
            tests/test_memory.c \
            tests/test_vic.c \
            tests/test_cia.c \
            tests/test_sid.c \
            tests/test_dormann.c \
            tests/test_lorenz.c

# Library sources (without main.c)
LIB_SRCS = src/c64.c \
           src/cpu.c \
           src/memory.c \
           src/vic.c \
           src/cia.c \
           src/sid.c

# Test object files
TEST_OBJS = $(patsubst tests/%.c,$(OBJDIR)/test/%.o,$(TEST_SRCS))
LIB_OBJS = $(patsubst src/%.c,$(OBJDIR)/%.o,$(LIB_SRCS))

# Test executable
TEST_TARGET = run_tests

# Create test object directory
$(OBJDIR)/test:
	mkdir -p $(OBJDIR)/test

# Compile test source files
$(OBJDIR)/test/%.o: tests/%.c | $(OBJDIR)/test
	$(CC) $(CFLAGS) -I. -c $< -o $@

# Link test executable
$(TEST_TARGET): $(LIB_OBJS) $(TEST_OBJS)
	$(CC) $(LIB_OBJS) $(TEST_OBJS) -o $@ $(LDFLAGS)

# Run unit tests
test: $(TEST_TARGET)
	./$(TEST_TARGET)

# Run tests with verbose output
test-verbose: $(TEST_TARGET)
	./$(TEST_TARGET) -v

# Run Dormann tests
test-dormann: $(TEST_DORMANN_BIN)
	./$(TEST_DORMANN_BIN)

run-test-dormann: test-dormann

# Run Lorenz tests
test-lorenz: $(TEST_LORENZ_BIN)
	./$(TEST_LORENZ_BIN)

run-test-lorenz: test-lorenz

# ============================================================================
# Dependencies
# ============================================================================

$(OBJDIR)/main.o: src/main.c src/c64.h src/types.h
$(OBJDIR)/c64.o: src/c64.c src/c64.h src/cpu.h src/memory.h src/vic.h src/cia.h src/sid.h
$(OBJDIR)/cpu.o: src/cpu.c src/cpu.h src/c64.h
$(OBJDIR)/memory.o: src/memory.c src/memory.h src/c64.h
$(OBJDIR)/vic.o: src/vic.c src/vic.h src/c64.h
$(OBJDIR)/cia.o: src/cia.c src/cia.h src/c64.h
$(OBJDIR)/sid.o: src/sid.c src/sid.h src/c64.h

.PHONY: all clean run debug test test-verbose run-test-dormann run-test-lorenz quicktest
