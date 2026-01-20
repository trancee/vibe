/*
   Commodore 64 Emulator
   ---------------------
   SDL2 Audio + Video + Glue
*/

// #include <SDL2/SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mos6510.h"
#include "cia6526.h"
// #include "sid6581.h"
#include "vic.h"

#define DEBUG true

/* ===== Globals ===== */

CPU cpu;
CIA cia1, cia2;
// SID6581 sid;
VIC vic;

// SDL_AudioDeviceID audio_dev;
// SDL_Window *window;
// SDL_Renderer *renderer;
// SDL_Texture *texture;

#define SAMPLE_RATE 44100
#define FPS 50
#define CPU_CLOCK 985248
#define CYCLES_PER_FRAME (CPU_CLOCK / FPS)

// uint32_t framebuffer[320 * 200];

/* ============================================================
   SDL Audio Callback
   ============================================================ */

// void audio_callback(void *userdata, Uint8 *stream, int len)
// {
//     float *out = (float *)stream;
//     int samples = len / sizeof(float);

//     for (int i = 0; i < samples; i++)
//     {
//         /* sid_tick already produces samples internally */
//         out[i] = 0.0f;
//     }
// }

/* ============================================================
   ROM Loading
   ============================================================ */

void load_rom(const char *path, uint16_t addr, size_t size)
{
    FILE *f = fopen(path, "rb");
    if (!f)
    {
        printf("Missing ROM: %s\n", path);
        exit(1);
    }
    fread(&cpu.memory[addr], 1, size, f);
    fclose(f);
}

/* ============================================================
   Main
   ============================================================ */

int main(int argc, char **argv)
{
    cpu_init(&cpu, DEBUG);

    /* Load ROMs */
    load_rom("roms/basic.901226-01.bin", 0xA000, 0x2000);
    load_rom("roms/characters.906143-02.bin", 0xD000, 0x1000);
    load_rom("roms/kernal.901227-03.bin", 0xE000, 0x2000);

    /* Reset chips */
    cpu_reset(&cpu);
    cia_reset(&cia1);
    cia_reset(&cia2);
    vic_reset(&vic, cpu.memory);
    // sid_reset(SAMPLE_RATE);

    /* SDL Init */
    // SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);

    // window = SDL_CreateWindow(
    //     "C64 Emulator",
    //     SDL_WINDOWPOS_CENTERED,
    //     SDL_WINDOWPOS_CENTERED,
    //     640, 400,
    //     SDL_WINDOW_SHOWN);

    // renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    // texture = SDL_CreateTexture(
    //     renderer,
    //     SDL_PIXELFORMAT_ARGB8888,
    //     SDL_TEXTUREACCESS_STREAMING,
    //     320, 200);

    // /* Audio */
    // SDL_AudioSpec want = {0};
    // want.freq = SAMPLE_RATE;
    // want.format = AUDIO_F32SYS;
    // want.channels = 1;
    // want.samples = 1024;
    // want.callback = audio_callback;

    // audio_dev = SDL_OpenAudioDevice(NULL, 0, &want, NULL, 0);
    // SDL_PauseAudioDevice(audio_dev, 0);

    /* Main loop */
    int running = 1;
    // SDL_Event e;

    while (running)
    {

        // uint32_t start = SDL_GetTicks();

        /* One PAL frame */
        for (int i = 0; i < CYCLES_PER_FRAME; i++)
        {
            cpu_step(&cpu);

            vic_clock(&vic);
            vic_clock(&vic);
            cia_clock(&cia1);
            cia_clock(&cia2);
            // sid_tick();
        }

        /* Video (placeholder background) */
        // SDL_UpdateTexture(texture, NULL, framebuffer, 320 * sizeof(uint32_t));
        // SDL_RenderClear(renderer);
        // SDL_RenderCopy(renderer, texture, NULL, NULL);
        // SDL_RenderPresent(renderer);

        // while (SDL_PollEvent(&e))
        // {
        //     if (e.type == SDL_QUIT)
        //         running = 0;
        // }

        // /* Frame sync */
        // uint32_t elapsed = SDL_GetTicks() - start;
        // if (elapsed < (1000 / FPS))
        //     SDL_Delay((1000 / FPS) - elapsed);
    }

    // SDL_Quit();
    return 0;
}
