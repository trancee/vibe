/**
 * @file test_sid_file.c
 * @brief Unit tests for SID file parser
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sid_file.h"

/* Test framework */
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(condition, msg) do { \
    tests_run++; \
    if (condition) { \
        tests_passed++; \
        printf("  ✓ %s\n", msg); \
    } else { \
        tests_failed++; \
        printf("  ✗ %s\n", msg); \
    } \
} while(0)

#define TEST_ASSERT_EQ(expected, actual, msg) do { \
    tests_run++; \
    if ((expected) == (actual)) { \
        tests_passed++; \
        printf("  ✓ %s\n", msg); \
    } else { \
        tests_failed++; \
        printf("  ✗ %s (expected 0x%X, got 0x%X)\n", msg, (unsigned)(expected), (unsigned)(actual)); \
    } \
} while(0)

#define TEST_ASSERT_STR_EQ(expected, actual, msg) do { \
    tests_run++; \
    if (strcmp((expected), (actual)) == 0) { \
        tests_passed++; \
        printf("  ✓ %s\n", msg); \
    } else { \
        tests_failed++; \
        printf("  ✗ %s (expected \"%s\", got \"%s\")\n", msg, (expected), (actual)); \
    } \
} while(0)

/* ============================================================================
 * Test Data: Minimal valid PSID v1 header
 * ============================================================================ */

/* Create a minimal valid PSID v1 file in memory */
static uint8_t* create_psid_v1(size_t *size) {
    /* Header size (0x76) + 2 bytes load addr + 4 bytes data */
    *size = 0x76 + 2 + 4;
    uint8_t *buf = calloc(1, *size);
    if (!buf) return NULL;
    
    /* Magic ID: PSID */
    buf[0x00] = 'P'; buf[0x01] = 'S'; buf[0x02] = 'I'; buf[0x03] = 'D';
    /* Version: 1 */
    buf[0x04] = 0x00; buf[0x05] = 0x01;
    /* Data offset: 0x0076 */
    buf[0x06] = 0x00; buf[0x07] = 0x76;
    /* Load address: 0 (in data) */
    buf[0x08] = 0x00; buf[0x09] = 0x00;
    /* Init address: 0x1000 */
    buf[0x0A] = 0x10; buf[0x0B] = 0x00;
    /* Play address: 0x1003 */
    buf[0x0C] = 0x10; buf[0x0D] = 0x03;
    /* Songs: 3 */
    buf[0x0E] = 0x00; buf[0x0F] = 0x03;
    /* Start song: 1 */
    buf[0x10] = 0x00; buf[0x11] = 0x01;
    /* Speed: bit 1 set (song 2 uses CIA) */
    buf[0x12] = 0x00; buf[0x13] = 0x00; buf[0x14] = 0x00; buf[0x15] = 0x02;
    /* Name */
    strcpy((char*)&buf[0x16], "Test Tune");
    /* Author */
    strcpy((char*)&buf[0x36], "Test Author");
    /* Released */
    strcpy((char*)&buf[0x56], "2024 Test");
    
    /* Data: load address (little-endian) + code */
    buf[0x76] = 0x00; buf[0x77] = 0x10;  /* $1000 */
    buf[0x78] = 0x4C; buf[0x79] = 0x00;  /* JMP $1000 */
    buf[0x7A] = 0x10; buf[0x7B] = 0x60;  /* RTS */
    
    return buf;
}

/* Create a minimal valid PSID v2 file in memory */
static uint8_t* create_psid_v2(size_t *size) {
    /* Header size (0x7C) + 2 bytes load addr + 4 bytes data */
    *size = 0x7C + 2 + 4;
    uint8_t *buf = calloc(1, *size);
    if (!buf) return NULL;
    
    /* Magic ID: PSID */
    buf[0x00] = 'P'; buf[0x01] = 'S'; buf[0x02] = 'I'; buf[0x03] = 'D';
    /* Version: 2 */
    buf[0x04] = 0x00; buf[0x05] = 0x02;
    /* Data offset: 0x007C */
    buf[0x06] = 0x00; buf[0x07] = 0x7C;
    /* Load address: 0 (in data) */
    buf[0x08] = 0x00; buf[0x09] = 0x00;
    /* Init address: 0x1000 */
    buf[0x0A] = 0x10; buf[0x0B] = 0x00;
    /* Play address: 0x1003 */
    buf[0x0C] = 0x10; buf[0x0D] = 0x03;
    /* Songs: 1 */
    buf[0x0E] = 0x00; buf[0x0F] = 0x01;
    /* Start song: 1 */
    buf[0x10] = 0x00; buf[0x11] = 0x01;
    /* Speed: 0 (VBI) */
    buf[0x12] = 0x00; buf[0x13] = 0x00; buf[0x14] = 0x00; buf[0x15] = 0x00;
    /* Name */
    strcpy((char*)&buf[0x16], "PSID v2 Test");
    /* Author */
    strcpy((char*)&buf[0x36], "Unit Test");
    /* Released */
    strcpy((char*)&buf[0x56], "2024");
    
    /* Flags: PAL (01), MOS6581 (01) = 0x0014 */
    buf[0x76] = 0x00; buf[0x77] = 0x14;
    /* Start page: 0x20 */
    buf[0x78] = 0x20;
    /* Page length: 0x10 */
    buf[0x79] = 0x10;
    /* Second SID address: 0 */
    buf[0x7A] = 0x00;
    /* Third SID address: 0 */
    buf[0x7B] = 0x00;
    
    /* Data: load address (little-endian) + code */
    buf[0x7C] = 0x00; buf[0x7D] = 0x10;  /* $1000 */
    buf[0x7E] = 0x4C; buf[0x7F] = 0x00;  /* JMP $1000 */
    buf[0x80] = 0x10; buf[0x81] = 0x60;  /* RTS */
    
    return buf;
}

/* Create a minimal valid RSID v2 file in memory */
static uint8_t* create_rsid_v2(size_t *size) {
    /* Header size (0x7C) + 2 bytes load addr + 4 bytes data */
    *size = 0x7C + 2 + 4;
    uint8_t *buf = calloc(1, *size);
    if (!buf) return NULL;
    
    /* Magic ID: RSID */
    buf[0x00] = 'R'; buf[0x01] = 'S'; buf[0x02] = 'I'; buf[0x03] = 'D';
    /* Version: 2 */
    buf[0x04] = 0x00; buf[0x05] = 0x02;
    /* Data offset: 0x007C */
    buf[0x06] = 0x00; buf[0x07] = 0x7C;
    /* Load address: 0 (required for RSID) */
    buf[0x08] = 0x00; buf[0x09] = 0x00;
    /* Init address: 0x1000 */
    buf[0x0A] = 0x10; buf[0x0B] = 0x00;
    /* Play address: 0 (required for RSID) */
    buf[0x0C] = 0x00; buf[0x0D] = 0x00;
    /* Songs: 1 */
    buf[0x0E] = 0x00; buf[0x0F] = 0x01;
    /* Start song: 1 */
    buf[0x10] = 0x00; buf[0x11] = 0x01;
    /* Speed: 0 (required for RSID) */
    buf[0x12] = 0x00; buf[0x13] = 0x00; buf[0x14] = 0x00; buf[0x15] = 0x00;
    /* Name */
    strcpy((char*)&buf[0x16], "RSID Test");
    /* Author */
    strcpy((char*)&buf[0x36], "Unit Test");
    /* Released */
    strcpy((char*)&buf[0x56], "2024");
    
    /* Flags: NTSC (10), MOS8580 (10) = 0x0028 */
    buf[0x76] = 0x00; buf[0x77] = 0x28;
    /* Start page, page length, SID addresses */
    buf[0x78] = 0x00; buf[0x79] = 0x00;
    buf[0x7A] = 0x00; buf[0x7B] = 0x00;
    
    /* Data: load address (little-endian, must be >= $07E8) + code */
    buf[0x7C] = 0x00; buf[0x7D] = 0x10;  /* $1000 */
    buf[0x7E] = 0x4C; buf[0x7F] = 0x00;  /* JMP $1000 */
    buf[0x80] = 0x10; buf[0x81] = 0x60;  /* RTS */
    
    return buf;
}

/* ============================================================================
 * Error Handling Tests
 * ============================================================================ */

static void test_error_handling(void) {
    printf("\n=== Error Handling Tests ===\n");
    
    sid_file_t sid;
    sid_error_t err;
    
    /* Test null pointer */
    err = sid_file_parse(NULL, 100, &sid);
    TEST_ASSERT_EQ(SID_ERR_NULL_POINTER, err, "NULL buffer returns error");
    
    uint8_t dummy[10] = {0};
    err = sid_file_parse(dummy, 10, NULL);
    TEST_ASSERT_EQ(SID_ERR_NULL_POINTER, err, "NULL sid struct returns error");
    
    /* Test file too small */
    err = sid_file_parse(dummy, 10, &sid);
    TEST_ASSERT_EQ(SID_ERR_FILE_TOO_SMALL, err, "Small file returns error");
    
    /* Test invalid magic */
    uint8_t bad_magic[0x80] = {0};
    bad_magic[0] = 'X'; bad_magic[1] = 'S'; bad_magic[2] = 'I'; bad_magic[3] = 'D';
    bad_magic[4] = 0x00; bad_magic[5] = 0x01;
    bad_magic[6] = 0x00; bad_magic[7] = 0x76;
    err = sid_file_parse(bad_magic, 0x80, &sid);
    TEST_ASSERT_EQ(SID_ERR_INVALID_MAGIC, err, "Invalid magic returns error");
    
    /* Test error strings */
    TEST_ASSERT(strcmp(sid_error_string(SID_OK), "Success") == 0, 
                "SID_OK error string correct");
    TEST_ASSERT(strlen(sid_error_string(SID_ERR_INVALID_MAGIC)) > 0,
                "Error strings are non-empty");
}

/* ============================================================================
 * PSID v1 Parsing Tests
 * ============================================================================ */

static void test_psid_v1_parsing(void) {
    printf("\n=== PSID v1 Parsing Tests ===\n");
    
    size_t size;
    uint8_t *buf = create_psid_v1(&size);
    TEST_ASSERT(buf != NULL, "Test buffer allocated");
    if (!buf) return;
    
    sid_file_t sid;
    sid_error_t err = sid_file_parse(buf, size, &sid);
    TEST_ASSERT_EQ(SID_OK, err, "PSID v1 parsed successfully");
    
    TEST_ASSERT_EQ(SID_TYPE_PSID, sid.type, "Type is PSID");
    TEST_ASSERT_EQ(1, sid.version, "Version is 1");
    TEST_ASSERT_EQ(0x0076, sid.data_offset, "Data offset is 0x76");
    TEST_ASSERT_EQ(0x1000, sid.real_load_address, "Load address is $1000");
    TEST_ASSERT_EQ(0x1000, sid.init_address, "Init address is $1000");
    TEST_ASSERT_EQ(0x1003, sid.play_address, "Play address is $1003");
    TEST_ASSERT_EQ(3, sid.songs, "Song count is 3");
    TEST_ASSERT_EQ(1, sid.start_song, "Start song is 1");
    
    TEST_ASSERT_STR_EQ("Test Tune", sid.name, "Name parsed correctly");
    TEST_ASSERT_STR_EQ("Test Author", sid.author, "Author parsed correctly");
    TEST_ASSERT_STR_EQ("2024 Test", sid.released, "Released parsed correctly");
    
    /* Test speed flags */
    TEST_ASSERT(!sid_song_uses_cia(&sid, 1), "Song 1 uses VBI");
    TEST_ASSERT(sid_song_uses_cia(&sid, 2), "Song 2 uses CIA");
    TEST_ASSERT(!sid_song_uses_cia(&sid, 3), "Song 3 uses VBI");
    
    /* Test data */
    TEST_ASSERT_EQ(4, sid.data_length, "Data length is 4 bytes");
    TEST_ASSERT(sid.data != NULL, "Data pointer is valid");
    if (sid.data) {
        TEST_ASSERT_EQ(0x4C, sid.data[0], "First byte is JMP opcode");
    }
    
    sid_file_free(&sid);
    free(buf);
}

/* ============================================================================
 * PSID v2 Parsing Tests
 * ============================================================================ */

static void test_psid_v2_parsing(void) {
    printf("\n=== PSID v2 Parsing Tests ===\n");
    
    size_t size;
    uint8_t *buf = create_psid_v2(&size);
    TEST_ASSERT(buf != NULL, "Test buffer allocated");
    if (!buf) return;
    
    sid_file_t sid;
    sid_error_t err = sid_file_parse(buf, size, &sid);
    TEST_ASSERT_EQ(SID_OK, err, "PSID v2 parsed successfully");
    
    TEST_ASSERT_EQ(SID_TYPE_PSID, sid.type, "Type is PSID");
    TEST_ASSERT_EQ(2, sid.version, "Version is 2");
    TEST_ASSERT_EQ(0x007C, sid.data_offset, "Data offset is 0x7C");
    
    TEST_ASSERT_STR_EQ("PSID v2 Test", sid.name, "Name parsed correctly");
    
    /* Test v2 flags */
    TEST_ASSERT_EQ(SID_CLOCK_PAL, sid.flags.clock, "Clock is PAL");
    TEST_ASSERT_EQ(SID_MODEL_6581, sid.flags.sid_model1, "SID model is 6581");
    TEST_ASSERT(!sid.flags.mus_player, "MUS player flag is false");
    TEST_ASSERT(!sid.flags.psid_specific, "PSID specific flag is false");
    
    /* Test relocation info */
    TEST_ASSERT_EQ(0x20, sid.start_page, "Start page is 0x20");
    TEST_ASSERT_EQ(0x10, sid.page_length, "Page length is 0x10");
    
    sid_file_free(&sid);
    free(buf);
}

/* ============================================================================
 * RSID Parsing Tests
 * ============================================================================ */

static void test_rsid_parsing(void) {
    printf("\n=== RSID Parsing Tests ===\n");
    
    size_t size;
    uint8_t *buf = create_rsid_v2(&size);
    TEST_ASSERT(buf != NULL, "Test buffer allocated");
    if (!buf) return;
    
    sid_file_t sid;
    sid_error_t err = sid_file_parse(buf, size, &sid);
    TEST_ASSERT_EQ(SID_OK, err, "RSID v2 parsed successfully");
    
    TEST_ASSERT_EQ(SID_TYPE_RSID, sid.type, "Type is RSID");
    TEST_ASSERT_EQ(2, sid.version, "Version is 2");
    TEST_ASSERT_EQ(0, sid.play_address, "Play address is 0 (uses IRQ)");
    
    /* Test v2 flags */
    TEST_ASSERT_EQ(SID_CLOCK_NTSC, sid.flags.clock, "Clock is NTSC");
    TEST_ASSERT_EQ(SID_MODEL_8580, sid.flags.sid_model1, "SID model is 8580");
    
    sid_file_free(&sid);
    free(buf);
}

/* ============================================================================
 * RSID Validation Tests
 * ============================================================================ */

static void test_rsid_validation(void) {
    printf("\n=== RSID Validation Tests ===\n");
    
    size_t size;
    uint8_t *buf;
    sid_file_t sid;
    sid_error_t err;
    
    /* Test RSID with non-zero load address (should fail) */
    buf = create_rsid_v2(&size);
    TEST_ASSERT(buf != NULL, "Test buffer allocated");
    if (!buf) return;
    buf[0x08] = 0x10; buf[0x09] = 0x00;  /* Set load address to $1000 */
    err = sid_file_parse(buf, size, &sid);
    TEST_ASSERT_EQ(SID_ERR_INVALID_RSID, err, "RSID with load address fails");
    free(buf);
    
    /* Test RSID with non-zero play address (should fail) */
    buf = create_rsid_v2(&size);
    buf[0x0C] = 0x10; buf[0x0D] = 0x03;  /* Set play address to $1003 */
    err = sid_file_parse(buf, size, &sid);
    TEST_ASSERT_EQ(SID_ERR_INVALID_RSID, err, "RSID with play address fails");
    free(buf);
    
    /* Test RSID with non-zero speed (should fail) */
    buf = create_rsid_v2(&size);
    buf[0x15] = 0x01;  /* Set speed bit */
    err = sid_file_parse(buf, size, &sid);
    TEST_ASSERT_EQ(SID_ERR_INVALID_RSID, err, "RSID with speed flags fails");
    free(buf);
    
    /* Test RSID with load address < $07E8 (should fail) */
    buf = create_rsid_v2(&size);
    buf[0x7C] = 0xE0; buf[0x7D] = 0x07;  /* Set data load address to $07E0 (little-endian) */
    err = sid_file_parse(buf, size, &sid);
    TEST_ASSERT_EQ(SID_ERR_INVALID_LOAD_ADDRESS, err, "RSID with low load address fails");
    free(buf);
}

/* ============================================================================
 * Multi-SID Address Tests
 * ============================================================================ */

static void test_multi_sid_addresses(void) {
    printf("\n=== Multi-SID Address Tests ===\n");
    
    size_t size;
    uint8_t *buf = create_psid_v2(&size);
    TEST_ASSERT(buf != NULL, "Test buffer allocated");
    if (!buf) return;
    
    sid_file_t sid;
    
    /* Test v2 has no second SID */
    sid_file_parse(buf, size, &sid);
    TEST_ASSERT_EQ(0, sid_get_second_sid_address(&sid), "v2 has no second SID");
    TEST_ASSERT_EQ(0, sid_get_third_sid_address(&sid), "v2 has no third SID");
    sid_file_free(&sid);
    
    /* Upgrade to v3 with second SID at $D420 */
    buf[0x05] = 0x03;  /* Version 3 */
    buf[0x7A] = 0x42;  /* Second SID at $D420 */
    sid_file_parse(buf, size, &sid);
    TEST_ASSERT_EQ(0xD420, sid_get_second_sid_address(&sid), "Second SID at $D420");
    TEST_ASSERT_EQ(0, sid_get_third_sid_address(&sid), "v3 has no third SID");
    sid_file_free(&sid);
    
    /* Upgrade to v4 with third SID at $D500 */
    buf[0x05] = 0x04;  /* Version 4 */
    buf[0x7B] = 0x50;  /* Third SID at $D500 */
    sid_file_parse(buf, size, &sid);
    TEST_ASSERT_EQ(0xD420, sid_get_second_sid_address(&sid), "Second SID still at $D420");
    TEST_ASSERT_EQ(0xD500, sid_get_third_sid_address(&sid), "Third SID at $D500");
    sid_file_free(&sid);
    
    /* Test invalid SID address (odd value) */
    buf[0x7A] = 0x43;  /* Invalid: odd address */
    sid_file_parse(buf, size, &sid);
    TEST_ASSERT_EQ(0, sid_get_second_sid_address(&sid), "Odd address returns 0");
    sid_file_free(&sid);
    
    /* Test invalid SID address (in color RAM range) */
    buf[0x7A] = 0x80;  /* Invalid: $D800 range */
    sid_file_parse(buf, size, &sid);
    TEST_ASSERT_EQ(0, sid_get_second_sid_address(&sid), "Color RAM range returns 0");
    sid_file_free(&sid);
    
    free(buf);
}

/* ============================================================================
 * Real File Test (if available)
 * ============================================================================ */

static void test_real_file(void) {
    printf("\n=== Real File Test ===\n");
    
    sid_file_t sid;
    sid_error_t err = sid_file_load("roms/Ikari_Union.sid", &sid);
    
    if (err == SID_ERR_FILE_OPEN) {
        printf("  - Skipped (file not found)\n");
        return;
    }
    
    TEST_ASSERT_EQ(SID_OK, err, "Real SID file loaded successfully");
    TEST_ASSERT_EQ(SID_TYPE_PSID, sid.type, "Type is PSID");
    TEST_ASSERT(sid.version >= 1 && sid.version <= 4, "Version is valid");
    TEST_ASSERT(sid.songs >= 1, "Has at least one song");
    TEST_ASSERT(sid.data != NULL, "Data is loaded");
    TEST_ASSERT(sid.data_length > 0, "Data has content");
    TEST_ASSERT(strlen(sid.name) > 0, "Name is not empty");
    TEST_ASSERT(strlen(sid.author) > 0, "Author is not empty");
    
    printf("  - Loaded: \"%s\" by %s\n", sid.name, sid.author);
    
    sid_file_free(&sid);
}

/* ============================================================================
 * Memory Management Tests
 * ============================================================================ */

static void test_memory_management(void) {
    printf("\n=== Memory Management Tests ===\n");
    
    size_t size;
    uint8_t *buf = create_psid_v1(&size);
    TEST_ASSERT(buf != NULL, "Test buffer allocated");
    if (!buf) return;
    
    sid_file_t sid;
    sid_file_parse(buf, size, &sid);
    
    TEST_ASSERT(sid.data != NULL, "Data allocated after parse");
    
    sid_file_free(&sid);
    TEST_ASSERT(sid.data == NULL, "Data freed after sid_file_free");
    TEST_ASSERT_EQ(0, sid.data_length, "Data length reset after free");
    
    /* Test double free safety */
    sid_file_free(&sid);  /* Should not crash */
    TEST_ASSERT(1, "Double free is safe");
    
    /* Test free with NULL */
    sid_file_free(NULL);  /* Should not crash */
    TEST_ASSERT(1, "Free NULL is safe");
    
    free(buf);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(void) {
    printf("SID File Parser Unit Tests\n");
    printf("==========================\n");
    
    test_error_handling();
    test_psid_v1_parsing();
    test_psid_v2_parsing();
    test_rsid_parsing();
    test_rsid_validation();
    test_multi_sid_addresses();
    test_real_file();
    test_memory_management();
    
    printf("\n==========================\n");
    printf("Tests: %d | Passed: %d | Failed: %d\n", 
           tests_run, tests_passed, tests_failed);
    
    return tests_failed > 0 ? 1 : 0;
}
