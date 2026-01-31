/**
 * test_framework.h - Simple test framework for C64 emulator
 */

#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// Test result tracking
typedef struct {
    int total;
    int passed;
    int failed;
    const char *current_suite;
} TestContext;

// Global test context - defined in test_main.c
#ifdef TEST_MAIN
TestContext g_test_ctx = {0, 0, 0, NULL};
#else
extern TestContext g_test_ctx;
#endif

// Colors for output
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_RESET   "\033[0m"

// Start a test suite
#define TEST_SUITE(name) \
    do { \
        g_test_ctx.current_suite = name; \
        printf("\n" COLOR_YELLOW "=== Test Suite: %s ===" COLOR_RESET "\n", name); \
    } while(0)

// Define a test
#define TEST(name) \
    static void test_##name(void); \
    static void run_test_##name(void) { \
        g_test_ctx.total++; \
        printf("  %-50s ", #name); \
        fflush(stdout); \
        test_##name(); \
    } \
    static void test_##name(void)

// Run a test
#define RUN_TEST(name) run_test_##name()

// Assertions
#define ASSERT(cond) \
    do { \
        if (!(cond)) { \
            printf(COLOR_RED "FAIL" COLOR_RESET "\n"); \
            printf("    Assertion failed: %s\n", #cond); \
            printf("    At: %s:%d\n", __FILE__, __LINE__); \
            g_test_ctx.failed++; \
            return; \
        } \
    } while(0)

#define ASSERT_EQ(expected, actual) \
    do { \
        if ((expected) != (actual)) { \
            printf(COLOR_RED "FAIL" COLOR_RESET "\n"); \
            printf("    Expected: 0x%X (%d)\n", (unsigned)(expected), (int)(expected)); \
            printf("    Actual:   0x%X (%d)\n", (unsigned)(actual), (int)(actual)); \
            printf("    At: %s:%d\n", __FILE__, __LINE__); \
            g_test_ctx.failed++; \
            return; \
        } \
    } while(0)

#define ASSERT_NEQ(not_expected, actual) \
    do { \
        if ((not_expected) == (actual)) { \
            printf(COLOR_RED "FAIL" COLOR_RESET "\n"); \
            printf("    Expected NOT: 0x%X\n", (unsigned)(not_expected)); \
            printf("    Actual:       0x%X\n", (unsigned)(actual)); \
            printf("    At: %s:%d\n", __FILE__, __LINE__); \
            g_test_ctx.failed++; \
            return; \
        } \
    } while(0)

// Note: ASSERT_TRUE checks if expression is non-zero (truthy)
//       ASSERT_FALSE checks if expression is zero (falsy)
#define ASSERT_TRUE(cond) \
    do { \
        if (!(cond)) { \
            printf(COLOR_RED "FAIL" COLOR_RESET "\n"); \
            printf("    Expected: true (non-zero)\n"); \
            printf("    Actual:   0 (false)\n"); \
            printf("    At: %s:%d\n", __FILE__, __LINE__); \
            g_test_ctx.failed++; \
            return; \
        } \
    } while(0)

#define ASSERT_FALSE(cond) \
    do { \
        if (cond) { \
            printf(COLOR_RED "FAIL" COLOR_RESET "\n"); \
            printf("    Expected: false (0)\n"); \
            printf("    Actual:   0x%X (non-zero)\n", (unsigned)(cond)); \
            printf("    At: %s:%d\n", __FILE__, __LINE__); \
            g_test_ctx.failed++; \
            return; \
        } \
    } while(0)

#define ASSERT_MEM_EQ(addr, expected) \
    do { \
        u8 _val = mem_read_raw(&sys.mem, addr); \
        if (_val != (expected)) { \
            printf(COLOR_RED "FAIL" COLOR_RESET "\n"); \
            printf("    Memory[$%04X]: Expected 0x%02X, got 0x%02X\n", \
                   (unsigned)(addr), (unsigned)(expected), (unsigned)_val); \
            printf("    At: %s:%d\n", __FILE__, __LINE__); \
            g_test_ctx.failed++; \
            return; \
        } \
    } while(0)

// Pass the current test
#define PASS() \
    do { \
        printf(COLOR_GREEN "PASS" COLOR_RESET "\n"); \
        g_test_ctx.passed++; \
    } while(0)

// Print test summary
#define TEST_SUMMARY() \
    do { \
        printf("\n" COLOR_YELLOW "=== Test Summary ===" COLOR_RESET "\n"); \
        printf("  Total:  %d\n", g_test_ctx.total); \
        printf("  Passed: " COLOR_GREEN "%d" COLOR_RESET "\n", g_test_ctx.passed); \
        printf("  Failed: %s%d" COLOR_RESET "\n", \
               g_test_ctx.failed > 0 ? COLOR_RED : COLOR_GREEN, g_test_ctx.failed); \
        printf("\n"); \
    } while(0)

// Return test exit code
#define TEST_EXIT_CODE() (g_test_ctx.failed > 0 ? 1 : 0)

#endif // TEST_FRAMEWORK_H
