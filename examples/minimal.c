#include <stdio.h>
#include <string.h>

#include "zeropack.h"

// -----------------------------------------------------------------------------
// Mutex Stubs (Dummy implementation for single-threaded / bare-metal systems)
// -----------------------------------------------------------------------------
zp_mutex_t* zp_mutex_tx = NULL;
zp_mutex_t* zp_mutex_pool = NULL;
void zp_mutex_lock(zp_mutex_t* m) { (void)m; }
void zp_mutex_unlock(zp_mutex_t* m) { (void)m; }

// -----------------------------------------------------------------------------
// Schema Definition (Compile-time macros, no external codegen required)
// -----------------------------------------------------------------------------
#define APP_TYPES(X) \
    X(SensorReport, SENSOR_FIELDS)

#define SENSOR_FIELDS(S, F, A) \
    F(S, f32, temperature)     \
    F(S, f32, humidity)        \
    F(S, u32, timestamp)

#define APP_ACTIONS(V, T, A) \
    V(ACTION_PING)           \
    V(ACTION_PONG)           \
    T(ACTION_REPORT, SensorReport)

// Declare type IDs, structs, and dispatcher prototypes
ZP_DECLARE(APP_TYPES, APP_ACTIONS)

// Generate the runtime schema metadata and dispatch table
ZP_GENERATE_SCHEMA(APP_TYPES, APP_ACTIONS)

// -----------------------------------------------------------------------------
// Simulated Loopback Transport
// -----------------------------------------------------------------------------
static uint8_t s_bus_buffer[4096];
static size_t s_bus_len = 0;

static zp_ssize_t loopback_writev(const zp_iovec_t* iov, int iovcnt) {
    zp_ssize_t written = 0;
    for (int i = 0; i < iovcnt; i++) {
        if (s_bus_len + iov[i].len > sizeof(s_bus_buffer)) {
            return -1; // Overflow
        }
        memcpy(s_bus_buffer + s_bus_len, iov[i].buf, iov[i].len);
        s_bus_len += iov[i].len;
        written += (zp_ssize_t)iov[i].len;
    }
    return written;
}

static const zp_transport_t s_transport = {
    .start = NULL,
    .writev = loopback_writev,
    .recv = NULL,
    .close = NULL,
};

const zp_transport_t* zp_transport = &s_transport;

static void flush_bus_to_receiver(const char* label) {
    printf("\n--> Transport: Transmitting %zu bytes [%s]...\n", s_bus_len, label);
    // Feed raw bytes into the ZeroPack receiver state machine
    zp_process_rx_bytes(s_bus_buffer, s_bus_len);
    s_bus_len = 0;
}

// -----------------------------------------------------------------------------
// Action Handlers (Callbacks)
// -----------------------------------------------------------------------------
static void on_ping(void) {
    printf("  [Callback] Handled ACTION_PING (void action)\n");
}

static void on_report(const SensorReport* report) {
    printf("  [Callback] Handled ACTION_REPORT:\n");
    printf("    Temperature : %.2f C\n", (double)report->temperature);
    printf("    Humidity    : %.2f %%\n", (double)report->humidity);
    printf("    Timestamp   : %u ms\n", (unsigned int)report->timestamp);
}

// -----------------------------------------------------------------------------
// Main Entry Point
// -----------------------------------------------------------------------------
int main(void) {
    printf("=== ZeroPack Protocol Minimal Loopback Example ===\n");

    // Register event callbacks
    zp_on_action(ACTION_PING, on_ping);
    zp_on_action(ACTION_REPORT, on_report);

    // Emit a void action (no payload)
    printf("\n[Step 1] Emitting ACTION_PING...");
    zp_emit(ACTION_PING);
    flush_bus_to_receiver("ACTION_PING");

    // Emit a structured action
    SensorReport sample = {
        .temperature = 23.45f,
        .humidity = 58.20f,
        .timestamp = 42000,
    };
    printf("\n[Step 2] Emitting ACTION_REPORT (SensorReport payload)...");
    zp_emit(ACTION_REPORT, &sample);
    flush_bus_to_receiver("ACTION_REPORT");

    // Send binary handshake schema
    printf("\n[Step 3] Emitting self-describing binary handshake...\n");
    zp_send_handshake();
    printf("  Handshake binary size: %zu bytes (contains complete schema & field definitions)\n", s_bus_len);
    s_bus_len = 0;

    printf("\n=== Execution completed successfully! ===\n");
    return 0;
}
