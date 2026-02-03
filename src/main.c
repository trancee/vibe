/*
   Commodore 64 Emulator
   ---------------------
   SDL2 Audio + Video + Glue
*/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "c64.h"

#include "sid_file.h"

#define DEBUG true

/* ===== Globals ===== */

#define SAMPLE_RATE 44100
#define FPS 50
#define CPU_CLOCK 985248
#define CYCLES_PER_FRAME (CPU_CLOCK / FPS)

int siddy(const char *filename, sid_file_t *sid)
{
    sid_error_t err;

    err = sid_file_load(filename, sid);
    if (err != SID_OK)
    {
        fprintf(stderr, "Error loading SID file: %s\n", sid_error_string(err));
        return 1;
    }

    sid_file_print_info(sid);

    /* Print speed information for first few songs */
    printf("\nSong speed information:\n");
    for (uint16_t i = 1; i <= sid->songs && i <= 8; i++)
    {
        printf("  Song %2d: %s\n", i,
               sid_song_uses_cia(sid, i) ? "CIA timer (60Hz)" : "VBI (50/60Hz)");
    }

    /* Show first few bytes of data */
    printf("\nFirst 32 bytes of C64 data:\n");
    for (size_t i = 0; i < 32 && i < sid->data_length; i++)
    {
        printf("%02X ", sid->data[i]);
        if ((i + 1) % 16 == 0)
            printf("\n");
    }
    printf("\n");
    return 0;
}

/* ============================================================
   Main
   ============================================================ */

int main(int argc, char **argv)
{
    for (int i = 0; i < argc; i++)
        printf("\t%d: %s\n", i, argv[i]);

    const char *filename;

    if (argc < 2)
    {
        filename = "roms/Ikari_Union.sid";
        printf("No file specified, using default: %s\n\n", filename);
    }
    else
    {
        filename = argv[1];
    }

    sid_file_t sid;
    if (siddy(filename, &sid) != 0)
    {
        return 1;
    }

    C64 c64;
    c64_init(&c64);
    c64_set_debug(&c64, DEBUG, NULL);

    c64_write_data(&c64, sid.real_load_address, sid.data, sid.data_length);
    c64_set_pc(&c64, sid.init_address ? sid.init_address : sid.real_load_address);
    
    /* Main loop */
    int running = 1;
    uint64_t cycles = 0;

    while (running)
    {
        /* One PAL frame */
        for (size_t i = 0; i < CYCLES_PER_FRAME; i++)
        {
            cycles += c64_step(&c64);

            if (cycles >= 10000 || c64_get_pc(&c64) == 0x0001) {
                running = 0;
                break;
            }
        }
    }

    sid_file_free(&sid);

    return 0;
}
