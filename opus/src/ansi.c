#include "vic.h"
#include <stdio.h>

// ANSI color codes for C64 colors (256-color mode)
static const int ansi_colors[16] = {
    16,  // 0: Black
    231, // 1: White
    124, // 2: Red
    44,  // 3: Cyan
    128, // 4: Purple
    34,  // 5: Green
    20,  // 6: Blue
    226, // 7: Yellow
    208, // 8: Orange
    94,  // 9: Brown
    203, // 10: Light Red
    240, // 11: Dark Grey
    248, // 12: Grey
    120, // 13: Light Green
    75,  // 14: Light Blue
    252  // 15: Light Grey
};

// PETSCII to ASCII conversion (simplified)
static char petscii_to_ascii(u8 c)
{
    // Handle common printable characters
    if (c >= 0x41 && c <= 0x5A)
        return c + 0x20; // A-Z -> a-z (shifted)
    if (c >= 0x61 && c <= 0x7A)
        return c - 0x20; // a-z -> A-Z (unshifted)
    if (c >= 0x20 && c <= 0x3F)
        return c; // Space, numbers, symbols
    if (c == 0x40)
        return '@';
    if (c == 0x5B)
        return '[';
    if (c == 0x5D)
        return ']';

    // Graphics characters - show as block or space
    if (c >= 0x00 && c <= 0x1F)
        return ' ';
    if (c >= 0xA0 && c <= 0xBF)
        return '#'; // Graphic chars
    if (c >= 0xC0 && c <= 0xDF)
        return '#'; // Graphic chars

    return '?'; // Unknown
}

void vic_render_frame_ansi(C64Vic *vic)
{
    // Only render every few frames to avoid flooding terminal
    if (vic->frame_count % 10 != 0)
        return;

    u8 bg_color = vic->regs[VIC_BG0] & 0x0F;

    // Clear screen and home cursor
    printf("\033[H");

    // Render 40x25 character screen
    for (int row = 0; row < SCREEN_ROWS; row++)
    {
        for (int col = 0; col < SCREEN_COLS; col++)
        {
            int idx = row * SCREEN_COLS + col;
            u8 ch = vic->screen_chars[idx];
            u8 fg = vic->screen_colors[idx] & 0x0F;

            char ascii = petscii_to_ascii(ch);

            // Set foreground and background colors
            printf("\033[38;5;%dm\033[48;5;%dm%c",
                   ansi_colors[fg], ansi_colors[bg_color], ascii);
        }
        printf("\033[0m\n");
    }

    // Reset colors
    printf("\033[0m");

    // Show status line
    printf("Frame: %u  Raster: %03d  Cycle: %02d\n",
           vic->frame_count, vic->raster_line, vic->raster_cycle);

    fflush(stdout);
}
