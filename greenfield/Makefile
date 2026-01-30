# C64 Emulator Makefile
#
# Targets:
#   make          - Build the emulator
#   make clean    - Remove build files
#   make debug    - Build with debug symbols
#   make test     - Run tests (if available)

CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c99 -O2
CFLAGS_DEBUG = -Wall -Wextra -pedantic -std=c99 -g -O0 -DDEBUG
LDFLAGS = -lm

# Directories
SRC_DIR = src
INC_DIR = include
BUILD_DIR = build
TEST_DIR = ../tests

# Source files
SRCS = $(SRC_DIR)/main.c \
       $(SRC_DIR)/c64.c \
       $(SRC_DIR)/cpu.c \
       $(SRC_DIR)/opcodes.c \
       $(SRC_DIR)/illegal_opcodes.c \
       $(SRC_DIR)/instructions.c \
       $(SRC_DIR)/vic.c \
       $(SRC_DIR)/cia6526.c \
       $(SRC_DIR)/sid6581.c \
       $(SRC_DIR)/clock.c

# Object files
OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))
OBJS_DEBUG = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%-debug.o,$(SRCS))

# Output binary
TARGET = $(BUILD_DIR)/c64emu
TARGET_DEBUG = $(BUILD_DIR)/c64emu-debug

# Include path
INCLUDES = -I$(INC_DIR)

.PHONY: all clean debug dirs test test-opcodes test-dormann test-vic test-cia1

all: dirs $(TARGET)

debug: dirs $(TARGET_DEBUG)

dirs:
	@mkdir -p $(BUILD_DIR)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)
	@echo "Build complete: $@"

$(TARGET_DEBUG): $(OBJS_DEBUG)
	$(CC) $(OBJS_DEBUG) -o $@ $(LDFLAGS)
	@echo "Debug build complete: $@"

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD_DIR)/%-debug.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS_DEBUG) $(INCLUDES) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)
	@echo "Clean complete"

# Dependencies
$(BUILD_DIR)/main.o: $(SRC_DIR)/main.c $(INC_DIR)/c64.h
$(BUILD_DIR)/c64.o: $(SRC_DIR)/c64.c $(INC_DIR)/c64.h $(INC_DIR)/cpu.h $(INC_DIR)/vic.h $(INC_DIR)/cia6526.h $(INC_DIR)/sid6581.h $(INC_DIR)/clock.h
$(BUILD_DIR)/cpu.o: $(SRC_DIR)/cpu.c $(INC_DIR)/cpu.h $(INC_DIR)/mos6510.h
$(BUILD_DIR)/opcodes.o: $(SRC_DIR)/opcodes.c $(INC_DIR)/cpu.h $(INC_DIR)/mos6510.h
$(BUILD_DIR)/illegal_opcodes.o: $(SRC_DIR)/illegal_opcodes.c $(INC_DIR)/cpu.h $(INC_DIR)/mos6510.h
$(BUILD_DIR)/instructions.o: $(SRC_DIR)/instructions.c $(INC_DIR)/cpu.h $(INC_DIR)/mos6510.h
$(BUILD_DIR)/vic.o: $(SRC_DIR)/vic.c $(INC_DIR)/vic.h
$(BUILD_DIR)/cia6526.o: $(SRC_DIR)/cia6526.c $(INC_DIR)/cia6526.h
$(BUILD_DIR)/sid6581.o: $(SRC_DIR)/sid6581.c $(INC_DIR)/sid6581.h
$(BUILD_DIR)/clock.o: $(SRC_DIR)/clock.c $(INC_DIR)/clock.h

# Test sources (excluding main.c)
TEST_OBJS = $(BUILD_DIR)/cpu.o \
            $(BUILD_DIR)/opcodes.o \
            $(BUILD_DIR)/illegal_opcodes.o \
            $(BUILD_DIR)/instructions.o \
            $(BUILD_DIR)/clock.o

# Test targets
TEST_OPCODES = $(BUILD_DIR)/test_opcodes
TEST_DORMANN = $(BUILD_DIR)/test_dormann
TEST_NESTEST = $(BUILD_DIR)/test_nestest
TEST_LORENZ = $(BUILD_DIR)/test_lorenz
TEST_VIC = $(BUILD_DIR)/test_vic
TEST_CIA1 = $(BUILD_DIR)/test_cia1
TEST_CIA2 = $(BUILD_DIR)/test_cia2
TEST_SID = $(BUILD_DIR)/test_sid

test: test-opcodes

test-opcodes: dirs $(TEST_OPCODES)
	@echo "Running opcode tests..."
	./$(TEST_OPCODES)

test-dormann: dirs $(TEST_DORMANN)
	@echo "Running Dormann test suite..."
	cd .. && ./greenfield/$(TEST_DORMANN)

test-vic: dirs $(TEST_VIC)
	@echo "Running VIC-II tests..."
	./$(TEST_VIC)

test-cia1: dirs $(TEST_CIA1)
	@echo "Running CIA1 tests..."
	./$(TEST_CIA1)

test-cia2: dirs $(TEST_CIA2)
	@echo "Running CIA2 tests..."
	./$(TEST_CIA2)

test-sid: dirs $(TEST_SID)
	@echo "Running SID tests..."
	./$(TEST_SID)

test-nestest: dirs $(TEST_NESTEST)
	@echo "Running NESTEST suite..."
	cd .. && ./greenfield/$(TEST_NESTEST)

test-lorenz: dirs $(TEST_LORENZ)
	@echo "Running Lorenz test suite..."
	cd .. && ./greenfield/$(TEST_LORENZ)

$(TEST_OPCODES): $(BUILD_DIR)/test_opcodes.o $(TEST_OBJS)
	$(CC) $^ -o $@ $(LDFLAGS)
	@echo "Test build complete: $@"

$(TEST_DORMANN): $(BUILD_DIR)/test_dormann.o $(TEST_OBJS)
	$(CC) $^ -o $@ $(LDFLAGS)
	@echo "Test build complete: $@"

$(BUILD_DIR)/test_opcodes.o: $(TEST_DIR)/test_opcodes.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD_DIR)/test_dormann.o: $(TEST_DIR)/test_dormann.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(TEST_NESTEST): $(BUILD_DIR)/test_nestest.o $(TEST_OBJS)
	$(CC) $^ -o $@ $(LDFLAGS)
	@echo "Test build complete: $@"

$(BUILD_DIR)/test_nestest.o: $(TEST_DIR)/test_nestest.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(TEST_LORENZ): $(BUILD_DIR)/test_lorenz.o $(BUILD_DIR)/c64.o $(TEST_OBJS) $(BUILD_DIR)/vic.o $(BUILD_DIR)/cia6526.o $(BUILD_DIR)/sid6581.o
	$(CC) $^ -o $@ $(LDFLAGS)
	@echo "Test build complete: $@"

$(BUILD_DIR)/test_lorenz.o: $(TEST_DIR)/test_lorenz.c
	$(CC) $(CFLAGS) $(INCLUDES) -DLORENZ_TEST_COMPAT -c $< -o $@

$(TEST_VIC): $(BUILD_DIR)/test_vic.o $(BUILD_DIR)/vic.o $(BUILD_DIR)/clock.o
	$(CC) $^ -o $@ $(LDFLAGS)
	@echo "Test build complete: $@"

$(BUILD_DIR)/test_vic.o: $(TEST_DIR)/test_vic.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(TEST_CIA1): $(BUILD_DIR)/test_cia1.o $(BUILD_DIR)/cia6526.o $(BUILD_DIR)/clock.o
	$(CC) $^ -o $@ $(LDFLAGS)
	@echo "Test build complete: $@"

$(BUILD_DIR)/test_cia1.o: $(TEST_DIR)/test_cia1.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(TEST_CIA2): $(BUILD_DIR)/test_cia2.o $(BUILD_DIR)/cia6526.o $(BUILD_DIR)/clock.o
	$(CC) $^ -o $@ $(LDFLAGS)
	@echo "Test build complete: $@"

$(BUILD_DIR)/test_cia2.o: $(TEST_DIR)/test_cia2.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(TEST_SID): $(BUILD_DIR)/test_sid.o $(BUILD_DIR)/sid6581.o $(BUILD_DIR)/clock.o
	$(CC) $^ -o $@ $(LDFLAGS)
	@echo "Test build complete: $@"

$(BUILD_DIR)/test_sid.o: $(TEST_DIR)/test_sid.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Run target
run: $(TARGET)
	./$(TARGET)

run-debug: $(TARGET_DEBUG)
	./$(TARGET_DEBUG) -d
