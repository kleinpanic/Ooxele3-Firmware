#pragma once
#include <furi.h>
#include <stddef.h>

// @complexity O(1)
// Format a single thread info struct into a text buffer
void flipperhtop_render_row(char* buf, size_t buf_size, const FuriThreadListItem* item);
