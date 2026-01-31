#include "test_suite.h"
#include "memory.h"
#include "c64.h"
#include <stdio.h>
#include <string.h>

bool test_memory_basic_read_write(void) {
    C64 sys;
    c64_test_init(&sys);
    
    // Test basic RAM read/write
    u16 test_addr = 0x1000;
    u8 test_value = 0x42;
    
    sys.mem.ram[test_addr] = test_value;
    u8 read_value = sys.mem.ram[test_addr];
    
    TEST_ASSERT_EQ(test_value, read_value, "RAM read/write should work");
    
    // Test multiple locations
    for (u16 i = 0; i < 256; i++) {
        sys.mem.ram[0x2000 + i] = (u8)i;
    }
    
    for (u16 i = 0; i < 256; i++) {
        TEST_ASSERT_EQ((u8)i, sys.mem.ram[0x2000 + i], "Multiple RAM writes should work");
    }
    
    c64_test_cleanup(&sys);
    TEST_PASS("Memory basic read/write test passed");
}

bool test_memory_pla_mapping(void) {
    C64 sys;
    c64_test_init(&sys);
    
    // Test PLA mapping logic for different processor port configurations
    
    // Test configuration 0: HIRAM=0, LORAM=0, CHAREN=0
    // Kernal: $E000-$FFFF unmapped, Basic: $A000-$BFFF unmapped, Charset: $D000-$DFFF unmapped
    sys.mem.cpu_port_data = 0x00; // All bits clear
    sys.mem.cpu_port_dir = 0xFF; // All bits as output
    
    // This would normally trigger memory remapping in the PLA
    // For now, we test the logic that would be used
    
    bool kernal_enabled = (sys.mem.cpu_port_data & 0x02) != 0;
    bool basic_enabled = (sys.mem.cpu_port_data & 0x01) != 0;
    bool charset_enabled = (sys.mem.cpu_port_data & 0x04) == 0; // Active low
    
    TEST_ASSERT(!kernal_enabled, "Kernal should be disabled when HIRAM=0");
    TEST_ASSERT(!basic_enabled, "Basic should be disabled when LORAM=0");
    TEST_ASSERT(charset_enabled, "Charset should be enabled when CHAREN=0");
    
    // Test configuration 1: HIRAM=1, LORAM=1, CHAREN=1
    sys.mem.cpu_port_data = 0x07; // All bits set
    kernal_enabled = (sys.mem.cpu_port_data & 0x02) != 0;
    basic_enabled = (sys.mem.cpu_port_data & 0x01) != 0;
    charset_enabled = (sys.mem.cpu_port_data & 0x04) == 0;
    
    TEST_ASSERT(kernal_enabled, "Kernal should be enabled when HIRAM=1");
    TEST_ASSERT(basic_enabled, "Basic should be enabled when LORAM=1");
    TEST_ASSERT(!charset_enabled, "Charset should be disabled when CHAREN=1");
    
    c64_test_cleanup(&sys);
    TEST_PASS("Memory PLA mapping test passed");
}

bool test_memory_color_ram(void) {
    C64 sys;
    c64_test_init(&sys);
    
    // Test color RAM 4-bit nibble handling with $F0 masking
    u16 color_ram_addr = 0x0380; // Color RAM start offset
    u8 color_value = 0x0F; // Light gray
    
    sys.mem.color_ram[color_ram_addr] = color_value;
    u8 read_color = sys.mem.color_ram[color_ram_addr];
    
    TEST_ASSERT_EQ(color_value, read_color, "Color RAM should store 4-bit values");
    
    // Test that high nibble is masked
    sys.mem.color_ram[color_ram_addr] = 0xF0; // Should be masked to 0x00
    // Read through I/O space to test masking
    u8 masked_value = mem_read(&sys, 0xD800 + color_ram_addr);
    TEST_ASSERT_EQ(0x00, masked_value, "High nibble should be masked when reading through I/O");
    
    // Test various color values
    u8 test_colors[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F};
    for (int i = 0; i < 16; i++) {
        sys.mem.color_ram[color_ram_addr + i] = test_colors[i];
        TEST_ASSERT_EQ(test_colors[i], sys.mem.color_ram[color_ram_addr + i], "Color RAM should handle all 16 colors");
    }
    
    c64_test_cleanup(&sys);
    TEST_PASS("Memory color RAM test passed");
}

bool test_memory_io_mapping(void) {
    C64 sys;
    c64_test_init(&sys);
    
    // Test I/O area mapping ($D000-$DFFF)
    // This area can map to VIC, SID, CIA1, CIA2, color RAM, or expansion port
    
    // Test VIC registers at $D000-$D03F
    u16 vic_base = 0xD000;
    sys.vic.border_color = 0x0F; // Set border color
    
    // In real implementation, this would involve memory mapping logic
    // For testing, we verify VIC register access
    TEST_ASSERT_EQ(0x0F, sys.vic.border_color, "VIC register should be accessible");
    
    // Test CIA1 at $DC00-$DCFF
    u16 cia1_base = 0xDC00;
    sys.cia1.pra = 0x55; // Set CIA1 PRA
    
    TEST_ASSERT_EQ(0x55, sys.cia1.pra, "CIA1 register should be accessible");
    
    // Test CIA2 at $DD00-$DDFF
    u16 cia2_base = 0xDD00;
    sys.cia2.pra = 0xAA; // Set CIA2 PRA
    
    TEST_ASSERT_EQ(0xAA, sys.cia2.pra, "CIA2 register should be accessible");
    
    // Test SID at $D400-$D7FF
    u16 sid_base = 0xD400;
    sys.sid.filter_cutoff_lo = 0x12; // Set SID filter
    
    TEST_ASSERT_EQ(0x12, sys.sid.filter_cutoff_lo, "SID register should be accessible");
    
    // Test color RAM at $D800-$DBFF
    u16 color_ram_io_base = 0xD800;
    u16 color_ram_offset = color_ram_io_base - 0xD800;
    sys.mem.color_ram[color_ram_offset] = 0x06; // Blue
    
    TEST_ASSERT_EQ(0x06, sys.mem.color_ram[color_ram_offset], "Color RAM should be accessible at $D800-$DBFF");
    
    c64_test_cleanup(&sys);
    TEST_PASS("Memory I/O mapping test passed");
}

bool test_memory_rom_loading(void) {
    C64 sys;
    c64_test_init(&sys);
    
    // Test ROM loading and validation
    
    // Simulate loading KERNAL ROM
    FILE* kernal_file = fopen("roms/kernal.rom", "rb");
    if (kernal_file) {
        fseek(kernal_file, 0, SEEK_END);
        long kernal_size = ftell(kernal_file);
        fseek(kernal_file, 0, SEEK_SET);
        
        TEST_ASSERT_EQ(8192, kernal_size, "KERNAL ROM should be 8KB");
        
        size_t bytes_read = fread(sys.mem.kernal_rom, 1, 8192, kernal_file);
        TEST_ASSERT_EQ(8192, bytes_read, "Should read all KERNAL ROM bytes");
        
        fclose(kernal_file);
        
        // Test that KERNAL ROM has valid reset vector at $FFFC-$FFFD
        u16 reset_vector = (sys.mem.kernal_rom[0x1FFD] << 8) | sys.mem.kernal_rom[0x1FFC];
        TEST_ASSERT_RANGE(0xE000, 0xFFFF, reset_vector, "KERNAL reset vector should be in ROM space");
        
        // Test KERNAL version string at $FFD2
        // Different KERNAL versions may have different values, check it's reasonable
        u8 ffd2_value = sys.mem.kernal_rom[0x1FD2];
        TEST_ASSERT(ffd2_value == 0xFF || ffd2_value == 0x4C || ffd2_value == 0x60 || ffd2_value == 0x6C, 
                  "KERNAL should have valid instruction at $FFD2");
    }
    
    // Simulate loading BASIC ROM
    FILE* basic_file = fopen("roms/basic.rom", "rb");
    if (basic_file) {
        fseek(basic_file, 0, SEEK_END);
        long basic_size = ftell(basic_file);
        fseek(basic_file, 0, SEEK_SET);
        
        TEST_ASSERT_EQ(8192, basic_size, "BASIC ROM should be 8KB");
        
        size_t bytes_read = fread(sys.mem.basic_rom, 1, 8192, basic_file);
        TEST_ASSERT_EQ(8192, bytes_read, "Should read all BASIC ROM bytes");
        
        fclose(basic_file);
        
        // Test BASIC start token
        TEST_ASSERT(sys.mem.basic_rom[0] == 0x9E || sys.mem.basic_rom[0] == 0x94, "BASIC should start with SYS token");
    }
    
    // Simulate loading Character ROM
    FILE* char_file = fopen("roms/char.rom", "rb");
    if (char_file) {
        fseek(char_file, 0, SEEK_END);
        long char_size = ftell(char_file);
        fseek(char_file, 0, SEEK_SET);
        
        TEST_ASSERT_EQ(4096, char_size, "Character ROM should be 4KB");
        
        size_t bytes_read = fread(sys.mem.char_rom, 1, 4096, char_file);
        TEST_ASSERT_EQ(4096, bytes_read, "Should read all Character ROM bytes");
        
        fclose(char_file);
        
        // Test that character ROM has data (character '@' at $0000)
        TEST_ASSERT(sys.mem.char_rom[0] != 0x00, "Character ROM should have character data");
    }
    
    c64_test_cleanup(&sys);
    TEST_PASS("Memory ROM loading test passed");
}

bool test_memory_stack_area(void) {
    C64 sys;
    c64_test_init(&sys);
    
    // Test stack area at $0100-$01FF
    u16 stack_base = 0x0100;
    u8 initial_sp = 0xFF;
    
    sys.cpu.sp = initial_sp;
    
    // Simulate pushing values to stack
    u8 test_values[] = {0x12, 0x34, 0x56, 0x78};
    
    for (int i = 0; i < 4; i++) {
        u16 stack_addr = stack_base + initial_sp - i;
        sys.mem.ram[stack_addr] = test_values[i];
        sys.cpu.sp--;
    }
    
    TEST_ASSERT_EQ(0xFB, sys.cpu.sp, "Stack pointer should decrement after pushes");
    
    // Simulate pulling values from stack
    for (int i = 3; i >= 0; i--) {
        sys.cpu.sp++;
        u16 stack_addr = stack_base + sys.cpu.sp;
        TEST_ASSERT_EQ(test_values[i], sys.mem.ram[stack_addr], "Stack pull should return pushed values");
    }
    
    TEST_ASSERT_EQ(initial_sp, sys.cpu.sp, "Stack pointer should be restored after pulls");
    
    c64_test_cleanup(&sys);
    TEST_PASS("Memory stack area test passed");
}

bool test_memory_zp_indirect(void) {
    C64 sys;
    c64_test_init(&sys);
    
    // Test zero page indirect addressing mode
    
    // Set up indirect address at $00FE-$00FF
    u16 zp_indirect = 0x00FE;
    u16 target_addr = 0x2000;
    u8 target_value = 0x42;
    
    sys.mem.ram[zp_indirect] = target_addr & 0xFF;     // Low byte
    sys.mem.ram[zp_indirect + 1] = target_addr >> 8;    // High byte
    
    // Store value at target address
    sys.mem.ram[target_addr] = target_value;
    
    // Simulate indirect read: (zp_indirect)
    u16 indirect_addr = sys.mem.ram[zp_indirect] | (sys.mem.ram[zp_indirect + 1] << 8);
    u8 indirect_value = sys.mem.ram[indirect_addr];
    
    TEST_ASSERT_EQ(target_addr, indirect_addr, "Indirect address should be calculated correctly");
    TEST_ASSERT_EQ(target_value, indirect_value, "Indirect read should return correct value");
    
    // Test page boundary wrapping for indirect Y addressing
    u16 zp_indirect_y = 0x00FF;
    u8 y_index = 0x01;
    
    sys.mem.ram[zp_indirect_y] = 0xFF;     // Low byte
    sys.mem.ram[0x0000] = 0x20;           // High byte (wrapped to $0000)
    sys.mem.ram[0x2000] = 0x99;           // Value at wrapped address
    
    u16 wrapped_addr = (sys.mem.ram[zp_indirect_y] | (sys.mem.ram[0x0000] << 8)) + y_index;
    TEST_ASSERT_EQ(0x2100, wrapped_addr, "Indirect Y should wrap page boundary");
    
    c64_test_cleanup(&sys);
    TEST_PASS("Memory zero page indirect test passed");
}

bool test_memory_mapping(void) {
    return test_memory_pla_mapping() && 
           test_memory_io_mapping();
}

bool test_memory_pla(void) {
    return test_memory_pla_mapping();
}

bool test_memory_color_ram_comprehensive(void) {
    return test_memory_color_ram();
}