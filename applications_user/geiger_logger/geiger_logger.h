#pragma once

#include <furi.h>
#include <furi/core/message_queue.h>
#include <input/input.h>

// Forward declaration
typedef struct GeigerLoggerApp GeigerLoggerApp;

// Message/event types for the app message queue
typedef enum {
    GeigerLoggerEventTick,
    GeigerLoggerEventInput,
    GeigerLoggerEventStop,
} GeigerLoggerEventType;

// Event struct posted to the message queue
typedef struct {
    GeigerLoggerEventType type;
    InputEvent input;  // For input events; unused for Tick/Stop
} GeigerLoggerEvent;

// Application state struct
struct GeigerLoggerApp {
    FuriMessageQueue* queue;      // Message queue for event dispatch
    uint16_t ring[60];             // Per-second ring buffer (60 seconds)
    uint32_t head;                 // Current write index in ring buffer
    volatile uint32_t pulse_counter;  // Atomic counter incremented by GPIO ISR
    uint32_t total_counts;         // Total pulses accumulated during session
    FuriTimer* timer;              // 1-Hz tick timer
    float usvh_factor;             // µSv/h conversion factor (default 0.0081 for J305)
};

// Math function declarations
uint16_t geiger_get_cps(GeigerLoggerApp* app);
uint32_t geiger_get_cpm(GeigerLoggerApp* app);
float geiger_get_usvh(GeigerLoggerApp* app);

// GPIO setup (placeholder for now, implemented in plan 05-02)
void geiger_setup_gpio(GeigerLoggerApp* app);

// Entry point
int32_t geiger_logger_app(void* p);
