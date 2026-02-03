/**
 * @file sid_file.h
 * @brief SID file format parser
 * 
 * Parser for the SID file format (PSID/RSID) used in the High Voltage SID Collection.
 * Supports PSID v1, v2, v2NG, v3, v4 and RSID v2, v3, v4 formats.
 */

#ifndef SID_FILE_H
#define SID_FILE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Magic IDs */
#define SID_MAGIC_PSID  0x50534944  /* 'PSID' */
#define SID_MAGIC_RSID  0x52534944  /* 'RSID' */

/* Header sizes */
#define SID_HEADER_SIZE_V1  0x76
#define SID_HEADER_SIZE_V2  0x7C

/* String field sizes */
#define SID_STRING_LENGTH   32

/**
 * @brief SID file type
 */
typedef enum {
    SID_TYPE_UNKNOWN = 0,
    SID_TYPE_PSID,
    SID_TYPE_RSID
} sid_file_type_t;

/**
 * @brief Video standard (clock)
 */
typedef enum {
    SID_CLOCK_UNKNOWN  = 0,
    SID_CLOCK_PAL      = 1,
    SID_CLOCK_NTSC     = 2,
    SID_CLOCK_PAL_NTSC = 3
} sid_clock_t;

/**
 * @brief SID chip model
 */
typedef enum {
    SID_MODEL_UNKNOWN  = 0,
    SID_MODEL_6581     = 1,
    SID_MODEL_8580     = 2,
    SID_MODEL_6581_8580 = 3
} sid_model_t;

/**
 * @brief SID file flags (extracted from the flags field)
 */
typedef struct {
    bool mus_player;        /* Compute!'s Sidplayer MUS data */
    bool psid_specific;     /* PlaySID specific (PSID) or C64 BASIC flag (RSID) */
    sid_clock_t clock;      /* Video standard */
    sid_model_t sid_model1; /* First SID model */
    sid_model_t sid_model2; /* Second SID model (v3+) */
    sid_model_t sid_model3; /* Third SID model (v4+) */
} sid_flags_t;

/**
 * @brief Parsed SID file structure
 */
typedef struct {
    /* Magic ID and version */
    sid_file_type_t type;
    uint16_t version;
    
    /* Header info */
    uint16_t data_offset;
    uint16_t load_address;      /* 0 means load address is in data */
    uint16_t init_address;
    uint16_t play_address;
    uint16_t songs;
    uint16_t start_song;
    uint32_t speed;             /* Speed flags for each song */
    
    /* Metadata (null-terminated) */
    char name[SID_STRING_LENGTH + 1];
    char author[SID_STRING_LENGTH + 1];
    char released[SID_STRING_LENGTH + 1];
    
    /* v2+ fields */
    sid_flags_t flags;
    uint8_t start_page;         /* Relocation start page */
    uint8_t page_length;        /* Number of free pages */
    uint8_t second_sid_address; /* v3+: Second SID address ($Dxx0) */
    uint8_t third_sid_address;  /* v4+: Third SID address ($Dxx0) */
    
    /* Binary data */
    uint16_t real_load_address; /* Actual load address (from header or data) */
    uint8_t *data;              /* Pointer to C64 binary data (allocated) */
    size_t data_length;         /* Length of binary data */
} sid_file_t;

/**
 * @brief Error codes for SID file parsing
 */
typedef enum {
    SID_OK = 0,
    SID_ERR_NULL_POINTER,
    SID_ERR_FILE_OPEN,
    SID_ERR_FILE_READ,
    SID_ERR_FILE_TOO_SMALL,
    SID_ERR_INVALID_MAGIC,
    SID_ERR_INVALID_VERSION,
    SID_ERR_INVALID_DATA_OFFSET,
    SID_ERR_MEMORY_ALLOC,
    SID_ERR_INVALID_LOAD_ADDRESS,
    SID_ERR_INVALID_RSID
} sid_error_t;

/**
 * @brief Load and parse a SID file from disk
 * 
 * @param filename Path to the SID file
 * @param sid Pointer to sid_file_t structure to fill
 * @return sid_error_t Error code (SID_OK on success)
 */
sid_error_t sid_file_load(const char *filename, sid_file_t *sid);

/**
 * @brief Parse a SID file from a memory buffer
 * 
 * @param buffer Pointer to buffer containing SID file data
 * @param size Size of the buffer
 * @param sid Pointer to sid_file_t structure to fill
 * @return sid_error_t Error code (SID_OK on success)
 */
sid_error_t sid_file_parse(const uint8_t *buffer, size_t size, sid_file_t *sid);

/**
 * @brief Free resources allocated for a SID file
 * 
 * @param sid Pointer to sid_file_t structure to free
 */
void sid_file_free(sid_file_t *sid);

/**
 * @brief Get error message for a SID error code
 * 
 * @param error Error code
 * @return const char* Human-readable error message
 */
const char* sid_error_string(sid_error_t error);

/**
 * @brief Check if a song should use CIA timer (60Hz) or VBI
 * 
 * @param sid Pointer to parsed SID file
 * @param song Song number (1-based)
 * @return true if song uses CIA timer, false for VBI
 */
bool sid_song_uses_cia(const sid_file_t *sid, uint16_t song);

/**
 * @brief Get the full address of the second SID chip
 * 
 * @param sid Pointer to parsed SID file
 * @return uint16_t Full address (e.g., 0xD420) or 0 if no second SID
 */
uint16_t sid_get_second_sid_address(const sid_file_t *sid);

/**
 * @brief Get the full address of the third SID chip
 * 
 * @param sid Pointer to parsed SID file
 * @return uint16_t Full address (e.g., 0xD420) or 0 if no third SID
 */
uint16_t sid_get_third_sid_address(const sid_file_t *sid);

/**
 * @brief Print SID file information to stdout
 * 
 * @param sid Pointer to parsed SID file
 */
void sid_file_print_info(const sid_file_t *sid);

#ifdef __cplusplus
}
#endif

#endif /* SID_FILE_H */
