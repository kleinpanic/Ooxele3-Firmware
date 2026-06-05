
#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>

static void diag_draw(Canvas* c, void* ctx) {
    (void)ctx;
    canvas_clear(c);
    // Horizontal ruler at y=20 with tick marks every 8 pixels
    canvas_draw_line(c, 0, 20, 127, 20);
    for (int x = 0; x <= 128; x += 8) {
        canvas_draw_line(c, x, 18, x, 22);
    }
    // Number labels at key positions
    canvas_set_font(c, FontSecondary);
    canvas_draw_str(c, 0, 32, "0");
    canvas_draw_str(c, 60, 32, "64");
    canvas_draw_str(c, 116, 32, "127");
    // Black box right-aligned
    canvas_draw_box(c, 100, 40, 28, 10);
    canvas_set_color(c, ColorWhite);
    canvas_draw_str(c, 102, 48, "edge");
    canvas_set_color(c, ColorBlack);
    // Box at left edge
    canvas_draw_box(c, 0, 40, 28, 10);
    canvas_set_color(c, ColorWhite);
    canvas_draw_str(c, 2, 48, "L0");
    canvas_set_color(c, ColorBlack);
}
static void diag_input(InputEvent* e, void* ctx) {
    FuriMessageQueue* q = ctx;
    furi_message_queue_put(q, e, FuriWaitForever);
}
int32_t diag_app(void* p) {
    (void)p;
    FuriMessageQueue* q = furi_message_queue_alloc(8, sizeof(InputEvent));
    ViewPort* vp = view_port_alloc();
    view_port_draw_callback_set(vp, diag_draw, NULL);
    view_port_input_callback_set(vp, diag_input, q);
    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, vp, GuiLayerFullscreen);
    InputEvent ev;
    while(furi_message_queue_get(q, &ev, FuriWaitForever) == FuriStatusOk) {
        if(ev.type == InputTypeShort && ev.key == InputKeyBack) break;
    }
    gui_remove_view_port(gui, vp);
    view_port_free(vp);
    furi_record_close(RECORD_GUI);
    furi_message_queue_free(q);
    return 0;
}
