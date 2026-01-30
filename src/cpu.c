#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cpu.h"
#include "mos6510.h"

#define read8() cpu_read_byte(cpu, pc + 1)
#define read16() cpu_read_word(cpu, pc + 1)

trap_t traps[] = {{}, {}, {}, {}, {}};

bool add_trap(trap_t *trap)
{
    for (size_t i = 0; i < sizeof(traps) / sizeof(*traps); i++)
    {
        if (traps[i].handler == NULL || traps[i].address == trap->address)
        {
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

#define DUMP_BUFFER_SIZE 128

void dump_step(CPU *cpu, const instruction_t *instruction)
{
    char *buffer = NULL;
    size_t buffer_size = 0;
    FILE *stream = open_memstream(&buffer, &buffer_size);

    uint16_t pc = cpu_get_pc(cpu);
    fprintf(stream, "%04X ", pc);

    for (int i = 0; i < 3; i++)
    {
        if (i < instruction->size)
            fprintf(stream, " %02X", cpu_read_byte(cpu, pc + i));
        else
            fprintf(stream, "   ");
    }

    fprintf(stream, " %c%s", instruction->illegal ? '*' : ' ', instruction->name);

    switch (instruction->mode)
    {
    case Implied:
        // Implied Addressing
        // https://www.c64-wiki.com/wiki/Implied_addressing
        //
        // Does not require the specification of any additional information.
        // BRK, CLC, CLD, CLI, CLV, DEX, DEY, INX, INY, NOP, PHA, PHP, PLA, PLP, RTI, RTS, SEC, SED, SEI, TAX, TAY, TSX, TXA, TXS, and TYA.

        fprintf(stream, "%28s", ""); // 28 - 0 = 28
        break;
    case Accumulator:
        // Accumulator Addressing
        // https://www.c64-wiki.com/wiki/Accumulator_addressing
        //
        // The input for the shift operation is picked up from the CPU's accumulator, and the output is stored back into the accumulator.
        // ASL, LSR, ROL, and ROR.

        fprintf(stream, " A");
        fprintf(stream, "%26s", ""); // 28 - 1 - 1 = 26
        break;
    case Immediate:
        // Immediate Addressing
        // https://www.c64-wiki.com/wiki/Immediate_addressing
        //
        // The byte value to be used or retrieved in the instruction, located immediately after the opcode for the instruction itself.
        // ADC, AND, CMP, CPX, CPY, EOR, LDA, LDX, LDY, ORA, and SBC.

        fprintf(stream, " #$%02X", read8());
        fprintf(stream, "%23s", ""); // 28 - 1 - 4 = 23
        break;
    case Indirect:
        // Absolute-Indirect Addressing
        // https://www.c64-wiki.com/wiki/Absolute-indirect_addressing
        //
        // The given address is a vector to the effective address.
        // JMP

        fprintf(stream, " ($%04X)", read16());
        fprintf(stream, " = %04X", cpu_read_word_zp(cpu, read16()));
        fprintf(stream, "%13s", ""); // 28 - 1 - 7 - 3 - 4 = 13
        break;
    case IndexedIndirect:
        // Indexed-Indirect Addressing
        // https://www.c64-wiki.com/wiki/Indexed-indirect_addressing
        //
        // The X index register is used to offset the zero page vector used to determine the effective address.
        // ADC, AND, CMP, EOR, LDA, ORA, SBC, STA.

        fprintf(stream, " ($%02X,X)", read8());
        fprintf(stream, " @ %02X", (read8() + cpu->X) & 0xFF);
        fprintf(stream, " = %04X", cpu_read_word_zp(cpu, (read8() + cpu->X) & 0xFF));
        fprintf(stream, " = %02X", cpu_read_byte(cpu, cpu_read_word_zp(cpu, (read8() + cpu->X) & 0xFF)));
        fprintf(stream, "%3s", ""); // 28 - 1 - 7 - 3 - 2 - 3 - 4 - 3 - 2 = 3
        break;
    case IndirectIndexed:
        // Indirect-Indexed Addressing
        // https://www.c64-wiki.com/wiki/Indirect-indexed_addressing
        //
        // The Y Index Register is used as an offset from the given zero page vector.
        // The effective address is calculated as the vector plus the value in Y.
        // ADC, AND, CMP, EOR, LDA, ORA, SBC, and STA.

        fprintf(stream, " ($%02X),Y", read8());
        fprintf(stream, " = %04X", cpu_read_word_zp(cpu, read8()));
        fprintf(stream, " @ %04X", (cpu_read_word_zp(cpu, read8()) + cpu->Y) & 0xFFFF);
        fprintf(stream, " = %02X", cpu_read_byte(cpu, (cpu_read_word_zp(cpu, read8()) + cpu->Y) & 0xFFFF));
        fprintf(stream, "%1s", ""); // 28 - 1 - 7 - 3 - 4 - 3 - 4 - 3 - 2 = 1
        break;
    case Absolute:
        // Absolute Addressing
        // https://www.c64-wiki.com/wiki/Absolute_addressing
        //
        // Specifies an address in memory which is to be the "object" of the instruction.
        // ADC, AND, ASL, BIT, CMP, CPX, CPY, DEC, EOR, INC, JMP, JSR, LDA, LDX, LDY, LSR, ORA, ROL, ROR, SBC, STA, STX, and STY.

        fprintf(stream, " $%04X", read16());
        if (instruction->opcode == 0x20 || instruction->opcode == 0x4C || instruction->opcode == 0x6C) // JSR or JMP
            fprintf(stream, "     ");
        else
            fprintf(stream, " = %02X", cpu_read_byte(cpu, read16()));
        fprintf(stream, "%17s", ""); // 28 - 1 - 5 - 3 - 2 = 17
        break;
    case AbsoluteX:
        // Indexed Absolute Addressing
        // https://www.c64-wiki.com/wiki/Indexed_absolute_addressing
        //
        // The contents of the X index register is added to a given base address.
        // ADC, AND, ASL, CMP, DEC, EOR, INC, LDA, LDX, LDY, LSR, ORA, ROL, ROR, SBC, STA.

        fprintf(stream, " $%04X,X", read16());
        fprintf(stream, " @ %04X", (read16() + cpu->X) & 0xFFFF);
        fprintf(stream, " = %02X", cpu_read_byte(cpu, (read16() + cpu->X) & 0xFFFF));
        fprintf(stream, "%8s", ""); // 28 - 1 - 7 - 3 - 4 - 3 - 2 = 8
        break;
    case AbsoluteY:
        // Indexed Absolute Addressing
        // https://www.c64-wiki.com/wiki/Indexed_absolute_addressing
        //
        // The contents of the Y index register is added to a given base address.
        // ADC, AND, ASL, CMP, DEC, EOR, INC, LDA, LDX, LDY, LSR, ORA, ROL, ROR, SBC, STA.

        fprintf(stream, " $%04X,Y", read16());
        fprintf(stream, " @ %04X", (read16() + cpu->Y) & 0xFFFF);
        fprintf(stream, " = %02X", cpu_read_byte(cpu, (read16() + cpu->Y) & 0xFFFF));
        fprintf(stream, "%8s", ""); // 28 - 1 - 7 - 3 - 4 - 3 - 2 = 8
        break;
    case ZeroPage:
        // Zeropage Addressing
        // https://www.c64-wiki.com/wiki/Zeropage_addressing
        //
        // Specifies an address in zero-page which is to be the "object" of the instruction.
        // ADC, AND, ASL, BIT, CMP, CPX, CPY, DEC, EOR, INC, LDA, LDX, LDY, LSR, ORA, ROL, ROR, SBC, STA, STX, and STY.

        fprintf(stream, " $%02X", read8());
        fprintf(stream, " = %02X", cpu_read_byte(cpu, read8()));
        fprintf(stream, "%19s", ""); // 28 - 1 - 3 - 3 - 2 = 19
        break;
    case ZeroPageX:
        // Indexed Zeropage Addressing
        // https://www.c64-wiki.com/wiki/Indexed_zeropage_addressing
        //
        // The contents of the X index register is added to a given base address in zero-page.
        // ADC, AND, ASL, CMP, DEC, EOR, INC, LDA, LDX, LDY, LSR, ORA, ROL, ROR, SBC, STA, STX, STY.

        fprintf(stream, " $%02X,X", read8());
        fprintf(stream, " @ %02X", (read8() + cpu->X) & 0xFF);
        fprintf(stream, " = %02X", cpu_read_byte(cpu, (read8() + cpu->X) & 0xFF));
        fprintf(stream, "%12s", ""); // 28 - 1 - 5 - 3 - 2 - 3 - 2 = 12
        break;
    case ZeroPageY:
        // Indexed Zeropage Addressing
        // https://www.c64-wiki.com/wiki/Indexed_zeropage_addressing
        //
        // The contents of the Y index register is added to a given base address in zero-page.
        // ADC, AND, ASL, CMP, DEC, EOR, INC, LDA, LDX, LDY, LSR, ORA, ROL, ROR, SBC, STA, STX, STY.

        fprintf(stream, " $%02X,Y", read8());
        fprintf(stream, " @ %02X", (read8() + cpu->Y) & 0xFF);
        fprintf(stream, " = %02X", cpu_read_byte(cpu, (read8() + cpu->Y) & 0xFF));
        fprintf(stream, "%12s", ""); // 28 - 1 - 5 - 3 - 2 - 3 - 2 = 12
        break;
    case Relative:
        // Relative Addressing
        // https://www.c64-wiki.com/wiki/Relative_addressing
        //
        // A single, signed-integer byte that specifies, in relative terms, how far "up" or "down" to jump if the required conditions are met.
        // This signed 8-bit figure is called the offset.
        // BCC, BCS, BEQ, BMI, BNE, BPL, BVC, and BVS.

        fprintf(stream, " $%04X", (pc + 2 + (int8_t)read8()) & 0xFFFF);
        fprintf(stream, "%22s", ""); // 28 - 1 - 5 = 22
        break;
    default:
        fprintf(stream, " ???");
        fprintf(stream, "%24s", ""); // 28 - 1 - 3 = 24
        break;
    }

    fprintf(stream, " A:%02X X:%02X Y:%02X P:%02X SP:%02X",
            cpu->A, cpu->X, cpu->Y, cpu->P, cpu->SP);

    // fprintf(stream, "  %02X %02X %%%c%c%c",
    //         cpu_read_byte(cpu, 0x0000), cpu_read_byte(cpu, 0x0001), cpu_read_byte(cpu, 0x0001) & 0x04 ? '1' : '0', cpu_read_byte(cpu, 0x0001) & 0x02 ? '1' : '0', cpu_read_byte(cpu, 0x0001) & 0x01 ? '1' : '0');

    // printf(" P:%c%c%c%c%c%c%c%c",
    //        (get_flag_negative(cpu) ? 'N' : '-'),
    //        (get_flag_overflow(cpu) ? 'V' : '-'),
    //        '1', // Reserved flag always set
    //        (get_flag_break(cpu) ? 'B' : '-'),
    //        (get_flag_decimal(cpu) ? 'D' : '-'),
    //        (get_flag_interrupt(cpu) ? 'I' : '-'),
    //        (get_flag_zero(cpu) ? 'Z' : '-'),
    //        (get_flag_carry(cpu) ? 'C' : '-'));

    fprintf(stream, "\n");
    fclose(stream);

    if (cpu->debug_file != NULL)
        fprintf(cpu->debug_file, "%s", buffer);

    printf("%s", buffer);

    free(buffer);
}

void cpu_init(CPU *cpu)
{
    memset(cpu, 0, sizeof(CPU));

    cpu->P = FLAG_RESERVED | FLAG_INTERRUPT_DISABLE;
    cpu->SP = 0xFF;

    cpu_set_decimal_mode(cpu, true);
    cpu_set_read_write(cpu, cpu_read, cpu_write);
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
    cpu->PC = addr;
}

uint16_t cpu_get_pc(CPU *cpu)
{
    return cpu->PC;
}
void cpu_set_pc(CPU *cpu, uint16_t addr)
{
    cpu->PC = addr;
}

uint8_t cpu_read(void *cpu, uint16_t addr)
{
    // printf("C64 #$%04X → $%02X\n", addr, ((CPU *)cpu)->memory[addr]);

    return ((CPU *)cpu)->memory[addr];
}
uint8_t cpu_read_byte(CPU *cpu, uint16_t addr)
{
    return cpu->read(cpu, addr);
}
uint16_t cpu_read_word(CPU *cpu, uint16_t addr)
{
    return cpu_read_byte(cpu, addr) | (cpu_read_byte(cpu, addr + 1) << 8);
}
uint16_t cpu_read_word_zp(CPU *cpu, uint16_t addr)
{
    return cpu_read_byte(cpu, addr) | (cpu_read_byte(cpu, ((addr + 1) & 0x00FF) | (addr & 0xFF00)) << 8);
}

void cpu_write(void *cpu, uint16_t addr, uint8_t data)
{
    // printf("C64 #$%04X ← $%02X\n", addr, data);

    ((CPU *)cpu)->memory[addr] = data;
}
void cpu_write_byte(CPU *cpu, uint16_t addr, uint8_t data)
{
    cpu->write(cpu, addr, data);
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

void cpu_set_read_write(CPU *cpu, read_t read, write_t write)
{
    cpu->read = read != NULL ? read : cpu_read;
    cpu->write = write != NULL ? write : cpu_write;
}

bool cpu_get_debug(CPU *cpu)
{
    return cpu->debug;
}
void cpu_set_debug(CPU *cpu, bool debug, FILE *debug_file)
{
    cpu->debug = debug;
    cpu->debug_file = debug_file;
}
FILE *cpu_get_debug_file(CPU *cpu)
{
    return cpu->debug_file;
}

bool cpu_get_decimal_mode(CPU *cpu)
{
    return cpu->decimal_mode;
}
void cpu_set_decimal_mode(CPU *cpu, bool decimal_mode)
{
    cpu->decimal_mode = decimal_mode;
}

bool cpu_trap(CPU *cpu, uint16_t addr, handler_t handler)
{
    if (cpu == NULL || handler == NULL)
        return false;

    trap_t trap = {addr, handler};
    return add_trap(&trap);
}

uint16_t fetch_address(CPU *cpu, addr_mode_t mode)
{
    uint16_t addr;
    uint8_t low_byte, high_byte;

    switch (mode)
    {
    case Implied:
        return 0; // Not used for implied addressing

    case Immediate:
        return cpu_get_pc(cpu) + 1;

    case ZeroPage:
        return cpu_read_byte(cpu, cpu_get_pc(cpu) + 1);

    case ZeroPageX:
        return (cpu_read_byte(cpu, cpu_get_pc(cpu) + 1) + cpu->X) & 0xFF;

    case ZeroPageY:
        return (cpu_read_byte(cpu, cpu_get_pc(cpu) + 1) + cpu->Y) & 0xFF;

    case Absolute:
        low_byte = cpu_read_byte(cpu, cpu_get_pc(cpu) + 1);
        high_byte = cpu_read_byte(cpu, cpu_get_pc(cpu) + 2);
        return (high_byte << 8) | low_byte;

    case AbsoluteX:
        low_byte = cpu_read_byte(cpu, cpu_get_pc(cpu) + 1);
        high_byte = cpu_read_byte(cpu, cpu_get_pc(cpu) + 2);
        addr = (high_byte << 8) | low_byte;
        return addr + cpu->X;

    case AbsoluteY:
        low_byte = cpu_read_byte(cpu, cpu_get_pc(cpu) + 1);
        high_byte = cpu_read_byte(cpu, cpu_get_pc(cpu) + 2);
        addr = (high_byte << 8) | low_byte;
        return addr + cpu->Y;

    case Indirect:
        low_byte = cpu_read_byte(cpu, cpu_get_pc(cpu) + 1);
        high_byte = cpu_read_byte(cpu, cpu_get_pc(cpu) + 2);
        addr = (high_byte << 8) | low_byte;

        // 6502 bug: if page boundary crossed, high byte wraps
        if (low_byte == 0xFF)
            return cpu_read_word_zp(cpu, addr);
        else
            return cpu_read_word(cpu, addr);

    case IndexedIndirect:
    {
        uint8_t ptr = cpu_read_byte(cpu, cpu_get_pc(cpu) + 1) + cpu->X;
        low_byte = cpu_read_byte(cpu, ptr);
        high_byte = cpu_read_byte(cpu, (ptr + 1) & 0xFF);
        return (high_byte << 8) | low_byte;
    }

    case IndirectIndexed:
    {
        uint8_t ptr = cpu_read_byte(cpu, cpu_get_pc(cpu) + 1);
        low_byte = cpu_read_byte(cpu, ptr);
        high_byte = cpu_read_byte(cpu, (ptr + 1) & 0xFF);
        addr = (high_byte << 8) | low_byte;
        return addr + cpu->Y;
    }

    case Relative:
        return cpu_get_pc(cpu) + 1;

    default:
        return 0;
    }
}

uint8_t fetch_operand(CPU *cpu, addr_mode_t mode)
{
    uint16_t addr = fetch_address(cpu, mode);
    return cpu_read_byte(cpu, addr);
}

const instruction_t *fetch_instruction(CPU *cpu)
{
    uint8_t opcode = cpu_read_byte(cpu, cpu_get_pc(cpu));
    return &instructions[opcode];
}

/// triggers a NMI IRQ in the processor
/// this is very similar to the BRK instruction
void cpu_nmi(CPU *cpu)
{
    printf("*** NMI $%04X #$%04X\n", NMI, cpu_read_word(cpu, NMI));

    cpu_push16(cpu, cpu_get_pc(cpu));
    cpu_push(cpu, cpu->P & ~FLAG_BREAK);

    set_flag_interrupt(cpu, true);

    cpu_set_pc(cpu, cpu_read_word(cpu, NMI));
}

/// triggers a normal IRQ
/// this is very similar to the BRK instruction
void cpu_irq(CPU *cpu)
{
    if (has_interrupt(cpu))
        return;

    printf("*** IRQ $%04X #$%04X\n", IRQ, cpu_read_word(cpu, IRQ));

    cpu_push16(cpu, cpu_get_pc(cpu));
    cpu_push(cpu, cpu->P & ~FLAG_BREAK);

    set_flag_interrupt(cpu, true);

    cpu_set_pc(cpu, cpu_read_word(cpu, IRQ));
}

bool cpu_interrupts(CPU *cpu)
{
    if (cpu->nmi)
    {
        cpu->nmi = false;
        cpu_nmi(cpu);
        return true;
    }
    else if (cpu->irq && !has_interrupt(cpu))
    {
        cpu->irq = false;
        cpu_irq(cpu);
        return true;
    }
    return false;
}

uint8_t cpu_step(CPU *cpu)
{
    if (cpu_interrupts(cpu))
    {
        return 7; // IRQ/NMI interrupt handling takes 7 cycles
    }

    uint16_t pc = cpu_get_pc(cpu);

    handler_t handler = find_trap(pc);
    if (handler != NULL)
        handler(cpu);

    uint8_t opcode = cpu_read_byte(cpu, pc);
    const instruction_t *instruction = &instructions[opcode];

    if (cpu->debug)
        dump_step(cpu, instruction);

    // uint16_t pc = cpu_get_pc(cpu);
    instruction->execute(cpu);

    return instruction->cycles;
}
