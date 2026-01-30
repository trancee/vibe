/*
 * C64 Emulator - Main Entry Point
 * 
 * Usage: c64emu [options]
 * Options:
 *   -r <path>   ROM directory (default: ./roms)
 *   -d          Debug mode (single step)
 *   -h          Show help
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include "c64.h"

static C64 g_c64;
static volatile bool g_running = true;
static struct termios g_old_termios;
static bool g_debug_mode = false;

/* Signal handler for clean shutdown */
static void signal_handler(int sig)
{
    (void)sig;
    g_running = false;
}

/* Set terminal to raw mode for keyboard input */
static void terminal_raw_mode(void)
{
    struct termios raw;
    tcgetattr(STDIN_FILENO, &g_old_termios);
    raw = g_old_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    
    /* Set non-blocking */
    fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL) | O_NONBLOCK);
}

/* Restore terminal settings */
static void terminal_restore(void)
{
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_old_termios);
}

/* C64 keyboard matrix layout */
/* Maps ASCII to (row, col) in C64 matrix */
typedef struct {
    char key;
    int row;
    int col;
} KeyMapping;

static const KeyMapping key_map[] = {
    /* Row 0 */
    {'1', 0, 0}, {'3', 0, 1}, {'5', 0, 2}, {'7', 0, 3},
    {'9', 0, 4}, {'+', 0, 5}, /* POUND*/ {'\b', 0, 7},  /* DEL */
    /* Row 1 */
    {'w', 1, 0}, {'r', 1, 1}, {'y', 1, 2}, {'i', 1, 3},
    {'p', 1, 4}, {'*', 1, 5}, {'\n', 1, 6}, /* RETURN */
    /* Row 2 */
    {'a', 2, 0}, {'d', 2, 1}, {'g', 2, 2}, {'j', 2, 3},
    {'l', 2, 4}, {';', 2, 5}, /* cursor right */ /* cursor down */
    /* Row 3 */
    {'4', 3, 0}, {'6', 3, 1}, {'8', 3, 2}, {'0', 3, 3},
    {'-', 3, 4}, /* HOME */ /* cursor up */
    /* Row 4 */
    {'z', 4, 0}, {'c', 4, 1}, {'b', 4, 2}, {'m', 4, 3},
    {'.', 4, 4}, /* Right SHIFT */ {' ', 4, 7}, /* SPACE */
    /* Row 5 */
    {'s', 5, 0}, {'f', 5, 1}, {'h', 5, 2}, {'k', 5, 3},
    {':', 5, 4}, {'=', 5, 5}, /* commodore key */
    /* Row 6 */
    {'e', 6, 0}, {'t', 6, 1}, {'u', 6, 2}, {'o', 6, 3},
    {'@', 6, 4}, {'^', 6, 5}, /* PI */
    /* Row 7 */
    {'2', 7, 0}, {'q', 7, 1}, /* C= */ /* SPACE */ 
    /* Left SHIFT */ {'x', 7, 4}, {'v', 7, 5}, {'n', 7, 6}, {',', 7, 7},
    {0, 0, 0}
};

static void process_key(C64 *c64, char key)
{
    for (const KeyMapping *km = key_map; km->key != 0; km++) {
        if (km->key == key) {
            c64_key_press(c64, km->row, km->col);
            return;
        }
    }
}

static void usage(const char *prog)
{
    printf("Usage: %s [options]\n", prog);
    printf("Options:\n");
    printf("  -r <path>   ROM directory (default: ./roms)\n");
    printf("  -d          Debug mode (single step)\n");
    printf("  -h          Show help\n");
}

/* Get time in nanoseconds */
static uint64_t get_time_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

int main(int argc, char *argv[])
{
    const char *rom_path = "./roms";
    int opt;
    
    while ((opt = getopt(argc, argv, "r:dh")) != -1) {
        switch (opt) {
            case 'r':
                rom_path = optarg;
                break;
            case 'd':
                g_debug_mode = true;
                break;
            case 'h':
            default:
                usage(argv[0]);
                return opt == 'h' ? 0 : 1;
        }
    }
    
    printf("C64 Emulator\n");
    printf("============\n\n");
    
    /* Initialize C64 */
    c64_init(&g_c64);
    
    /* Load ROMs */
    printf("Loading ROMs from: %s\n", rom_path);
    if (!c64_load_roms(&g_c64, rom_path)) {
        printf("\nWarning: Some ROMs could not be loaded.\n");
        printf("The emulator requires:\n");
        printf("  - basic.rom (8192 bytes)\n");
        printf("  - kernal.rom (8192 bytes)\n");
        printf("  - char.rom (4096 bytes)\n");
        printf("\nContinuing with missing ROMs may cause unexpected behavior.\n\n");
    }
    
    /* Reset system */
    c64_reset(&g_c64);
    
    printf("System initialized.\n");
    printf("PC: $%04X\n\n", c64_get_pc(&g_c64));
    
    if (g_debug_mode) {
        printf("Debug mode enabled. Press Enter to step, 'q' to quit.\n\n");
    } else {
        printf("Press Ctrl+C to quit.\n\n");
    }
    
    /* Setup signal handlers */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    /* Setup terminal */
    terminal_raw_mode();
    
    /* Timing */
    const uint64_t ns_per_frame = 1000000000ULL / C64_PAL_FPS;
    uint64_t last_frame_time = get_time_ns();
    uint32_t frame_count = 0;
    
    /* Main loop */
    while (g_running && g_c64.running) {
        /* Process keyboard input */
        char key;
        if (read(STDIN_FILENO, &key, 1) == 1) {
            if (key == 3) {  /* Ctrl+C */
                g_running = false;
                break;
            }
            if (g_debug_mode && key == 'q') {
                g_running = false;
                break;
            }
            process_key(&g_c64, key);
        } else {
            /* Release all keys after a short time */
            c64_key_clear(&g_c64);
        }
        
        if (g_debug_mode) {
            /* Single step mode */
            c64_dump_state(&g_c64);
            
            char input;
            while (read(STDIN_FILENO, &input, 1) != 1) {
                usleep(10000);
            }
            
            if (input == 'q') {
                g_running = false;
                break;
            }
            
            /* Step one instruction */
            cpu_step(&g_c64.cpu);
        } else {
            /* Run one frame */
            c64_run_frame(&g_c64);
            frame_count++;
            
            /* Render screen */
            if (frame_count % 10 == 0) {  /* Render every 10 frames for performance */
                c64_render_screen(&g_c64);
            }
            
            /* Frame timing - sync to 50 Hz */
            uint64_t now = get_time_ns();
            uint64_t elapsed = now - last_frame_time;
            if (elapsed < ns_per_frame) {
                usleep((ns_per_frame - elapsed) / 1000);
            }
            last_frame_time = get_time_ns();
        }
    }
    
    /* Cleanup */
    terminal_restore();
    
    printf("\n\nEmulator stopped.\n");
    c64_dump_state(&g_c64);
    
    return 0;
}
