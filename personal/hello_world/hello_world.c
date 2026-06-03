#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>

#include <hello_world_icons.h>

typedef struct {
    FuriMutex* mutex;
    bool exit_pressed;
} HelloWorldState;

static void hw_draw(Canvas* canvas, void* ctx) {
    HelloWorldState* st = ctx;
    furi_mutex_acquire(st->mutex, FuriWaitForever);
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 8, 16, "Hello, Ooxele3!");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 8, 36, "Flipper0 toolchain OK");
    canvas_draw_str(canvas, 8, 52, "Press BACK to exit");
    furi_mutex_release(st->mutex);
}

static void hw_input(InputEvent* event, void* ctx) {
    FuriMessageQueue* q = ctx;
    furi_message_queue_put(q, event, FuriWaitForever);
}

int32_t hello_world_app(void* p) {
    UNUSED(p);
    FURI_LOG_I("hello_world", "starting on Ooxele3");

    HelloWorldState state = {
        .mutex = furi_mutex_alloc(FuriMutexTypeNormal),
        .exit_pressed = false,
    };
    FuriMessageQueue* q = furi_message_queue_alloc(8, sizeof(InputEvent));

    ViewPort* vp = view_port_alloc();
    view_port_draw_callback_set(vp, hw_draw, &state);
    view_port_input_callback_set(vp, hw_input, q);

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, vp, GuiLayerFullscreen);

    InputEvent ev;
    while(!state.exit_pressed) {
        if(furi_message_queue_get(q, &ev, 100) == FuriStatusOk) {
            if(ev.type == InputTypeShort && ev.key == InputKeyBack) {
                state.exit_pressed = true;
            }
        }
        view_port_update(vp);
    }

    gui_remove_view_port(gui, vp);
    view_port_free(vp);
    furi_record_close(RECORD_GUI);
    furi_message_queue_free(q);
    furi_mutex_free(state.mutex);

    FURI_LOG_I("hello_world", "exit");
    return 0;
}
