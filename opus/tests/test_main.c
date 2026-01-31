/**
 * test_main.c - Main test runner
 * 
 * Runs all test suites for the C64 emulator.
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define TEST_MAIN
#include "test_framework.h"

// External test suite runners
extern void run_cpu_tests(void);
extern void run_memory_tests(void);
extern void run_vic_tests(void);
extern void run_cia_tests(void);
extern void run_sid_tests(void);
extern void run_dormann_tests(void);
extern void run_lorenz_tests(void);

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    
    // Initialize random seed for any random tests
    srand((unsigned int)time(NULL));
    
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║              C64 Emulator Test Suite                        ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    clock_t start = clock();
    
    // Run all test suites
    run_cpu_tests();
    run_memory_tests();
    run_vic_tests();
    run_cia_tests();
    run_sid_tests();
    run_dormann_tests();
    run_lorenz_tests();
    
    clock_t end = clock();
    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;
    
    // Summary
    printf("\n");
    printf("══════════════════════════════════════════════════════════════════\n");
    printf("                        TEST SUMMARY\n");
    printf("══════════════════════════════════════════════════════════════════\n");
    printf("\n");
    printf("  Total tests:  %d\n", g_test_ctx.total);
    printf("  " COLOR_GREEN "Passed:      %d" COLOR_RESET "\n", g_test_ctx.passed);
    
    if (g_test_ctx.failed > 0) {
        printf("  " COLOR_RED "Failed:      %d" COLOR_RESET "\n", g_test_ctx.failed);
    } else {
        printf("  Failed:      0\n");
    }
    
    printf("  Time:        %.3f seconds\n", elapsed);
    printf("\n");
    
    if (g_test_ctx.failed == 0) {
        printf(COLOR_GREEN "  ✓ All tests passed!" COLOR_RESET "\n");
    } else {
        printf(COLOR_RED "  ✗ Some tests failed!" COLOR_RESET "\n");
    }
    printf("\n");
    
    return g_test_ctx.failed > 0 ? 1 : 0;
}
