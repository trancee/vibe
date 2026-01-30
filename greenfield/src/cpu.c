/*
 * C64 Emulator - CPU Core Implementation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cpu.h"
#include "mos6510.h"

/* Default memory read/write (internal memory array) */
static uint8_t default_read(void *context, uint16_t addr) {
    CPU *cpu = (CPU *)context;
    return cpu->memory[addr];
}

static void default_write(void *context, uint16_t addr, uint8_t data) {
    CPU *cpu = (CPU *)context;
    cpu->memory[addr] = data;
}

void cpu_init(CPU *cpu)
{
    memset(cpu, 0, sizeof(CPU));
    
    cpu->P = FLAG_RESERVED | FLAG_INTERRUPT;
    cpu->SP = 0xFF;
    cpu->decimal_mode = true;
    
    cpu->read = default_read;
    cpu->write = default_write;
    cpu->context = cpu;
}

void cpu_reset(CPU *cpu)
{
    cpu->PC = cpu_read_word(cpu, RESET_VECTOR);
    cpu->A = 0x00;
    cpu->X = 0x00;
    cpu->Y = 0x00;
    cpu->P = FLAG_RESERVED | FLAG_INTERRUPT;
    cpu->SP = 0xFD;  /* Reset pulls SP down by 3 */
    
    cpu->nmi_pending = false;
    cpu->irq_pending = false;
    cpu->nmi_edge = false;
    cpu->cycle_count = 0;
    cpu->extra_cycles = 0;
}

void cpu_reset_at(CPU *cpu, uint16_t addr)
{
    cpu->A = 0x00;
    cpu->X = 0x00;
    cpu->Y = 0x00;
    cpu->P = FLAG_RESERVED | FLAG_INTERRUPT;
    cpu->SP = 0xFF;
    cpu->PC = addr;
    
    cpu->nmi_pending = false;
    cpu->irq_pending = false;
    cpu->nmi_edge = false;
    cpu->cycle_count = 0;
    cpu->extra_cycles = 0;
}

uint16_t cpu_get_pc(CPU *cpu)
{
    return cpu->PC;
}

void cpu_set_pc(CPU *cpu, uint16_t addr)
{
    cpu->PC = addr;
}

uint8_t cpu_read(CPU *cpu, uint16_t addr)
{
    return cpu->read(cpu->context, addr);
}

uint16_t cpu_read_word(CPU *cpu, uint16_t addr)
{
    return cpu_read(cpu, addr) | (cpu_read(cpu, addr + 1) << 8);
}

uint16_t cpu_read_word_zp(CPU *cpu, uint8_t addr)
{
    /* Zero page wrap-around: addr+1 stays in zero page */
    return cpu_read(cpu, addr) | (cpu_read(cpu, (addr + 1) & 0xFF) << 8);
}

void cpu_write(CPU *cpu, uint16_t addr, uint8_t data)
{
    cpu->write(cpu->context, addr, data);
}

void cpu_write_word(CPU *cpu, uint16_t addr, uint16_t data)
{
    cpu_write(cpu, addr, data & 0xFF);
    cpu_write(cpu, addr + 1, (data >> 8) & 0xFF);
}

/* Stack Operations */
void cpu_push(CPU *cpu, uint8_t value)
{
    cpu_write(cpu, 0x0100 | cpu->SP, value);
    cpu->SP--;
}

void cpu_push_word(CPU *cpu, uint16_t value)
{
    cpu_push(cpu, (value >> 8) & 0xFF);  /* High byte first */
    cpu_push(cpu, value & 0xFF);          /* Low byte second */
}

uint8_t cpu_pull(CPU *cpu)
{
    cpu->SP++;
    return cpu_read(cpu, 0x0100 | cpu->SP);
}

uint16_t cpu_pull_word(CPU *cpu)
{
    uint8_t lo = cpu_pull(cpu);
    uint8_t hi = cpu_pull(cpu);
    return lo | (hi << 8);
}

/* Interrupt Handling */
void cpu_trigger_nmi(CPU *cpu)
{
    if (!cpu->nmi_edge) {
        cpu->nmi_pending = true;
        cpu->nmi_edge = true;
    }
}

void cpu_trigger_irq(CPU *cpu)
{
    cpu->irq_pending = true;
}

void cpu_clear_irq(CPU *cpu)
{
    cpu->irq_pending = false;
}

void cpu_nmi(CPU *cpu)
{
    cpu_push_word(cpu, cpu->PC);
    cpu_push(cpu, (cpu->P | FLAG_RESERVED) & ~FLAG_BREAK);
    set_flag_interrupt(cpu, true);
    cpu->PC = cpu_read_word(cpu, NMI_VECTOR);
    cpu->nmi_pending = false;
    cpu->cycle_count += 7;
}

void cpu_irq(CPU *cpu)
{
    if (get_flag_interrupt(cpu)) return;
    
    cpu_push_word(cpu, cpu->PC);
    cpu_push(cpu, (cpu->P | FLAG_RESERVED) & ~FLAG_BREAK);
    set_flag_interrupt(cpu, true);
    cpu->PC = cpu_read_word(cpu, IRQ_VECTOR);
    cpu->irq_pending = false;
    cpu->cycle_count += 7;
}

/* Configuration */
void cpu_set_callbacks(CPU *cpu, read_fn read, write_fn write, void *context)
{
    cpu->read = read ? read : default_read;
    cpu->write = write ? write : default_write;
    cpu->context = context ? context : cpu;
}

void cpu_set_debug(CPU *cpu, bool debug, FILE *debug_file)
{
    cpu->debug = debug;
    cpu->debug_file = debug_file;
}

void cpu_set_decimal_mode(CPU *cpu, bool enabled)
{
    cpu->decimal_mode = enabled;
}

/* Trap handler - register a trap at a specific address */
bool cpu_trap(CPU *cpu, uint16_t addr, handler_t handler)
{
    if (cpu->trap_count >= MAX_TRAPS) {
        return false;
    }
    
    /* Check if trap already exists at this address */
    for (int i = 0; i < cpu->trap_count; i++) {
        if (cpu->traps[i].addr == addr) {
            cpu->traps[i].handler = (void (*)(void *))handler;
            cpu->traps[i].active = true;
            return true;
        }
    }
    
    /* Add new trap */
    cpu->traps[cpu->trap_count].addr = addr;
    cpu->traps[cpu->trap_count].handler = (void (*)(void *))handler;
    cpu->traps[cpu->trap_count].active = true;
    cpu->trap_count++;
    return true;
}

/* Check and execute trap at current PC (returns true if trap was found) */
static bool check_trap(CPU *cpu)
{
    for (int i = 0; i < cpu->trap_count; i++) {
        if (cpu->traps[i].active && cpu->traps[i].addr == cpu->PC) {
            cpu->traps[i].handler(cpu);
            return true;
        }
    }
    return false;
}

/* Fetch instruction at current PC */
const Instruction *fetch_instruction(CPU *cpu)
{
    return &instructions[cpu_read(cpu, cpu->PC)];
}

/* Fetch the effective address for an addressing mode */
uint16_t fetch_address(CPU *cpu, AddressMode mode)
{
    uint16_t addr;
    uint8_t lo, hi;
    
    switch (mode) {
        case ZeroPage:
            return cpu_read(cpu, cpu->PC + 1);
            
        case ZeroPageX:
            return (cpu_read(cpu, cpu->PC + 1) + cpu->X) & 0xFF;
            
        case ZeroPageY:
            return (cpu_read(cpu, cpu->PC + 1) + cpu->Y) & 0xFF;
            
        case Absolute:
            return cpu_read_word(cpu, cpu->PC + 1);
            
        case AbsoluteX:
            {
                uint16_t base = cpu_read_word(cpu, cpu->PC + 1);
                uint16_t final_addr = (base + cpu->X) & 0xFFFF;
                /* Check for page crossing - caller may use for penalty */
                if ((base & 0xFF00) != (final_addr & 0xFF00)) {
                    cpu->page_crossed = true;
                }
                return final_addr;
            }
            
        case AbsoluteY:
            {
                uint16_t base = cpu_read_word(cpu, cpu->PC + 1);
                uint16_t final_addr = (base + cpu->Y) & 0xFFFF;
                if ((base & 0xFF00) != (final_addr & 0xFF00)) {
                    cpu->page_crossed = true;
                }
                return final_addr;
            }
            
        case Indirect:
            addr = cpu_read_word(cpu, cpu->PC + 1);
            /* 6502 bug: wraps within page */
            lo = cpu_read(cpu, addr);
            hi = cpu_read(cpu, (addr & 0xFF00) | ((addr + 1) & 0x00FF));
            return lo | (hi << 8);
            
        case IndexedIndirect:
            addr = (cpu_read(cpu, cpu->PC + 1) + cpu->X) & 0xFF;
            return cpu_read_word_zp(cpu, addr);
            
        case IndirectIndexed:
            {
                uint16_t base = cpu_read_word_zp(cpu, cpu_read(cpu, cpu->PC + 1));
                uint16_t final_addr = (base + cpu->Y) & 0xFFFF;
                if ((base & 0xFF00) != (final_addr & 0xFF00)) {
                    cpu->page_crossed = true;
                }
                return final_addr;
            }
            
        case Relative:
            addr = cpu_read(cpu, cpu->PC + 1);
            if (addr & 0x80)
                return cpu->PC + 2 + addr - 256;
            else
                return cpu->PC + 2 + addr;
            
        default:
            return 0;
    }
}

/* Fetch the operand value for an addressing mode */
uint8_t fetch_operand(CPU *cpu, AddressMode mode)
{
    switch (mode) {
        case Immediate:
            return cpu_read(cpu, cpu->PC + 1);
            
        case Accumulator:
            return cpu->A;
            
        case Implied:
            return 0;
            
        default:
            return cpu_read(cpu, fetch_address(cpu, mode));
    }
}

/* Debug output */
static void dump_instruction(CPU *cpu, const Instruction *inst)
{
    if (!cpu->debug) return;
    
    FILE *out = cpu->debug_file ? cpu->debug_file : stdout;
    
    fprintf(out, "%04X  ", cpu->PC);
    
    for (int i = 0; i < 3; i++) {
        if (i < inst->size)
            fprintf(out, "%02X ", cpu_read(cpu, cpu->PC + i));
        else
            fprintf(out, "   ");
    }
    
    fprintf(out, "%c%s", inst->illegal ? '*' : ' ', inst->name);
    
    /* Print operand based on addressing mode */
    switch (inst->mode) {
        case Implied:
            fprintf(out, "%28s", "");
            break;
        case Accumulator:
            fprintf(out, " A%26s", "");
            break;
        case Immediate:
            fprintf(out, " #$%02X%23s", cpu_read(cpu, cpu->PC + 1), "");
            break;
        case ZeroPage:
            fprintf(out, " $%02X = %02X%19s",
                    cpu_read(cpu, cpu->PC + 1),
                    cpu_read(cpu, cpu_read(cpu, cpu->PC + 1)), "");
            break;
        case ZeroPageX:
            fprintf(out, " $%02X,X @ %02X = %02X%12s",
                    cpu_read(cpu, cpu->PC + 1),
                    fetch_address(cpu, ZeroPageX),
                    cpu_read(cpu, fetch_address(cpu, ZeroPageX)), "");
            break;
        case ZeroPageY:
            fprintf(out, " $%02X,Y @ %02X = %02X%12s",
                    cpu_read(cpu, cpu->PC + 1),
                    fetch_address(cpu, ZeroPageY),
                    cpu_read(cpu, fetch_address(cpu, ZeroPageY)), "");
            break;
        case Absolute:
            /* JMP and JSR don't read from the address, just jump to it */
            if (strcmp(inst->name, "JMP") == 0 || strcmp(inst->name, "JSR") == 0) {
                fprintf(out, " $%04X%22s",
                        cpu_read_word(cpu, cpu->PC + 1), "");
            } else {
                fprintf(out, " $%04X = %02X%17s",
                        cpu_read_word(cpu, cpu->PC + 1),
                        cpu_read(cpu, cpu_read_word(cpu, cpu->PC + 1)), "");
            }
            break;
        case AbsoluteX:
            fprintf(out, " $%04X,X @ %04X = %02X%8s",
                    cpu_read_word(cpu, cpu->PC + 1),
                    fetch_address(cpu, AbsoluteX),
                    cpu_read(cpu, fetch_address(cpu, AbsoluteX)), "");
            break;
        case AbsoluteY:
            fprintf(out, " $%04X,Y @ %04X = %02X%8s",
                    cpu_read_word(cpu, cpu->PC + 1),
                    fetch_address(cpu, AbsoluteY),
                    cpu_read(cpu, fetch_address(cpu, AbsoluteY)), "");
            break;
        case Indirect:
            fprintf(out, " ($%04X) = %04X%13s",
                    cpu_read_word(cpu, cpu->PC + 1),
                    fetch_address(cpu, Indirect), "");
            break;
        case IndexedIndirect:
            fprintf(out, " ($%02X,X) @ %02X = %04X = %02X%3s",
                    cpu_read(cpu, cpu->PC + 1),
                    (cpu_read(cpu, cpu->PC + 1) + cpu->X) & 0xFF,
                    fetch_address(cpu, IndexedIndirect),
                    cpu_read(cpu, fetch_address(cpu, IndexedIndirect)), "");
            break;
        case IndirectIndexed:
            fprintf(out, " ($%02X),Y = %04X @ %04X = %02X%1s",
                    cpu_read(cpu, cpu->PC + 1),
                    cpu_read_word_zp(cpu, cpu_read(cpu, cpu->PC + 1)),
                    fetch_address(cpu, IndirectIndexed),
                    cpu_read(cpu, fetch_address(cpu, IndirectIndexed)), "");
            break;
        case Relative:
            fprintf(out, " $%04X%22s", fetch_address(cpu, Relative), "");
            break;
    }
    
    fprintf(out, " A:%02X X:%02X Y:%02X P:%02X SP:%02X\n",
            cpu->A, cpu->X, cpu->Y, cpu->P, cpu->SP);
}

/* Execute one instruction and return cycles used */
uint8_t cpu_step(CPU *cpu)
{
    /* Check for interrupts first */
    if (cpu->nmi_pending) {
        cpu_nmi(cpu);
        return 7;
    }
    if (cpu->irq_pending && !get_flag_interrupt(cpu)) {
        cpu_irq(cpu);
        return 7;
    }
    
    /* Check for traps - handler may modify PC */
    check_trap(cpu);
    
    /* Reset extra cycles and page crossing flag before executing instruction */
    cpu->extra_cycles = 0;
    cpu->page_crossed = false;
    
    const Instruction *inst = fetch_instruction(cpu);
    
    if (cpu->debug) {
        dump_instruction(cpu, inst);
    }
    
    /* Execute the instruction */
    if (inst->execute) {
        inst->execute(cpu);
    }
    
    /* Add page crossing penalty for read instructions with indexed addressing
     * Read instructions: LDA, LDX, LDY, ADC, SBC, CMP, CPX, CPY, AND, ORA, EOR, BIT, NOP
     * Also some illegal opcodes: LAX, SBC (0xEB), etc.
     * Indexed modes: AbsoluteX, AbsoluteY, IndirectIndexed */
    if (cpu->page_crossed) {
        bool is_read_inst = (
            strcmp(inst->name, "LDA") == 0 ||
            strcmp(inst->name, "LDX") == 0 ||
            strcmp(inst->name, "LDY") == 0 ||
            strcmp(inst->name, "ADC") == 0 ||
            strcmp(inst->name, "SBC") == 0 ||
            strcmp(inst->name, "CMP") == 0 ||
            strcmp(inst->name, "CPX") == 0 ||
            strcmp(inst->name, "CPY") == 0 ||
            strcmp(inst->name, "AND") == 0 ||
            strcmp(inst->name, "ORA") == 0 ||
            strcmp(inst->name, "EOR") == 0 ||
            strcmp(inst->name, "BIT") == 0 ||
            strcmp(inst->name, "NOP") == 0 ||
            strcmp(inst->name, "LAX") == 0 ||
            strcmp(inst->name, "LAS") == 0
        );
        if (is_read_inst) {
            cpu->extra_cycles++;
        }
    }
    
    uint8_t cycles = inst->cycles + cpu->extra_cycles;
    cpu->cycle_count += cycles;
    return cycles;
}
