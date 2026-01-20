#include "../include/mos6510.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define read8() cpu_read_byte(cpu, pc + 1)
#define read16() cpu_read_word(cpu, pc + 1)

trap_t traps[] = {{}, {}, {}, {}, {}};

bool add_trap(trap_t *trap)
{
    for (size_t i = 0; i < sizeof(traps) / sizeof(*traps); i++)
    {
        if (traps[i].handler == NULL || traps[i].address == trap->address)
        {
            printf("TRAP[%04X]=%p\n", trap->address, trap->handler);
            traps[i].address = trap->address;
            traps[i].handler = trap->handler;
            return true;
        }
    }
    return false;
}
handler_t find_trap(const uint16_t address)
{
    for (size_t i = 0; i < sizeof(traps) / sizeof(*traps); i++)
    {
        if (traps[i].address == address)
        {
            return traps[i].handler;
        }
    }
    return NULL;
}

void dump_step(CPU *cpu, const instruction_t *instruction)
{
    uint16_t pc = cpu_get_pc(cpu);

    printf("%04X ", pc);

    for (int i = 0; i < 3; i++)
    {
        if (i < instruction->size)
            printf(" %02X", cpu_read_byte(cpu, pc + i));
        else
            printf("   ");
    }

    printf(" %c%s", instruction->illegal ? '*' : ' ', instruction->name);

    switch (instruction->mode)
    {
    case Implied:
        // Implied Addressing
        // https://www.c64-wiki.com/wiki/Implied_addressing
        //
        // Does not require the specification of any additional information.
        // BRK, CLC, CLD, CLI, CLV, DEX, DEY, INX, INY, NOP, PHA, PHP, PLA, PLP, RTI, RTS, SEC, SED, SEI, TAX, TAY, TSX, TXA, TXS, and TYA.

        printf("%28s", ""); // 28 - 0 = 28
        break;
    case Accumulator:
        // Accumulator Addressing
        // https://www.c64-wiki.com/wiki/Accumulator_addressing
        //
        // The input for the shift operation is picked up from the CPU's accumulator, and the output is stored back into the accumulator.
        // ASL, LSR, ROL, and ROR.

        printf(" A");
        printf("%26s", ""); // 28 - 1 - 1 = 26
        break;
    case Immediate:
        // Immediate Addressing
        // https://www.c64-wiki.com/wiki/Immediate_addressing
        //
        // The byte value to be used or retrieved in the instruction, located immediately after the opcode for the instruction itself.
        // ADC, AND, CMP, CPX, CPY, EOR, LDA, LDX, LDY, ORA, and SBC.

        printf(" #$%02X", read8());
        printf("%23s", ""); // 28 - 1 - 4 = 23
        break;
    case Indirect:
        // Absolute-Indirect Addressing
        // https://www.c64-wiki.com/wiki/Absolute-indirect_addressing
        //
        // The given address is a vector to the effective address.
        // JMP

        printf(" ($%04X)", read16());
        printf(" = %04X", cpu_read_word_zp(cpu, read16()));
        printf("%13s", ""); // 28 - 1 - 7 - 3 - 4 = 13
        break;
    case IndexedIndirect:
        // Indexed-Indirect Addressing
        // https://www.c64-wiki.com/wiki/Indexed-indirect_addressing
        //
        // The X index register is used to offset the zero page vector used to determine the effective address.
        // ADC, AND, CMP, EOR, LDA, ORA, SBC, STA.

        printf(" ($%02X,X)", read8());
        printf(" @ %02X", (read8() + cpu->X) & 0xFF);
        printf(" = %04X", cpu_read_word_zp(cpu, (read8() + cpu->X) & 0xFF));
        printf(" = %02X", cpu_read_byte(cpu, cpu_read_word_zp(cpu, (read8() + cpu->X) & 0xFF)));
        printf("%3s", ""); // 28 - 1 - 7 - 3 - 2 - 3 - 4 - 3 - 2 = 3
        break;
    case IndirectIndexed:
        // Indirect-Indexed Addressing
        // https://www.c64-wiki.com/wiki/Indirect-indexed_addressing
        //
        // The Y Index Register is used as an offset from the given zero page vector.
        // The effective address is calculated as the vector plus the value in Y.
        // ADC, AND, CMP, EOR, LDA, ORA, SBC, and STA.

        printf(" ($%02X),Y", read8());
        printf(" = %04X", cpu_read_word_zp(cpu, read8()));
        printf(" @ %04X", (cpu_read_word_zp(cpu, read8()) + cpu->Y) & 0xFFFF);
        printf(" = %02X", cpu_read_byte(cpu, (cpu_read_word_zp(cpu, read8()) + cpu->Y) & 0xFFFF));
        printf("%1s", ""); // 28 - 1 - 7 - 3 - 4 - 3 - 4 - 3 - 2 = 1
        break;
    case Absolute:
        // Absolute Addressing
        // https://www.c64-wiki.com/wiki/Absolute_addressing
        //
        // Specifies an address in memory which is to be the "object" of the instruction.
        // ADC, AND, ASL, BIT, CMP, CPX, CPY, DEC, EOR, INC, JMP, JSR, LDA, LDX, LDY, LSR, ORA, ROL, ROR, SBC, STA, STX, and STY.

        printf(" $%04X", read16());
        if (instruction->execute == JSR || instruction->execute == JMP)
            printf("     ");
        else
            printf(" = %02X", cpu_read_byte(cpu, read16()));
        printf("%17s", ""); // 28 - 1 - 5 - 3 - 2 = 17
        break;
    case AbsoluteX:
        // Indexed Absolute Addressing
        // https://www.c64-wiki.com/wiki/Indexed_absolute_addressing
        //
        // The contents of the X index register is added to a given base address.
        // ADC, AND, ASL, CMP, DEC, EOR, INC, LDA, LDX, LDY, LSR, ORA, ROL, ROR, SBC, STA.

        printf(" $%04X,X", read16());
        printf(" @ %04X", (read16() + cpu->X) & 0xFFFF);
        printf(" = %02X", cpu_read_byte(cpu, (read16() + cpu->X) & 0xFFFF));
        printf("%8s", ""); // 28 - 1 - 7 - 3 - 4 - 3 - 2 = 8
        break;
    case AbsoluteY:
        // Indexed Absolute Addressing
        // https://www.c64-wiki.com/wiki/Indexed_absolute_addressing
        //
        // The contents of the Y index register is added to a given base address.
        // ADC, AND, ASL, CMP, DEC, EOR, INC, LDA, LDX, LDY, LSR, ORA, ROL, ROR, SBC, STA.

        printf(" $%04X,Y", read16());
        printf(" @ %04X", (read16() + cpu->Y) & 0xFFFF);
        printf(" = %02X", cpu_read_byte(cpu, (read16() + cpu->Y) & 0xFFFF));
        printf("%8s", ""); // 28 - 1 - 7 - 3 - 4 - 3 - 2 = 8
        break;
    case ZeroPage:
        // Zeropage Addressing
        // https://www.c64-wiki.com/wiki/Zeropage_addressing
        //
        // Specifies an address in zero-page which is to be the "object" of the instruction.
        // ADC, AND, ASL, BIT, CMP, CPX, CPY, DEC, EOR, INC, LDA, LDX, LDY, LSR, ORA, ROL, ROR, SBC, STA, STX, and STY.

        printf(" $%02X", read8());
        printf(" = %02X", cpu_read_byte(cpu, read8()));
        printf("%19s", ""); // 28 - 1 - 3 - 3 - 2 = 19
        break;
    case ZeroPageX:
        // Indexed Zeropage Addressing
        // https://www.c64-wiki.com/wiki/Indexed_zeropage_addressing
        //
        // The contents of the X index register is added to a given base address in zero-page.
        // ADC, AND, ASL, CMP, DEC, EOR, INC, LDA, LDX, LDY, LSR, ORA, ROL, ROR, SBC, STA, STX, STY.

        printf(" $%02X,X", read8());
        printf(" @ %02X", (read8() + cpu->X) & 0xFF);
        printf(" = %02X", cpu_read_byte(cpu, (read8() + cpu->X) & 0xFF));
        printf("%12s", ""); // 28 - 1 - 5 - 3 - 2 - 3 - 2 = 12
        break;
    case ZeroPageY:
        // Indexed Zeropage Addressing
        // https://www.c64-wiki.com/wiki/Indexed_zeropage_addressing
        //
        // The contents of the Y index register is added to a given base address in zero-page.
        // ADC, AND, ASL, CMP, DEC, EOR, INC, LDA, LDX, LDY, LSR, ORA, ROL, ROR, SBC, STA, STX, STY.

        printf(" $%02X,Y", read8());
        printf(" @ %02X", (read8() + cpu->Y) & 0xFF);
        printf(" = %02X", cpu_read_byte(cpu, (read8() + cpu->Y) & 0xFF));
        printf("%12s", ""); // 28 - 1 - 5 - 3 - 2 - 3 - 2 = 12
        break;
    case Relative:
        // Relative Addressing
        // https://www.c64-wiki.com/wiki/Relative_addressing
        //
        // A single, signed-integer byte that specifies, in relative terms, how far "up" or "down" to jump if the required conditions are met.
        // This signed 8-bit figure is called the offset.
        // BCC, BCS, BEQ, BMI, BNE, BPL, BVC, and BVS.

        printf(" $%04X", (pc + 2 + (int8_t)read8()) & 0xFFFF);
        printf("%22s", ""); // 28 - 1 - 5 = 22
        break;
    default:
        printf(" ???");
        printf("%24s", ""); // 28 - 1 - 3 = 24
        break;
    }

    printf(" A:%02X X:%02X Y:%02X P:%02X SP:%02X",
           cpu->A, cpu->X, cpu->Y, cpu->P, cpu->SP);

    // printf(" P:%c%c%c%c%c%c%c%c",
    //        (get_flag_negative(cpu) ? 'N' : '-'),
    //        (get_flag_overflow(cpu) ? 'V' : '-'),
    //        '1', // Reserved flag always set
    //        (get_flag_break(cpu) ? 'B' : '-'),
    //        (get_flag_decimal(cpu) ? 'D' : '-'),
    //        (get_flag_interrupt(cpu) ? 'I' : '-'),
    //        (get_flag_zero(cpu) ? 'Z' : '-'),
    //        (get_flag_carry(cpu) ? 'C' : '-'));

    printf("\n");
}

void cpu_init(CPU *cpu, bool debug)
{
    memset(cpu, 0, sizeof(CPU));
    cpu->P = FLAG_RESERVED | FLAG_INTERRUPT_DISABLE;
    cpu->SP = 0xFF;
    cpu->debug = debug;
}

void cpu_reset(CPU *cpu)
{
    cpu_reset_pc(cpu, cpu_read_word(cpu, 0xFFFC));
}
void cpu_reset_pc(CPU *cpu, uint16_t addr)
{
    cpu->A = 0x00;
    cpu->X = 0x00;
    cpu->Y = 0x00;
    cpu->P = FLAG_RESERVED | FLAG_INTERRUPT_DISABLE;
    cpu->SP = 0xFF;

    cpu_set_pc(cpu, addr);
}

uint16_t cpu_get_pc(CPU *cpu)
{
    // return cpu->PC | (cpu->PCH << 8);
    return cpu->pc;
}
void cpu_set_pc(CPU *cpu, uint16_t addr)
{
    // cpu->PC = addr & 0xFF;
    // cpu->PCH = (addr >> 8) & 0xFF;
    cpu->pc = addr;
}

uint8_t cpu_read_byte(CPU *cpu, uint16_t addr)
{
    return cpu->memory[addr];
}
uint16_t cpu_read_word(CPU *cpu, uint16_t addr)
{
    return cpu_read_byte(cpu, addr) | (cpu_read_byte(cpu, addr + 1) << 8);
}
uint16_t cpu_read_word_zp(CPU *cpu, uint16_t addr)
{
    return cpu_read_byte(cpu, addr) | (cpu_read_byte(cpu, ((addr + 1) & 0x00FF) | (addr & 0xFF00)) << 8);
}

void cpu_write_byte(CPU *cpu, uint16_t addr, uint8_t data)
{
    cpu->memory[addr] = data;
}
void cpu_write_word(CPU *cpu, uint16_t addr, uint16_t data)
{
    cpu_write_byte(cpu, addr, data & 0xFF);
    cpu_write_byte(cpu, addr + 1, (data >> 8) & 0xFF);
}

void cpu_write_data(CPU *cpu, uint16_t addr, uint8_t data[], size_t size)
{
    for (size_t i = 0; i < size; i++)
    {
        cpu_write_byte(cpu, addr + i, data[i]);
    }
}

void cpu_push(CPU *cpu, uint8_t data)
{
    cpu_write_byte(cpu, 0x0100 | cpu->SP, data);
    cpu->SP--;
}
void cpu_push16(CPU *cpu, uint16_t data)
{
    cpu_push(cpu, (data >> 8) & 0xFF);
    cpu_push(cpu, data & 0xFF);
}
uint8_t cpu_pull(CPU *cpu)
{
    cpu->SP++;
    return cpu_read_byte(cpu, 0x0100 | cpu->SP);
}
uint16_t cpu_pull16(CPU *cpu)
{
    return cpu_pull(cpu) | (cpu_pull(cpu) << 8);
}

bool cpu_trap(CPU *cpu, uint16_t addr, handler_t handler)
{
    trap_t trap = {addr, handler};
    return add_trap(&trap);
}

uint16_t get_operand_address(CPU *cpu, addressing_mode_t mode)
{
    uint16_t addr;
    uint8_t low_byte, high_byte;

    switch (mode)
    {
    case ADDR_IMP:
        return 0; // Not used for implied addressing

    case ADDR_IMM:
        return cpu_get_pc(cpu) + 1;

    case ADDR_ZP:
        return cpu->memory[cpu_get_pc(cpu) + 1];

    case ADDR_ZPX:
        return (cpu->memory[cpu_get_pc(cpu) + 1] + cpu->X) & 0xFF;

    case ADDR_ZPY:
        return (cpu->memory[cpu_get_pc(cpu) + 1] + cpu->Y) & 0xFF;

    case ADDR_ABS:
        low_byte = cpu->memory[cpu_get_pc(cpu) + 1];
        high_byte = cpu->memory[cpu_get_pc(cpu) + 2];
        return (high_byte << 8) | low_byte;

    case ADDR_ABSX:
        low_byte = cpu->memory[cpu_get_pc(cpu) + 1];
        high_byte = cpu->memory[cpu_get_pc(cpu) + 2];
        addr = (high_byte << 8) | low_byte;
        return addr + cpu->X;

    case ADDR_ABSY:
        low_byte = cpu->memory[cpu_get_pc(cpu) + 1];
        high_byte = cpu->memory[cpu_get_pc(cpu) + 2];
        addr = (high_byte << 8) | low_byte;
        return addr + cpu->Y;

    case ADDR_IND:
        low_byte = cpu->memory[cpu_get_pc(cpu) + 1];
        high_byte = cpu->memory[cpu_get_pc(cpu) + 2];
        addr = (high_byte << 8) | low_byte;
        // 6502 bug: if page boundary crossed, high byte wraps
        if (low_byte == 0xFF)
        {
            // return (cpu->memory[addr] << 8) | cpu->memory[addr & 0xFF00];
            return cpu_read_word_zp(cpu, addr);
        }
        else
        {
            // return (cpu->memory[addr + 1] << 8) | cpu->memory[addr];
            return cpu_read_word(cpu, addr);
        }

    case ADDR_INDX:
    {
        uint8_t ptr = cpu->memory[cpu_get_pc(cpu) + 1] + cpu->X;
        low_byte = cpu->memory[ptr];
        high_byte = cpu->memory[(ptr + 1) & 0xFF];
        return (high_byte << 8) | low_byte;
    }

    case ADDR_INDY:
    {
        uint8_t ptr = cpu->memory[cpu_get_pc(cpu) + 1];
        low_byte = cpu->memory[ptr];
        high_byte = cpu->memory[(ptr + 1) & 0xFF];
        addr = (high_byte << 8) | low_byte;
        return addr + cpu->Y;
    }

    case ADDR_REL:
        return cpu_get_pc(cpu) + 1;

    default:
        return 0;
    }
}

uint8_t fetch_operand(CPU *cpu, addressing_mode_t mode)
{
    uint16_t addr = get_operand_address(cpu, mode);
    return cpu->memory[addr];
}

uint8_t cpu_step(CPU *cpu)
{
    uint16_t pc = cpu_get_pc(cpu);

    uint8_t opcode = cpu->memory[pc];
    const instruction_t *instruction = &instructions[opcode];

    // if (op->execute == NOP) {
    //     // Handle JMP for NOP opcodes that should be JMP
    //     if (opcode == 0x4C || opcode == 0x6C) {
    //         JMP(cpu);
    //     } else {
    //         // Just skip the opcode and any operands
    //         cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    //         switch (op->mode) {
    //             case ADDR_IMM:
    //             case ADDR_ZP:
    //             case ADDR_ZPX:
    //             case ADDR_ZPY:
    //             case ADDR_REL:
    //                 cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    //                 break;
    //             case ADDR_ABS:
    //             case ADDR_ABSX:
    //             case ADDR_ABSY:
    //             case ADDR_IND:
    //                 cpu_set_pc(cpu, cpu_get_pc(cpu) + 2);
    //                 break;
    //             default:
    //                 break;
    //         }
    //     }
    //     return op->cycles;
    // }

    if (cpu->debug)
        dump_step(cpu, instruction);

    // uint16_t pc = cpu_get_pc(cpu);
    instruction->execute(cpu);

    handler_t handler = find_trap(pc);
    if (handler != NULL)
        handler(cpu);

    // if (cpu_get_pc(cpu) != pc + instruction->size)
    // {
    //     printf("*** PC:$%04X != $%04X ($%04X +%d)\n", cpu_get_pc(cpu), pc + instruction->size, pc, instruction->size);
    //     // exit(1);
    // }

    return instruction->cycles;
}
