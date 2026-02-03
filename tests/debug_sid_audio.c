#include <stdio.h>
#include "c64.h"
#include "sid_file.h"

int main() {
    C64 c64;
    c64_init(&c64);
    
    sid_file_t sid;
    if (sid_file_load("roms/Ikari_Union.sid", &sid) != SID_OK) {
        printf("Failed to load SID\n");
        return 1;
    }
    
    c64_write_data(&c64, sid.real_load_address, sid.data, sid.data_length);
    
    int16_t buffer[2000];
    sid_set_audio_buffer(&c64.sid, buffer, 2000);
    
    // Bootstrap - call init
    c64_write_byte(&c64, 0x0340, 0xA9);  // LDA #song
    c64_write_byte(&c64, 0x0341, 0x00);
    c64_write_byte(&c64, 0x0342, 0x20);  // JSR init
    c64_write_byte(&c64, 0x0343, sid.init_address & 0xFF);
    c64_write_byte(&c64, 0x0344, (sid.init_address >> 8) & 0xFF);
    c64_write_byte(&c64, 0x0345, 0x60);  // RTS
    c64_set_pc(&c64, 0x0340);
    
    // Run init
    for (int i = 0; i < 100000 && c64_get_pc(&c64) != 0x0346; i++) {
        c64_step(&c64);
        c64.sid.buffer_pos = 0;
    }
    
    printf("After init:\n");
    printf("  Volume: %d\n", c64.sid.volume);
    printf("  Voice 1 ctrl: $%02X (gate=%d)\n", 
           c64.sid.voice[0].control,
           (c64.sid.voice[0].control & 1));
    
    // Play routine bootstrap
    c64_write_byte(&c64, 0x0350, 0x20);  // JSR play
    c64_write_byte(&c64, 0x0351, sid.play_address & 0xFF);
    c64_write_byte(&c64, 0x0352, (sid.play_address >> 8) & 0xFF);
    c64_write_byte(&c64, 0x0353, 0x60);  // RTS
    
    // Track volume changes
    int volume_changes = 0;
    uint8_t last_volume = c64.sid.volume;
    
    // Call play 50 times with proper frame simulation
    for (int frame = 0; frame < 50; frame++) {
        c64.sid.buffer_pos = 0;
        c64_set_pc(&c64, 0x0350);
        
        // Run ~20000 cycles (one PAL frame)
        for (int i = 0; i < 20000; i++) {
            c64_step(&c64);
            if (c64.sid.volume != last_volume) {
                printf("Frame %d: Volume changed from %d to %d\n", 
                       frame, last_volume, c64.sid.volume);
                last_volume = c64.sid.volume;
                volume_changes++;
            }
            if (c64_get_pc(&c64) == 0x0354) break;
        }
    }
    
    printf("\nAfter 50 frames:\n");
    printf("  Voice 1:\n");
    printf("    freq: $%04X\n", c64.sid.voice[0].frequency);
    printf("    ctrl: $%02X (gate=%d)\n", 
           c64.sid.voice[0].control,
           (c64.sid.voice[0].control & 1));
    printf("    accumulator: $%06X\n", c64.sid.voice[0].accumulator);
    printf("    shift_register: $%06X\n", c64.sid.voice[0].shift_register);
    printf("    env_level: %d\n", c64.sid.voice[0].env_level);
    printf("    env_state: %d (0=ATK,1=DEC,2=SUS,3=REL)\n", c64.sid.voice[0].env_state);
    printf("  Voice 2:\n");
    printf("    freq: $%04X, ctrl: $%02X\n", 
           c64.sid.voice[1].frequency, c64.sid.voice[1].control);
    printf("  Voice 3:\n");
    printf("    freq: $%04X, ctrl: $%02X\n",
           c64.sid.voice[2].frequency, c64.sid.voice[2].control);
    printf("  Volume: %d\n", c64.sid.volume);
    printf("  Filter voices: $%02X\n", c64.sid.filter.filter_voices);
    printf("  Filter mode: $%02X (LP=%d BP=%d HP=%d 3OFF=%d)\n", 
           c64.sid.filter.mode,
           (c64.sid.filter.mode & 0x01),
           (c64.sid.filter.mode & 0x02) >> 1,
           (c64.sid.filter.mode & 0x04) >> 2,
           (c64.sid.filter.mode & 0x08) >> 3);
    printf("  Samples in buffer: %d\n", c64.sid.buffer_pos);
    
    // Look for non-zero samples
    int nonzero = 0;
    int16_t max_sample = 0, min_sample = 0;
    for (uint32_t i = 0; i < c64.sid.buffer_pos; i++) {
        if (buffer[i] != 0) nonzero++;
        if (buffer[i] > max_sample) max_sample = buffer[i];
        if (buffer[i] < min_sample) min_sample = buffer[i];
    }
    printf("  Non-zero samples: %d\n", nonzero);
    printf("  Sample range: %d to %d\n", min_sample, max_sample);
    
    // Test: generate single sample manually
    int16_t sample = sid_output(&c64.sid);
    printf("\nManual sid_output() call: %d\n", sample);
    
    // Check oscillator output directly
    SID_Voice *v = &c64.sid.voice[0];
    uint16_t osc = sid_oscillator(v, &c64.sid.voice[2], 0);
    printf("Voice 1 oscillator output: %d (0x%03X)\n", osc, osc);
    printf("Voice 1 env applied: %d * %d = %d\n", 
           (int)osc - 2048, v->env_level, ((int)osc - 2048) * v->env_level);
    
    sid_file_free(&sid);
    return 0;
}
