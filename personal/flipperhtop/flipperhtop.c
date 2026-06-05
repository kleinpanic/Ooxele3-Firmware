#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include "flipperhtop_lib.h"

typedef struct {
    FuriMutex* mutex;
    FuriThreadList* thread_list;
    size_t scroll_offset;
    uint32_t refresh_rate_hz;
    bool show_menu;
    bool exit_pressed;
} FlipperHtopState;

static const uint32_t REFRESH_RATES[] = {1, 2, 5, 10};
static const size_t REFRESH_RATE_COUNT = 4;

static void flipperhtop_draw(Canvas* canvas, void* ctx) {
    FlipperHtopState* st = ctx;
    furi_mutex_acquire(st->mutex, FuriWaitForever);

    // Polished UI: inverted header (kp branding), column-aligned table, highlighted footer.
    canvas_clear(canvas);

    // Header: black bar y=0-11 with white text
    canvas_draw_box(canvas, 0, 0, 128, 11);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 9, "kp/Htop");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 84, 9, "[OK]cfg");
    canvas_set_color(canvas, ColorBlack);

    canvas_set_font(canvas, FontSecondary);

    if(st->show_menu) {
        canvas_draw_str(canvas, 2, 21, "Refresh rate:");
        for(size_t i = 0; i < REFRESH_RATE_COUNT; i++) {
            char buf[16];
            snprintf(buf, sizeof(buf), "%lu Hz", (unsigned long)REFRESH_RATES[i]);
            int y = 30 + (i * 8);
            if(st->refresh_rate_hz == REFRESH_RATES[i]) {
                // Highlight selected row
                canvas_draw_box(canvas, 6, y - 7, 60, 9);
                canvas_set_color(canvas, ColorWhite);
                canvas_draw_str(canvas, 10, y, "* ");
                canvas_draw_str(canvas, 22, y, buf);
                canvas_set_color(canvas, ColorBlack);
            } else {
                canvas_draw_str(canvas, 10, y, "  ");
                canvas_draw_str(canvas, 22, y, buf);
            }
        }
    } else {
        size_t thread_count = furi_thread_list_size(st->thread_list);

        if(thread_count == 0) {
            canvas_draw_str(canvas, 2, 25, "no threads");
        } else {
            // Column header at y=18 with underline at y=20
            canvas_draw_str(canvas, 2, 18, "name     s pri  fr");
            canvas_draw_line(canvas, 0, 20, 127, 20);

            // Row data y=27,35,43,51 (8px spacing)
            const size_t max_display = 4;
            for(size_t i = 0; i < max_display && (st->scroll_offset + i) < thread_count; i++) {
                const FuriThreadListItem* item = furi_thread_list_get_at(st->thread_list, st->scroll_offset + i);
                if(item) {
                    char buf[32];
                    flipperhtop_render_row(buf, sizeof(buf), item);
                    int y = 27 + (i * 8);
                    // Highlight first visible row (currently focused)
                    if(i == 0) {
                        canvas_draw_box(canvas, 0, y - 7, 128, 9);
                        canvas_set_color(canvas, ColorWhite);
                        canvas_draw_str(canvas, 2, y, buf);
                        canvas_set_color(canvas, ColorBlack);
                    } else {
                        canvas_draw_str(canvas, 2, y, buf);
                    }
                }
            }

            // Footer divider at y=55, inverted footer y=56-63
            canvas_draw_box(canvas, 0, 55, 128, 9);
            canvas_set_color(canvas, ColorWhite);
            char info[32];
            snprintf(info, sizeof(info), "%zu/%zu %luHz",
                st->scroll_offset + 1, thread_count, (unsigned long)st->refresh_rate_hz);
            canvas_draw_str(canvas, 2, 63, info);
            canvas_draw_str(canvas, 90, 63, "BACK exit");
            canvas_set_color(canvas, ColorBlack);
        }
    }

    furi_mutex_release(st->mutex);
}

static void flipperhtop_input(InputEvent* event, void* ctx) {
    FuriMessageQueue* q = ctx;
    furi_message_queue_put(q, event, FuriWaitForever);
}

static void flipperhtop_timer_callback(void* ctx) {
    FlipperHtopState* st = ctx;
    furi_mutex_acquire(st->mutex, FuriWaitForever);

    if(st->thread_list) {
        furi_thread_list_free(st->thread_list);
    }
    st->thread_list = furi_thread_list_alloc();
    furi_thread_enumerate(st->thread_list);

    furi_mutex_release(st->mutex);
}

int32_t flipperhtop_app(void* p) {
    UNUSED(p);
    FURI_LOG_I("flipperhtop", "starting");

    FlipperHtopState state = {
        .mutex = furi_mutex_alloc(FuriMutexTypeNormal),
        .thread_list = furi_thread_list_alloc(),
        .scroll_offset = 0,
        .refresh_rate_hz = 2,
        .show_menu = false,
        .exit_pressed = false,
    };

    furi_thread_enumerate(state.thread_list);

    FuriMessageQueue* q = furi_message_queue_alloc(8, sizeof(InputEvent));
    FuriTimer* timer = furi_timer_alloc(flipperhtop_timer_callback, FuriTimerTypePeriodic, &state);

    ViewPort* vp = view_port_alloc();
    view_port_draw_callback_set(vp, flipperhtop_draw, &state);
    view_port_input_callback_set(vp, flipperhtop_input, q);

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, vp, GuiLayerFullscreen);

    uint32_t timer_period_ms = 1000 / state.refresh_rate_hz;
    furi_timer_start(timer, timer_period_ms);

    InputEvent ev;
    while(!state.exit_pressed) {
        if(furi_message_queue_get(q, &ev, 100) == FuriStatusOk) {
            furi_mutex_acquire(state.mutex, FuriWaitForever);

            if(ev.type == InputTypeShort && ev.key == InputKeyBack) {
                state.exit_pressed = true;
            } else if(ev.type == InputTypeShort && ev.key == InputKeyUp) {
                if(!state.show_menu && state.scroll_offset > 0) {
                    state.scroll_offset--;
                }
            } else if(ev.type == InputTypeShort && ev.key == InputKeyDown) {
                if(!state.show_menu) {
                    size_t thread_count = furi_thread_list_size(state.thread_list);
                    if(state.scroll_offset + 4 < thread_count) state.scroll_offset++;
                }
            } else if(ev.type == InputTypeShort && ev.key == InputKeyOk) {
                state.show_menu = !state.show_menu;
            }

            furi_mutex_release(state.mutex);
        }
        view_port_update(vp);
    }

    furi_timer_stop(timer);
    furi_timer_free(timer);
    gui_remove_view_port(gui, vp);
    view_port_free(vp);
    furi_record_close(RECORD_GUI);
    furi_message_queue_free(q);

    if(state.thread_list) {
        furi_thread_list_free(state.thread_list);
    }
    furi_mutex_free(state.mutex);

    FURI_LOG_I("flipperhtop", "exit");
    return 0;
}
