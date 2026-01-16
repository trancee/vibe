#include "../include/mos6510.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>



void mos6510_init(MOS6510 *cpu) {
    memset(cpu, 0, sizeof(MOS6510));
    cpu->P = FLAG_RESERVED | FLAG_INTERRUPT_DISABLE;
    cpu->SP = 0xFF;
}

void mos6510_reset(MOS6510 *cpu) {
    cpu->PC = cpu->memory[0xFFFC] | (cpu->memory[0xFFFD] << 8);
    cpu->P = FLAG_RESERVED | FLAG_INTERRUPT_DISABLE;
    cpu->SP = 0xFF;
}

uint16_t mos6510_get_pc(MOS6510 *cpu) {
    return cpu->PC | (cpu->PCH << 8);
}

void mos6510_set_pc(MOS6510 *cpu, uint16_t addr) {
    cpu->PC = addr & 0xFF;
    cpu->PCH = (addr >> 8) & 0xFF;
}

uint8_t mos6510_read_byte(MOS6510 *cpu, uint16_t addr) {
    return cpu->memory[addr];
}

void mos6510_write_byte(MOS6510 *cpu, uint16_t addr, uint8_t data) {
    cpu->memory[addr] = data;
}

void mos6510_push(MOS6510 *cpu, uint8_t data) {
    cpu->memory[0x0100 | cpu->SP] = data;
    cpu->SP--;
}

uint8_t mos6510_pull(MOS6510 *cpu) {
    cpu->SP++;
    return cpu->memory[0x0100 | cpu->SP];
}

uint16_t get_operand_address(MOS6510 *cpu, addressing_mode_t mode) {
    uint16_t addr;
    uint8_t low_byte, high_byte;
    
    switch (mode) {
        case ADDR_IMP:
            return 0;  // Not used for implied addressing
            
        case ADDR_IMM:
            return mos6510_get_pc(cpu) + 1;
            
        case ADDR_ZP:
            return cpu->memory[mos6510_get_pc(cpu) + 1];
            
        case ADDR_ZPX:
            return (cpu->memory[mos6510_get_pc(cpu) + 1] + cpu->X) & 0xFF;
            
        case ADDR_ZPY:
            return (cpu->memory[mos6510_get_pc(cpu) + 1] + cpu->Y) & 0xFF;
            
        case ADDR_ABS:
            low_byte = cpu->memory[mos6510_get_pc(cpu) + 1];
            high_byte = cpu->memory[mos6510_get_pc(cpu) + 2];
            return (high_byte << 8) | low_byte;
            
        case ADDR_ABSX:
            low_byte = cpu->memory[mos6510_get_pc(cpu) + 1];
            high_byte = cpu->memory[mos6510_get_pc(cpu) + 2];
            addr = (high_byte << 8) | low_byte;
            return addr + cpu->X;
            
        case ADDR_ABSY:
            low_byte = cpu->memory[mos6510_get_pc(cpu) + 1];
            high_byte = cpu->memory[mos6510_get_pc(cpu) + 2];
            addr = (high_byte << 8) | low_byte;
            return addr + cpu->Y;
            
        case ADDR_IND:
            low_byte = cpu->memory[mos6510_get_pc(cpu) + 1];
            high_byte = cpu->memory[mos6510_get_pc(cpu) + 2];
            addr = (high_byte << 8) | low_byte;
            // 6502 bug: if page boundary crossed, high byte wraps
            if (low_byte == 0xFF) {
                return (cpu->memory[addr] << 8) | cpu->memory[addr & 0xFF00];
            } else {
                return (cpu->memory[addr + 1] << 8) | cpu->memory[addr];
            }
            
        case ADDR_INDX:
            {
                uint8_t ptr = cpu->memory[mos6510_get_pc(cpu) + 1] + cpu->X;
                low_byte = cpu->memory[ptr];
                high_byte = cpu->memory[(ptr + 1) & 0xFF];
                return (high_byte << 8) | low_byte;
            }
            
        case ADDR_INDY:
            {
                uint8_t ptr = cpu->memory[mos6510_get_pc(cpu) + 1];
                low_byte = cpu->memory[ptr];
                high_byte = cpu->memory[(ptr + 1) & 0xFF];
                addr = (high_byte << 8) | low_byte;
                return addr + cpu->Y;
            }
            
        case ADDR_REL:
            return mos6510_get_pc(cpu) + 1;
            
        default:
            return 0;
    }
}

void advance_pc(MOS6510 *cpu, addressing_mode_t mode) {
    uint16_t pc = mos6510_get_pc(cpu);
    switch (mode) {
        case ADDR_IMP:
        case ADDR_REL:
            mos6510_set_pc(cpu, pc + 1);
            break;
        case ADDR_IMM:
        case ADDR_ZP:
        case ADDR_ZPX:
        case ADDR_ZPY:
            mos6510_set_pc(cpu, pc + 2);
            break;
        case ADDR_ABS:
        case ADDR_ABSX:
        case ADDR_ABSY:
        case ADDR_IND:
        case ADDR_INDX:
        case ADDR_INDY:
            mos6510_set_pc(cpu, pc + 3);
            break;
        default:
            mos6510_set_pc(cpu, pc + 1);
            break;
    }
}

uint8_t fetch_operand(MOS6510 *cpu, addressing_mode_t mode) {
    uint16_t addr = get_operand_address(cpu, mode);
    if (mode == ADDR_IMM) {
        return cpu->memory[addr];
    }
    return cpu->memory[addr];
}

uint8_t mos6510_step(MOS6510 *cpu) {
    uint8_t opcode = cpu->memory[mos6510_get_pc(cpu)];
    const opcode_t *op = &opcode_table[opcode];
    
    if (op->execute == NOP) {
        // Handle JMP for NOP opcodes that should be JMP
        if (opcode == 0x4C || opcode == 0x6C) {
            JMP(cpu);
        } else {
            // Just skip the opcode and any operands
            mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
            switch (op->addr_mode) {
                case ADDR_IMM:
                case ADDR_ZP:
                case ADDR_ZPX:
                case ADDR_ZPY:
                case ADDR_REL:
                    mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
                    break;
                case ADDR_ABS:
                case ADDR_ABSX:
                case ADDR_ABSY:
                case ADDR_IND:
                    mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 2);
                    break;
                default:
                    break;
            }
        }
        return op->cycles;
    }
    
    op->execute(cpu);
    return op->cycles;
}