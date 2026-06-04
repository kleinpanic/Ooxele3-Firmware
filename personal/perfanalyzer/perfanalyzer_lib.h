#pragma once
#include <furi.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// @complexity O(1)
// Match target appid string against thread appid (using strcmp)
bool perfanalyzer_match_appid(const char* thread_appid, const char* target_appid);

// @complexity O(1)
// Format a single heap sample as JSON into buffer
void perfanalyzer_format_sample_json(char* buf, size_t buf_size, uint32_t t_ms, uint32_t heap_bytes);
