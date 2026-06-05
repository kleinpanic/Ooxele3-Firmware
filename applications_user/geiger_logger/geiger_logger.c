#include "geiger_logger.h"
#include <furi_hal_gpio.h>
#include <furi/core/timer.h>

// Debounce timing (milliseconds)
#define GEIGER_DEAD_TIME_MS 2

// GPIO ISR callback: increment atomic pulse counter
static void geiger_isr_callback(void* context) {
    GeigerLoggerApp* app = (GeigerLoggerApp*)context;
    // Simple implementation: just increment on every edge
    // TODO: Add dead-time debounce if needed (use furi_get_tick() for ms granularity)
    __atomic_fetch_add(&app->pulse_counter, 1, __ATOMIC_RELAXED);
}

// 1-Hz timer callback: post TICK event to queue
static void geiger_timer_callback(void* context) {
    GeigerLoggerApp* app = (GeigerLoggerApp*)context;

    // Create a TICK event
    GeigerLoggerEvent event = {.type = GeigerLoggerEventTick};

    // Post event to queue (nonblocking, timeout 0 for timer context)
    furi_message_queue_put(app->queue, &event, 0);
}

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

// GPIO setup: configure PA7 for rising-edge interrupt counting
void geiger_setup_gpio(GeigerLoggerApp* app) {
    // Configure PA7 as an interrupt input with pull-down and slow speed
    // This prevents noise and EMI from corrupting the pulse signal
    furi_hal_gpio_init(&gpio_ext_pa7, GpioModeInterruptRise, GpioPullDown, GpioSpeedLow);

    // Register the ISR callback for this GPIO
    // The callback will be called on each rising edge
    furi_hal_gpio_add_int_callback(&gpio_ext_pa7, geiger_isr_callback, app);
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

    // Set up GPIO for pulse counting
    geiger_setup_gpio(app);

    // Set up 1-Hz timer for periodic tick events
    app->timer = furi_timer_alloc(geiger_timer_callback, FuriTimerTypePeriodic, app);
    if (!app->timer) {
        furi_message_queue_free(app->queue);
        furi_hal_gpio_remove_int_callback(&gpio_ext_pa7);
        free(app);
        return -1;
    }

    // Start the timer with 1000 ms period (1 Hz)
    furi_timer_start(app->timer, furi_ms_to_ticks(1000));

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
            case GeigerLoggerEventTick: {
                // Timer tick: snapshot the atomic counter, update ring buffer, accumulate total_counts
                // Atomically read and reset the counter to 0 in one operation
                uint32_t second_count = __atomic_exchange_n(&app->pulse_counter, 0, __ATOMIC_RELAXED);

                // Cast to uint16_t for the ring buffer (cap at 65535 if overflow)
                uint16_t ring_value = (second_count > 65535) ? 65535 : (uint16_t)second_count;

                // Write the count to the current ring position
                app->ring[app->head] = ring_value;

                // Advance the head pointer (circular buffer, 60 entries)
                app->head = (app->head + 1) % 60;

                // Accumulate the total counts (sum of all pulses during the session)
                app->total_counts += second_count;

                // TODO: Post VIEW_UPDATE message or call view_port_update() to trigger UI refresh
                break;
            }

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
    // Cleanup: stop and free GPIO/timer before exiting
    // This prevents stale callbacks after app state is freed

    // Stop and free the timer
    if (app->timer) {
        furi_timer_stop(app->timer);
        furi_timer_free(app->timer);
    }

    // Remove GPIO interrupt callback
    furi_hal_gpio_remove_int_callback(&gpio_ext_pa7);

    // Free message queue
    furi_message_queue_free(app->queue);

    // Free app state
    free(app);
    return 0;
}
