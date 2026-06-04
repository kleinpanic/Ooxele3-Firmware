#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Crash record structure (parsed from /int/.crash.log per CRASH_LOG_FORMAT.md)
typedef struct {
    uint32_t timestamp;
    char tag[32];
    uint32_t error_code;
} CrashRecord;

// @complexity O(1)
// Parse raw bytes from crash log into structured CrashRecord
bool processissues_parse_record(const uint8_t* bytes, size_t len, CrashRecord* out);

// @complexity O(1)
// Format a crash record into summary text buffer
void processissues_format_summary(char* buf, size_t buf_size, size_t crash_count, const char* last_tag);
