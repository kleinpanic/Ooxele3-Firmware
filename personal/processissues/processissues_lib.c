#include "processissues_lib.h"
#include <stdio.h>
#include <string.h>

bool processissues_parse_record(const uint8_t* bytes, size_t len, CrashRecord* out) {
    if(!bytes || !out || len < 40) return false;

    out->timestamp = *(uint32_t*)bytes;
    memcpy(out->tag, bytes + 4, sizeof(out->tag) - 1);
    out->tag[sizeof(out->tag) - 1] = '\0';
    out->error_code = *(uint32_t*)(bytes + 36);
    return true;
}

void processissues_format_summary(char* buf, size_t buf_size, size_t crash_count, const char* last_tag) {
    if(!buf) return;
    snprintf(buf, buf_size, "Crashes: %zu | Last: %s",
             crash_count, last_tag ? last_tag : "none");
}
