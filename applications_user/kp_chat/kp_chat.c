/* kp Chat — host-offload LLM chat client.
 * MVP scaffold: UI on Flipper, awaits host daemon over USB-CDC.
 * Per research (.planning/research/local-llm-on-flipper-truly-local.md):
 * on-device LLM is INFEASIBLE on STM32WB55 (256KB SRAM vs 1MB needed).
 * Host bridge runs llama.cpp with LFM2.5-1.2B (CC0/Apache).
 */
#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdio.h>
#include <string.h>

#define VIEW_PORT_OK 0

typedef struct {
    FuriMessageQueue* queue;
    FuriMutex* mtx;
    ViewPort* vp;
    Gui* gui;
    bool exiting;
    int blink;
    char status[32];
    char reply_preview[64];  // first 60 chars of reply buffer
} KpChat;

static void kp_chat_draw(Canvas* canvas, void* ctx) {
    KpChat* a = ctx;
    canvas_clear(canvas);

    // Header
    canvas_draw_box(canvas, 0, 13, 128, 11);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 22, "kp/Chat");
    canvas_set_font(canvas, FontSecondary);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_str(canvas, 88, 22, a->blink ? "*AW*" : "AW8");
    canvas_set_color(canvas, ColorBlack);

    canvas_set_font(canvas, FontSecondary);

    // ASCII chat bubble
    canvas_draw_str(canvas, 2, 32, "   __");
    canvas_draw_str(canvas, 2, 39, " ,'  '.   kp LFM chat");
    canvas_draw_str(canvas, 2, 46, "( o  o )  host daemon");
    canvas_draw_str(canvas, 2, 53, " '----'   USB-CDC");

    // Footer
    canvas_draw_box(canvas, 0, 55, 128, 9);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_str(canvas, 2, 63, "OK=ask BACK=ex");
    canvas_set_color(canvas, ColorBlack);
}

static void kp_chat_input(InputEvent* e, void* ctx) {
    KpChat* a = ctx;
    if(e->type == InputTypeShort) {
        if(e->key == InputKeyBack) {
            a->exiting = true;
            view_port_update(a->vp);
        } else if(e->key == InputKeyOk) {
            // Stub: pretend to send a query and wait for daemon
            snprintf(a->status, sizeof(a->status), "queried daemon...");
            // In real impl: would write to USB-CDC + poll for response
            snprintf(a->reply_preview, sizeof(a->reply_preview), "(host daemon not running)");
            view_port_update(a->vp);
        }
    }
}

static void kp_chat_timer(void* ctx) {
    KpChat* a = ctx;
    a->blink = !a->blink;
    view_port_update(a->vp);
}

int32_t kp_chat_app(void* p) {
    (void)p;
    KpChat* a = malloc(sizeof(KpChat));
    if(!a) return -1;
    a->exiting = false;
    a->blink = 0;
    strncpy(a->status, "idle", sizeof(a->status));
    a->reply_preview[0] = '\0';

    a->queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    a->mtx = furi_mutex_alloc(FuriMutexTypeNormal);
    a->vp = view_port_alloc();
    view_port_draw_callback_set(a->vp, kp_chat_draw, a);
    view_port_input_callback_set(a->vp, kp_chat_input, a);
    a->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(a->gui, a->vp, GuiLayerFullscreen);

    // Blink timer for "*await*" indicator
    FuriTimer* timer = furi_timer_alloc(kp_chat_timer, FuriTimerTypePeriodic, a);
    furi_timer_start(timer, furi_ms_to_ticks(500));

    InputEvent ev;
    while(!a->exiting) {
        FuriStatus s = furi_message_queue_get(a->queue, &ev, 1000);
        (void)s;
    }

    furi_timer_stop(timer);
    furi_timer_free(timer);
    gui_remove_view_port(a->gui, a->vp);
    view_port_free(a->vp);
    furi_record_close(RECORD_GUI);
    furi_mutex_free(a->mtx);
    furi_message_queue_free(a->queue);
    free(a);
    return 0;
}
