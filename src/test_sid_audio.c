/*
 * Simple SID Audio Test
 * Generates a basic tone to verify the audio pipeline works correctly
 * without the complexity of SID file playback
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include "SDL.h"
#include "sid6581.h"

#define SAMPLE_RATE 44100
#define AUDIO_BUFFER_SIZE 2048
#define CPU_CLOCK 985248

/* Audio ring buffer */
#define RING_BUFFER_SIZE (SAMPLE_RATE * 2)
static int16_t audio_ring_buffer[RING_BUFFER_SIZE];
static volatile uint32_t ring_write_pos = 0;
static volatile uint32_t ring_read_pos = 0;

static volatile int running = 1;

static void signal_handler(int sig) {
    (void)sig;
    running = 0;
}

static void audio_callback(void *userdata, Uint8 *stream, int len) {
    (void)userdata;
    int16_t *out = (int16_t *)stream;
    int samples = len / sizeof(int16_t);

    for (int i = 0; i < samples; i++) {
        if (ring_read_pos != ring_write_pos) {
            out[i] = audio_ring_buffer[ring_read_pos];
            ring_read_pos = (ring_read_pos + 1) % RING_BUFFER_SIZE;
        } else {
            out[i] = 0;
        }
    }
}

static void audio_push_samples(int16_t *samples, uint32_t count) {
    for (uint32_t i = 0; i < count; i++) {
        uint32_t next_pos = (ring_write_pos + 1) % RING_BUFFER_SIZE;
        if (next_pos != ring_read_pos) {
            audio_ring_buffer[ring_write_pos] = samples[i];
            ring_write_pos = next_pos;
        }
    }
}

static uint32_t audio_buffered_samples(void) {
    if (ring_write_pos >= ring_read_pos) {
        return ring_write_pos - ring_read_pos;
    } else {
        return RING_BUFFER_SIZE - ring_read_pos + ring_write_pos;
    }
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    
    printf("Simple SID Audio Test\n");
    printf("=====================\n\n");

    /* Initialize SDL */
    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
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
    if (audio_dev == 0) {
        fprintf(stderr, "SDL_OpenAudioDevice failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    printf("Audio initialized: %d Hz, %d samples buffer\n\n", have.freq, have.samples);

    /* Initialize SID */
    SID sid;
    sid_init(&sid, CPU_CLOCK, SAMPLE_RATE);

    /* Set up audio buffer */
    int16_t sid_buffer[SAMPLE_RATE / 50 + 100];
    sid_set_audio_buffer(&sid, sid_buffer, sizeof(sid_buffer) / sizeof(sid_buffer[0]));

    /* Configure SID for a simple tone */
    printf("Setting up Voice 1: Triangle wave at ~440Hz\n");
    
    /* Voice 1: Triangle wave at ~440Hz */
    /* Frequency = 440Hz, Formula: Fn = Fout * 16777216 / Fclk */
    /* Fn = 440 * 16777216 / 985248 = 7494 = $1D46 */
    sid_write(&sid, 0x00, 0x46);  /* Freq lo */
    sid_write(&sid, 0x01, 0x1D);  /* Freq hi */
    sid_write(&sid, 0x05, 0x00);  /* Attack=0, Decay=0 */
    sid_write(&sid, 0x06, 0xF0);  /* Sustain=15, Release=0 */
    sid_write(&sid, 0x04, 0x11);  /* Triangle + Gate ON */
    
    printf("Setting up Voice 2: Sawtooth wave at ~554Hz (C#5)\n");
    
    /* Voice 2: Sawtooth at ~554Hz */
    /* Fn = 554 * 16777216 / 985248 = 9436 = $24DC */
    sid_write(&sid, 0x07, 0xDC);  /* Freq lo */
    sid_write(&sid, 0x08, 0x24);  /* Freq hi */
    sid_write(&sid, 0x0C, 0x09);  /* Attack=0, Decay=9 */
    sid_write(&sid, 0x0D, 0x80);  /* Sustain=8, Release=0 */
    sid_write(&sid, 0x0B, 0x21);  /* Sawtooth + Gate ON */
    
    printf("Setting up Voice 3: Pulse wave at ~659Hz (E5)\n");
    
    /* Voice 3: Pulse at ~659Hz */
    /* Fn = 659 * 16777216 / 985248 = 11222 = $2BD6 */
    sid_write(&sid, 0x0E, 0xD6);  /* Freq lo */
    sid_write(&sid, 0x0F, 0x2B);  /* Freq hi */
    sid_write(&sid, 0x10, 0x00);  /* Pulse width lo = 50% */
    sid_write(&sid, 0x11, 0x08);  /* Pulse width hi */
    sid_write(&sid, 0x13, 0x0A);  /* Attack=0, Decay=10 */
    sid_write(&sid, 0x14, 0x50);  /* Sustain=5, Release=0 */
    sid_write(&sid, 0x12, 0x41);  /* Pulse + Gate ON */
    
    /* Master volume = 15 (max) */
    sid_write(&sid, 0x18, 0x0F);
    
    printf("\nMaster volume: 15\n");
    printf("Press Ctrl+C to stop...\n\n");

    signal(SIGINT, signal_handler);
    SDL_PauseAudioDevice(audio_dev, 0);

    uint32_t frames = 0;
    Uint32 start_time = SDL_GetTicks();
    uint32_t cycles_per_frame = CPU_CLOCK / 50;  /* PAL: 50 Hz */

    while (running) {
        /* Reset buffer for this frame */
        sid.buffer_pos = 0;

        /* Clock SID for one frame */
        sid_clock(&sid, cycles_per_frame);

        /* Push samples to audio */
        uint32_t samples = sid_get_samples(&sid);
        if (samples > 0) {
            audio_push_samples(sid_buffer, samples);
        }

        frames++;

        /* Change notes every 50 frames (1 second) */
        if (frames % 50 == 0) {
            /* Cycle through different waveforms on voice 1 */
            static int waveform = 0;
            waveform = (waveform + 1) % 4;
            uint8_t ctrl = 0x01;  /* Gate on */
            switch (waveform) {
                case 0: ctrl |= 0x10; printf("Voice 1: Triangle\n"); break;
                case 1: ctrl |= 0x20; printf("Voice 1: Sawtooth\n"); break;
                case 2: ctrl |= 0x40; printf("Voice 1: Pulse\n"); break;
                case 3: ctrl |= 0x80; printf("Voice 1: Noise\n"); break;
            }
            sid_write(&sid, 0x04, ctrl);
        }

        /* Sync to real time */
        while (audio_buffered_samples() > SAMPLE_RATE / 4 && running) {
            SDL_Delay(5);
        }

        /* Handle SDL events */
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = 0;
            }
        }

        /* Print status every second */
        if (frames % 50 == 0) {
            Uint32 elapsed = SDL_GetTicks() - start_time;
            printf("Time: %02u:%02u  Buffer: %5u samples  Env1: %3d  Env2: %3d  Env3: %3d\n",
                   (elapsed / 1000) / 60, (elapsed / 1000) % 60,
                   audio_buffered_samples(),
                   sid.voice[0].env_level,
                   sid.voice[1].env_level,
                   sid.voice[2].env_level);
        }
    }

    printf("\nStopped after %u frames\n", frames);

    SDL_CloseAudioDevice(audio_dev);
    SDL_Quit();

    return 0;
}
