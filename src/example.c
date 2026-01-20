#include "../include/mos6510.h"
#include <stdio.h>
#include <stdlib.h>

#define DEBUG true

int main() {
    printf("=== MOS 6510 CPU Example ===\n\n");
    
    // Initialize CPU
    CPU cpu;
    cpu_init(&cpu, DEBUG);
    
    printf("Initial CPU state:\n");
    printf("  PC: $%04X\n", cpu_get_pc(&cpu));
    printf("  A: $%02X\n", cpu.A);
    printf("  X: $%02X\n", cpu.X);
    printf("  Y: $%02X\n", cpu.Y);
    printf("  SP: $%02X\n", cpu.SP);
    printf("  P: $%02X\n", cpu.P);
    printf("\n");
    
    // Load a simple program: LDA #$42, STA $2000, BRK
    cpu.memory[0x1000] = 0xA9;  // LDA #$42
    cpu.memory[0x1001] = 0x42;
    cpu.memory[0x1002] = 0x8D;  // STA $2000
    cpu.memory[0x1003] = 0x00;
    cpu.memory[0x1004] = 0x20;
    cpu.memory[0x1005] = 0x00;  // BRK
    
    cpu_set_pc(&cpu, 0x1000);
    
    printf("Program loaded at $1000:\n");
    printf("  A9 42    LDA #$42\n");
    printf("  8D 00 20 STA $2000\n");
    printf("  00       BRK\n\n");
    
    // Execute program step by step
    int step = 1;
    uint16_t pc;
    do {
        pc = cpu_get_pc(&cpu);
        uint8_t opcode = cpu.memory[pc];
        
        printf("Step %d: PC=$%04X, Opcode=$%02X", step, pc, opcode);
        
        // Find opcode name
        if (opcode <= 0xFF) {
            const char *name = "???";
            for (int i = 0; i < 256; i++) {
                if (opcode_table[i].opcode == opcode) {
                    name = opcode_table[i].name;
                    break;
                }
            }
            printf(" (%s)", name);
        }
        
        printf(", A=$%02X, X=$%02X, Y=$%02X, SP=$%02X, P=$%02X\n", 
               cpu.A, cpu.X, cpu.Y, cpu.SP, cpu.P);
        
        uint8_t cycles = cpu_step(&cpu);
        printf("  Cycles used: %d\n\n", cycles);
        
        step++;
        if (step > 20) {  // Safety limit
            printf("Too many steps, stopping...\n");
            break;
        }
    } while (pc != cpu_get_pc(&cpu) && cpu.memory[cpu_get_pc(&cpu)] != 0x00);
    
    printf("Final CPU state:\n");
    printf("  PC: $%04X\n", cpu_get_pc(&cpu));
    printf("  A: $%02X\n", cpu.A);
    printf("  X: $%02X\n", cpu.X);
    printf("  Y: $%02X\n", cpu.Y);
    printf("  SP: $%02X\n", cpu.SP);
    printf("  P: $%02X\n", cpu.P);
    printf("  Memory $2000: $%02X\n", cpu.memory[0x2000]);
    printf("\n");
    
    // Test flag operations
    printf("Testing flag operations:\n");
    printf("  Setting flags...\n");
    set_flag_carry(&cpu, true);
    set_flag_zero(&cpu, true);
    set_flag_negative(&cpu, true);
    printf("  Flags after setting: C=%d, Z=%d, N=%d\n",
           get_flag_carry(&cpu), get_flag_zero(&cpu), get_flag_negative(&cpu));
    
    printf("  Clearing flags...\n");
    set_flag_carry(&cpu, false);
    set_flag_zero(&cpu, false);
    set_flag_negative(&cpu, false);
    printf("  Flags after clearing: C=%d, Z=%d, N=%d\n",
           get_flag_carry(&cpu), get_flag_zero(&cpu), get_flag_negative(&cpu));
    printf("\n");
    
    // Test arithmetic operations
    printf("Testing arithmetic operations:\n");
    cpu.A = 0x50;
    printf("  A = $%02X\n", cpu.A);
    cpu.A = add_with_carry(&cpu, cpu.A, 0x30);
    printf("  A + $30 = $%02X (C=%d, Z=%d, N=%d, V=%d)\n", 
           cpu.A, get_flag_carry(&cpu), get_flag_zero(&cpu), 
           get_flag_negative(&cpu), get_flag_overflow(&cpu));
    
    set_flag_carry(&cpu, true);
    cpu.A = 0x80;
    printf("  A = $%02X (with C=1)\n", cpu.A);
    cpu.A = add_with_carry(&cpu, cpu.A, 0x01);
    printf("  A + $01 = $%02X (C=%d, Z=%d, N=%d, V=%d)\n", 
           cpu.A, get_flag_carry(&cpu), get_flag_zero(&cpu), 
           get_flag_negative(&cpu), get_flag_overflow(&cpu));
    
    printf("\nExample completed successfully!\n");
    
    return 0;
}