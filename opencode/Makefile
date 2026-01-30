CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2 -g
LDFLAGS = -lm

SRCDIR = src
# Exclude test files from main build
SOURCES = $(filter-out $(SRCDIR)/test_%.c, $(wildcard $(SRCDIR)/*.c))
OBJECTS = $(SOURCES:.c=.o)
TARGET = c64emu
TEST_TARGET = test_cia_simple

.PHONY: all clean test

all: $(TARGET)

test: $(TEST_TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)

$(TEST_TARGET): src/test_cia_simple.o src/cia.o src/c64.o src/cpu.o src/memory.o src/vic.o src/sid.o
	$(CC) src/test_cia_simple.o src/cia.o src/c64.o src/cpu.o src/memory.o src/vic.o src/sid.o -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET) $(TEST_TARGET)

run-test: test_cia_simple
	./test_cia_simple

help:
	@echo "Available targets:"
	@echo "  all     - Build the emulator"
	@echo "  clean   - Remove build artifacts"
	@echo "  test    - Build and run the emulator"
	@echo "  run-test - Build and run CIA timing test"
	@echo "  help    - Show this help message"