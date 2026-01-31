#include "test_suite.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Function declarations for all test functions
extern bool test_cpu_adc_immediate(void);
extern bool test_cpu_branch_timing(void);
extern bool test_cpu_page_cross_timing(void);
extern bool test_cpu_stack_operations(void);
extern bool test_cpu_flag_operations(void);
extern bool test_cpu_compare_instructions(void);
extern bool test_cpu_transfer_instructions(void);
extern bool test_cpu_illegal_opcodes(void);
extern bool test_cpu_decimal_mode(void);
extern bool test_cpu_instructions(void);
extern bool test_cpu_flags(void);
extern bool test_cpu_interrupts(void);
extern bool test_cpu_timing(void);

extern bool test_cia_timer_basic(void);
extern bool test_cia_timer_underflow(void);
extern bool test_cia_lorenz_irq(void);
extern bool test_cia_timer_pipeline(void);
extern bool test_cia_icr_timing(void);
extern bool test_cia_tod_clock(void);
extern bool test_cia_keyboard_matrix(void);
extern bool test_cia_timers(void);
extern bool test_cia_interrupts(void);
extern bool test_cia_tod(void);

extern bool test_vic_bad_line_detection(void);
extern bool test_vic_sprite_dma(void);
extern bool test_vic_raster_interrupts(void);
extern bool test_vic_memory_fetching(void);
extern bool test_vic_color_registers(void);
extern bool test_vic_display_modes(void);
extern bool test_vic_timing(void);
extern bool test_vic_interrupts(void);
extern bool test_vic_rendering(void);
extern bool test_vic_bad_lines(void);
extern bool test_vic_sprite_dma_comprehensive(void);
extern bool test_vic_raster_interrupts_comprehensive(void);

extern bool test_memory_basic_read_write(void);
extern bool test_memory_pla_mapping(void);
extern bool test_memory_color_ram(void);
extern bool test_memory_io_mapping(void);
extern bool test_memory_rom_loading(void);
extern bool test_memory_stack_area(void);
extern bool test_memory_zp_indirect(void);
extern bool test_memory_mapping(void);
extern bool test_memory_pla(void);
extern bool test_memory_color_ram_comprehensive(void);

extern bool test_sid_voice_registers(void);
extern bool test_sid_filter_registers(void);
extern bool test_sid_voice_control_bits(void);
extern bool test_sid_envelope_generator(void);
extern bool test_sid_filter_types(void);
extern bool test_sid_voice3_output(void);
extern bool test_sid_potentiometer_inputs(void);
extern bool test_sid_registers(void);
extern bool test_sid_filter(void);
extern bool test_sid_audio_generation(void);

// Integration test declarations
extern bool test_kernal_boot(void);
extern bool test_basic_interpreter(void);
extern bool test_lorenz_suite_compliance(void);

int main(int argc, char* argv[]) {
    test_init();
    
    printf("🚀 C64 Emulator Comprehensive Test Suite\n");
    printf("==========================================\n\n");
    
    // Parse command line arguments
    bool run_all = true;
    bool run_cpu = false;
    bool run_cia = false;
    bool run_vic = false;
    bool run_memory = false;
    bool run_sid = false;
    bool run_integration = false;
    bool verbose = false;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--cpu") == 0) { run_cpu = true; run_all = false; }
        else if (strcmp(argv[i], "--cia") == 0) { run_cia = true; run_all = false; }
        else if (strcmp(argv[i], "--vic") == 0) { run_vic = true; run_all = false; }
        else if (strcmp(argv[i], "--memory") == 0) { run_memory = true; run_all = false; }
        else if (strcmp(argv[i], "--sid") == 0) { run_sid = true; run_all = false; }
        else if (strcmp(argv[i], "--integration") == 0) { run_integration = true; run_all = false; }
        else if (strcmp(argv[i], "--verbose") == 0) { verbose = true; }
        else if (strcmp(argv[i], "--help") == 0) {
            printf("Usage: %s [options]\n", argv[0]);
            printf("Options:\n");
            printf("  --cpu          Run CPU tests only\n");
            printf("  --cia          Run CIA tests only\n");
            printf("  --vic          Run VIC tests only\n");
            printf("  --memory       Run memory tests only\n");
            printf("  --sid          Run SID tests only\n");
            printf("  --integration  Run integration tests only\n");
            printf("  --verbose      Enable verbose output\n");
            printf("  --help         Show this help message\n");
            printf("  (no args)      Run all tests\n");
            return 0;
        }
    }
    
    g_verbose = verbose;
    
    // Define all test cases
    static const TestCase all_tests[] = {
        // CPU Tests
        {"cpu_adc_immediate", test_cpu_adc_immediate, "ADC immediate instruction", CPU_CATEGORY},
        {"cpu_branch_timing", test_cpu_branch_timing, "Branch instruction timing", CPU_CATEGORY},
        {"cpu_page_cross_timing", test_cpu_page_cross_timing, "Page cross timing", CPU_CATEGORY},
        {"cpu_stack_operations", test_cpu_stack_operations, "Stack operations", CPU_CATEGORY},
        {"cpu_flag_operations", test_cpu_flag_operations, "Flag operations", CPU_CATEGORY},
        {"cpu_compare_instructions", test_cpu_compare_instructions, "Compare instructions", CPU_CATEGORY},
        {"cpu_transfer_instructions", test_cpu_transfer_instructions, "Transfer instructions", CPU_CATEGORY},
        {"cpu_illegal_opcodes", test_cpu_illegal_opcodes, "Illegal opcodes", CPU_CATEGORY},
        {"cpu_decimal_mode", test_cpu_decimal_mode, "Decimal mode", CPU_CATEGORY},
        
        // CIA Tests
        {"cia_timer_basic", test_cia_timer_basic, "Basic timer operation", CIA_CATEGORY},
        {"cia_timer_underflow", test_cia_timer_underflow, "Timer underflow", CIA_CATEGORY},
        {"cia_lorenz_irq", test_cia_lorenz_irq, "Lorenz IRQ test", CIA_CATEGORY},
        {"cia_timer_pipeline", test_cia_timer_pipeline, "Timer pipeline delay", CIA_CATEGORY},
        {"cia_icr_timing", test_cia_icr_timing, "ICR timing", CIA_CATEGORY},
        {"cia_tod_clock", test_cia_tod_clock, "Time-of-Day clock", CIA_CATEGORY},
        {"cia_keyboard_matrix", test_cia_keyboard_matrix, "Keyboard matrix", CIA_CATEGORY},
        
        // VIC Tests
        {"vic_bad_line_detection", test_vic_bad_line_detection, "Bad line detection", VIC_CATEGORY},
        {"vic_sprite_dma", test_vic_sprite_dma, "Sprite DMA", VIC_CATEGORY},
        {"vic_raster_interrupts", test_vic_raster_interrupts, "Raster interrupts", VIC_CATEGORY},
        {"vic_memory_fetching", test_vic_memory_fetching, "Memory fetching", VIC_CATEGORY},
        {"vic_color_registers", test_vic_color_registers, "Color registers", VIC_CATEGORY},
        {"vic_display_modes", test_vic_display_modes, "Display modes", VIC_CATEGORY},
        
        // Memory Tests
        {"memory_basic_read_write", test_memory_basic_read_write, "Basic read/write", MEMORY_CATEGORY},
        {"memory_pla_mapping", test_memory_pla_mapping, "PLA mapping", MEMORY_CATEGORY},
        {"memory_color_ram", test_memory_color_ram, "Color RAM", MEMORY_CATEGORY},
        {"memory_io_mapping", test_memory_io_mapping, "I/O mapping", MEMORY_CATEGORY},
        {"memory_rom_loading", test_memory_rom_loading, "ROM loading", MEMORY_CATEGORY},
        {"memory_stack_area", test_memory_stack_area, "Stack area", MEMORY_CATEGORY},
        {"memory_zp_indirect", test_memory_zp_indirect, "Zero page indirect", MEMORY_CATEGORY},
        
        // SID Tests
        {"sid_voice_registers", test_sid_voice_registers, "Voice registers", SID_CATEGORY},
        {"sid_filter_registers", test_sid_filter_registers, "Filter registers", SID_CATEGORY},
        {"sid_voice_control_bits", test_sid_voice_control_bits, "Voice control bits", SID_CATEGORY},
        {"sid_envelope_generator", test_sid_envelope_generator, "Envelope generator", SID_CATEGORY},
        {"sid_filter_types", test_sid_filter_types, "Filter types", SID_CATEGORY},
        {"sid_voice3_output", test_sid_voice3_output, "Voice 3 output", SID_CATEGORY},
        {"sid_potentiometer_inputs", test_sid_potentiometer_inputs, "Potentiometer inputs", SID_CATEGORY},
        {"sid_audio_generation", test_sid_audio_generation, "Audio generation", SID_CATEGORY},
    };
    
    int num_tests = sizeof(all_tests) / sizeof(TestCase);
    
    // Run selected tests
    for (int i = 0; i < num_tests; i++) {
        const TestCase* test = &all_tests[i];
        
        bool should_run = run_all;
        if (!should_run) {
            if (run_cpu && strcmp(test->category, CPU_CATEGORY) == 0) should_run = true;
            else if (run_cia && strcmp(test->category, CIA_CATEGORY) == 0) should_run = true;
            else if (run_vic && strcmp(test->category, VIC_CATEGORY) == 0) should_run = true;
            else if (run_memory && strcmp(test->category, MEMORY_CATEGORY) == 0) should_run = true;
            else if (run_sid && strcmp(test->category, SID_CATEGORY) == 0) should_run = true;
            else if (run_integration && strcmp(test->category, INTEGRATION_CATEGORY) == 0) should_run = true;
        }
        
        if (should_run) {
            printf("\n🧪 Running: %s (%s)\n", test->name, test->description);
            g_test_results.total++;
            
            bool result = test->test_func();
            if (result) {
                g_test_results.passed++;
                if (verbose) printf("✅ %s PASSED\n", test->name);
            } else {
                g_test_results.failed++;
                printf("❌ %s FAILED\n", test->name);
            }
        }
    }
    
    test_report();
    
    return (g_test_results.failed == 0) ? 0 : 1;
}