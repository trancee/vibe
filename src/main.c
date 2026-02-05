/*
   Commodore 64 Emulator
   ---------------------
   SDL2 Audio + Video + Glue
*/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include "SDL.h"

#include "c64.h"
#include "sid_file.h"

#define DEBUG false

/* ===== Globals ===== */

#define SAMPLE_RATE 44100
#define AUDIO_BUFFER_SIZE 2048
#define CPU_CLOCK 985248

/* Audio ring buffer */
#define RING_BUFFER_SIZE (SAMPLE_RATE * 2) /* 2 seconds of audio */
static int16_t audio_ring_buffer[RING_BUFFER_SIZE];
static volatile uint32_t ring_write_pos = 0;
static volatile uint32_t ring_read_pos = 0;

/* Global running flag for signal handler */
static volatile int running = 1;

static void signal_handler(int sig)
{
    (void)sig;
    running = 0;
}

/* SDL audio callback - called from audio thread */
static void audio_callback(void *userdata, Uint8 *stream, int len)
{
    (void)userdata;
    int16_t *out = (int16_t *)stream;
    int samples = len / sizeof(int16_t);

    for (int i = 0; i < samples; i++)
    {
        if (ring_read_pos != ring_write_pos)
        {
            out[i] = audio_ring_buffer[ring_read_pos];
            ring_read_pos = (ring_read_pos + 1) % RING_BUFFER_SIZE;
        }
        else
        {
            /* Buffer underrun - output silence */
            out[i] = 0;
        }
    }
}

/* Add samples to ring buffer */
static void audio_push_samples(int16_t *samples, uint32_t count)
{
    for (uint32_t i = 0; i < count; i++)
    {
        uint32_t next_pos = (ring_write_pos + 1) % RING_BUFFER_SIZE;
        if (next_pos != ring_read_pos)
        { /* Don't overflow */
            audio_ring_buffer[ring_write_pos] = samples[i];
            ring_write_pos = next_pos;
        }
    }
}

/* Get number of samples in ring buffer */
static uint32_t audio_buffered_samples(void)
{
    if (ring_write_pos >= ring_read_pos)
    {
        return ring_write_pos - ring_read_pos;
    }
    else
    {
        return RING_BUFFER_SIZE - ring_read_pos + ring_write_pos;
    }
}

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
        // filename = "roms/Ikari_Union.sid";
        filename = "roms/Wizball.sid";
        // filename = "roms/Hawkeye.sid";
        // filename = "roms/RoboCop.sid";
        // filename = "roms/Cybernoid.sid";
        // filename = "roms/Cybernoid_II.sid";
        printf("No file specified, using default: %s\n\n", filename);
    }
    else
    {
        filename = argv[1];
    }

    /* Initialize SDL */
    if (SDL_Init(SDL_INIT_AUDIO) < 0)
    {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    /* Set up audio */
    SDL_AudioSpec want, have;
    SDL_memset(&want, 0, sizeof(want));
    want.freq = SAMPLE_RATE;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = AUDIO_BUFFER_SIZE;
    want.callback = audio_callback;

    SDL_AudioDeviceID audio_dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (audio_dev == 0)
    {
        fprintf(stderr, "SDL_OpenAudioDevice failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    printf("\nAudio initialized: %d Hz, %d samples buffer\n", have.freq, have.samples);

    /* Initialize C64 */
    C64 c64;
    c64_init(&c64);
    c64_set_debug(&c64, DEBUG, NULL);

    sid_file_t sid;
    if (siddy(filename, &sid) != 0)
    {
        return 1;
    }

    /* Load SID data into C64 memory */
    c64_write_data(&c64, sid.real_load_address, sid.data, sid.data_length);

    /* Determine playback rate: CIA timer = 60Hz, VBI = 50Hz (PAL) or 60Hz (NTSC) */
    int fps;
    if (sid_song_uses_cia(&sid, sid.start_song))
    {
        fps = 60; /* CIA timer tunes run at 60Hz */
    }
    else
    {
        fps = (sid.flags.clock == SID_CLOCK_NTSC) ? 60 : 50; /* VBI rate */
    }
    uint32_t cycles_per_frame = CPU_CLOCK / fps;
    printf("Playback rate: %d Hz (%u cycles/frame)\n", fps, cycles_per_frame);

    /* Set up SID audio buffer */
    int16_t sid_buffer[SAMPLE_RATE / 50 + 100]; /* Samples per frame + margin (50Hz worst case) */
    sid_set_audio_buffer(&c64.sid, sid_buffer, sizeof(sid_buffer) / sizeof(sid_buffer[0]));

    /* Set up C64 environment for SID playback */
    /* $02A6 = PAL/NTSC flag (1 = PAL, 0 = NTSC) */
    c64_write_byte(&c64, 0x02A6, (sid.flags.clock == SID_CLOCK_NTSC) ? 0x00 : 0x01);

    /* Set up signal handler for clean exit */
    signal(SIGINT, signal_handler);

    /* Start audio playback */
    SDL_PauseAudioDevice(audio_dev, 0);

    printf("\nPlaying: %s by %s\n", sid.name, sid.author);
    printf("Press Ctrl+C to stop...\n\n");

    /* Main loop */
    uint64_t total_cycles = 0;
    uint32_t frames = 0;
    Uint32 start_time = SDL_GetTicks();

    /* Initialize the tune: call init_address with song number in A */
    uint16_t init_addr = sid.init_address ? sid.init_address : sid.real_load_address;
    uint16_t play_addr = sid.play_address ? sid.play_address : sid.real_load_address + 2;

    /* Start execution at init bootstrap */
    c64_set_pc(&c64, init_addr);

    c64.cpu.A = sid.start_song; /* Song number in A register */

    /* Run init first (execute until we reach the idle loop) */
    while (running && c64_get_pc(&c64) != 0x0001)
    {
        c64_step(&c64);
        total_cycles++;

        /* Discard any audio generated during init */
        c64.sid.buffer_pos = 0;

        if (total_cycles > 1000000)
        { /* Timeout after 1M cycles */
            printf("Init took too long, starting playback anyway\n");
            break;
        }
    }
    total_cycles = 0;

    /* Clear any stale audio in ring buffer before starting playback */
    ring_write_pos = 0;
    ring_read_pos = 0;
    c64.sid.buffer_pos = 0;

    while (running)
    {
        /* Reset SID audio buffer for this frame */
        c64.sid.buffer_pos = 0;

        c64_set_pc(&c64, play_addr);

        /* Execute until play returns (CPU reaches idle loop) */
        uint32_t play_cycles = 0;
        while (c64_get_pc(&c64) != 0x0001 && play_cycles < cycles_per_frame)
        {
            uint8_t cycles = c64_step(&c64);
            play_cycles += cycles;
        }
        total_cycles += play_cycles;

        /* Now run remaining cycles for this frame (clocking SID) */
        uint32_t remaining_cycles = cycles_per_frame > play_cycles ? cycles_per_frame - play_cycles : 0;
        if (remaining_cycles > 0)
        {
            sid_clock(&c64.sid, remaining_cycles);
        }

        frames++;

        /* Push generated samples to audio buffer */
        uint32_t samples = sid_get_samples(&c64.sid);
        if (samples > 0)
        {
            audio_push_samples(sid_buffer, samples);
        }

        /* Handle SDL events */
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT)
            {
                running = 0;
            }
        }

        /*
         * Sync to real time using wall clock.
         * Each frame should take (1000/fps) milliseconds.
         * Use cumulative timing to avoid drift.
         */
        Uint32 target_time = start_time + (frames * 1000 / fps);
        Uint32 now = SDL_GetTicks();

        if (target_time > now)
        {
            SDL_Delay(target_time - now);
        }

        /* Print status every second */
        if (frames % fps == 0)
        {
            Uint32 elapsed = SDL_GetTicks() - start_time;
            uint32_t actual_fps = frames * 1000 / (elapsed ? elapsed : 1);
            printf("\rPlaying: %02u:%02u  Buffer: %5u  FPS: %u  ",
                   (elapsed / 1000) / 60, (elapsed / 1000) % 60,
                   audio_buffered_samples(), actual_fps);
            fflush(stdout);
        }
    }

    printf("\n\nStopped after %llu cycles (%u frames)\n",
           (unsigned long long)total_cycles, frames);

    /* Cleanup */
    SDL_CloseAudioDevice(audio_dev);
    SDL_Quit();

    sid_file_free(&sid);

    return 0;
}
