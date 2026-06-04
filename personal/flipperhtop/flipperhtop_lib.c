#include "flipperhtop_lib.h"
#include <stdio.h>
#include <string.h>

void flipperhtop_render_row(char* buf, size_t buf_size, const FuriThreadListItem* item) {
    if(!buf || !item) return;

    char state_char = 'S';
    if(item->state) {
        if(strcmp(item->state, "Running") == 0) {
            state_char = 'R';
        } else if(strcmp(item->state, "Ready") == 0) {
            state_char = 'R';
        } else if(strcmp(item->state, "Blocked") == 0) {
            state_char = 'B';
        } else if(strcmp(item->state, "Suspended") == 0) {
            state_char = 'S';
        }
    }

    snprintf(buf, buf_size, "%-8s %c %2u %4u",
             item->name ? item->name : "---",
             state_char,
             (unsigned int)item->priority,
             (unsigned int)item->stack_min_free);
}
