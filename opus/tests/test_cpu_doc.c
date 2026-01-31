#include "test_framework.h"
#include "../src/c64.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define OPCODE_PC      0x0800
#define ZP_ADDR        0x40
#define ZP_PTR_ADDR    0x60
#define ABS_ADDR       0x3000
#define ABS_IDX_ADDR   0x3100
#define ABS_JMP_ADDR   0x3800
#define INDIRECT_VEC   0x3A00
#define STACK_BASE     0x0100
#define PTR_TARGET     0x4200
#define PTR2_TARGET    0x4300
#define BRK_VECTOR     0x4800

#define DEFAULT_A 0x33
#define DEFAULT_X 0x11
#define DEFAULT_Y 0x22

static C64System sys;

typedef enum {
    ADDR_IMPLIED,
    ADDR_IMMEDIATE,
    ADDR_RELATIVE,
    ADDR_ZEROPAGE,
    ADDR_ZEROPAGE_X,
    ADDR_ZEROPAGE_Y,
    ADDR_ABSOLUTE,
    ADDR_ABSOLUTE_X,
    ADDR_ABSOLUTE_Y,
    ADDR_INDIRECT,
    ADDR_INDEXED_INDIRECT,
    ADDR_INDIRECT_INDEXED,
    ADDR_ACCUMULATOR,
    ADDR_SKIP
} AddressingMode;

typedef enum {
    OP_ADC,
    OP_AND,
    OP_ASL,
    OP_BCC,
    OP_BCS,
    OP_BEQ,
    OP_BIT,
    OP_BMI,
    OP_BNE,
    OP_BPL,
    OP_BRK,
    OP_BVC,
    OP_BVS,
    OP_CLC,
    OP_CLD,
    OP_CLI,
    OP_CLV,
    OP_CMP,
    OP_CPX,
    OP_CPY,
    OP_DEC,
    OP_DEX,
    OP_DEY,
    OP_EOR,
    OP_INC,
    OP_INX,
    OP_INY,
    OP_JMP,
    OP_JSR,
    OP_LDA,
    OP_LDX,
    OP_LDY,
    OP_LSR,
    OP_NOP,
    OP_ORA,
    OP_PHA,
    OP_PHP,
    OP_PLA,
    OP_PLP,
    OP_ROL,
    OP_ROR,
    OP_RTI,
    OP_RTS,
    OP_SBC,
    OP_SEC,
    OP_SED,
    OP_SEI,
    OP_STA,
    OP_STX,
    OP_STY,
    OP_TAX,
    OP_TAY,
    OP_TSX,
    OP_TXA,
    OP_TXS,
    OP_TYA,
    OP_UNSUPPORTED
} OperationKind;

typedef struct {
    u8 opcode;
    char token[32];
    char mnemonic[8];
    AddressingMode mode;
    OperationKind op;
    bool undocumented;
    bool unusual;
    bool jam;
    bool uses_y_index;
    bool uses_indirect_suffix;
} OpcodeEntry;

static OpcodeEntry g_entries[256];
static bool g_table_loaded = false;

static const int column_bases[8] = {0x00, 0x20, 0x40, 0x60, 0x80, 0xA0, 0xC0, 0xE0};

typedef enum {
    ROW_IMPL_IMMED,
    ROW_INDIRECT_X,
    ROW_IMMEDIATE,
    ROW_UNKNOWN,
    ROW_ZEROPAGE,
    ROW_ZEROPAGE_X,
    ROW_IMPLIED,
    ROW_ACCUM_IMPL,
    ROW_ABSOLUTE,
    ROW_RELATIVE,
    ROW_INDIRECT_Y,
    ROW_ABSOLUTE_Y,
    ROW_ABSOLUTE_X
} RowMode;

typedef struct {
    u16 effective_addr;
    u16 pointer_addr;
    u16 branch_target;
    u16 target_addr;
    u8 operand_value;
    u8 bytes;
} OperandContext;

static void trim(char *s) {
    if (!s) return;
    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) {
        s[--len] = '\0';
    }
    size_t start = 0;
    while (s[start] && isspace((unsigned char)s[start])) start++;
    if (start > 0) memmove(s, s + start, len - start + 1);
}

static RowMode parse_row_mode(const char *mode) {
    if (strcmp(mode, "Impl/immed") == 0) return ROW_IMPL_IMMED;
    if (strcmp(mode, "(indir,x)") == 0) return ROW_INDIRECT_X;
    if (strcmp(mode, "Immediate") == 0) return ROW_IMMEDIATE;
    if (strcmp(mode, "? /immed") == 0) return ROW_IMMEDIATE;
    if (strcmp(mode, "Zeropage") == 0) return ROW_ZEROPAGE;
    if (strcmp(mode, "Zeropage,x") == 0) return ROW_ZEROPAGE_X;
    if (strcmp(mode, "Implied") == 0) return ROW_IMPLIED;
    if (strcmp(mode, "Accu/impl") == 0) return ROW_ACCUM_IMPL;
    if (strcmp(mode, "Absolute") == 0) return ROW_ABSOLUTE;
    if (strcmp(mode, "Relative") == 0) return ROW_RELATIVE;
    if (strcmp(mode, "(indir),y") == 0) return ROW_INDIRECT_Y;
    if (strcmp(mode, "Absolute,y") == 0) return ROW_ABSOLUTE_Y;
    if (strcmp(mode, "Absolute,x") == 0) return ROW_ABSOLUTE_X;
    return ROW_UNKNOWN;
}

static void parse_token(const char *src, OpcodeEntry *entry) {
    size_t len = strlen(src);
    size_t i = 0;
    size_t mlen = 0;
    while (i < len && isupper((unsigned char)src[i]) && mlen < sizeof(entry->mnemonic) - 1) {
        entry->mnemonic[mlen++] = src[i++];
    }
    entry->mnemonic[mlen] = '\0';
    const char *suffix = src + i;
    if (strstr(suffix, "y)") != NULL) entry->uses_y_index = true;
    if (strstr(suffix, "()") != NULL) entry->uses_indirect_suffix = true;
    if (strstr(suffix, "**") != NULL) entry->unusual = true;
    if (strchr(suffix, '*') != NULL) entry->undocumented = true;
    if (strstr(suffix, "*t") != NULL) entry->jam = true;
    if (entry->mnemonic[0] == '\0' && strchr(suffix, 't')) {
        strcpy(entry->mnemonic, "JAM");
        entry->jam = true;
    }
    if (entry->mnemonic[0] == '\0') {
        strncpy(entry->mnemonic, src, sizeof(entry->mnemonic) - 1);
        entry->mnemonic[sizeof(entry->mnemonic) - 1] = '\0';
    }
}

static OperationKind deduce_operation(const char *mnemonic, bool undocumented) {
    if (undocumented) return OP_UNSUPPORTED;
    if (strcmp(mnemonic, "ADC") == 0) return OP_ADC;
    if (strcmp(mnemonic, "AND") == 0) return OP_AND;
    if (strcmp(mnemonic, "ASL") == 0) return OP_ASL;
    if (strcmp(mnemonic, "BCC") == 0) return OP_BCC;
    if (strcmp(mnemonic, "BCS") == 0) return OP_BCS;
    if (strcmp(mnemonic, "BEQ") == 0) return OP_BEQ;
    if (strcmp(mnemonic, "BIT") == 0) return OP_BIT;
    if (strcmp(mnemonic, "BMI") == 0) return OP_BMI;
    if (strcmp(mnemonic, "BNE") == 0) return OP_BNE;
    if (strcmp(mnemonic, "BPL") == 0) return OP_BPL;
    if (strcmp(mnemonic, "BRK") == 0) return OP_BRK;
    if (strcmp(mnemonic, "BVC") == 0) return OP_BVC;
    if (strcmp(mnemonic, "BVS") == 0) return OP_BVS;
    if (strcmp(mnemonic, "CLC") == 0) return OP_CLC;
    if (strcmp(mnemonic, "CLD") == 0) return OP_CLD;
    if (strcmp(mnemonic, "CLI") == 0) return OP_CLI;
    if (strcmp(mnemonic, "CLV") == 0) return OP_CLV;
    if (strcmp(mnemonic, "CMP") == 0) return OP_CMP;
    if (strcmp(mnemonic, "CPX") == 0) return OP_CPX;
    if (strcmp(mnemonic, "CPY") == 0) return OP_CPY;
    if (strcmp(mnemonic, "DEC") == 0) return OP_DEC;
    if (strcmp(mnemonic, "DEX") == 0) return OP_DEX;
    if (strcmp(mnemonic, "DEY") == 0) return OP_DEY;
    if (strcmp(mnemonic, "EOR") == 0) return OP_EOR;
    if (strcmp(mnemonic, "INC") == 0) return OP_INC;
    if (strcmp(mnemonic, "INX") == 0) return OP_INX;
    if (strcmp(mnemonic, "INY") == 0) return OP_INY;
    if (strcmp(mnemonic, "JMP") == 0) return OP_JMP;
    if (strcmp(mnemonic, "JSR") == 0) return OP_JSR;
    if (strcmp(mnemonic, "LDA") == 0) return OP_LDA;
    if (strcmp(mnemonic, "LDX") == 0) return OP_LDX;
    if (strcmp(mnemonic, "LDY") == 0) return OP_LDY;
    if (strcmp(mnemonic, "LSR") == 0) return OP_LSR;
    if (strcmp(mnemonic, "NOP") == 0) return OP_NOP;
    if (strcmp(mnemonic, "ORA") == 0) return OP_ORA;
    if (strcmp(mnemonic, "PHA") == 0) return OP_PHA;
    if (strcmp(mnemonic, "PHP") == 0) return OP_PHP;
    if (strcmp(mnemonic, "PLA") == 0) return OP_PLA;
    if (strcmp(mnemonic, "PLP") == 0) return OP_PLP;
    if (strcmp(mnemonic, "ROL") == 0) return OP_ROL;
    if (strcmp(mnemonic, "ROR") == 0) return OP_ROR;
    if (strcmp(mnemonic, "RTI") == 0) return OP_RTI;
    if (strcmp(mnemonic, "RTS") == 0) return OP_RTS;
    if (strcmp(mnemonic, "SBC") == 0) return OP_SBC;
    if (strcmp(mnemonic, "SEC") == 0) return OP_SEC;
    if (strcmp(mnemonic, "SED") == 0) return OP_SED;
    if (strcmp(mnemonic, "SEI") == 0) return OP_SEI;
    if (strcmp(mnemonic, "STA") == 0) return OP_STA;
    if (strcmp(mnemonic, "STX") == 0) return OP_STX;
    if (strcmp(mnemonic, "STY") == 0) return OP_STY;
    if (strcmp(mnemonic, "TAX") == 0) return OP_TAX;
    if (strcmp(mnemonic, "TAY") == 0) return OP_TAY;
    if (strcmp(mnemonic, "TSX") == 0) return OP_TSX;
    if (strcmp(mnemonic, "TXA") == 0) return OP_TXA;
    if (strcmp(mnemonic, "TXS") == 0) return OP_TXS;
    if (strcmp(mnemonic, "TYA") == 0) return OP_TYA;
    return OP_UNSUPPORTED;
}

static AddressingMode deduce_mode(RowMode row_mode, const OpcodeEntry *entry, int row) {
    switch (row_mode) {
        case ROW_IMPL_IMMED:
            if (strcmp(entry->mnemonic, "JSR") == 0) return ADDR_ABSOLUTE;
            if (strcmp(entry->mnemonic, "LDY") == 0 || strcmp(entry->mnemonic, "CPY") == 0 || strcmp(entry->mnemonic, "CPX") == 0)
                return ADDR_IMMEDIATE;
            return ADDR_IMPLIED;
        case ROW_INDIRECT_X:
            return ADDR_INDEXED_INDIRECT;
        case ROW_IMMEDIATE:
            return ADDR_IMMEDIATE;
        case ROW_ZEROPAGE:
            return ADDR_ZEROPAGE;
        case ROW_ZEROPAGE_X:
            return entry->uses_y_index ? ADDR_ZEROPAGE_Y : ADDR_ZEROPAGE_X;
        case ROW_IMPLIED:
            return ADDR_IMPLIED;
        case ROW_ACCUM_IMPL:
            if (strcmp(entry->mnemonic, "ASL") == 0 || strcmp(entry->mnemonic, "ROL") == 0 ||
                strcmp(entry->mnemonic, "LSR") == 0 || strcmp(entry->mnemonic, "ROR") == 0)
                return ADDR_ACCUMULATOR;
            return ADDR_IMPLIED;
        case ROW_ABSOLUTE:
            if (entry->uses_indirect_suffix) return ADDR_INDIRECT;
            return ADDR_ABSOLUTE;
        case ROW_RELATIVE:
            return ADDR_RELATIVE;
        case ROW_INDIRECT_Y:
            return ADDR_INDIRECT_INDEXED;
        case ROW_ABSOLUTE_Y:
            return ADDR_ABSOLUTE_Y;
        case ROW_ABSOLUTE_X:
            return ADDR_ABSOLUTE_X;
        default:
            (void)row;
            return ADDR_SKIP;
    }
}

static bool parse_row(char *line, int *row_value, char tokens[8][32], char *mode_buf, size_t mode_buf_size) {
    char *saveptr = NULL;
    char *token = strtok_r(line, " \t\r\n", &saveptr);
    if (!token || token[0] != '+') return false;
    *row_value = (int)strtol(token + 1, NULL, 16);
    int col = 0;
    while ((token = strtok_r(NULL, " \t\r\n", &saveptr)) != NULL) {
        if (strcmp(token, "y)") == 0 || strcmp(token, "()") == 0) {
            if (col > 0) {
                strncat(tokens[col - 1], token, sizeof(tokens[col - 1]) - strlen(tokens[col - 1]) - 1);
            }
            continue;
        }
        if (col < 8) {
            strncpy(tokens[col], token, sizeof(tokens[col]) - 1);
            tokens[col][sizeof(tokens[col]) - 1] = '\0';
            col++;
        } else {
            if (mode_buf[0]) strncat(mode_buf, " ", mode_buf_size - strlen(mode_buf) - 1);
            strncat(mode_buf, token, mode_buf_size - strlen(mode_buf) - 1);
        }
    }
    return col == 8 && mode_buf[0] != '\0';
}

static bool load_opcode_table(void) {
    if (g_table_loaded) return true;
    FILE *f = fopen("docs/65xx_ins.txt", "r");
    if (!f) {
        printf("    Unable to open docs/65xx_ins.txt\n");
        return false;
    }
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] != '+') continue;
        char tokens[8][32] = {{0}};
        char mode_buf[64] = {0};
        int row = 0;
        char line_copy[256];
        strncpy(line_copy, line, sizeof(line_copy) - 1);
        line_copy[sizeof(line_copy) - 1] = '\0';
        if (!parse_row(line_copy, &row, tokens, mode_buf, sizeof(mode_buf))) {
            continue;
        }
        trim(mode_buf);
        RowMode row_mode = parse_row_mode(mode_buf);
        for (int col = 0; col < 8; ++col) {
            if (tokens[col][0] == '\0') continue;
            OpcodeEntry entry = {0};
            entry.opcode = (u8)(column_bases[col] | row);
            strncpy(entry.token, tokens[col], sizeof(entry.token) - 1);
            parse_token(tokens[col], &entry);
            entry.mode = deduce_mode(row_mode, &entry, row);
            entry.op = deduce_operation(entry.mnemonic, entry.undocumented);
            g_entries[entry.opcode] = entry;
        }
    }
    fclose(f);
    g_table_loaded = true;
    return true;
}

static void opcode_setup(void) {
    mem_reset(&sys.mem);
    cpu_reset(&sys.cpu);
    sys.cpu.PC = OPCODE_PC;
    sys.cpu.A = DEFAULT_A;
    sys.cpu.X = DEFAULT_X;
    sys.cpu.Y = DEFAULT_Y;
    sys.cpu.SP = 0xFD;
    sys.cpu.P = FLAG_U;
}

static bool is_store(OperationKind op) {
    return op == OP_STA || op == OP_STX || op == OP_STY;
}

static u8 default_operand(OperationKind op) {
    switch (op) {
        case OP_ADC: return 0x22;
        case OP_AND: return 0x0F;
        case OP_ORA: return 0x0F;
        case OP_EOR: return 0x0F;
        case OP_LDA: return 0x7B;
        case OP_LDX: return 0x5D;
        case OP_LDY: return 0x4C;
        case OP_CMP: return 0x20;
        case OP_CPX: return 0x10;
        case OP_CPY: return 0x10;
        case OP_BIT: return 0xC0;
        case OP_INC: return 0x7F;
        case OP_DEC: return 0x00;
        case OP_ASL: return 0x81;
        case OP_LSR: return 0x03;
        case OP_ROL: return 0x80;
        case OP_ROR: return 0x01;
        case OP_SBC: return 0x11;
        default:     return 0x3C;
    }
}

static void init_state_for_operation(OperationKind op) {
    switch (op) {
        case OP_ADC:
            sys.cpu.A = 0x14;
            sys.cpu.P |= FLAG_C;
            break;
        case OP_SBC:
            sys.cpu.A = 0x40;
            break;
        case OP_AND:
            sys.cpu.A = 0xF0;
            break;
        case OP_ORA:
            sys.cpu.A = 0xAA;
            break;
        case OP_EOR:
            sys.cpu.A = 0xF0;
            break;
        case OP_BIT:
            sys.cpu.A = 0x02;
            break;
        case OP_CMP:
            sys.cpu.A = 0x50;
            break;
        case OP_CPX:
            sys.cpu.X = 0x41;
            break;
        case OP_CPY:
            sys.cpu.Y = 0x41;
            break;
        case OP_DEX:
            sys.cpu.X = 0x01;
            break;
        case OP_DEY:
            sys.cpu.Y = 0x01;
            break;
        case OP_INX:
            sys.cpu.X = 0x7F;
            break;
        case OP_INY:
            sys.cpu.Y = 0x7F;
            break;
        case OP_ASL:
        case OP_ROL:
            sys.cpu.P |= FLAG_C;
            break;
        case OP_ROR:
            sys.cpu.P &= ~FLAG_C;
            break;
        case OP_CLC:
            sys.cpu.P |= FLAG_C;
            break;
        case OP_SEC:
            sys.cpu.P &= ~FLAG_C;
            break;
        case OP_CLD:
            sys.cpu.P |= FLAG_D;
            break;
        case OP_SED:
            sys.cpu.P &= ~FLAG_D;
            break;
        case OP_CLI:
            sys.cpu.P |= FLAG_I;
            break;
        case OP_SEI:
            sys.cpu.P &= ~FLAG_I;
            break;
        case OP_CLV:
            sys.cpu.P |= FLAG_V;
            break;
        case OP_BCC:
            sys.cpu.P &= ~FLAG_C;
            break;
        case OP_BCS:
            sys.cpu.P |= FLAG_C;
            break;
        case OP_BNE:
            sys.cpu.P &= ~FLAG_Z;
            break;
        case OP_BEQ:
            sys.cpu.P |= FLAG_Z;
            break;
        case OP_BPL:
            sys.cpu.P &= ~FLAG_N;
            break;
        case OP_BMI:
            sys.cpu.P |= FLAG_N;
            break;
        case OP_BVC:
            sys.cpu.P &= ~FLAG_V;
            break;
        case OP_BVS:
            sys.cpu.P |= FLAG_V;
            break;
        default:
            break;
    }
    sys.cpu.P |= FLAG_U;
}

static void encode_operand(const OpcodeEntry *entry, OperandContext *ctx, u8 value) {
    ctx->bytes = 1;
    ctx->operand_value = value;
    const bool store = is_store(entry->op);
    switch (entry->mode) {
        case ADDR_IMMEDIATE:
            mem_write_raw(&sys.mem, OPCODE_PC + 1, value);
            ctx->bytes = 2;
            break;
        case ADDR_ZEROPAGE:
            mem_write_raw(&sys.mem, OPCODE_PC + 1, ZP_ADDR);
            if (!store) mem_write_raw(&sys.mem, ZP_ADDR, value);
            ctx->effective_addr = ZP_ADDR;
            ctx->bytes = 2;
            break;
        case ADDR_ZEROPAGE_X:
        case ADDR_ZEROPAGE_Y: {
            u8 base = 0x20;
            mem_write_raw(&sys.mem, OPCODE_PC + 1, base);
            u8 index = (entry->mode == ADDR_ZEROPAGE_Y) ? sys.cpu.Y : sys.cpu.X;
            u8 addr = (base + index) & 0xFF;
            if (!store) mem_write_raw(&sys.mem, addr, value);
            ctx->effective_addr = addr;
            ctx->bytes = 2;
            break;
        }
        case ADDR_ABSOLUTE:
            mem_write_raw(&sys.mem, OPCODE_PC + 1, ABS_ADDR & 0xFF);
            mem_write_raw(&sys.mem, OPCODE_PC + 2, ABS_ADDR >> 8);
            ctx->effective_addr = ABS_ADDR;
            if (!store) mem_write_raw(&sys.mem, ABS_ADDR, value);
            if (entry->op == OP_JSR || entry->op == OP_JMP)
                ctx->target_addr = ABS_ADDR;
            ctx->bytes = 3;
            break;
        case ADDR_ABSOLUTE_X:
        case ADDR_ABSOLUTE_Y: {
            u16 base = ABS_IDX_ADDR;
            mem_write_raw(&sys.mem, OPCODE_PC + 1, base & 0xFF);
            mem_write_raw(&sys.mem, OPCODE_PC + 2, base >> 8);
            u8 index = (entry->mode == ADDR_ABSOLUTE_Y) ? sys.cpu.Y : sys.cpu.X;
            ctx->effective_addr = base + index;
            if (!store) mem_write_raw(&sys.mem, ctx->effective_addr, value);
            if (entry->op == OP_JSR || entry->op == OP_JMP)
                ctx->target_addr = ctx->effective_addr;
            ctx->bytes = 3;
            break;
        }
        case ADDR_INDEXED_INDIRECT: {
            mem_write_raw(&sys.mem, OPCODE_PC + 1, ZP_PTR_ADDR);
            u8 ptr = (ZP_PTR_ADDR + sys.cpu.X) & 0xFF;
            ctx->effective_addr = PTR_TARGET;
            mem_write_raw(&sys.mem, ptr, ctx->effective_addr & 0xFF);
            mem_write_raw(&sys.mem, (ptr + 1) & 0xFF, ctx->effective_addr >> 8);
            if (!store) mem_write_raw(&sys.mem, ctx->effective_addr, value);
            ctx->bytes = 2;
            break;
        }
        case ADDR_INDIRECT_INDEXED: {
            mem_write_raw(&sys.mem, OPCODE_PC + 1, ZP_PTR_ADDR);
            u16 base = PTR2_TARGET - sys.cpu.Y;
            mem_write_raw(&sys.mem, ZP_PTR_ADDR, base & 0xFF);
            mem_write_raw(&sys.mem, (ZP_PTR_ADDR + 1) & 0xFF, base >> 8);
            ctx->effective_addr = base + sys.cpu.Y;
            if (!store) mem_write_raw(&sys.mem, ctx->effective_addr, value);
            ctx->bytes = 2;
            break;
        }
        case ADDR_INDIRECT:
            mem_write_raw(&sys.mem, OPCODE_PC + 1, INDIRECT_VEC & 0xFF);
            mem_write_raw(&sys.mem, OPCODE_PC + 2, INDIRECT_VEC >> 8);
            ctx->target_addr = ABS_JMP_ADDR;
            mem_write_raw(&sys.mem, INDIRECT_VEC, ctx->target_addr & 0xFF);
            mem_write_raw(&sys.mem, (INDIRECT_VEC & 0xFF00) | ((INDIRECT_VEC + 1) & 0xFF), ctx->target_addr >> 8);
            ctx->bytes = 3;
            break;
        case ADDR_RELATIVE:
            mem_write_raw(&sys.mem, OPCODE_PC + 1, 0x04);
            ctx->branch_target = OPCODE_PC + 2 + 0x04;
            ctx->bytes = 2;
            break;
        case ADDR_ACCUMULATOR:
            sys.cpu.A = value;
            ctx->bytes = 1;
            break;
        case ADDR_IMPLIED:
            ctx->bytes = 1;
            break;
        default:
            ctx->bytes = 1;
            break;
    }
    mem_write_raw(&sys.mem, OPCODE_PC, entry->opcode);
}

static void prepare_stack_for_rts(OperandContext *ctx) {
    sys.cpu.SP = 0xFB;
    u16 ret = OPCODE_PC + 1;
    mem_write_raw(&sys.mem, STACK_BASE + sys.cpu.SP + 1, (ret - 1) & 0xFF);
    mem_write_raw(&sys.mem, STACK_BASE + sys.cpu.SP + 2, (ret - 1) >> 8);
    ctx->target_addr = ret;
}

static void prepare_stack_for_rti(OperandContext *ctx) {
    sys.cpu.SP = 0xFA;
    u8 status = FLAG_C | FLAG_N | FLAG_U;
    mem_write_raw(&sys.mem, STACK_BASE + sys.cpu.SP + 1, status);
    mem_write_raw(&sys.mem, STACK_BASE + sys.cpu.SP + 2, 0x34);
    mem_write_raw(&sys.mem, STACK_BASE + sys.cpu.SP + 3, 0x12);
    ctx->target_addr = 0x1234;
}

static void prepare_stack_for_pla(u8 value) {
    sys.cpu.SP = 0xFC;
    mem_write_raw(&sys.mem, STACK_BASE + sys.cpu.SP + 1, value);
}

static void prepare_stack_for_plp(u8 value) {
    sys.cpu.SP = 0xFC;
    mem_write_raw(&sys.mem, STACK_BASE + sys.cpu.SP + 1, value | FLAG_U);
}

static void prepare_brk_vector(OperandContext *ctx) {
    mem_write_raw(&sys.mem, 0xFFFE, BRK_VECTOR & 0xFF);
    mem_write_raw(&sys.mem, 0xFFFF, BRK_VECTOR >> 8);
    ctx->target_addr = BRK_VECTOR;
}

static int cycles_memory_read(AddressingMode mode) {
    switch (mode) {
        case ADDR_IMMEDIATE: return 2;
        case ADDR_ZEROPAGE: return 3;
        case ADDR_ZEROPAGE_X:
        case ADDR_ZEROPAGE_Y: return 4;
        case ADDR_ABSOLUTE: return 4;
        case ADDR_ABSOLUTE_X:
        case ADDR_ABSOLUTE_Y: return 4;
        case ADDR_INDEXED_INDIRECT: return 6;
        case ADDR_INDIRECT_INDEXED: return 5;
        case ADDR_ACCUMULATOR: return 2;
        default: return 0;
    }
}

static int cycles_store(AddressingMode mode) {
    switch (mode) {
        case ADDR_ZEROPAGE: return 3;
        case ADDR_ZEROPAGE_X:
        case ADDR_ZEROPAGE_Y: return 4;
        case ADDR_ABSOLUTE: return 4;
        case ADDR_ABSOLUTE_X:
        case ADDR_ABSOLUTE_Y: return 5;
        case ADDR_INDEXED_INDIRECT: return 6;
        case ADDR_INDIRECT_INDEXED: return 6;
        default: return 0;
    }
}

static int cycles_read_modify(AddressingMode mode) {
    switch (mode) {
        case ADDR_ZEROPAGE: return 5;
        case ADDR_ZEROPAGE_X:
        case ADDR_ZEROPAGE_Y: return 6;
        case ADDR_ABSOLUTE: return 6;
        case ADDR_ABSOLUTE_X: return 7;
        case ADDR_ACCUMULATOR: return 2;
        default: return 0;
    }
}

static int expected_cycles(const OpcodeEntry *entry) {
    switch (entry->op) {
        case OP_LDA:
        case OP_LDX:
        case OP_LDY:
        case OP_ORA:
        case OP_AND:
        case OP_EOR:
        case OP_ADC:
        case OP_SBC:
        case OP_CMP:
        case OP_CPX:
        case OP_CPY:
        case OP_BIT:
            return cycles_memory_read(entry->mode);
        case OP_STA:
        case OP_STX:
        case OP_STY:
            return cycles_store(entry->mode);
        case OP_ASL:
        case OP_LSR:
        case OP_ROL:
        case OP_ROR:
        case OP_INC:
        case OP_DEC:
            return cycles_read_modify(entry->mode);
        case OP_BCC:
        case OP_BCS:
        case OP_BEQ:
        case OP_BMI:
        case OP_BNE:
        case OP_BPL:
        case OP_BVC:
        case OP_BVS:
            return 3;
        case OP_JSR: return 6;
        case OP_JMP:
            return (entry->mode == ADDR_INDIRECT) ? 5 : 3;
        case OP_BRK: return 7;
        case OP_RTS: return 6;
        case OP_RTI: return 6;
        case OP_PHA:
        case OP_PHP: return 3;
        case OP_PLA:
        case OP_PLP: return 4;
        case OP_TAX:
        case OP_TAY:
        case OP_TSX:
        case OP_TXA:
        case OP_TXS:
        case OP_TYA:
        case OP_DEX:
        case OP_DEY:
        case OP_INX:
        case OP_INY:
        case OP_CLC:
        case OP_SEC:
        case OP_CLD:
        case OP_SED:
        case OP_CLI:
        case OP_SEI:
        case OP_CLV:
        case OP_NOP:
            return 2;
        default:
            return 0;
    }
}

static void expect_pc_and_cycles(const OpcodeEntry *entry, const OperandContext *ctx, int cycles) {
    u16 expected_pc = OPCODE_PC + ctx->bytes;
    switch (entry->op) {
        case OP_BCC: case OP_BCS: case OP_BEQ: case OP_BMI:
        case OP_BNE: case OP_BPL: case OP_BVC: case OP_BVS:
            expected_pc = ctx->branch_target;
            break;
        case OP_JSR:
        case OP_JMP:
            expected_pc = ctx->target_addr;
            break;
        case OP_BRK:
            expected_pc = ctx->target_addr;
            break;
        case OP_RTS:
        case OP_RTI:
            expected_pc = ctx->target_addr;
            break;
        default:
            break;
    }
    ASSERT_EQ(expected_pc, sys.cpu.PC);
    int expected = expected_cycles(entry);
    ASSERT_EQ(expected, cycles);
}

static void verify_nz(u8 value) {
    ASSERT_EQ((value == 0), (bool)(sys.cpu.P & FLAG_Z));
    ASSERT_EQ((value & 0x80) != 0, (bool)(sys.cpu.P & FLAG_N));
}

static void verify_branch(const OpcodeEntry *entry, const OperandContext *ctx, int cycles) {
    (void)entry;
    expect_pc_and_cycles(entry, ctx, cycles);
}

static void verify_load(u8 reg_value, u8 expected, const OpcodeEntry *entry, const OperandContext *ctx, int cycles) {
    ASSERT_EQ(expected, reg_value);
    verify_nz(reg_value);
    expect_pc_and_cycles(entry, ctx, cycles);
}

static void verify_store(const OpcodeEntry *entry, const OperandContext *ctx, int cycles, u8 expected) {
    ASSERT_EQ(expected, mem_read_raw(&sys.mem, ctx->effective_addr));
    expect_pc_and_cycles(entry, ctx, cycles);
}

static void verify_compare(u8 reg, u8 operand, const OpcodeEntry *entry, const OperandContext *ctx, int cycles) {
    u16 result = reg - operand;
    ASSERT_EQ(reg >= operand, (bool)(sys.cpu.P & FLAG_C));
    ASSERT_EQ((result & 0xFF) == 0, (bool)(sys.cpu.P & FLAG_Z));
    ASSERT_EQ((result & 0x80) != 0, (bool)(sys.cpu.P & FLAG_N));
    expect_pc_and_cycles(entry, ctx, cycles);
}

static void verify_shift(const OpcodeEntry *entry, const OperandContext *ctx, int cycles, u8 result) {
    if (entry->mode == ADDR_ACCUMULATOR) {
        ASSERT_EQ(result, sys.cpu.A);
        verify_nz(sys.cpu.A);
    } else {
        ASSERT_EQ(result, mem_read_raw(&sys.mem, ctx->effective_addr));
        verify_nz(result);
    }
    expect_pc_and_cycles(entry, ctx, cycles);
}

static void run_opcode_case(const OpcodeEntry *entry) {
    OperandContext ctx = {0};
    opcode_setup();
    init_state_for_operation(entry->op);
    u8 operand = default_operand(entry->op);
    encode_operand(entry, &ctx, operand);

    switch (entry->op) {
        case OP_RTS:
            prepare_stack_for_rts(&ctx);
            break;
        case OP_RTI:
            prepare_stack_for_rti(&ctx);
            break;
        case OP_PLA:
            prepare_stack_for_pla(operand);
            break;
        case OP_PLP:
            prepare_stack_for_plp(FLAG_C | FLAG_N);
            break;
        case OP_PHP:
        case OP_PHA:
            break;
        case OP_BRK:
            prepare_brk_vector(&ctx);
            break;
        default:
            break;
    }

    int cycles = cpu_step(&sys.cpu);

    switch (entry->op) {
        case OP_LDA:
            verify_load(sys.cpu.A, operand, entry, &ctx, cycles);
            break;
        case OP_LDX:
            verify_load(sys.cpu.X, operand, entry, &ctx, cycles);
            break;
        case OP_LDY:
            verify_load(sys.cpu.Y, operand, entry, &ctx, cycles);
            break;
        case OP_STA:
            verify_store(entry, &ctx, cycles, sys.cpu.A);
            break;
        case OP_STX:
            verify_store(entry, &ctx, cycles, sys.cpu.X);
            break;
        case OP_STY:
            verify_store(entry, &ctx, cycles, sys.cpu.Y);
            break;
        case OP_AND: {
            u8 expected = 0xF0 & operand;
            ASSERT_EQ(expected, sys.cpu.A);
            verify_nz(sys.cpu.A);
            expect_pc_and_cycles(entry, &ctx, cycles);
            break;
        }
        case OP_ORA: {
            u8 expected = 0x80 | operand;
            ASSERT_EQ(expected, sys.cpu.A);
            verify_nz(sys.cpu.A);
            expect_pc_and_cycles(entry, &ctx, cycles);
            break;
        }
        case OP_EOR: {
            u8 expected = 0xF0 ^ operand;
            ASSERT_EQ(expected, sys.cpu.A);
            verify_nz(sys.cpu.A);
            expect_pc_and_cycles(entry, &ctx, cycles);
            break;
        }
        case OP_ADC: {
            u16 sum = 0x14 + operand + 1;
            ASSERT_EQ(sum & 0xFF, sys.cpu.A);
            ASSERT_EQ(sum > 0xFF, (bool)(sys.cpu.P & FLAG_C));
            ASSERT_EQ((~(0x14 ^ operand) & (0x14 ^ (sum & 0xFF)) & 0x80) != 0, (bool)(sys.cpu.P & FLAG_V));
            verify_nz(sys.cpu.A);
            expect_pc_and_cycles(entry, &ctx, cycles);
            break;
        }
        case OP_SBC: {
            u16 diff = 0x40 - operand - 0;
            ASSERT_EQ(diff & 0xFF, sys.cpu.A);
            ASSERT_EQ(diff < 0x100, (bool)(sys.cpu.P & FLAG_C));
            ASSERT_EQ(((0x40 ^ operand) & (0x40 ^ (diff & 0xFF)) & 0x80) != 0, (bool)(sys.cpu.P & FLAG_V));
            verify_nz(sys.cpu.A);
            expect_pc_and_cycles(entry, &ctx, cycles);
            break;
        }
        case OP_CMP:
            verify_compare(0x50, operand, entry, &ctx, cycles);
            break;
        case OP_CPX:
            verify_compare(0x41, operand, entry, &ctx, cycles);
            break;
        case OP_CPY:
            verify_compare(0x41, operand, entry, &ctx, cycles);
            break;
        case OP_BIT:
            ASSERT_EQ((bool)(operand & 0x80), (bool)(sys.cpu.P & FLAG_N));
            ASSERT_EQ((bool)(operand & 0x40), (bool)(sys.cpu.P & FLAG_V));
            ASSERT_TRUE(sys.cpu.P & FLAG_Z);
            expect_pc_and_cycles(entry, &ctx, cycles);
            break;
        case OP_INC: {
            u8 expected = (operand + 1) & 0xFF;
            ASSERT_EQ(expected, mem_read_raw(&sys.mem, ctx.effective_addr));
            verify_nz(expected);
            expect_pc_and_cycles(entry, &ctx, cycles);
            break;
        }
        case OP_DEC: {
            u8 expected = (operand - 1) & 0xFF;
            ASSERT_EQ(expected, mem_read_raw(&sys.mem, ctx.effective_addr));
            verify_nz(expected);
            expect_pc_and_cycles(entry, &ctx, cycles);
            break;
        }
        case OP_ASL: {
            u8 source = (entry->mode == ADDR_ACCUMULATOR) ? operand : operand;
            u8 result = (source << 1) & 0xFF;
            ASSERT_EQ((source & 0x80) != 0, (bool)(sys.cpu.P & FLAG_C));
            verify_shift(entry, &ctx, cycles, result);
            break;
        }
        case OP_LSR: {
            u8 result = operand >> 1;
            ASSERT_EQ(operand & 0x01, sys.cpu.P & FLAG_C ? 1 : 0);
            verify_shift(entry, &ctx, cycles, result);
            break;
        }
        case OP_ROL: {
            u8 result = (u8)((operand << 1) | 1);
            ASSERT_EQ((operand & 0x80) != 0, (bool)(sys.cpu.P & FLAG_C));
            verify_shift(entry, &ctx, cycles, result);
            break;
        }
        case OP_ROR: {
            u8 result = operand >> 1;
            ASSERT_EQ((operand & 0x01) != 0, (bool)(sys.cpu.P & FLAG_C));
            verify_shift(entry, &ctx, cycles, result);
            break;
        }
        case OP_PHA:
            ASSERT_EQ(DEFAULT_A, mem_read_raw(&sys.mem, STACK_BASE + sys.cpu.SP + 1));
            expect_pc_and_cycles(entry, &ctx, cycles);
            break;
        case OP_PLA:
            ASSERT_EQ(operand, sys.cpu.A);
            verify_nz(sys.cpu.A);
            expect_pc_and_cycles(entry, &ctx, cycles);
            break;
        case OP_PHP:
            ASSERT_EQ((sys.cpu.P | FLAG_B), mem_read_raw(&sys.mem, STACK_BASE + sys.cpu.SP + 1));
            expect_pc_and_cycles(entry, &ctx, cycles);
            break;
        case OP_PLP:
            ASSERT_TRUE(sys.cpu.P & FLAG_U);
            expect_pc_and_cycles(entry, &ctx, cycles);
            break;
        case OP_TAX:
            verify_load(sys.cpu.X, sys.cpu.A, entry, &ctx, cycles);
            break;
        case OP_TAY:
            verify_load(sys.cpu.Y, sys.cpu.A, entry, &ctx, cycles);
            break;
        case OP_TXA:
            verify_load(sys.cpu.A, sys.cpu.X, entry, &ctx, cycles);
            break;
        case OP_TYA:
            verify_load(sys.cpu.A, sys.cpu.Y, entry, &ctx, cycles);
            break;
        case OP_TSX:
            verify_load(sys.cpu.X, sys.cpu.SP, entry, &ctx, cycles);
            break;
        case OP_TXS:
            ASSERT_EQ(sys.cpu.X, sys.cpu.SP);
            expect_pc_and_cycles(entry, &ctx, cycles);
            break;
        case OP_DEX:
            verify_load(sys.cpu.X, 0x00, entry, &ctx, cycles);
            break;
        case OP_DEY:
            verify_load(sys.cpu.Y, 0x00, entry, &ctx, cycles);
            break;
        case OP_INX:
            verify_load(sys.cpu.X, 0x80, entry, &ctx, cycles);
            break;
        case OP_INY:
            verify_load(sys.cpu.Y, 0x80, entry, &ctx, cycles);
            break;
        case OP_CLC:
            ASSERT_FALSE(sys.cpu.P & FLAG_C);
            expect_pc_and_cycles(entry, &ctx, cycles);
            break;
        case OP_SEC:
            ASSERT_TRUE(sys.cpu.P & FLAG_C);
            expect_pc_and_cycles(entry, &ctx, cycles);
            break;
        case OP_CLD:
            ASSERT_FALSE(sys.cpu.P & FLAG_D);
            expect_pc_and_cycles(entry, &ctx, cycles);
            break;
        case OP_SED:
            ASSERT_TRUE(sys.cpu.P & FLAG_D);
            expect_pc_and_cycles(entry, &ctx, cycles);
            break;
        case OP_CLI:
            ASSERT_FALSE(sys.cpu.P & FLAG_I);
            expect_pc_and_cycles(entry, &ctx, cycles);
            break;
        case OP_SEI:
            ASSERT_TRUE(sys.cpu.P & FLAG_I);
            expect_pc_and_cycles(entry, &ctx, cycles);
            break;
        case OP_CLV:
            ASSERT_FALSE(sys.cpu.P & FLAG_V);
            expect_pc_and_cycles(entry, &ctx, cycles);
            break;
        case OP_BCC: case OP_BCS: case OP_BEQ: case OP_BMI:
        case OP_BNE: case OP_BPL: case OP_BVC: case OP_BVS:
            verify_branch(entry, &ctx, cycles);
            break;
        case OP_JSR:
            ASSERT_EQ(ctx.target_addr, sys.cpu.PC);
            ASSERT_EQ(0xFB, sys.cpu.SP);
            expect_pc_and_cycles(entry, &ctx, cycles);
            break;
        case OP_JMP:
            ASSERT_EQ(ctx.target_addr, sys.cpu.PC);
            expect_pc_and_cycles(entry, &ctx, cycles);
            break;
        case OP_RTS:
            ASSERT_EQ(ctx.target_addr, sys.cpu.PC);
            ASSERT_EQ(0xFD, sys.cpu.SP);
            expect_pc_and_cycles(entry, &ctx, cycles);
            break;
        case OP_RTI:
            ASSERT_EQ(ctx.target_addr, sys.cpu.PC);
            ASSERT_EQ(0xFD, sys.cpu.SP);
            expect_pc_and_cycles(entry, &ctx, cycles);
            break;
        case OP_BRK:
            ASSERT_EQ(ctx.target_addr, sys.cpu.PC);
            ASSERT_TRUE(sys.cpu.P & FLAG_I);
            ASSERT_EQ(0xFA, sys.cpu.SP);
            expect_pc_and_cycles(entry, &ctx, cycles);
            break;
        case OP_NOP:
            expect_pc_and_cycles(entry, &ctx, cycles);
            break;
        default:
            break;
    }
}

TEST(cpu_opcode_matrix_from_docs) {
    ASSERT(load_opcode_table());
    int executed = 0;
    int skipped = 0;
    for (int i = 0; i < 256; ++i) {
        OpcodeEntry *entry = &g_entries[i];
        if (entry->mnemonic[0] == '\0') {
            skipped++;
            continue;
        }
        if (entry->jam || entry->undocumented || entry->mode == ADDR_SKIP || entry->op == OP_UNSUPPORTED) {
            skipped++;
            continue;
        }
        run_opcode_case(entry);
        executed++;
    }
    printf("    Documented opcodes executed: %d, skipped: %d\n", executed, skipped);
    ASSERT_EQ(151, executed);
    PASS();
}

void run_cpu_doc_tests(void) {
    static bool initialized = false;
    if (!initialized) {
        c64_init(&sys);
        initialized = true;
    }
    TEST_SUITE("CPU - Opcode Matrix (Docs)");
    RUN_TEST(cpu_opcode_matrix_from_docs);
}
