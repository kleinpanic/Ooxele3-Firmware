#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <storage/storage.h>
#include "processissues_lib.h"

#define MAX_CRASHES 50

typedef struct {
    FuriMutex* mutex;
    CrashRecord crashes[MAX_CRASHES];
    size_t crash_count;
    size_t scroll_offset;
    bool has_crashes;
    bool exit_pressed;
} ProcessIssuesState;

static void processissues_draw(Canvas* canvas, void* ctx) {
    ProcessIssuesState* st = ctx;
    furi_mutex_acquire(st->mutex, FuriWaitForever);

    // Flipper screen 128x64. Keep y in [8..63].
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 9, "ProcessIssues");
    canvas_draw_line(canvas, 0, 12, 127, 12);

    canvas_set_font(canvas, FontSecondary);

    if(!st->has_crashes) {
        canvas_draw_str(canvas, 2, 22, "No crashes recorded");
        canvas_draw_str(canvas, 2, 32, "since OTP provisioning");
        canvas_draw_str(canvas, 2, 46, "/int/.crash.log absent");
        canvas_draw_str(canvas, 2, 62, "BACK=exit");
    } else {
        char buf[48];
        processissues_format_summary(buf, sizeof(buf), st->crash_count,
                                     st->crash_count > 0 ? st->crashes[0].tag : NULL);
        canvas_draw_str(canvas, 2, 19, buf);

        canvas_draw_str(canvas, 2, 28, "Recent crashes:");

        // Rows y=35,42,49,56. Footer y=62.
        const size_t max_display = 4;
        for(size_t i = 0; i < max_display && (st->scroll_offset + i) < st->crash_count; i++) {
            snprintf(buf, sizeof(buf), "%s %04u",
                     st->crashes[st->scroll_offset + i].tag,
                     (unsigned int)(st->crashes[st->scroll_offset + i].error_code & 0xFFFF));
            canvas_draw_str(canvas, 2, 35 + (i * 7), buf);
        }

        snprintf(buf, sizeof(buf), "%zu/%zu", st->scroll_offset + 1, st->crash_count);
        canvas_draw_str(canvas, 2, 62, buf);
    }

    furi_mutex_release(st->mutex);
}

static void processissues_input(InputEvent* event, void* ctx) {
    FuriMessageQueue* q = ctx;
    furi_message_queue_put(q, event, FuriWaitForever);
}

static void processissues_load_crashes(ProcessIssuesState* st) {
    st->crash_count = 0;
    st->has_crashes = false;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    if(storage_file_open(file, "/int/.crash.log", FSAM_READ, FSOM_OPEN_EXISTING)) {
        st->has_crashes = true;

        uint8_t buf[40];
        size_t bytes_read;
        size_t offset = 0;

        while(st->crash_count < MAX_CRASHES) {
            bytes_read = storage_file_read(file, buf, sizeof(buf));
            if(bytes_read == 0) break;

            if(processissues_parse_record(buf, bytes_read, &st->crashes[st->crash_count])) {
                st->crash_count++;
            }
            offset += bytes_read;
        }

        storage_file_close(file);
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

int32_t processissues_app(void* p) {
    UNUSED(p);
    FURI_LOG_I("processissues", "starting");

    ProcessIssuesState state = {
        .mutex = furi_mutex_alloc(FuriMutexTypeNormal),
        .crash_count = 0,
        .scroll_offset = 0,
        .has_crashes = false,
        .exit_pressed = false,
    };

    processissues_load_crashes(&state);

    FuriMessageQueue* q = furi_message_queue_alloc(8, sizeof(InputEvent));

    ViewPort* vp = view_port_alloc();
    view_port_draw_callback_set(vp, processissues_draw, &state);
    view_port_input_callback_set(vp, processissues_input, q);

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, vp, GuiLayerFullscreen);

    InputEvent ev;
    while(!state.exit_pressed) {
        if(furi_message_queue_get(q, &ev, 100) == FuriStatusOk) {
            furi_mutex_acquire(state.mutex, FuriWaitForever);

            if(ev.type == InputTypeShort && ev.key == InputKeyBack) {
                state.exit_pressed = true;
            } else if(ev.type == InputTypeShort && ev.key == InputKeyUp) {
                if(state.has_crashes && state.scroll_offset > 0) {
                    state.scroll_offset--;
                }
            } else if(ev.type == InputTypeShort && ev.key == InputKeyDown) {
                if(state.has_crashes && state.scroll_offset + 4 < state.crash_count) {
                    state.scroll_offset++;
                }
            }

            furi_mutex_release(state.mutex);
        }
        view_port_update(vp);
    }

    gui_remove_view_port(gui, vp);
    view_port_free(vp);
    furi_record_close(RECORD_GUI);
    furi_message_queue_free(q);
    furi_mutex_free(state.mutex);

    FURI_LOG_I("processissues", "exit");
    return 0;
}
