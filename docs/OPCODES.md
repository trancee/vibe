# MOS 6510 Opcode Reference

This document provides a comprehensive reference for all 256 MOS 6510 opcodes implemented in this emulator.

## Opcode Table Format

Each entry in the opcode table contains:
- **Opcode**: Hexadecimal value (0x00 - 0xFF)
- **Name**: 3-character instruction mnemonic
- **Addressing Mode**: How the operand is accessed
- **Cycles**: Number of CPU cycles required
- **Function**: Pointer to implementation function

## Addressing Modes

| Mode | Description | Example | Cycles |
|------|-------------|----------|--------|
| IMP | Implied | CLC | 2 |
| IMM | Immediate | LDA #$42 | 2 |
| ZP | Zero Page | LDA $42 | 3 |
| ZPX | Zero Page, X | LDA $42,X | 4 |
| ZPY | Zero Page, Y | LDX $42,Y | 4 |
| ABS | Absolute | LDA $1234 | 4 |
| ABSX | Absolute, X | LDA $1234,X | 4+ |
| ABSY | Absolute, Y | LDA $1234,Y | 4+ |
| IND | Indirect | JMP ($1234) | 5 |
| INDX | Indexed Indirect | LDA ($42,X) | 6 |
| INDY | Indirect Indexed | LDA ($42),Y | 5+ |
| REL | Relative | BEQ $42 | 2+ |

*+ indicates additional cycle for page boundary crossing*

## Legal Opcodes

### Load/Store Operations

| Opcode | Name | Mode | Cycles | Description |
|--------|------|------|--------|-------------|
| A9 | LDA | IMM | 2 | Load Accumulator |
| A5 | LDA | ZP | 3 | Load Accumulator |
| B5 | LDA | ZPX | 4 | Load Accumulator |
| AD | LDA | ABS | 4 | Load Accumulator |
| BD | LDA | ABSX | 4+ | Load Accumulator |
| B9 | LDA | ABSY | 4+ | Load Accumulator |
| A1 | LDA | INDX | 6 | Load Accumulator |
| B1 | LDA | INDY | 5+ | Load Accumulator |

### Arithmetic Operations

| Opcode | Name | Mode | Cycles | Description |
|--------|------|------|--------|-------------|
| 69 | ADC | IMM | 2 | Add with Carry |
| 65 | ADC | ZP | 3 | Add with Carry |
| 75 | ADC | ZPX | 4 | Add with Carry |
| 6D | ADC | ABS | 4 | Add with Carry |
| 7D | ADC | ABSX | 4+ | Add with Carry |
| 79 | ADC | ABSY | 4+ | Add with Carry |
| 61 | ADC | INDX | 6 | Add with Carry |
| 71 | ADC | INDY | 5+ | Add with Carry |

### Logical Operations

| Opcode | Name | Mode | Cycles | Description |
|--------|------|------|--------|-------------|
| 29 | AND | IMM | 2 | Logical AND |
| 25 | AND | ZP | 3 | Logical AND |
| 35 | AND | ZPX | 4 | Logical AND |
| 2D | AND | ABS | 4 | Logical AND |
| 3D | AND | ABSX | 4+ | Logical AND |
| 39 | AND | ABSY | 4+ | Logical AND |
| 21 | AND | INDX | 6 | Logical AND |
| 31 | AND | INDY | 5+ | Logical AND |

### Shift and Rotate Operations

| Opcode | Name | Mode | Cycles | Description |
|--------|------|------|--------|-------------|
| 0A | ASL | ACC | 2 | Arithmetic Shift Left |
| 06 | ASL | ZP | 5 | Arithmetic Shift Left |
| 16 | ASL | ZPX | 6 | Arithmetic Shift Left |
| 0E | ASL | ABS | 6 | Arithmetic Shift Left |
| 1E | ASL | ABSX | 7 | Arithmetic Shift Left |

### Jump and Branch Operations

| Opcode | Name | Mode | Cycles | Description |
|--------|------|------|--------|-------------|
| 4C | JMP | ABS | 3 | Jump |
| 6C | JMP | IND | 5 | Jump Indirect |
| 20 | JSR | ABS | 6 | Jump to Subroutine |
| 60 | RTS | IMP | 6 | Return from Subroutine |
| 90 | BCC | REL | 2+ | Branch if Carry Clear |
| B0 | BCS | REL | 2+ | Branch if Carry Set |
| F0 | BEQ | REL | 2+ | Branch if Equal |
| D0 | BNE | REL | 2+ | Branch if Not Equal |
| 30 | BMI | REL | 2+ | Branch if Minus |
| 10 | BPL | REL | 2+ | Branch if Plus |
| 50 | BVC | REL | 2+ | Branch if Overflow Clear |
| 70 | BVS | REL | 2+ | Branch if Overflow Set |

### Status Flag Operations

| Opcode | Name | Mode | Cycles | Description |
|--------|------|------|--------|-------------|
| 18 | CLC | IMP | 2 | Clear Carry Flag |
| D8 | CLD | IMP | 2 | Clear Decimal Flag |
| 58 | CLI | IMP | 2 | Clear Interrupt Flag |
| B8 | CLV | IMP | 2 | Clear Overflow Flag |
| 38 | SEC | IMP | 2 | Set Carry Flag |
| F8 | SED | IMP | 2 | Set Decimal Flag |
| 78 | SEI | IMP | 2 | Set Interrupt Flag |

### Stack Operations

| Opcode | Name | Mode | Cycles | Description |
|--------|------|------|--------|-------------|
| 48 | PHA | IMP | 3 | Push Accumulator |
| 08 | PHP | IMP | 3 | Push Processor Status |
| 68 | PLA | IMP | 4 | Pull Accumulator |
| 28 | PLP | IMP | 4 | Pull Processor Status |

### Transfer Operations

| Opcode | Name | Mode | Cycles | Description |
|--------|------|------|--------|-------------|
| AA | TAX | IMP | 2 | Transfer A to X |
| A8 | TAY | IMP | 2 | Transfer A to Y |
| BA | TSX | IMP | 2 | Transfer SP to X |
| 8A | TXA | IMP | 2 | Transfer X to A |
| 9A | TXS | IMP | 2 | Transfer X to SP |
| 98 | TYA | IMP | 2 | Transfer Y to A |

### Comparison Operations

| Opcode | Name | Mode | Cycles | Description |
|--------|------|------|--------|-------------|
| C9 | CMP | IMM | 2 | Compare Accumulator |
| C5 | CMP | ZP | 3 | Compare Accumulator |
| D5 | CMP | ZPX | 4 | Compare Accumulator |
| CD | CMP | ABS | 4 | Compare Accumulator |
| DD | CMP | ABSX | 4+ | Compare Accumulator |
| D9 | CMP | ABSY | 4+ | Compare Accumulator |
| C1 | CMP | INDX | 6 | Compare Accumulator |
| D1 | CMP | INDY | 5+ | Compare Accumulator |

### Increment/Decrement Operations

| Opcode | Name | Mode | Cycles | Description |
|--------|------|------|--------|-------------|
| E8 | INX | IMP | 2 | Increment X |
| C8 | INY | IMP | 2 | Increment Y |
| E6 | INC | ZP | 5 | Increment Memory |
| F6 | INC | ZPX | 6 | Increment Memory |
| EE | INC | ABS | 6 | Increment Memory |
| FE | INC | ABSX | 7 | Increment Memory |
| CA | DEX | IMP | 2 | Decrement X |
| 88 | DEY | IMP | 2 | Decrement Y |
| C6 | DEC | ZP | 5 | Decrement Memory |
| D6 | DEC | ZPX | 6 | Decrement Memory |
| CE | DEC | ABS | 6 | Decrement Memory |
| DE | DEC | ABSX | 7 | Decrement Memory |

## Illegal Opcodes

The emulator also implements 105 illegal/undocumented opcodes for completeness:

### Combined Operations
- **ANC** (0B, 2B): AND then set carry
- **ARR** (6B): AND then rotate right with carry
- **DCP** (C3, C7, D3, D7, F3, F7): DEC then CMP
- **ISC** (E3, E7, F3, F7): INC then SBC
- **LAS** (BB): LDA from stack with AND
- **LAX** (A3, A7, AF, B3, B7, BF): LDA then TAX
- **RLA** (23, 27, 2F, 33, 37, 3F): ROL then AND
- **RRA** (63, 67, 6F, 73, 77, 7F): ROR then ADC

### Store Operations
- **SAX** (83, 87, 8F, 97): Store A & X
- **SLO** (03, 07, 0F, 13, 17, 1F): ASL then ORA
- **SRE** (43, 47, 4F, 53, 57, 5F): LSR then EOR
- **TAS** (9B): Transfer A & X to SP
- **XAA** (8B): Transfer X then AND with A

### NOP Variants
Various no-operation opcodes with different addressing modes and cycle counts.

## Implementation Notes

1. **Illegal Opcodes**: Implemented for completeness and compatibility with existing software that may use them
2. **Timing**: Accurate cycle counting including page boundary penalties
3. **6502 Bug**: The indirect JMP bug on page boundaries is correctly implemented
4. **Flags**: All flag changes follow original 6510 specifications
5. **Stack**: Stack operations use the $0100-$01FF memory region as per spec

For complete opcode table with all 256 entries, see `src/instructions.c`.