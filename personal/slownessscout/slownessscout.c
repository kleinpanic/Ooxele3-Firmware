#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <storage/storage.h>
#include "slownessscout_lib.h"

#define MAX_SAMPLES 100
#define CSV_ROTATION_THRESHOLD 10485760

typedef struct {
    FuriMutex* mutex;
    int32_t frame_samples[MAX_SAMPLES];
    size_t frame_sample_count;
    uint32_t last_frame_tick;
    uint32_t last_input_tick;
    int32_t current_frame_time;
    int32_t current_variance;
    int32_t current_input_latency;
    int32_t latency_threshold;
    uint32_t flagged_count;
    File* csv_file;
    size_t csv_bytes_written;
    int32_t csv_rotation_index;
    bool tracing_active;
    bool exit_pressed;
} SlownessScoutState;

static void slownessscout_draw(Canvas* canvas, void* ctx) {
    SlownessScoutState* st = ctx;
    furi_mutex_acquire(st->mutex, FuriWaitForever);

    // Polished — header below status bar (y=13-23), timeline visualization, footer.
    canvas_clear(canvas);

    canvas_draw_box(canvas, 0, 13, 128, 11);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 22, "kp/Scout");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 80, 22, st->tracing_active ? "TRACING" : "idle");
    canvas_set_color(canvas, ColorBlack);

    canvas_set_font(canvas, FontSecondary);

    if(!st->tracing_active) {
        // ASCII turtle banner
        canvas_draw_str(canvas, 2, 32, "    .--.   slow+steady");
        canvas_draw_str(canvas, 2, 39, "   (o o)   thresh det");
        canvas_draw_str(canvas, 2, 46, "  /  v  \\");
        char buf[32];
        snprintf(buf, sizeof(buf), "  threshold %ld ms", (long)st->latency_threshold);
        canvas_draw_str(canvas, 2, 53, buf);
        canvas_draw_box(canvas, 0, 55, 128, 9);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str(canvas, 2, 63, "OK=start UP/DN=thr");
        canvas_set_color(canvas, ColorBlack);
    } else {
        char buf[32];
        snprintf(buf, sizeof(buf), "fr %3ld", (long)st->current_frame_time);
        canvas_draw_str(canvas, 2, 31, buf);
        snprintf(buf, sizeof(buf), "var %3ld", (long)st->current_variance);
        canvas_draw_str(canvas, 44, 31, buf);
        snprintf(buf, sizeof(buf), "in %3ld", (long)st->current_input_latency);
        canvas_draw_str(canvas, 90, 31, buf);
        snprintf(buf, sizeof(buf), "flag %3lu thr %ld", (unsigned long)st->flagged_count, (long)st->latency_threshold);
        canvas_draw_str(canvas, 2, 40, buf);

        // Timeline mini-bars from frame_samples
        size_t n = st->frame_sample_count < 40 ? st->frame_sample_count : 40;
        if(n > 0) {
            int32_t maxv = 1;
            for(size_t i = 0; i < n; i++) {
                int32_t s = st->frame_samples[(st->frame_sample_count - n + i) % MAX_SAMPLES];
                if(s > maxv) maxv = s;
            }
            int bw = 3, bx = 2, by = 53, bmax_h = 10;
            for(size_t i = 0; i < n; i++) {
                int32_t s = st->frame_samples[(st->frame_sample_count - n + i) % MAX_SAMPLES];
                int h = (int)((s * bmax_h) / maxv);
                if(h < 1) h = 1;
                canvas_draw_box(canvas, bx, by - h, bw - 1, h);
                bx += bw;
                if(bx > 122) break;
            }
        }
        canvas_draw_box(canvas, 0, 55, 128, 9);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str(canvas, 2, 63, "BACK=stop+save");
        canvas_set_color(canvas, ColorBlack);
    }

    furi_mutex_release(st->mutex);
}

static void slownessscout_input(InputEvent* event, void* ctx) {
    FuriMessageQueue* q = ctx;
    furi_message_queue_put(q, event, FuriWaitForever);
}

int32_t slownessscout_app(void* p) {
    UNUSED(p);
    FURI_LOG_I("slownessscout", "starting");

    SlownessScoutState state = {
        .mutex = furi_mutex_alloc(FuriMutexTypeNormal),
        .frame_sample_count = 0,
        .last_frame_tick = 0,
        .last_input_tick = 0,
        .current_frame_time = 0,
        .current_variance = 0,
        .current_input_latency = 0,
        .latency_threshold = 50,
        .flagged_count = 0,
        .csv_file = NULL,
        .csv_bytes_written = 0,
        .csv_rotation_index = 1,
        .tracing_active = false,
        .exit_pressed = false,
    };

    FuriMessageQueue* q = furi_message_queue_alloc(8, sizeof(InputEvent));

    ViewPort* vp = view_port_alloc();
    view_port_draw_callback_set(vp, slownessscout_draw, &state);
    view_port_input_callback_set(vp, slownessscout_input, q);

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, vp, GuiLayerFullscreen);

    InputEvent ev;
    while(!state.exit_pressed) {
        if(furi_message_queue_get(q, &ev, 100) == FuriStatusOk) {
            furi_mutex_acquire(state.mutex, FuriWaitForever);

            if(ev.type == InputTypeShort && ev.key == InputKeyBack) {
                if(state.tracing_active && state.csv_file) {
                    storage_file_close(state.csv_file);
                    state.csv_file = NULL;
                }
                state.exit_pressed = true;
            } else if(ev.type == InputTypeShort && ev.key == InputKeyUp) {
                if(state.latency_threshold > 10) state.latency_threshold -= 5;
            } else if(ev.type == InputTypeShort && ev.key == InputKeyDown) {
                state.latency_threshold += 5;
            } else if(ev.type == InputTypeShort && ev.key == InputKeyOk) {
                if(!state.tracing_active) {
                    state.tracing_active = true;
                    state.frame_sample_count = 0;
                    state.flagged_count = 0;
                    state.csv_bytes_written = 0;
                    state.csv_rotation_index = 1;

                    Storage* storage = furi_record_open(RECORD_STORAGE);
                    char csv_filename[256];
                    snprintf(csv_filename, sizeof(csv_filename),
                             "/ext/logs/slowness-%03ld.csv", (long)state.csv_rotation_index);

                    state.csv_file = storage_file_alloc(storage);
                    storage_file_open(state.csv_file, csv_filename, FSAM_WRITE, FSOM_CREATE_NEW);
                    furi_record_close(RECORD_STORAGE);
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

    FURI_LOG_I("slownessscout", "exit");
    return 0;
}
