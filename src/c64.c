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

#include "../include/c64.h"

#define DEBUG true

/* ===== Globals ===== */

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

void c64_init(C64 *c64, bool debug)
{
    cpu_init(&c64->cpu, debug);

    /* Load ROMs */
    c64_load_rom(c64, "roms/basic.901226-01.bin", 0xA000, 0x2000);
    c64_load_rom(c64, "roms/characters.906143-02.bin", 0xD000, 0x1000);
    c64_load_rom(c64, "roms/kernal.901227-03.bin", 0xE000, 0x2000);

    /* Reset chips */
    c64_reset(c64);
}

void c64_reset(C64 *c64)
{
    cpu_reset(&c64->cpu);

    cia_reset(&c64->cia1);
    cia_reset(&c64->cia2);

    vic_reset(&c64->vic, c64->cpu.memory);

    // sid_reset(SAMPLE_RATE);
}
void c64_reset_pc(C64 *c64, uint16_t addr)
{
    cpu_reset_pc(&c64->cpu, addr);
}

uint16_t c64_get_pc(C64 *c64)
{
    return c64->cpu.pc;
}
void c64_set_pc(C64 *c64, uint16_t addr)
{
    c64->cpu.pc = addr;
}

uint8_t c64_read_byte(C64 *c64, uint16_t addr)
{
    return cpu_read_byte(&c64->cpu, addr);
}
uint16_t c64_read_word(C64 *c64, uint16_t addr)
{
    return cpu_read_word(&c64->cpu, addr);
}

void c64_write_byte(C64 *c64, uint16_t addr, uint8_t data)
{
    cpu_write_byte(&c64->cpu, addr, data);
}
void c64_write_word(C64 *c64, uint16_t addr, uint16_t data)
{
    cpu_write_word(&c64->cpu, addr, data);
}

void c64_write_data(C64 *c64, uint16_t addr, uint8_t data[], size_t size)
{
    cpu_write_data(&c64->cpu, addr, data, size);
}

bool c64_trap(C64 *c64, uint16_t addr, handler_t handler)
{
    cpu_trap(&c64->cpu, addr, handler);
}

uint8_t c64_step(C64 *c64)
{
    cpu_step(&c64->cpu);

    vic_clock(&c64->vic /*, CYCLES_PER_FRAME*/);
    vic_clock(&c64->vic /*, CYCLES_PER_FRAME*/);

    cia_clock(&c64->cia1 /*, CYCLES_PER_FRAME*/);
    cia_clock(&c64->cia2 /*, CYCLES_PER_FRAME*/);

    // sid_clock(&sid, CYCLES_PER_FRAME);
}

/* ============================================================
   ROM Loading
   ============================================================ */

void c64_load_rom(C64 *c64, const char *path, uint16_t addr, size_t size)
{
    FILE *f = fopen(path, "rb");
    if (!f)
    {
        printf("Missing ROM: %s\n", path);
        exit(1);
    }
    fread(&c64->cpu.memory[addr], 1, size, f);
    fclose(f);
}

/* ============================================================
   Main
   ============================================================ */

// int main(int argc, char **argv)
// {
//     C64 c64;
//     c64_init(&c64, DEBUG);

//     /* SDL Init */
//     // SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);

//     // window = SDL_CreateWindow(
//     //     "C64 Emulator",
//     //     SDL_WINDOWPOS_CENTERED,
//     //     SDL_WINDOWPOS_CENTERED,
//     //     640, 400,
//     //     SDL_WINDOW_SHOWN);

//     // renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

//     // texture = SDL_CreateTexture(
//     //     renderer,
//     //     SDL_PIXELFORMAT_ARGB8888,
//     //     SDL_TEXTUREACCESS_STREAMING,
//     //     320, 200);

//     // /* Audio */
//     // SDL_AudioSpec want = {0};
//     // want.freq = SAMPLE_RATE;
//     // want.format = AUDIO_F32SYS;
//     // want.channels = 1;
//     // want.samples = 1024;
//     // want.callback = audio_callback;

//     // audio_dev = SDL_OpenAudioDevice(NULL, 0, &want, NULL, 0);
//     // SDL_PauseAudioDevice(audio_dev, 0);

//     /* Main loop */
//     int running = 1;
//     // SDL_Event e;

//     while (running)
//     {

//         // uint32_t start = SDL_GetTicks();

//         /* One PAL frame */
//         for (int i = 0; i < CYCLES_PER_FRAME; i++)
//         {
//             c64_step(&c64);
//         }

//         /* Video (placeholder background) */
//         // SDL_UpdateTexture(texture, NULL, framebuffer, 320 * sizeof(uint32_t));
//         // SDL_RenderClear(renderer);
//         // SDL_RenderCopy(renderer, texture, NULL, NULL);
//         // SDL_RenderPresent(renderer);

//         // while (SDL_PollEvent(&e))
//         // {
//         //     if (e.type == SDL_QUIT)
//         //         running = 0;
//         // }

//         // /* Frame sync */
//         // uint32_t elapsed = SDL_GetTicks() - start;
//         // if (elapsed < (1000 / FPS))
//         //     SDL_Delay((1000 / FPS) - elapsed);
//     }

//     // SDL_Quit();
//     return 0;
// }
