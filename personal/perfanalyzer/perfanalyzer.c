#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <storage/storage.h>
#include "perfanalyzer_lib.h"
#include <stdlib.h>
#include <string.h>

#define MAX_APPS 32
#define MAX_SAMPLES 100

typedef struct {
    char name[64];
    char appid[32];
} AppEntry;

typedef struct {
    FuriMutex* mutex;
    AppEntry apps[MAX_APPS];
    size_t app_count;
    size_t selected_app;
    FuriThreadId target_thread_id;
    uint32_t samples[MAX_SAMPLES];
    size_t sample_count;
    bool tracing_active;
    bool show_detail;
    size_t scroll_offset;
    bool exit_pressed;
} PerfAnalyzerState;

static void perfanalyzer_draw(Canvas* canvas, void* ctx) {
    PerfAnalyzerState* st = ctx;
    furi_mutex_acquire(st->mutex, FuriWaitForever);

    // Polished UI: inverted header below status bar (y=13-23). Status bar owns y=0-12.
    canvas_clear(canvas);

    // Header bar
    canvas_draw_box(canvas, 0, 13, 128, 11);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 22, "kp/Perf");
    canvas_set_font(canvas, FontSecondary);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_str(canvas, 100, 22, st->show_detail ? "res" : "pick");
    canvas_set_color(canvas, ColorBlack);

    canvas_set_font(canvas, FontSecondary);

    if(st->app_count == 0) {
        canvas_draw_str(canvas, 2, 40, "no fap files in /ext/apps");
        canvas_draw_box(canvas, 0, 55, 128, 9);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str(canvas, 2, 63, "BACK=exit");
        canvas_set_color(canvas, ColorBlack);
    } else if(st->show_detail) {
        // Detail / results view
        char buf[48];
        snprintf(buf, sizeof(buf), "app: %s", st->apps[st->selected_app].appid);
        // truncate to 21 chars to fit 128px
        if(strlen(buf) > 21) buf[21] = '\0';
        canvas_draw_str(canvas, 2, 32, buf);

        if(st->tracing_active) {
            snprintf(buf, sizeof(buf), "samples: %zu", st->sample_count);
            canvas_draw_str(canvas, 2, 40, buf);

            // Mini bar graph of samples (right half)
            if(st->sample_count > 0) {
                snprintf(buf, sizeof(buf), "peak %luB", (unsigned long)st->samples[0]);
                canvas_draw_str(canvas, 70, 40, buf);
                // Render sample history as vertical bars in y=44-52, x=2-126
                size_t bars = st->sample_count < 60 ? st->sample_count : 60;
                uint32_t max = 1;
                for(size_t i = 0; i < bars; i++) if(st->samples[i] > max) max = st->samples[i];
                int bar_x = 2;
                int bar_w = 122 / (bars > 0 ? bars : 1);
                if(bar_w < 1) bar_w = 1;
                if(bar_w > 4) bar_w = 4;
                for(size_t i = 0; i < bars; i++) {
                    int h = (int)((st->samples[i] * 9) / max);
                    if(h < 1) h = 1;
                    canvas_draw_box(canvas, bar_x, 52 - h, bar_w, h);
                    bar_x += bar_w;
                    if(bar_x > 124) break;
                }
                // Baseline
                canvas_draw_line(canvas, 2, 52, 126, 52);
            }
            // Footer: inverted
            canvas_draw_box(canvas, 0, 55, 128, 9);
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_str(canvas, 2, 63, "BACK=stop+save");
            canvas_set_color(canvas, ColorBlack);
        } else {
            canvas_draw_str(canvas, 2, 42, "OK to start trace");
            canvas_draw_str(canvas, 2, 51, "(writes /ext/logs)");
            canvas_draw_box(canvas, 0, 55, 128, 9);
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_str(canvas, 2, 63, "OK=start  BACK=exit");
            canvas_set_color(canvas, ColorBlack);
        }
    } else {
        // App picker with highlighted row
        // 3 rows at y=33,42,51 (9px spacing for clean inverted highlight)
        const size_t max_display = 3;
        for(size_t i = 0; i < max_display && (st->scroll_offset + i) < st->app_count; i++) {
            size_t idx = st->scroll_offset + i;
            const char* name = st->apps[idx].name;
            int y = 33 + (i * 9);
            if(idx == st->selected_app) {
                canvas_draw_box(canvas, 0, y - 7, 128, 9);
                canvas_set_color(canvas, ColorWhite);
                canvas_draw_str(canvas, 2, y, ">");
                canvas_draw_str(canvas, 10, y, name);
                canvas_set_color(canvas, ColorBlack);
            } else {
                canvas_draw_str(canvas, 2, y, " ");
                canvas_draw_str(canvas, 10, y, name);
            }
        }
        // Footer
        canvas_draw_box(canvas, 0, 55, 128, 9);
        canvas_set_color(canvas, ColorWhite);
        char info[32];
        snprintf(info, sizeof(info), "%zu/%zu", st->selected_app + 1, st->app_count);
        canvas_draw_str(canvas, 2, 63, info);
        canvas_draw_str(canvas, 60, 63, "OK=pick");
        canvas_draw_str(canvas, 95, 63, "BACK=exit");
        canvas_set_color(canvas, ColorBlack);
    }

    furi_mutex_release(st->mutex);
}

static void perfanalyzer_input(InputEvent* event, void* ctx) {
    FuriMessageQueue* q = ctx;
    furi_message_queue_put(q, event, FuriWaitForever);
}

static void perfanalyzer_enumerate_apps(PerfAnalyzerState* st) {
    st->app_count = 0;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* dir = storage_file_alloc(storage);

    if(storage_dir_open(dir, "/ext/apps/Tools")) {
        char name_buf[256];
        FileInfo info;

        while(storage_dir_read(dir, &info, name_buf, sizeof(name_buf)) && st->app_count < MAX_APPS) {
            if(!(info.flags & FSF_DIRECTORY)) {
                size_t len = strlen(name_buf);
                if(len > 4 && strcmp(&name_buf[len - 4], ".fap") == 0) {
                    strncpy(st->apps[st->app_count].name, name_buf,
                           sizeof(st->apps[st->app_count].name) - 1);
                    st->apps[st->app_count].name[sizeof(st->apps[st->app_count].name) - 1] = '\0';

                    name_buf[len - 4] = '\0';
                    strncpy(st->apps[st->app_count].appid, name_buf,
                           sizeof(st->apps[st->app_count].appid) - 1);
                    st->apps[st->app_count].appid[sizeof(st->apps[st->app_count].appid) - 1] = '\0';

                    st->app_count++;
                }
            }
        }
        storage_dir_close(dir);
    }

    storage_file_free(dir);
    furi_record_close(RECORD_STORAGE);
}

int32_t perfanalyzer_app(void* p) {
    UNUSED(p);
    FURI_LOG_I("perfanalyzer", "starting");

    // State struct is ~3.5KB (AppEntry apps[32] = 32*96 = 3072 bytes plus samples[100]).
    // 4KB stack would overflow with locals + furi runtime overhead -> MPU fault.
    // Heap-allocate via malloc + zero-init, free on exit.
    PerfAnalyzerState* state = malloc(sizeof(PerfAnalyzerState));
    if(!state) {
        FURI_LOG_E("perfanalyzer", "malloc failed");
        return -1;
    }
    memset(state, 0, sizeof(PerfAnalyzerState));
    state->mutex = furi_mutex_alloc(FuriMutexTypeNormal);

    perfanalyzer_enumerate_apps(state);

    FuriMessageQueue* q = furi_message_queue_alloc(8, sizeof(InputEvent));

    ViewPort* vp = view_port_alloc();
    view_port_draw_callback_set(vp, perfanalyzer_draw, state);
    view_port_input_callback_set(vp, perfanalyzer_input, q);

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, vp, GuiLayerFullscreen);

    InputEvent ev;
    while(!state->exit_pressed) {
        if(furi_message_queue_get(q, &ev, 100) == FuriStatusOk) {
            furi_mutex_acquire(state->mutex, FuriWaitForever);

            if(ev.type == InputTypeShort && ev.key == InputKeyBack) {
                if(state->show_detail) {
                    state->show_detail = false;
                    state->tracing_active = false;
                    state->sample_count = 0;
                } else {
                    state->exit_pressed = true;
                }
            } else if(ev.type == InputTypeShort && ev.key == InputKeyUp) {
                if(!state->show_detail && state->selected_app > 0) {
                    state->selected_app--;
                    if(state->selected_app < state->scroll_offset) {
                        state->scroll_offset = state->selected_app;
                    }
                }
            } else if(ev.type == InputTypeShort && ev.key == InputKeyDown) {
                if(!state->show_detail && state->selected_app < state->app_count - 1) {
                    state->selected_app++;
                    if(state->selected_app >= state->scroll_offset + 4) {
                        state->scroll_offset = state->selected_app - 3;
                    }
                }
            } else if(ev.type == InputTypeShort && ev.key == InputKeyOk) {
                if(!state->show_detail) {
                    state->show_detail = true;
                } else if(!state->tracing_active) {
                    state->tracing_active = true;
                    state->sample_count = 0;
                }
            }

            furi_mutex_release(state->mutex);
        }
        view_port_update(vp);
    }

    gui_remove_view_port(gui, vp);
    view_port_free(vp);
    furi_record_close(RECORD_GUI);
    furi_message_queue_free(q);
    furi_mutex_free(state->mutex);
    free(state);

    FURI_LOG_I("perfanalyzer", "exit");
    return 0;
}
