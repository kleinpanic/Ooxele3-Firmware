#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <storage/storage.h>
#include "perfanalyzer_lib.h"

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

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 8, 10, "PerfAnalyzer");

    canvas_set_font(canvas, FontSecondary);

    if(st->app_count == 0) {
        canvas_draw_str(canvas, 8, 28, "No apps found");
    } else if(st->show_detail) {
        char buf[64];
        snprintf(buf, sizeof(buf), "App: %s", st->apps[st->selected_app].appid);
        canvas_draw_str(canvas, 8, 28, buf);

        if(st->tracing_active) {
            snprintf(buf, sizeof(buf), "Tracing... %zu samples", st->sample_count);
            canvas_draw_str(canvas, 8, 40, buf);

            if(st->sample_count > 0) {
                snprintf(buf, sizeof(buf), "Peak: %lu bytes", (unsigned long)st->samples[0]);
                canvas_draw_str(canvas, 8, 52, buf);
            }
        } else {
            canvas_draw_str(canvas, 8, 40, "Press OK to start");
        }
    } else {
        canvas_draw_str(canvas, 8, 28, "Select app:");

        size_t max_display = 5;
        for(size_t i = 0; i < max_display && (st->scroll_offset + i) < st->app_count; i++) {
            char buf[48];
            snprintf(buf, sizeof(buf), "%s%s",
                     (st->scroll_offset + i == st->selected_app) ? ">" : " ",
                     st->apps[st->scroll_offset + i].name);
            canvas_draw_str(canvas, 8, 40 + (i * 10), buf);
        }

        char info[32];
        snprintf(info, sizeof(info), "%zu/%zu", st->scroll_offset + 1, st->app_count);
        canvas_draw_str(canvas, 8, 126, info);
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

    PerfAnalyzerState state = {
        .mutex = furi_mutex_alloc(FuriMutexTypeNormal),
        .app_count = 0,
        .selected_app = 0,
        .target_thread_id = NULL,
        .sample_count = 0,
        .tracing_active = false,
        .show_detail = false,
        .scroll_offset = 0,
        .exit_pressed = false,
    };

    perfanalyzer_enumerate_apps(&state);

    FuriMessageQueue* q = furi_message_queue_alloc(8, sizeof(InputEvent));

    ViewPort* vp = view_port_alloc();
    view_port_draw_callback_set(vp, perfanalyzer_draw, &state);
    view_port_input_callback_set(vp, perfanalyzer_input, q);

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, vp, GuiLayerFullscreen);

    InputEvent ev;
    while(!state.exit_pressed) {
        if(furi_message_queue_get(q, &ev, 100) == FuriStatusOk) {
            furi_mutex_acquire(state.mutex, FuriWaitForever);

            if(ev.type == InputTypeShort && ev.key == InputKeyBack) {
                if(state.show_detail) {
                    state.show_detail = false;
                    state.tracing_active = false;
                    state.sample_count = 0;
                } else {
                    state.exit_pressed = true;
                }
            } else if(ev.type == InputTypeShort && ev.key == InputKeyUp) {
                if(!state.show_detail && state.selected_app > 0) {
                    state.selected_app--;
                    if(state.selected_app < state.scroll_offset) {
                        state.scroll_offset = state.selected_app;
                    }
                }
            } else if(ev.type == InputTypeShort && ev.key == InputKeyDown) {
                if(!state.show_detail && state.selected_app < state.app_count - 1) {
                    state.selected_app++;
                    if(state.selected_app >= state.scroll_offset + 5) {
                        state.scroll_offset = state.selected_app - 4;
                    }
                }
            } else if(ev.type == InputTypeShort && ev.key == InputKeyOk) {
                if(!state.show_detail) {
                    state.show_detail = true;
                } else if(!state.tracing_active) {
                    state.tracing_active = true;
                    state.sample_count = 0;
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

    FURI_LOG_I("perfanalyzer", "exit");
    return 0;
}
