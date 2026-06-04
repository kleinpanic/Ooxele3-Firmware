#include "perfanalyzer_lib.h"
#include <stdio.h>
#include <string.h>

bool perfanalyzer_match_appid(const char* thread_appid, const char* target_appid) {
    return (thread_appid && target_appid && strcmp(thread_appid, target_appid) == 0);
}

void perfanalyzer_format_sample_json(char* buf, size_t buf_size, uint32_t t_ms, uint32_t heap_bytes) {
    if(!buf) return;
    snprintf(buf, buf_size, "{\"t_ms\":%lu,\"heap_bytes\":%lu}", (unsigned long)t_ms, (unsigned long)heap_bytes);
}
