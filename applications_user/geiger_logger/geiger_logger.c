#include "geiger_logger.h"

// Math function implementations
uint16_t geiger_get_cps(GeigerLoggerApp* app) {
    // Return the most recent second's count from the ring buffer
    return app->ring[(app->head - 1 + 60) % 60];
}

uint32_t geiger_get_cpm(GeigerLoggerApp* app) {
    // Sum all 60 entries in the ring buffer
    uint32_t sum = 0;
    for (int i = 0; i < 60; i++) {
        sum += app->ring[i];
    }
    // If app has been running < 60 seconds, extrapolate to a full minute
    // For now, just return the sum (will be refined in later plans)
    return sum;
}

float geiger_get_usvh(GeigerLoggerApp* app) {
    // J305 Geiger-Müller tube conversion factor: 0.0081 µSv/h per CPM
    return (float)geiger_get_cpm(app) * app->usvh_factor;
}

// Placeholder for GPIO setup (implemented in plan 05-02)
void geiger_setup_gpio(GeigerLoggerApp* app) {
    // To be implemented in plan 05-02
    (void)app;
}

// Main entry point for the Geiger Logger FAP
int32_t geiger_logger_app(void* p) {
    (void)p;

    // Allocate application state
    GeigerLoggerApp* app = (GeigerLoggerApp*)malloc(sizeof(GeigerLoggerApp));
    if (!app) {
        return -1;
    }

    // Initialize app state
    for (int i = 0; i < 60; i++) {
        app->ring[i] = 0;
    }
    app->head = 0;
    app->pulse_counter = 0;
    app->total_counts = 0;
    app->timer = NULL;
    app->usvh_factor = 0.0081f;  // Default J305 conversion factor

    // Create message queue for event dispatch
    app->queue = furi_message_queue_alloc(10, sizeof(GeigerLoggerEvent));
    if (!app->queue) {
        free(app);
        return -1;
    }

    // Main event loop
    GeigerLoggerEvent event;
    while (true) {
        // Wait for next event (blocking)
        FuriStatus status = furi_message_queue_get(app->queue, &event, FuriWaitForever);

        if (status != FuriStatusOk) {
            continue;
        }

        // Handle different event types
        switch (event.type) {
            case GeigerLoggerEventTick:
                // Timer tick: will be processed in plan 05-02
                break;

            case GeigerLoggerEventInput:
                // Input event: handle back button, etc.
                // For now, if BACK button pressed, exit
                if (event.input.key == InputKeyBack && event.input.type == InputTypePress) {
                    GeigerLoggerEvent stop_event = {.type = GeigerLoggerEventStop};
                    furi_message_queue_put(app->queue, &stop_event, FuriWaitForever);
                }
                break;

            case GeigerLoggerEventStop:
                // Exit signal received
                goto app_exit;

            default:
                break;
        }
    }

app_exit:
    // Cleanup: will add GPIO/timer cleanup in plan 05-02
    furi_message_queue_free(app->queue);
    free(app);
    return 0;
}
