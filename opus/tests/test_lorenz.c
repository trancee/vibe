/**
 * test_lorenz.c - Wolfgang Lorenz's C64 Test Suite
 * 
 * Runs a selection of the Lorenz C64 test suite to validate
 * CPU instruction accuracy. The test files are in tests/lorenz/
 * 
 * The Lorenz suite uses KERNAL traps for I/O:
 *   $FFD2 (CHROUT) - Print character
 *   $E16F (LOAD)   - Load next test
 *   $8000 (WARM)   - Test passed, continue to next
 *   $A474 (READY)  - All tests completed
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "test_framework.h"
#include "../src/c64.h"

// Memory for Lorenz tests (64KB)
static u8 lorenz_memory[65536];

// Output buffer to capture test output
static char lorenz_output[4096];
static size_t lorenz_output_pos = 0;

// Test state
static bool lorenz_test_passed = false;
static bool lorenz_test_failed = false;
static const char *lorenz_next_test = NULL;
static const char *lorenz_current_test = NULL;

// PETSCII to ASCII conversion
static char petscii_to_ascii(u8 c) {
    if (c >= 0xC1 && c <= 0xDA) return c - 0xC1 + 'A';
    if (c >= 0x41 && c <= 0x5A) return c - 0x41 + 'a';
    if (c < 32 || c >= 127) return '.';
    return c;
}

// Initialize memory with ROM-like vectors and stubs
static void lorenz_init_memory(void) {
    memset(lorenz_memory, 0, sizeof(lorenz_memory));
    
    // Set up CPU port
    lorenz_memory[0x0000] = 0x2F;  // Data direction
    lorenz_memory[0x0001] = 0x37;  // Default banking
    
    // Set up important vectors
    lorenz_memory[0xFFFA] = 0x00; lorenz_memory[0xFFFB] = 0x80;  // NMI -> $8000
    lorenz_memory[0xFFFC] = 0x00; lorenz_memory[0xFFFD] = 0x08;  // RESET -> $0800
    lorenz_memory[0xFFFE] = 0x48; lorenz_memory[0xFFFF] = 0xFF;  // IRQ -> $FF48
    
    // KERNAL stubs - these are trap addresses we'll intercept
    lorenz_memory[0xFFD2] = 0x60;  // CHROUT - RTS (trapped)
    lorenz_memory[0xFFE4] = 0xA9;  // GETIN - LDA #3; RTS
    lorenz_memory[0xFFE5] = 0x03;
    lorenz_memory[0xFFE6] = 0x60;
    
    // Test harness addresses
    lorenz_memory[0x8000] = 0x60;  // WARM/CARTROM - RTS (trapped)
    lorenz_memory[0xA474] = 0x60;  // READY - RTS (trapped)
    lorenz_memory[0xE16F] = 0xEA;  // LOAD - NOP (trapped)
    
    // IRQ handler at $FF48 (standard location)
    static const u8 irq_handler[] = {
        0x48,             // PHA
        0x8A,             // TXA
        0x48,             // PHA
        0x98,             // TYA
        0x48,             // PHA
        0xBA,             // TSX
        0xBD, 0x04, 0x01, // LDA $0104,X
        0x29, 0x10,       // AND #$10
        0xF0, 0x03,       // BEQ +3
        0x6C, 0x16, 0x03, // JMP ($0316) - BRK vector
        0x6C, 0x14, 0x03, // JMP ($0314) - IRQ vector
    };
    memcpy(&lorenz_memory[0xFF48], irq_handler, sizeof(irq_handler));
    
    // Default IRQ/BRK vectors
    lorenz_memory[0x0314] = 0x31; lorenz_memory[0x0315] = 0xEA;  // IRQ -> $EA31
    lorenz_memory[0x0316] = 0x66; lorenz_memory[0x0317] = 0xFE;  // BRK -> $FE66
    
    // RTI at common return points
    lorenz_memory[0xEA31] = 0x40;  // RTI
    lorenz_memory[0xFE66] = 0x40;  // RTI
    
    // WARM vector ($0302-$0303) points to $8000
    lorenz_memory[0x0302] = 0x00;
    lorenz_memory[0x0303] = 0x80;
}

// Load a Lorenz test file
static bool lorenz_load_test(const char *testcase) {
    char path[256];
    snprintf(path, sizeof(path), "tests/lorenz/%s", testcase);
    
    FILE *f = fopen(path, "rb");
    if (!f) {
        return false;
    }
    
    // Get file size
    fseek(f, 0, SEEK_END);
    long filesize = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (filesize < 3) {
        fclose(f);
        return false;
    }
    
    // Read 2-byte load address (little endian)
    u16 load_addr = fgetc(f) | (fgetc(f) << 8);
    long datasize = filesize - 2;
    
    // Read program data
    size_t read = fread(&lorenz_memory[load_addr], 1, datasize, f);
    fclose(f);
    
    if (read != (size_t)datasize) {
        return false;
    }
    
    return true;
}

// Execute one CPU instruction using Lorenz memory
static int lorenz_cpu_step(C64Cpu *cpu) {
    u8 opcode = lorenz_memory[cpu->PC];
    cpu->PC++;
    
    u8 op1 = lorenz_memory[cpu->PC];
    u8 op2 = lorenz_memory[cpu->PC + 1];
    
    int cycles = 2;
    u16 addr = 0;
    u8 value = 0;
    
    #define READ(a) lorenz_memory[a]
    #define WRITE(a, v) lorenz_memory[a] = (v)
    #define PUSH(v) do { lorenz_memory[0x100 + cpu->SP] = (v); cpu->SP--; } while(0)
    #define POP() lorenz_memory[0x100 + (++cpu->SP)]
    #define SET_NZ(v) do { \
        cpu_set_flag(cpu, FLAG_Z, (v) == 0); \
        cpu_set_flag(cpu, FLAG_N, (v) & 0x80); \
    } while(0)
    #define BRANCH(cond) do { \
        if (cond) { \
            i8 offset = (i8)op1; \
            u16 new_pc = cpu->PC + 1 + offset; \
            cycles++; \
            if ((cpu->PC & 0xFF00) != (new_pc & 0xFF00)) cycles++; \
            cpu->PC = new_pc; \
        } else { \
            cpu->PC++; \
        } \
    } while(0)
    
    #define ADDR_ZP() (op1)
    #define ADDR_ZPX() ((op1 + cpu->X) & 0xFF)
    #define ADDR_ZPY() ((op1 + cpu->Y) & 0xFF)
    #define ADDR_ABS() (op1 | (op2 << 8))
    #define ADDR_ABSX() ((op1 | (op2 << 8)) + cpu->X)
    #define ADDR_ABSY() ((op1 | (op2 << 8)) + cpu->Y)
    #define ADDR_INDX() (READ((op1 + cpu->X) & 0xFF) | (READ((op1 + cpu->X + 1) & 0xFF) << 8))
    #define ADDR_INDY() ((READ(op1) | (READ((op1 + 1) & 0xFF) << 8)) + cpu->Y)
    
    switch (opcode) {
        // LDA
        case 0xA9: cpu->A = op1; SET_NZ(cpu->A); cpu->PC++; cycles = 2; break;
        case 0xA5: cpu->A = READ(ADDR_ZP()); SET_NZ(cpu->A); cpu->PC++; cycles = 3; break;
        case 0xB5: cpu->A = READ(ADDR_ZPX()); SET_NZ(cpu->A); cpu->PC++; cycles = 4; break;
        case 0xAD: cpu->A = READ(ADDR_ABS()); SET_NZ(cpu->A); cpu->PC += 2; cycles = 4; break;
        case 0xBD: addr = ADDR_ABSX(); cpu->A = READ(addr); SET_NZ(cpu->A); cpu->PC += 2; 
                   cycles = 4 + ((((op1 | (op2 << 8)) & 0xFF00) != (addr & 0xFF00)) ? 1 : 0); break;
        case 0xB9: addr = ADDR_ABSY(); cpu->A = READ(addr); SET_NZ(cpu->A); cpu->PC += 2;
                   cycles = 4 + ((((op1 | (op2 << 8)) & 0xFF00) != (addr & 0xFF00)) ? 1 : 0); break;
        case 0xA1: cpu->A = READ(ADDR_INDX()); SET_NZ(cpu->A); cpu->PC++; cycles = 6; break;
        case 0xB1: addr = ADDR_INDY(); cpu->A = READ(addr); SET_NZ(cpu->A); cpu->PC++;
                   cycles = 5 + (((READ(op1) | (READ((op1+1)&0xFF)<<8)) & 0xFF00) != (addr & 0xFF00) ? 1 : 0); break;
        
        // LDX
        case 0xA2: cpu->X = op1; SET_NZ(cpu->X); cpu->PC++; cycles = 2; break;
        case 0xA6: cpu->X = READ(ADDR_ZP()); SET_NZ(cpu->X); cpu->PC++; cycles = 3; break;
        case 0xB6: cpu->X = READ(ADDR_ZPY()); SET_NZ(cpu->X); cpu->PC++; cycles = 4; break;
        case 0xAE: cpu->X = READ(ADDR_ABS()); SET_NZ(cpu->X); cpu->PC += 2; cycles = 4; break;
        case 0xBE: addr = ADDR_ABSY(); cpu->X = READ(addr); SET_NZ(cpu->X); cpu->PC += 2;
                   cycles = 4 + ((((op1 | (op2 << 8)) & 0xFF00) != (addr & 0xFF00)) ? 1 : 0); break;
        
        // LDY
        case 0xA0: cpu->Y = op1; SET_NZ(cpu->Y); cpu->PC++; cycles = 2; break;
        case 0xA4: cpu->Y = READ(ADDR_ZP()); SET_NZ(cpu->Y); cpu->PC++; cycles = 3; break;
        case 0xB4: cpu->Y = READ(ADDR_ZPX()); SET_NZ(cpu->Y); cpu->PC++; cycles = 4; break;
        case 0xAC: cpu->Y = READ(ADDR_ABS()); SET_NZ(cpu->Y); cpu->PC += 2; cycles = 4; break;
        case 0xBC: addr = ADDR_ABSX(); cpu->Y = READ(addr); SET_NZ(cpu->Y); cpu->PC += 2;
                   cycles = 4 + ((((op1 | (op2 << 8)) & 0xFF00) != (addr & 0xFF00)) ? 1 : 0); break;
        
        // STA
        case 0x85: WRITE(ADDR_ZP(), cpu->A); cpu->PC++; cycles = 3; break;
        case 0x95: WRITE(ADDR_ZPX(), cpu->A); cpu->PC++; cycles = 4; break;
        case 0x8D: WRITE(ADDR_ABS(), cpu->A); cpu->PC += 2; cycles = 4; break;
        case 0x9D: WRITE(ADDR_ABSX(), cpu->A); cpu->PC += 2; cycles = 5; break;
        case 0x99: WRITE(ADDR_ABSY(), cpu->A); cpu->PC += 2; cycles = 5; break;
        case 0x81: WRITE(ADDR_INDX(), cpu->A); cpu->PC++; cycles = 6; break;
        case 0x91: WRITE(ADDR_INDY(), cpu->A); cpu->PC++; cycles = 6; break;
        
        // STX
        case 0x86: WRITE(ADDR_ZP(), cpu->X); cpu->PC++; cycles = 3; break;
        case 0x96: WRITE(ADDR_ZPY(), cpu->X); cpu->PC++; cycles = 4; break;
        case 0x8E: WRITE(ADDR_ABS(), cpu->X); cpu->PC += 2; cycles = 4; break;
        
        // STY
        case 0x84: WRITE(ADDR_ZP(), cpu->Y); cpu->PC++; cycles = 3; break;
        case 0x94: WRITE(ADDR_ZPX(), cpu->Y); cpu->PC++; cycles = 4; break;
        case 0x8C: WRITE(ADDR_ABS(), cpu->Y); cpu->PC += 2; cycles = 4; break;
        
        // Transfers
        case 0xAA: cpu->X = cpu->A; SET_NZ(cpu->X); cycles = 2; break;
        case 0xA8: cpu->Y = cpu->A; SET_NZ(cpu->Y); cycles = 2; break;
        case 0x8A: cpu->A = cpu->X; SET_NZ(cpu->A); cycles = 2; break;
        case 0x98: cpu->A = cpu->Y; SET_NZ(cpu->A); cycles = 2; break;
        case 0xBA: cpu->X = cpu->SP; SET_NZ(cpu->X); cycles = 2; break;
        case 0x9A: cpu->SP = cpu->X; cycles = 2; break;
        
        // Stack
        case 0x48: PUSH(cpu->A); cycles = 3; break;
        case 0x68: cpu->A = POP(); SET_NZ(cpu->A); cycles = 4; break;
        case 0x08: PUSH(cpu->P | FLAG_B | FLAG_U); cycles = 3; break;
        case 0x28: cpu->P = (POP() & ~FLAG_B) | FLAG_U; cycles = 4; break;
        
        // ADC
        case 0x69: value = op1; cpu->PC++; goto do_adc;
        case 0x65: value = READ(ADDR_ZP()); cpu->PC++; cycles = 3; goto do_adc;
        case 0x75: value = READ(ADDR_ZPX()); cpu->PC++; cycles = 4; goto do_adc;
        case 0x6D: value = READ(ADDR_ABS()); cpu->PC += 2; cycles = 4; goto do_adc;
        case 0x7D: addr = ADDR_ABSX(); value = READ(addr); cpu->PC += 2;
                   cycles = 4 + ((((op1|(op2<<8))&0xFF00) != (addr&0xFF00)) ? 1 : 0); goto do_adc;
        case 0x79: addr = ADDR_ABSY(); value = READ(addr); cpu->PC += 2;
                   cycles = 4 + ((((op1|(op2<<8))&0xFF00) != (addr&0xFF00)) ? 1 : 0); goto do_adc;
        case 0x61: value = READ(ADDR_INDX()); cpu->PC++; cycles = 6; goto do_adc;
        case 0x71: addr = ADDR_INDY(); value = READ(addr); cpu->PC++;
                   cycles = 5 + (((READ(op1)|(READ((op1+1)&0xFF)<<8))&0xFF00) != (addr&0xFF00) ? 1 : 0);
        do_adc: {
            if (cpu->P & FLAG_D) {
                u8 al = (cpu->A & 0x0F) + (value & 0x0F) + (cpu->P & FLAG_C ? 1 : 0);
                if (al > 9) al += 6;
                u8 ah = (cpu->A >> 4) + (value >> 4) + (al > 0x0F ? 1 : 0);
                cpu_set_flag(cpu, FLAG_Z, ((cpu->A + value + (cpu->P & FLAG_C ? 1 : 0)) & 0xFF) == 0);
                cpu_set_flag(cpu, FLAG_N, ah & 0x08);
                cpu_set_flag(cpu, FLAG_V, (~(cpu->A ^ value) & (cpu->A ^ (ah << 4)) & 0x80) != 0);
                if (ah > 9) ah += 6;
                cpu_set_flag(cpu, FLAG_C, ah > 0x0F);
                cpu->A = ((ah << 4) | (al & 0x0F)) & 0xFF;
            } else {
                u16 sum = cpu->A + value + (cpu->P & FLAG_C ? 1 : 0);
                cpu_set_flag(cpu, FLAG_V, (~(cpu->A ^ value) & (cpu->A ^ sum) & 0x80) != 0);
                cpu_set_flag(cpu, FLAG_C, sum > 0xFF);
                cpu->A = sum & 0xFF;
                SET_NZ(cpu->A);
            }
            break;
        }
        
        // SBC
        case 0xE9: value = op1; cpu->PC++; goto do_sbc;
        case 0xE5: value = READ(ADDR_ZP()); cpu->PC++; cycles = 3; goto do_sbc;
        case 0xF5: value = READ(ADDR_ZPX()); cpu->PC++; cycles = 4; goto do_sbc;
        case 0xED: value = READ(ADDR_ABS()); cpu->PC += 2; cycles = 4; goto do_sbc;
        case 0xFD: addr = ADDR_ABSX(); value = READ(addr); cpu->PC += 2;
                   cycles = 4 + ((((op1|(op2<<8))&0xFF00) != (addr&0xFF00)) ? 1 : 0); goto do_sbc;
        case 0xF9: addr = ADDR_ABSY(); value = READ(addr); cpu->PC += 2;
                   cycles = 4 + ((((op1|(op2<<8))&0xFF00) != (addr&0xFF00)) ? 1 : 0); goto do_sbc;
        case 0xE1: value = READ(ADDR_INDX()); cpu->PC++; cycles = 6; goto do_sbc;
        case 0xF1: addr = ADDR_INDY(); value = READ(addr); cpu->PC++;
                   cycles = 5 + (((READ(op1)|(READ((op1+1)&0xFF)<<8))&0xFF00) != (addr&0xFF00) ? 1 : 0);
        do_sbc: {
            if (cpu->P & FLAG_D) {
                u8 borrow = (cpu->P & FLAG_C) ? 0 : 1;
                i16 al = (cpu->A & 0x0F) - (value & 0x0F) - borrow;
                i16 ah = (cpu->A >> 4) - (value >> 4);
                if (al < 0) { al -= 6; ah--; }
                if (ah < 0) ah -= 6;
                u16 diff = cpu->A - value - borrow;
                cpu_set_flag(cpu, FLAG_C, diff < 0x100);
                cpu_set_flag(cpu, FLAG_V, ((cpu->A ^ value) & (cpu->A ^ diff) & 0x80) != 0);
                cpu_set_flag(cpu, FLAG_Z, (diff & 0xFF) == 0);
                cpu_set_flag(cpu, FLAG_N, diff & 0x80);
                cpu->A = ((ah << 4) | (al & 0x0F)) & 0xFF;
            } else {
                u16 diff = cpu->A - value - ((cpu->P & FLAG_C) ? 0 : 1);
                cpu_set_flag(cpu, FLAG_V, ((cpu->A ^ value) & (cpu->A ^ diff) & 0x80) != 0);
                cpu_set_flag(cpu, FLAG_C, diff < 0x100);
                cpu->A = diff & 0xFF;
                SET_NZ(cpu->A);
            }
            break;
        }
        
        // AND
        case 0x29: cpu->A &= op1; SET_NZ(cpu->A); cpu->PC++; cycles = 2; break;
        case 0x25: cpu->A &= READ(ADDR_ZP()); SET_NZ(cpu->A); cpu->PC++; cycles = 3; break;
        case 0x35: cpu->A &= READ(ADDR_ZPX()); SET_NZ(cpu->A); cpu->PC++; cycles = 4; break;
        case 0x2D: cpu->A &= READ(ADDR_ABS()); SET_NZ(cpu->A); cpu->PC += 2; cycles = 4; break;
        case 0x3D: addr = ADDR_ABSX(); cpu->A &= READ(addr); SET_NZ(cpu->A); cpu->PC += 2;
                   cycles = 4 + ((((op1|(op2<<8))&0xFF00) != (addr&0xFF00)) ? 1 : 0); break;
        case 0x39: addr = ADDR_ABSY(); cpu->A &= READ(addr); SET_NZ(cpu->A); cpu->PC += 2;
                   cycles = 4 + ((((op1|(op2<<8))&0xFF00) != (addr&0xFF00)) ? 1 : 0); break;
        case 0x21: cpu->A &= READ(ADDR_INDX()); SET_NZ(cpu->A); cpu->PC++; cycles = 6; break;
        case 0x31: addr = ADDR_INDY(); cpu->A &= READ(addr); SET_NZ(cpu->A); cpu->PC++;
                   cycles = 5 + (((READ(op1)|(READ((op1+1)&0xFF)<<8))&0xFF00) != (addr&0xFF00) ? 1 : 0); break;
        
        // ORA
        case 0x09: cpu->A |= op1; SET_NZ(cpu->A); cpu->PC++; cycles = 2; break;
        case 0x05: cpu->A |= READ(ADDR_ZP()); SET_NZ(cpu->A); cpu->PC++; cycles = 3; break;
        case 0x15: cpu->A |= READ(ADDR_ZPX()); SET_NZ(cpu->A); cpu->PC++; cycles = 4; break;
        case 0x0D: cpu->A |= READ(ADDR_ABS()); SET_NZ(cpu->A); cpu->PC += 2; cycles = 4; break;
        case 0x1D: addr = ADDR_ABSX(); cpu->A |= READ(addr); SET_NZ(cpu->A); cpu->PC += 2;
                   cycles = 4 + ((((op1|(op2<<8))&0xFF00) != (addr&0xFF00)) ? 1 : 0); break;
        case 0x19: addr = ADDR_ABSY(); cpu->A |= READ(addr); SET_NZ(cpu->A); cpu->PC += 2;
                   cycles = 4 + ((((op1|(op2<<8))&0xFF00) != (addr&0xFF00)) ? 1 : 0); break;
        case 0x01: cpu->A |= READ(ADDR_INDX()); SET_NZ(cpu->A); cpu->PC++; cycles = 6; break;
        case 0x11: addr = ADDR_INDY(); cpu->A |= READ(addr); SET_NZ(cpu->A); cpu->PC++;
                   cycles = 5 + (((READ(op1)|(READ((op1+1)&0xFF)<<8))&0xFF00) != (addr&0xFF00) ? 1 : 0); break;
        
        // EOR
        case 0x49: cpu->A ^= op1; SET_NZ(cpu->A); cpu->PC++; cycles = 2; break;
        case 0x45: cpu->A ^= READ(ADDR_ZP()); SET_NZ(cpu->A); cpu->PC++; cycles = 3; break;
        case 0x55: cpu->A ^= READ(ADDR_ZPX()); SET_NZ(cpu->A); cpu->PC++; cycles = 4; break;
        case 0x4D: cpu->A ^= READ(ADDR_ABS()); SET_NZ(cpu->A); cpu->PC += 2; cycles = 4; break;
        case 0x5D: addr = ADDR_ABSX(); cpu->A ^= READ(addr); SET_NZ(cpu->A); cpu->PC += 2;
                   cycles = 4 + ((((op1|(op2<<8))&0xFF00) != (addr&0xFF00)) ? 1 : 0); break;
        case 0x59: addr = ADDR_ABSY(); cpu->A ^= READ(addr); SET_NZ(cpu->A); cpu->PC += 2;
                   cycles = 4 + ((((op1|(op2<<8))&0xFF00) != (addr&0xFF00)) ? 1 : 0); break;
        case 0x41: cpu->A ^= READ(ADDR_INDX()); SET_NZ(cpu->A); cpu->PC++; cycles = 6; break;
        case 0x51: addr = ADDR_INDY(); cpu->A ^= READ(addr); SET_NZ(cpu->A); cpu->PC++;
                   cycles = 5 + (((READ(op1)|(READ((op1+1)&0xFF)<<8))&0xFF00) != (addr&0xFF00) ? 1 : 0); break;
        
        // BIT
        case 0x24: value = READ(ADDR_ZP()); cpu->PC++; cycles = 3; goto do_bit;
        case 0x2C: value = READ(ADDR_ABS()); cpu->PC += 2; cycles = 4;
        do_bit:
            cpu_set_flag(cpu, FLAG_Z, (cpu->A & value) == 0);
            cpu_set_flag(cpu, FLAG_V, value & 0x40);
            cpu_set_flag(cpu, FLAG_N, value & 0x80);
            break;
        
        // CMP
        case 0xC9: value = op1; cpu->PC++; cycles = 2; goto do_cmp;
        case 0xC5: value = READ(ADDR_ZP()); cpu->PC++; cycles = 3; goto do_cmp;
        case 0xD5: value = READ(ADDR_ZPX()); cpu->PC++; cycles = 4; goto do_cmp;
        case 0xCD: value = READ(ADDR_ABS()); cpu->PC += 2; cycles = 4; goto do_cmp;
        case 0xDD: addr = ADDR_ABSX(); value = READ(addr); cpu->PC += 2;
                   cycles = 4 + ((((op1|(op2<<8))&0xFF00) != (addr&0xFF00)) ? 1 : 0); goto do_cmp;
        case 0xD9: addr = ADDR_ABSY(); value = READ(addr); cpu->PC += 2;
                   cycles = 4 + ((((op1|(op2<<8))&0xFF00) != (addr&0xFF00)) ? 1 : 0); goto do_cmp;
        case 0xC1: value = READ(ADDR_INDX()); cpu->PC++; cycles = 6; goto do_cmp;
        case 0xD1: addr = ADDR_INDY(); value = READ(addr); cpu->PC++;
                   cycles = 5 + (((READ(op1)|(READ((op1+1)&0xFF)<<8))&0xFF00) != (addr&0xFF00) ? 1 : 0);
        do_cmp: {
            u16 result = cpu->A - value;
            cpu_set_flag(cpu, FLAG_C, cpu->A >= value);
            cpu_set_flag(cpu, FLAG_Z, (result & 0xFF) == 0);
            cpu_set_flag(cpu, FLAG_N, result & 0x80);
            break;
        }
        
        // CPX
        case 0xE0: value = op1; cpu->PC++; cycles = 2; goto do_cpx;
        case 0xE4: value = READ(ADDR_ZP()); cpu->PC++; cycles = 3; goto do_cpx;
        case 0xEC: value = READ(ADDR_ABS()); cpu->PC += 2; cycles = 4;
        do_cpx: {
            u16 result = cpu->X - value;
            cpu_set_flag(cpu, FLAG_C, cpu->X >= value);
            cpu_set_flag(cpu, FLAG_Z, (result & 0xFF) == 0);
            cpu_set_flag(cpu, FLAG_N, result & 0x80);
            break;
        }
        
        // CPY
        case 0xC0: value = op1; cpu->PC++; cycles = 2; goto do_cpy;
        case 0xC4: value = READ(ADDR_ZP()); cpu->PC++; cycles = 3; goto do_cpy;
        case 0xCC: value = READ(ADDR_ABS()); cpu->PC += 2; cycles = 4;
        do_cpy: {
            u16 result = cpu->Y - value;
            cpu_set_flag(cpu, FLAG_C, cpu->Y >= value);
            cpu_set_flag(cpu, FLAG_Z, (result & 0xFF) == 0);
            cpu_set_flag(cpu, FLAG_N, result & 0x80);
            break;
        }
        
        // INC
        case 0xE6: addr = ADDR_ZP(); value = READ(addr) + 1; WRITE(addr, value); SET_NZ(value); cpu->PC++; cycles = 5; break;
        case 0xF6: addr = ADDR_ZPX(); value = READ(addr) + 1; WRITE(addr, value); SET_NZ(value); cpu->PC++; cycles = 6; break;
        case 0xEE: addr = ADDR_ABS(); value = READ(addr) + 1; WRITE(addr, value); SET_NZ(value); cpu->PC += 2; cycles = 6; break;
        case 0xFE: addr = ADDR_ABSX(); value = READ(addr) + 1; WRITE(addr, value); SET_NZ(value); cpu->PC += 2; cycles = 7; break;
        
        // DEC
        case 0xC6: addr = ADDR_ZP(); value = READ(addr) - 1; WRITE(addr, value); SET_NZ(value); cpu->PC++; cycles = 5; break;
        case 0xD6: addr = ADDR_ZPX(); value = READ(addr) - 1; WRITE(addr, value); SET_NZ(value); cpu->PC++; cycles = 6; break;
        case 0xCE: addr = ADDR_ABS(); value = READ(addr) - 1; WRITE(addr, value); SET_NZ(value); cpu->PC += 2; cycles = 6; break;
        case 0xDE: addr = ADDR_ABSX(); value = READ(addr) - 1; WRITE(addr, value); SET_NZ(value); cpu->PC += 2; cycles = 7; break;
        
        // INX, INY, DEX, DEY
        case 0xE8: cpu->X++; SET_NZ(cpu->X); cycles = 2; break;
        case 0xC8: cpu->Y++; SET_NZ(cpu->Y); cycles = 2; break;
        case 0xCA: cpu->X--; SET_NZ(cpu->X); cycles = 2; break;
        case 0x88: cpu->Y--; SET_NZ(cpu->Y); cycles = 2; break;
        
        // ASL
        case 0x0A: cpu_set_flag(cpu, FLAG_C, cpu->A & 0x80); cpu->A <<= 1; SET_NZ(cpu->A); cycles = 2; break;
        case 0x06: addr = ADDR_ZP(); value = READ(addr); cpu_set_flag(cpu, FLAG_C, value & 0x80);
                   value <<= 1; WRITE(addr, value); SET_NZ(value); cpu->PC++; cycles = 5; break;
        case 0x16: addr = ADDR_ZPX(); value = READ(addr); cpu_set_flag(cpu, FLAG_C, value & 0x80);
                   value <<= 1; WRITE(addr, value); SET_NZ(value); cpu->PC++; cycles = 6; break;
        case 0x0E: addr = ADDR_ABS(); value = READ(addr); cpu_set_flag(cpu, FLAG_C, value & 0x80);
                   value <<= 1; WRITE(addr, value); SET_NZ(value); cpu->PC += 2; cycles = 6; break;
        case 0x1E: addr = ADDR_ABSX(); value = READ(addr); cpu_set_flag(cpu, FLAG_C, value & 0x80);
                   value <<= 1; WRITE(addr, value); SET_NZ(value); cpu->PC += 2; cycles = 7; break;
        
        // LSR
        case 0x4A: cpu_set_flag(cpu, FLAG_C, cpu->A & 0x01); cpu->A >>= 1; SET_NZ(cpu->A); cycles = 2; break;
        case 0x46: addr = ADDR_ZP(); value = READ(addr); cpu_set_flag(cpu, FLAG_C, value & 0x01);
                   value >>= 1; WRITE(addr, value); SET_NZ(value); cpu->PC++; cycles = 5; break;
        case 0x56: addr = ADDR_ZPX(); value = READ(addr); cpu_set_flag(cpu, FLAG_C, value & 0x01);
                   value >>= 1; WRITE(addr, value); SET_NZ(value); cpu->PC++; cycles = 6; break;
        case 0x4E: addr = ADDR_ABS(); value = READ(addr); cpu_set_flag(cpu, FLAG_C, value & 0x01);
                   value >>= 1; WRITE(addr, value); SET_NZ(value); cpu->PC += 2; cycles = 6; break;
        case 0x5E: addr = ADDR_ABSX(); value = READ(addr); cpu_set_flag(cpu, FLAG_C, value & 0x01);
                   value >>= 1; WRITE(addr, value); SET_NZ(value); cpu->PC += 2; cycles = 7; break;
        
        // ROL
        case 0x2A: { u8 c = cpu->P & FLAG_C ? 1 : 0; cpu_set_flag(cpu, FLAG_C, cpu->A & 0x80);
                   cpu->A = (cpu->A << 1) | c; SET_NZ(cpu->A); cycles = 2; break; }
        case 0x26: addr = ADDR_ZP(); value = READ(addr); { u8 c = cpu->P & FLAG_C ? 1 : 0;
                   cpu_set_flag(cpu, FLAG_C, value & 0x80); value = (value << 1) | c;
                   WRITE(addr, value); SET_NZ(value); } cpu->PC++; cycles = 5; break;
        case 0x36: addr = ADDR_ZPX(); value = READ(addr); { u8 c = cpu->P & FLAG_C ? 1 : 0;
                   cpu_set_flag(cpu, FLAG_C, value & 0x80); value = (value << 1) | c;
                   WRITE(addr, value); SET_NZ(value); } cpu->PC++; cycles = 6; break;
        case 0x2E: addr = ADDR_ABS(); value = READ(addr); { u8 c = cpu->P & FLAG_C ? 1 : 0;
                   cpu_set_flag(cpu, FLAG_C, value & 0x80); value = (value << 1) | c;
                   WRITE(addr, value); SET_NZ(value); } cpu->PC += 2; cycles = 6; break;
        case 0x3E: addr = ADDR_ABSX(); value = READ(addr); { u8 c = cpu->P & FLAG_C ? 1 : 0;
                   cpu_set_flag(cpu, FLAG_C, value & 0x80); value = (value << 1) | c;
                   WRITE(addr, value); SET_NZ(value); } cpu->PC += 2; cycles = 7; break;
        
        // ROR
        case 0x6A: { u8 c = cpu->P & FLAG_C ? 0x80 : 0; cpu_set_flag(cpu, FLAG_C, cpu->A & 0x01);
                   cpu->A = (cpu->A >> 1) | c; SET_NZ(cpu->A); cycles = 2; break; }
        case 0x66: addr = ADDR_ZP(); value = READ(addr); { u8 c = cpu->P & FLAG_C ? 0x80 : 0;
                   cpu_set_flag(cpu, FLAG_C, value & 0x01); value = (value >> 1) | c;
                   WRITE(addr, value); SET_NZ(value); } cpu->PC++; cycles = 5; break;
        case 0x76: addr = ADDR_ZPX(); value = READ(addr); { u8 c = cpu->P & FLAG_C ? 0x80 : 0;
                   cpu_set_flag(cpu, FLAG_C, value & 0x01); value = (value >> 1) | c;
                   WRITE(addr, value); SET_NZ(value); } cpu->PC++; cycles = 6; break;
        case 0x6E: addr = ADDR_ABS(); value = READ(addr); { u8 c = cpu->P & FLAG_C ? 0x80 : 0;
                   cpu_set_flag(cpu, FLAG_C, value & 0x01); value = (value >> 1) | c;
                   WRITE(addr, value); SET_NZ(value); } cpu->PC += 2; cycles = 6; break;
        case 0x7E: addr = ADDR_ABSX(); value = READ(addr); { u8 c = cpu->P & FLAG_C ? 0x80 : 0;
                   cpu_set_flag(cpu, FLAG_C, value & 0x01); value = (value >> 1) | c;
                   WRITE(addr, value); SET_NZ(value); } cpu->PC += 2; cycles = 7; break;
        
        // JMP
        case 0x4C: cpu->PC = ADDR_ABS(); cycles = 3; break;
        case 0x6C: { u16 ptr = ADDR_ABS();
                   u16 lo = READ(ptr);
                   u16 hi = READ((ptr & 0xFF00) | ((ptr + 1) & 0x00FF));
                   cpu->PC = lo | (hi << 8); cycles = 5; break; }
        
        // JSR
        case 0x20: { u16 ret = cpu->PC + 1;
                   PUSH((ret >> 8) & 0xFF);
                   PUSH(ret & 0xFF);
                   cpu->PC = ADDR_ABS(); cycles = 6; break; }
        
        // RTS
        case 0x60: { u16 lo = POP(); u16 hi = POP();
                   cpu->PC = (lo | (hi << 8)) + 1; cycles = 6; break; }
        
        // RTI
        case 0x40: cpu->P = (POP() & ~FLAG_B) | FLAG_U;
                   { u16 lo = POP(); u16 hi = POP();
                   cpu->PC = lo | (hi << 8); } cycles = 6; break;
        
        // BRK
        case 0x00: { u16 ret = cpu->PC + 1;
                   PUSH((ret >> 8) & 0xFF);
                   PUSH(ret & 0xFF);
                   PUSH(cpu->P | FLAG_B | FLAG_U);
                   cpu_set_flag(cpu, FLAG_I, true);
                   cpu->PC = READ(0xFFFE) | (READ(0xFFFF) << 8);
                   cycles = 7; break; }
        
        // Branches
        case 0x10: BRANCH(!(cpu->P & FLAG_N)); break;
        case 0x30: BRANCH(cpu->P & FLAG_N); break;
        case 0x50: BRANCH(!(cpu->P & FLAG_V)); break;
        case 0x70: BRANCH(cpu->P & FLAG_V); break;
        case 0x90: BRANCH(!(cpu->P & FLAG_C)); break;
        case 0xB0: BRANCH(cpu->P & FLAG_C); break;
        case 0xD0: BRANCH(!(cpu->P & FLAG_Z)); break;
        case 0xF0: BRANCH(cpu->P & FLAG_Z); break;
        
        // Flags
        case 0x18: cpu_set_flag(cpu, FLAG_C, false); cycles = 2; break;
        case 0x38: cpu_set_flag(cpu, FLAG_C, true); cycles = 2; break;
        case 0x58: cpu_set_flag(cpu, FLAG_I, false); cycles = 2; break;
        case 0x78: cpu_set_flag(cpu, FLAG_I, true); cycles = 2; break;
        case 0xB8: cpu_set_flag(cpu, FLAG_V, false); cycles = 2; break;
        case 0xD8: cpu_set_flag(cpu, FLAG_D, false); cycles = 2; break;
        case 0xF8: cpu_set_flag(cpu, FLAG_D, true); cycles = 2; break;
        
        // NOP
        case 0xEA: cycles = 2; break;
        
        default:
            // Unknown opcode - treat as NOP
            cycles = 2;
            break;
    }
    
    #undef READ
    #undef WRITE
    #undef PUSH
    #undef POP
    #undef SET_NZ
    #undef BRANCH
    #undef ADDR_ZP
    #undef ADDR_ZPX
    #undef ADDR_ZPY
    #undef ADDR_ABS
    #undef ADDR_ABSX
    #undef ADDR_ABSY
    #undef ADDR_INDX
    #undef ADDR_INDY
    
    return cycles;
}

// Run a single Lorenz test
static bool run_lorenz_test(const char *testname) {
    lorenz_init_memory();
    
    if (!lorenz_load_test(testname)) {
        return false;
    }
    
    lorenz_output_pos = 0;
    lorenz_output[0] = '\0';
    lorenz_test_passed = false;
    lorenz_test_failed = false;
    lorenz_current_test = testname;
    lorenz_next_test = NULL;
    
    // Initialize CPU
    C64Cpu cpu;
    memset(&cpu, 0, sizeof(cpu));
    cpu.P = FLAG_U | FLAG_I;
    cpu.SP = 0xFF;
    
    // Get start address from test file (already loaded)
    // Tests typically start at $0801 with BASIC stub, actual code at $0816
    cpu.PC = 0x0816;
    
    // Push return address to WARM trap
    lorenz_memory[0x100 + cpu.SP--] = 0x7F;  // High byte of $7FFF
    lorenz_memory[0x100 + cpu.SP--] = 0xFF;  // Low byte
    
    size_t max_cycles = 100000000;  // 100M cycles per test
    size_t cycles = 0;
    u16 last_pc = 0;
    int stuck_count = 0;
    
    while (cycles < max_cycles) {
        u16 pc = cpu.PC;
        
        // Check for trap addresses BEFORE executing
        
        // CHROUT ($FFD2) - print character
        if (pc == 0xFFD2) {
            char c = petscii_to_ascii(cpu.A);
            if (lorenz_output_pos < sizeof(lorenz_output) - 1) {
                lorenz_output[lorenz_output_pos++] = c;
                lorenz_output[lorenz_output_pos] = '\0';
            }
            // RTS
            u16 lo = lorenz_memory[0x100 + (++cpu.SP)];
            u16 hi = lorenz_memory[0x100 + (++cpu.SP)];
            cpu.PC = (lo | (hi << 8)) + 1;
            continue;
        }
        
        // GETIN ($FFE4) - get key, return non-zero to not wait forever
        if (pc == 0xFFE4) {
            cpu.A = 0x03;  // Return 3 (RUN/STOP key) - non-zero so loops exit
            cpu_set_flag(&cpu, FLAG_Z, false);  // Clear Z so BEQ fails
            // RTS
            u16 lo = lorenz_memory[0x100 + (++cpu.SP)];
            u16 hi = lorenz_memory[0x100 + (++cpu.SP)];
            cpu.PC = (lo | (hi << 8)) + 1;
            continue;
        }
        
        // WARM/CARTROM ($8000) - test passed
        if (pc == 0x8000) {
            lorenz_test_passed = true;
            break;
        }
        
        // READY ($A474) - return to BASIC (error)
        if (pc == 0xA474) {
            lorenz_test_failed = true;
            break;
        }
        
        // LOAD ($E16F) - load next test (we skip this for single tests)
        if (pc == 0xE16F) {
            // Get filename from $BB/$BC (address) and $B7 (length)
            u16 name_addr = lorenz_memory[0xBB] | (lorenz_memory[0xBC] << 8);
            u8 name_len = lorenz_memory[0xB7];
            
            static char next_name[64];
            for (int i = 0; i < name_len && i < 63; i++) {
                next_name[i] = petscii_to_ascii(lorenz_memory[name_addr + i]);
            }
            next_name[name_len < 63 ? name_len : 63] = '\0';
            lorenz_next_test = next_name;
            lorenz_test_passed = true;  // Current test passed, wants to load next
            break;
        }
        
        cycles += lorenz_cpu_step(&cpu);
        
        // Check for infinite loop
        if (cpu.PC == last_pc) {
            stuck_count++;
            if (stuck_count > 10) {
                lorenz_test_failed = true;
                break;
            }
        } else {
            stuck_count = 0;
            last_pc = pc;
        }
    }
    
    if (cycles >= max_cycles) {
        lorenz_test_failed = true;
    }
    
    return lorenz_test_passed && !lorenz_test_failed;
}

// ============================================================================
// Test cases - run selected Lorenz tests
// ============================================================================

// Core CPU instruction tests
static const char *lorenz_cpu_tests[] = {
    // Load/Store
    "ldab", "ldaz", "ldazx", "ldaa", "ldaax", "ldaay", "ldaix", "ldaiy",
    "ldxb", "ldxz", "ldxzy", "ldxa", "ldxay",
    "ldyb", "ldyz", "ldyzx", "ldya", "ldyax",
    "staz", "stazx", "staa", "staax", "staay", "staix", "staiy",
    "stxz", "stxzy", "stxa",
    "styz", "styzx", "stya",
    
    // Transfers
    "taxn", "tayn", "txan", "tyan", "tsxn", "txsn",
    
    // Stack
    "phan", "plan", "phpn", "plpn",
    
    // Arithmetic
    "adcb", "adcz", "adczx", "adca", "adcax", "adcay", "adcix", "adciy",
    "sbcb", "sbcz", "sbczx", "sbca", "sbcax", "sbcay", "sbcix", "sbciy",
    
    // Logical
    "andb", "andz", "andzx", "anda", "andax", "anday", "andix", "andiy",
    "orab", "oraz", "orazx", "oraa", "oraax", "oraay", "oraix", "oraiy",
    "eorb", "eorz", "eorzx", "eora", "eorax", "eoray", "eorix", "eoriy",
    "bitz", "bita",
    
    // Compare
    "cmpb", "cmpz", "cmpzx", "cmpa", "cmpax", "cmpay", "cmpix", "cmpiy",
    "cpxb", "cpxz", "cpxa",
    "cpyb", "cpyz", "cpya",
    
    // Inc/Dec
    "incz", "inczx", "inca", "incax",
    "decz", "deczx", "deca", "decax",
    "inxn", "inyn", "dexn", "deyn",
    
    // Shift/Rotate
    "asln", "aslz", "aslzx", "asla", "aslax",
    "lsrn", "lsrz", "lsrzx", "lsra", "lsrax",
    "roln", "rolz", "rolzx", "rola", "rolax",
    "rorn", "rorz", "rorzx", "rora", "rorax",
    
    // Jump/Branch
    "jmpw", "jmpi", "jsrw", "rtsn", "rtin",
    "bplr", "bmir", "bvcr", "bvsr", "bccr", "bcsr", "bner", "beqr",
    
    // Flags
    "clcn", "secn", "cldn", "sedn", "clin", "sein", "clvn",
    
    // BRK
    "brkn",
    
    // NOP
    "nopn",
    
    NULL
};

TEST(lorenz_cpu_instructions) {
    int passed = 0;
    int failed = 0;
    int skipped = 0;
    
    printf("\n");
    
    for (int i = 0; lorenz_cpu_tests[i] != NULL; i++) {
        const char *test = lorenz_cpu_tests[i];
        
        if (run_lorenz_test(test)) {
            passed++;
            // Don't print each passed test to reduce output
        } else {
            // Check if file exists
            char path[256];
            snprintf(path, sizeof(path), "tests/lorenz/%s", test);
            FILE *f = fopen(path, "rb");
            if (!f) {
                skipped++;
            } else {
                fclose(f);
                printf("    FAIL: %s", test);
                if (lorenz_output_pos > 0) {
                    printf(" - output: %s", lorenz_output);
                }
                printf("\n");
                failed++;
            }
        }
    }
    
    printf("    Lorenz CPU tests: %d passed, %d failed, %d skipped\n", 
           passed, failed, skipped);
    
    if (failed == 0) {
        PASS();
    } else {
        ASSERT_EQ(failed, 0);
    }
}

// ============================================================================
// Run all Lorenz tests
// ============================================================================

void run_lorenz_tests(void) {
    TEST_SUITE("Lorenz Test Suite");
    printf("    (Running CPU instruction tests...)\n");
    RUN_TEST(lorenz_cpu_instructions);
}
