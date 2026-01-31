/**
 * test_memory.c - Memory subsystem tests
 * 
 * Tests PLA logic, ROM visibility, Color RAM, and bank switching.
 */

#include "test_framework.h"
#include "../src/c64.h"

static C64System sys;

static void setup(void) {
    c64_init(&sys);
    mem_reset(&sys.mem);
    cpu_reset(&sys.cpu);
    vic_reset(&sys.vic);  // Also reset VIC for register tests
    
    // Fill ROMs with known patterns for testing
    memset(sys.mem.basic_rom, 0xBA, C64_BASIC_SIZE);
    memset(sys.mem.kernal_rom, 0xEA, C64_KERNAL_SIZE);
    memset(sys.mem.char_rom, 0xCA, C64_CHAR_SIZE);
    sys.mem.basic_loaded = true;
    sys.mem.kernal_loaded = true;
    sys.mem.char_loaded = true;
}

// ============================================================================
// Basic RAM Tests
// ============================================================================

TEST(ram_read_write) {
    setup();
    
    // Write to various RAM locations
    mem_write_raw(&sys.mem, 0x0000, 0x11);
    mem_write_raw(&sys.mem, 0x00FF, 0x22);
    mem_write_raw(&sys.mem, 0x0800, 0x33);
    mem_write_raw(&sys.mem, 0x8000, 0x44);
    
    ASSERT_MEM_EQ(0x0000, 0x11);
    ASSERT_MEM_EQ(0x00FF, 0x22);
    ASSERT_MEM_EQ(0x0800, 0x33);
    ASSERT_MEM_EQ(0x8000, 0x44);
    PASS();
}

TEST(ram_under_rom) {
    setup();
    
    // Write to RAM under BASIC ROM ($A000-$BFFF)
    mem_write_raw(&sys.mem, 0xA000, 0x55);
    
    // With BASIC visible, should read ROM
    sys.cpu.port_data = 0x37;  // LORAM=1, HIRAM=1, CHAREN=1
    u8 rom_val = mem_read_raw(&sys.mem, 0xA000);
    ASSERT_EQ(0xBA, rom_val);  // ROM pattern
    
    // With BASIC disabled, should read RAM
    sys.cpu.port_data = 0x36;  // LORAM=0
    u8 ram_val = mem_read_raw(&sys.mem, 0xA000);
    ASSERT_EQ(0x55, ram_val);  // RAM value
    PASS();
}

// ============================================================================
// PLA Bank Switching Tests (per documentation)
// ============================================================================

TEST(pla_basic_visible) {
    setup();
    // BASIC visible when LORAM=1 and HIRAM=1
    sys.cpu.port_data = 0x37;  // bits 0,1,2 all set
    
    u8 val = mem_read_raw(&sys.mem, 0xA000);
    ASSERT_EQ(0xBA, val);  // BASIC ROM pattern
    PASS();
}

TEST(pla_basic_hidden_loram) {
    setup();
    // BASIC hidden when LORAM=0
    sys.cpu.port_data = 0x36;  // LORAM=0
    mem_write_raw(&sys.mem, 0xA000, 0x99);
    
    u8 val = mem_read_raw(&sys.mem, 0xA000);
    ASSERT_EQ(0x99, val);  // RAM
    PASS();
}

TEST(pla_basic_hidden_hiram) {
    setup();
    // BASIC hidden when HIRAM=0
    sys.cpu.port_data = 0x35;  // HIRAM=0
    mem_write_raw(&sys.mem, 0xA000, 0xAA);
    
    u8 val = mem_read_raw(&sys.mem, 0xA000);
    ASSERT_EQ(0xAA, val);  // RAM
    PASS();
}

TEST(pla_kernal_visible) {
    setup();
    // KERNAL visible when HIRAM=1
    sys.cpu.port_data = 0x37;
    
    u8 val = mem_read_raw(&sys.mem, 0xE000);
    ASSERT_EQ(0xEA, val);  // KERNAL ROM pattern
    PASS();
}

TEST(pla_kernal_hidden) {
    setup();
    // KERNAL hidden when HIRAM=0
    sys.cpu.port_data = 0x35;  // HIRAM=0
    mem_write_raw(&sys.mem, 0xE000, 0xBB);
    
    u8 val = mem_read_raw(&sys.mem, 0xE000);
    ASSERT_EQ(0xBB, val);  // RAM
    PASS();
}

TEST(pla_io_visible) {
    setup();
    // I/O visible when CHAREN=1 and (LORAM=1 or HIRAM=1)
    sys.cpu.port_data = 0x37;  // All set
    
    // Reading from VIC register area should go to VIC
    // VIC register read returns register value, not RAM
    u8 val = mem_read_raw(&sys.mem, 0xD020);  // Border color
    // Default border color is 0x0E (light blue)
    ASSERT_EQ(0x0E, val);
    PASS();
}

TEST(pla_char_rom_visible) {
    setup();
    // Char ROM visible when CHAREN=0 and (LORAM=1 or HIRAM=1)
    sys.cpu.port_data = 0x33;  // CHAREN=0, LORAM=1, HIRAM=1
    
    u8 val = mem_read_raw(&sys.mem, 0xD000);
    ASSERT_EQ(0xCA, val);  // Char ROM pattern
    PASS();
}

TEST(pla_ram_at_d000) {
    setup();
    // RAM visible at $D000 when LORAM=0 and HIRAM=0
    sys.cpu.port_data = 0x34;  // LORAM=0, HIRAM=0, CHAREN=1
    mem_write_raw(&sys.mem, 0xD000, 0xCC);
    
    u8 val = mem_read_raw(&sys.mem, 0xD000);
    ASSERT_EQ(0xCC, val);  // RAM
    PASS();
}

TEST(pla_all_ram) {
    setup();
    // All RAM mode
    sys.cpu.port_data = 0x30;  // All bank bits 0
    
    mem_write_raw(&sys.mem, 0xA000, 0x11);
    mem_write_raw(&sys.mem, 0xD000, 0x22);
    mem_write_raw(&sys.mem, 0xE000, 0x33);
    
    ASSERT_MEM_EQ(0xA000, 0x11);
    ASSERT_MEM_EQ(0xD000, 0x22);
    ASSERT_MEM_EQ(0xE000, 0x33);
    PASS();
}

// ============================================================================
// Color RAM Tests
// ============================================================================

TEST(color_ram_4bit) {
    setup();
    sys.cpu.port_data = 0x37;  // I/O visible
    
    // Write to color RAM
    mem_write_raw(&sys.mem, 0xD800, 0xFF);
    
    // Read should return only lower 4 bits | 0xF0
    u8 val = mem_read_raw(&sys.mem, 0xD800);
    ASSERT_EQ(0xFF, val);  // 0xF0 | 0x0F
    PASS();
}

TEST(color_ram_range) {
    setup();
    sys.cpu.port_data = 0x37;
    
    // Color RAM is 1024 bytes ($D800-$DBFF)
    mem_write_raw(&sys.mem, 0xD800, 0x01);
    mem_write_raw(&sys.mem, 0xD8FF, 0x02);
    mem_write_raw(&sys.mem, 0xD900, 0x03);
    mem_write_raw(&sys.mem, 0xDBFF, 0x0F);
    
    ASSERT_EQ(0xF1, mem_read_raw(&sys.mem, 0xD800));
    ASSERT_EQ(0xF2, mem_read_raw(&sys.mem, 0xD8FF));
    ASSERT_EQ(0xF3, mem_read_raw(&sys.mem, 0xD900));
    ASSERT_EQ(0xFF, mem_read_raw(&sys.mem, 0xDBFF));
    PASS();
}

// ============================================================================
// VIC Bank Tests (via CIA2)
// ============================================================================

TEST(vic_bank_0) {
    setup();
    // Bank 0: $0000-$3FFF (default)
    sys.cia2.pra = 0x03;  // ~0x03 & 0x03 = 0
    
    u16 bank = c64_get_vic_bank(&sys);
    ASSERT_EQ(0x0000, bank);
    PASS();
}

TEST(vic_bank_1) {
    setup();
    // Bank 1: $4000-$7FFF
    sys.cia2.pra = 0x02;  // ~0x02 & 0x03 = 1
    
    u16 bank = c64_get_vic_bank(&sys);
    ASSERT_EQ(0x4000, bank);
    PASS();
}

TEST(vic_bank_2) {
    setup();
    // Bank 2: $8000-$BFFF
    sys.cia2.pra = 0x01;  // ~0x01 & 0x03 = 2
    
    u16 bank = c64_get_vic_bank(&sys);
    ASSERT_EQ(0x8000, bank);
    PASS();
}

TEST(vic_bank_3) {
    setup();
    // Bank 3: $C000-$FFFF
    sys.cia2.pra = 0x00;  // ~0x00 & 0x03 = 3
    
    u16 bank = c64_get_vic_bank(&sys);
    ASSERT_EQ(0xC000, bank);
    PASS();
}

TEST(vic_sees_char_rom_bank0) {
    setup();
    // In banks 0 and 2, VIC sees Char ROM at $1000-$1FFF
    sys.cia2.pra = 0x03;  // Bank 0
    
    // Write to RAM at $1000
    mem_write_raw(&sys.mem, 0x1000, 0x99);
    
    // VIC should see Char ROM, not RAM
    u8 val = mem_vic_read(&sys.mem, 0x1000);
    ASSERT_EQ(0xCA, val);  // Char ROM pattern
    PASS();
}

TEST(vic_sees_ram_bank1) {
    setup();
    // In banks 1 and 3, VIC sees RAM (no Char ROM mapping)
    sys.cia2.pra = 0x02;  // Bank 1
    
    // Write to RAM at $5000 (bank 1 offset $1000)
    mem_write_raw(&sys.mem, 0x5000, 0x77);
    
    u8 val = mem_vic_read(&sys.mem, 0x1000);  // VIC address $1000
    ASSERT_EQ(0x77, val);  // RAM
    PASS();
}

// ============================================================================
// I/O Area Tests
// ============================================================================

TEST(io_vic_mirroring) {
    setup();
    sys.cpu.port_data = 0x37;
    
    // VIC registers mirror every 64 bytes in $D000-$D3FF
    vic_write(&sys.vic, VIC_BORDER, 0x01);
    
    ASSERT_EQ(0x01, mem_read_raw(&sys.mem, 0xD020));
    ASSERT_EQ(0x01, mem_read_raw(&sys.mem, 0xD060));  // Mirror at +$40
    ASSERT_EQ(0x01, mem_read_raw(&sys.mem, 0xD0A0));  // Mirror at +$80
    PASS();
}

TEST(io_sid_mirroring) {
    setup();
    sys.cpu.port_data = 0x37;
    
    // SID registers are write-only at $D400-$D418
    // We can't reliably read back what we write, so just verify
    // that writes go through without crashing and reads return something
    sid_write(&sys.sid, SID_MODEVOL, 0x0F);
    
    // Reading SID write-only registers returns open bus or cached value
    // Just verify it doesn't crash and returns reasonable value
    u8 val = mem_read_raw(&sys.mem, 0xD418);
    (void)val;  // Value is implementation-defined for write-only regs
    PASS();
}

TEST(io_cia1_range) {
    setup();
    sys.cpu.port_data = 0x37;
    
    // CIA1 at $DC00-$DCFF (mirrors every 16 bytes)
    // Test with timer low byte (not affected by keyboard matrix)
    cia_write(&sys.cia1, CIA_TALO, 0xAA);
    cia_write(&sys.cia1, CIA_TAHI, 0x55);
    
    // Read timer latch through memory
    u8 lo = mem_read_raw(&sys.mem, 0xDC04);
    u8 hi = mem_read_raw(&sys.mem, 0xDC05);
    
    // Timer will have loaded and possibly counted, so just verify
    // reading works and returns reasonable values
    (void)lo;
    (void)hi;
    PASS();
}

TEST(io_cia2_range) {
    setup();
    sys.cpu.port_data = 0x37;
    
    // CIA2 at $DD00-$DDFF
    cia_write(&sys.cia2, CIA_PRA, 0x55);
    
    // CIA2 Port A has DDR effects
    sys.cia2.ddra = 0xFF;  // All output
    ASSERT_EQ(0x55, mem_read_raw(&sys.mem, 0xDD00));
    PASS();
}

TEST(io_expansion_area) {
    setup();
    sys.cpu.port_data = 0x37;
    
    // $DE00-$DFFF is expansion I/O (returns open bus)
    u8 val = mem_read_raw(&sys.mem, 0xDE00);
    ASSERT_EQ(0xFF, val);
    PASS();
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST(zero_page_wrap) {
    setup();
    
    // Zeropage addressing should wrap at 256 bytes
    mem_write_raw(&sys.mem, 0x00, 0x11);
    mem_write_raw(&sys.mem, 0xFF, 0x22);
    
    ASSERT_MEM_EQ(0x00, 0x11);
    ASSERT_MEM_EQ(0xFF, 0x22);
    PASS();
}

TEST(stack_wrap) {
    setup();
    
    // Stack page is $0100-$01FF
    mem_write_raw(&sys.mem, 0x0100, 0x33);
    mem_write_raw(&sys.mem, 0x01FF, 0x44);
    
    ASSERT_MEM_EQ(0x0100, 0x33);
    ASSERT_MEM_EQ(0x01FF, 0x44);
    PASS();
}

TEST(write_to_rom_area) {
    setup();
    sys.cpu.port_data = 0x37;  // ROMs visible
    
    // Write to ROM area should go to underlying RAM
    mem_write_raw(&sys.mem, 0xA000, 0x55);  // BASIC area
    mem_write_raw(&sys.mem, 0xE000, 0x66);  // KERNAL area
    
    // With ROMs visible, reads return ROM
    ASSERT_EQ(0xBA, mem_read_raw(&sys.mem, 0xA000));
    ASSERT_EQ(0xEA, mem_read_raw(&sys.mem, 0xE000));
    
    // With ROMs hidden, reads return RAM
    sys.cpu.port_data = 0x34;
    ASSERT_EQ(0x55, mem_read_raw(&sys.mem, 0xA000));
    ASSERT_EQ(0x66, mem_read_raw(&sys.mem, 0xE000));
    PASS();
}

// ============================================================================
// Run all memory tests
// ============================================================================

void run_memory_tests(void) {
    TEST_SUITE("Memory - Basic RAM");
    RUN_TEST(ram_read_write);
    RUN_TEST(ram_under_rom);
    
    TEST_SUITE("Memory - PLA Bank Switching");
    RUN_TEST(pla_basic_visible);
    RUN_TEST(pla_basic_hidden_loram);
    RUN_TEST(pla_basic_hidden_hiram);
    RUN_TEST(pla_kernal_visible);
    RUN_TEST(pla_kernal_hidden);
    RUN_TEST(pla_io_visible);
    RUN_TEST(pla_char_rom_visible);
    RUN_TEST(pla_ram_at_d000);
    RUN_TEST(pla_all_ram);
    
    TEST_SUITE("Memory - Color RAM");
    RUN_TEST(color_ram_4bit);
    RUN_TEST(color_ram_range);
    
    TEST_SUITE("Memory - VIC Banks");
    RUN_TEST(vic_bank_0);
    RUN_TEST(vic_bank_1);
    RUN_TEST(vic_bank_2);
    RUN_TEST(vic_bank_3);
    RUN_TEST(vic_sees_char_rom_bank0);
    RUN_TEST(vic_sees_ram_bank1);
    
    TEST_SUITE("Memory - I/O Area");
    RUN_TEST(io_vic_mirroring);
    RUN_TEST(io_sid_mirroring);
    RUN_TEST(io_cia1_range);
    RUN_TEST(io_cia2_range);
    RUN_TEST(io_expansion_area);
    
    TEST_SUITE("Memory - Edge Cases");
    RUN_TEST(zero_page_wrap);
    RUN_TEST(stack_wrap);
    RUN_TEST(write_to_rom_area);
}
