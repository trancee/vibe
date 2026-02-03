/**
 * @file sid_file.c
 * @brief SID file format parser implementation
 */

#include "sid_file.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Read a big-endian 16-bit value from buffer */
static uint16_t read_be16(const uint8_t *buf) {
    return ((uint16_t)buf[0] << 8) | (uint16_t)buf[1];
}

/* Read a big-endian 32-bit value from buffer */
static uint32_t read_be32(const uint8_t *buf) {
    return ((uint32_t)buf[0] << 24) | 
           ((uint32_t)buf[1] << 16) | 
           ((uint32_t)buf[2] << 8)  | 
           (uint32_t)buf[3];
}

/* Read a little-endian 16-bit value from buffer */
static uint16_t read_le16(const uint8_t *buf) {
    return (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
}

/* Copy and null-terminate a string field */
static void copy_string_field(char *dest, const uint8_t *src, size_t max_len) {
    size_t i;
    for (i = 0; i < max_len && src[i] != '\0'; i++) {
        dest[i] = (char)src[i];
    }
    dest[i] = '\0';
}

/* Parse the flags field (v2+) */
static void parse_flags(uint16_t flags_word, sid_flags_t *flags) {
    flags->mus_player = (flags_word & 0x01) != 0;
    flags->psid_specific = (flags_word & 0x02) != 0;
    flags->clock = (sid_clock_t)((flags_word >> 2) & 0x03);
    flags->sid_model1 = (sid_model_t)((flags_word >> 4) & 0x03);
    flags->sid_model2 = (sid_model_t)((flags_word >> 6) & 0x03);
    flags->sid_model3 = (sid_model_t)((flags_word >> 8) & 0x03);
}

/* Check if a SID address byte is valid */
static bool is_valid_sid_address(uint8_t addr) {
    if (addr == 0x00) {
        return false;  /* No second/third SID */
    }
    if (addr < 0x42) {
        return false;  /* Too low ($D000-$D410) */
    }
    if (addr > 0x7F && addr < 0xE0) {
        return false;  /* Invalid range ($D800-$DDF0) */
    }
    if (addr > 0xFE) {
        return false;  /* Too high */
    }
    if (addr & 0x01) {
        return false;  /* Must be even */
    }
    return true;
}

sid_error_t sid_file_parse(const uint8_t *buffer, size_t size, sid_file_t *sid) {
    uint32_t magic;
    uint16_t flags_word;
    
    if (buffer == NULL || sid == NULL) {
        return SID_ERR_NULL_POINTER;
    }
    
    /* Initialize structure */
    memset(sid, 0, sizeof(sid_file_t));
    
    /* Check minimum size for v1 header */
    if (size < SID_HEADER_SIZE_V1) {
        return SID_ERR_FILE_TOO_SMALL;
    }
    
    /* Parse magic ID */
    magic = read_be32(buffer);
    if (magic == SID_MAGIC_PSID) {
        sid->type = SID_TYPE_PSID;
    } else if (magic == SID_MAGIC_RSID) {
        sid->type = SID_TYPE_RSID;
    } else {
        return SID_ERR_INVALID_MAGIC;
    }
    
    /* Parse version */
    sid->version = read_be16(buffer + 0x04);
    if (sid->version < 1 || sid->version > 4) {
        return SID_ERR_INVALID_VERSION;
    }
    
    /* RSID requires version 2+ */
    if (sid->type == SID_TYPE_RSID && sid->version < 2) {
        return SID_ERR_INVALID_RSID;
    }
    
    /* Parse data offset */
    sid->data_offset = read_be16(buffer + 0x06);
    
    /* Validate data offset based on version */
    if (sid->version == 1) {
        if (sid->data_offset != SID_HEADER_SIZE_V1) {
            return SID_ERR_INVALID_DATA_OFFSET;
        }
    } else {
        if (sid->data_offset != SID_HEADER_SIZE_V2) {
            return SID_ERR_INVALID_DATA_OFFSET;
        }
        /* Check that we have enough data for v2+ header */
        if (size < SID_HEADER_SIZE_V2) {
            return SID_ERR_FILE_TOO_SMALL;
        }
    }
    
    /* Parse common v1 fields */
    sid->load_address = read_be16(buffer + 0x08);
    sid->init_address = read_be16(buffer + 0x0A);
    sid->play_address = read_be16(buffer + 0x0C);
    sid->songs = read_be16(buffer + 0x0E);
    sid->start_song = read_be16(buffer + 0x10);
    sid->speed = read_be32(buffer + 0x12);
    
    /* Parse string fields */
    copy_string_field(sid->name, buffer + 0x16, SID_STRING_LENGTH);
    copy_string_field(sid->author, buffer + 0x36, SID_STRING_LENGTH);
    copy_string_field(sid->released, buffer + 0x56, SID_STRING_LENGTH);
    
    /* Parse v2+ fields */
    if (sid->version >= 2) {
        flags_word = read_be16(buffer + 0x76);
        parse_flags(flags_word, &sid->flags);
        
        sid->start_page = buffer[0x78];
        sid->page_length = buffer[0x79];
        sid->second_sid_address = buffer[0x7A];
        sid->third_sid_address = buffer[0x7B];
    }
    
    /* Validate RSID specific requirements */
    if (sid->type == SID_TYPE_RSID) {
        if (sid->load_address != 0) {
            return SID_ERR_INVALID_RSID;
        }
        if (sid->play_address != 0) {
            return SID_ERR_INVALID_RSID;
        }
        if (sid->speed != 0) {
            return SID_ERR_INVALID_RSID;
        }
    }
    
    /* Check if there's data beyond the header */
    if (size <= sid->data_offset) {
        return SID_ERR_FILE_TOO_SMALL;
    }
    
    /* Calculate data length */
    sid->data_length = size - sid->data_offset;
    
    /* Determine real load address */
    if (sid->load_address == 0) {
        /* Load address is in first two bytes of data */
        if (sid->data_length < 2) {
            return SID_ERR_INVALID_LOAD_ADDRESS;
        }
        sid->real_load_address = read_le16(buffer + sid->data_offset);
        
        /* Allocate and copy data (skip load address bytes) */
        sid->data_length -= 2;
        sid->data = (uint8_t *)malloc(sid->data_length);
        if (sid->data == NULL) {
            return SID_ERR_MEMORY_ALLOC;
        }
        memcpy(sid->data, buffer + sid->data_offset + 2, sid->data_length);
    } else {
        sid->real_load_address = sid->load_address;
        
        /* Allocate and copy data */
        sid->data = (uint8_t *)malloc(sid->data_length);
        if (sid->data == NULL) {
            return SID_ERR_MEMORY_ALLOC;
        }
        memcpy(sid->data, buffer + sid->data_offset, sid->data_length);
    }
    
    /* Validate RSID load address range */
    if (sid->type == SID_TYPE_RSID && sid->real_load_address < 0x07E8) {
        free(sid->data);
        sid->data = NULL;
        return SID_ERR_INVALID_LOAD_ADDRESS;
    }
    
    return SID_OK;
}

sid_error_t sid_file_load(const char *filename, sid_file_t *sid) {
    FILE *file;
    uint8_t *buffer;
    size_t size;
    sid_error_t result;
    
    if (filename == NULL || sid == NULL) {
        return SID_ERR_NULL_POINTER;
    }
    
    file = fopen(filename, "rb");
    if (file == NULL) {
        return SID_ERR_FILE_OPEN;
    }
    
    /* Get file size */
    fseek(file, 0, SEEK_END);
    size = (size_t)ftell(file);
    fseek(file, 0, SEEK_SET);
    
    /* Allocate buffer */
    buffer = (uint8_t *)malloc(size);
    if (buffer == NULL) {
        fclose(file);
        return SID_ERR_MEMORY_ALLOC;
    }
    
    /* Read file */
    if (fread(buffer, 1, size, file) != size) {
        free(buffer);
        fclose(file);
        return SID_ERR_FILE_READ;
    }
    
    fclose(file);
    
    /* Parse the buffer */
    result = sid_file_parse(buffer, size, sid);
    
    free(buffer);
    return result;
}

void sid_file_free(sid_file_t *sid) {
    if (sid != NULL && sid->data != NULL) {
        free(sid->data);
        sid->data = NULL;
        sid->data_length = 0;
    }
}

const char* sid_error_string(sid_error_t error) {
    switch (error) {
        case SID_OK:
            return "Success";
        case SID_ERR_NULL_POINTER:
            return "Null pointer argument";
        case SID_ERR_FILE_OPEN:
            return "Failed to open file";
        case SID_ERR_FILE_READ:
            return "Failed to read file";
        case SID_ERR_FILE_TOO_SMALL:
            return "File is too small to be a valid SID file";
        case SID_ERR_INVALID_MAGIC:
            return "Invalid magic ID (not PSID or RSID)";
        case SID_ERR_INVALID_VERSION:
            return "Invalid or unsupported SID version";
        case SID_ERR_INVALID_DATA_OFFSET:
            return "Invalid data offset in header";
        case SID_ERR_MEMORY_ALLOC:
            return "Memory allocation failed";
        case SID_ERR_INVALID_LOAD_ADDRESS:
            return "Invalid load address";
        case SID_ERR_INVALID_RSID:
            return "Invalid RSID file (failed validation)";
        default:
            return "Unknown error";
    }
}

bool sid_song_uses_cia(const sid_file_t *sid, uint16_t song) {
    uint16_t bit_index;
    
    if (sid == NULL || song == 0 || song > sid->songs) {
        return false;
    }
    
    /* RSID files always report 0 speed (not meaningful) */
    if (sid->type == SID_TYPE_RSID) {
        return false;
    }
    
    /* Calculate bit index (song 1 = bit 0, etc.) */
    bit_index = song - 1;
    
    /* For v2NG+ with psid_specific flag cleared, song 32's speed applies to 33+ */
    if (sid->version >= 2 && !sid->flags.psid_specific) {
        if (bit_index >= 32) {
            bit_index = 31;
        }
    } else {
        /* For v1/v2 or psid_specific, wrap around */
        bit_index = bit_index % 32;
    }
    
    return (sid->speed & (1U << bit_index)) != 0;
}

uint16_t sid_get_second_sid_address(const sid_file_t *sid) {
    if (sid == NULL || sid->version < 3) {
        return 0;
    }
    
    if (!is_valid_sid_address(sid->second_sid_address)) {
        return 0;
    }
    
    return 0xD000 | ((uint16_t)sid->second_sid_address << 4);
}

uint16_t sid_get_third_sid_address(const sid_file_t *sid) {
    if (sid == NULL || sid->version < 4) {
        return 0;
    }
    
    if (!is_valid_sid_address(sid->third_sid_address)) {
        return 0;
    }
    
    /* Third SID cannot be same as second SID */
    if (sid->third_sid_address == sid->second_sid_address) {
        return 0;
    }
    
    return 0xD000 | ((uint16_t)sid->third_sid_address << 4);
}

static const char* sid_type_string(sid_file_type_t type) {
    switch (type) {
        case SID_TYPE_PSID: return "PSID";
        case SID_TYPE_RSID: return "RSID";
        default: return "Unknown";
    }
}

static const char* sid_clock_string(sid_clock_t clock) {
    switch (clock) {
        case SID_CLOCK_PAL: return "PAL";
        case SID_CLOCK_NTSC: return "NTSC";
        case SID_CLOCK_PAL_NTSC: return "PAL/NTSC";
        default: return "Unknown";
    }
}

static const char* sid_model_string(sid_model_t model) {
    switch (model) {
        case SID_MODEL_6581: return "MOS6581";
        case SID_MODEL_8580: return "MOS8580";
        case SID_MODEL_6581_8580: return "MOS6581/MOS8580";
        default: return "Unknown";
    }
}

void sid_file_print_info(const sid_file_t *sid) {
    uint16_t addr;
    
    if (sid == NULL) {
        printf("(null)\n");
        return;
    }
    
    printf("=== SID File Information ===\n");
    printf("Type:           %s v%d\n", sid_type_string(sid->type), sid->version);
    printf("Name:           %s\n", sid->name);
    printf("Author:         %s\n", sid->author);
    printf("Released:       %s\n", sid->released);
    printf("\n");
    printf("Load Address:   $%04X\n", sid->real_load_address);
    printf("Init Address:   $%04X\n", sid->init_address ? sid->init_address : sid->real_load_address);
    printf("Play Address:   $%04X%s\n", sid->play_address, 
           sid->play_address == 0 ? " (uses IRQ handler)" : "");
    printf("Songs:          %d (default: %d)\n", sid->songs, sid->start_song);
    printf("Data Length:    %zu bytes\n", sid->data_length);
    
    if (sid->version >= 2) {
        printf("\n");
        printf("Clock:          %s\n", sid_clock_string(sid->flags.clock));
        printf("SID Model:      %s\n", sid_model_string(sid->flags.sid_model1));
        
        if (sid->flags.mus_player) {
            printf("Format:         Compute!'s Sidplayer MUS\n");
        }
        if (sid->flags.psid_specific) {
            if (sid->type == SID_TYPE_PSID) {
                printf("Note:           PlaySID specific\n");
            } else {
                printf("Note:           Uses C64 BASIC\n");
            }
        }
        
        if (sid->start_page != 0 && sid->start_page != 0xFF) {
            printf("Reloc Range:    $%02X00-$%02X00 (%d pages)\n", 
                   sid->start_page, 
                   sid->start_page + sid->page_length,
                   sid->page_length);
        } else if (sid->start_page == 0xFF) {
            printf("Relocation:     Not possible\n");
        }
        
        addr = sid_get_second_sid_address(sid);
        if (addr != 0) {
            printf("Second SID:     $%04X (%s)\n", addr, 
                   sid_model_string(sid->flags.sid_model2));
        }
        
        addr = sid_get_third_sid_address(sid);
        if (addr != 0) {
            printf("Third SID:      $%04X (%s)\n", addr,
                   sid_model_string(sid->flags.sid_model3));
        }
    }
    
    printf("============================\n");
}
