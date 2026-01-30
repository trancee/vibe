/**
 * main.c - C64 Emulator entry point
 *
 * Initializes the system, loads ROMs, and runs the main loop.
 */

#define _BSD_SOURCE

#include "c64.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <termios.h>
#include <sys/time.h>

static C64System sys;
static bool running = true;
static struct termios orig_termios;

// Signal handler for clean shutdown
static void signal_handler(int sig)
{
    (void)sig;
    running = false;
}

// Restore terminal settings
static void restore_terminal(void)
{
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
    printf("\033[?25h"); // Show cursor
    printf("\033[0m");   // Reset colors
}

// Setup terminal for raw input
static void setup_terminal(void)
{
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(restore_terminal);

    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);

    printf("\033[?25l"); // Hide cursor
    printf("\033[2J");   // Clear screen
}

// Get current time in microseconds
static u64 get_time_us(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (u64)tv.tv_sec * 1000000 + tv.tv_usec;
}

// Print usage information
static void print_usage(const char *prog)
{
    printf("C64 Emulator\n");
    printf("Usage: %s [options]\n", prog);
    printf("Options:\n");
    printf("  -r <path>    Path to ROM directory (default: ./roms)\n");
    printf("  -d           Enable debug output\n");
    printf("  -f <frames>  Run for N frames then exit\n");
    printf("  -h           Show this help\n");
    printf("\n");
    printf("Required ROM files:\n");
    printf("  basic.rom    - BASIC interpreter (8KB)\n");
    printf("  kernal.rom   - KERNAL operating system (8KB)\n");
    printf("  char.rom     - Character set (4KB)\n");
    printf("\n");
    printf("Controls:\n");
    printf("  Ctrl+C       - Quit emulator\n");
}

int main(int argc, char *argv[])
{
    const char *rom_path = "roms";
    bool debug = false;
    int max_frames = 0;

    // Parse command line arguments
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-r") == 0 && i + 1 < argc)
        {
            rom_path = argv[++i];
        }
        else if (strcmp(argv[i], "-d") == 0)
        {
            debug = true;
        }
        else if (strcmp(argv[i], "-f") == 0 && i + 1 < argc)
        {
            max_frames = atoi(argv[++i]);
        }
        else if (strcmp(argv[i], "-h") == 0)
        {
            print_usage(argv[0]);
            return 0;
        }
        else
        {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    printf("C64 Emulator Starting...\n");
    printf("ROM path: %s\n", rom_path);

    // Initialize system
    c64_init(&sys);
    sys.debug_enabled = debug;

    // Load ROMs
    if (!c64_load_roms(&sys, rom_path))
    {
        fprintf(stderr, "Failed to load ROMs from %s\n", rom_path);
        fprintf(stderr, "Make sure basic.rom, kernal.rom, and char.rom exist.\n");
        return 1;
    }

    // Setup signal handler
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Setup terminal
    setup_terminal();

    // Reset system
    c64_reset(&sys);

    printf("System initialized. Starting emulation...\n");
    sleep(1); // Brief pause before starting

    // Main emulation loop
    u64 last_time = get_time_us();
    u32 frame_count = 0;
    (void)C64_FRAME_CYCLES; // Silence unused macro warning

    while (running && sys.running)
    {
        // Run one frame
        c64_run_frame(&sys);
        frame_count++;

        // Check for max frames limit
        if (max_frames > 0 && frame_count >= (u32)max_frames)
        {
            break;
        }

        // Frame timing (target ~50Hz for PAL)
        u64 current_time = get_time_us();
        u64 elapsed = current_time - last_time;
        u64 target_us = 20000; // 50 Hz = 20ms per frame

        if (elapsed < target_us)
        {
            usleep((u32)(target_us - elapsed));
        }

        last_time = get_time_us();

        // Check for keyboard input
        char c;
        if (read(STDIN_FILENO, &c, 1) == 1)
        {
            if (c == 3)
            { // Ctrl+C
                running = false;
            }
            // TODO: Map keyboard input to C64 keyboard matrix
        }
    }

    // Cleanup
    c64_destroy(&sys);

    printf("\nEmulation ended after %u frames (%llu cycles)\n",
           frame_count, sys.cycle_count);

    return 0;
}
