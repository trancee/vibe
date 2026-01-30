# C64 Emulator Makefile

CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O2 -g
LDFLAGS = -lm

# Source files
SRCS = src/main.c \
       src/c64.c \
       src/cpu.c \
       src/memory.c \
       src/vic.c \
       src/cia.c \
       src/sid.c

# Object files
OBJDIR = obj
OBJS = $(patsubst src/%.c,$(OBJDIR)/%.o,$(SRCS))

# Target executable
TARGET = c64emu

# Default target
all: $(TARGET)

# Create object directory
$(OBJDIR):
	mkdir -p $(OBJDIR)

# Compile source files
$(OBJDIR)/%.o: src/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Link executable
$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

# Clean build artifacts
clean:
	rm -rf $(OBJDIR) $(TARGET)

# Run the emulator
run: $(TARGET)
	./$(TARGET) -r roms

# Run with debug output
debug: $(TARGET)
	./$(TARGET) -r roms -d

# Run for limited frames (for testing)
test: $(TARGET)
	./$(TARGET) -r roms -f 100

# Dependencies
$(OBJDIR)/main.o: src/main.c src/c64.h src/types.h
$(OBJDIR)/c64.o: src/c64.c src/c64.h src/cpu.h src/memory.h src/vic.h src/cia.h src/sid.h
$(OBJDIR)/cpu.o: src/cpu.c src/cpu.h src/c64.h
$(OBJDIR)/memory.o: src/memory.c src/memory.h src/c64.h
$(OBJDIR)/vic.o: src/vic.c src/vic.h src/c64.h
$(OBJDIR)/cia.o: src/cia.c src/cia.h src/c64.h
$(OBJDIR)/sid.o: src/sid.c src/sid.h src/c64.h

.PHONY: all clean run debug test
