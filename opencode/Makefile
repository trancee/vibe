CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2 -g
LDFLAGS = -lm

SRCDIR = src
# Exclude test files from main build
SOURCES = $(filter-out $(SRCDIR)/test_%.c, $(wildcard $(SRCDIR)/*.c))
OBJECTS = $(SOURCES:.c=.o)
TARGET = c64emu
TEST_TARGET = test_cia_simple
TEST_SUITE_TARGET = test_suite
TEST_SOURCES = $(filter-out $(SRCDIR)/test_cia_simple.c, $(filter $(SRCDIR)/test_%.c, $(wildcard $(SRCDIR)/*.c)))
TEST_OBJECTS = $(TEST_SOURCES:.c=.o)
ALL_TEST_OBJECTS = $(TEST_OBJECTS) src/c64.o src/cpu.o src/memory.o src/vic.o src/sid.o src/cia.o

.PHONY: all clean test test-suite test-cpu test-cia test-vic test-memory test-sid test-all run-test

all: $(TARGET)

test: $(TEST_SUITE_TARGET)

test-suite: $(TEST_SUITE_TARGET)

# Individual test targets
test-cpu: $(TEST_SUITE_TARGET)
	./$(TEST_SUITE_TARGET) --cpu

test-cia: $(TEST_SUITE_TARGET)
	./$(TEST_SUITE_TARGET) --cia

test-vic: $(TEST_SUITE_TARGET)
	./$(TEST_SUITE_TARGET) --vic

test-memory: $(TEST_SUITE_TARGET)
	./$(TEST_SUITE_TARGET) --memory

test-sid: $(TEST_SUITE_TARGET)
	./$(TEST_SUITE_TARGET) --sid

test-all: $(TEST_SUITE_TARGET)
	./$(TEST_SUITE_TARGET) --verbose

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)

# Original simple CIA test - add minimal main
src/test_cia_simple_main.o: src/test_cia_simple.c
	$(CC) $(CFLAGS) -DSTANDALONE_MAIN -c src/test_cia_simple.c -o src/test_cia_simple_main.o

$(TEST_TARGET): src/test_cia_simple_main.o src/cia.o src/c64.o src/cpu.o src/memory.o src/vic.o src/sid.o
	$(CC) src/test_cia_simple_main.o src/cia.o src/c64.o src/cpu.o src/memory.o src/vic.o src/sid.o -o $@ $(LDFLAGS)

# Comprehensive test suite
$(TEST_SUITE_TARGET): $(ALL_TEST_OBJECTS)
	$(CC) $(ALL_TEST_OBJECTS) -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET) $(TEST_TARGET) $(TEST_SUITE_TARGET) $(ALL_TEST_OBJECTS)

run-test: $(TEST_TARGET)
	./$(TEST_TARGET)

run-test-suite: $(TEST_SUITE_TARGET)
	./$(TEST_SUITE_TARGET)

help:
	@echo "Available targets:"
	@echo "  all            - Build the emulator"
	@echo "  clean          - Remove build artifacts"
	@echo "  test           - Build original CIA test"
	@echo "  test-suite     - Build comprehensive test suite"
	@echo "  test-cpu       - Run CPU tests only"
	@echo "  test-cia       - Run CIA tests only"
	@echo "  test-vic       - Run VIC tests only"
	@echo "  test-memory    - Run memory tests only"
	@echo "  test-sid       - Run SID tests only"
	@echo "  test-all       - Run all tests with verbose output"
	@echo "  run-test       - Run original CIA test"
	@echo "  run-test-suite - Run comprehensive test suite"
	@echo "  help           - Show this help message"