#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "zeropack.h"
#include "zeropack/core.h"

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
#define APP_TYPES(X) X(SensorReport, SENSOR_FIELDS)

#define SENSOR_FIELDS(S, F, A) \
    F(S, f32, temperature)     \
    F(S, f32, humidity)        \
    F(S, u32, timestamp)

#define APP_ACTIONS(V, T, A)    \
    V(ACTION_PING, "WIRE_PING") \
    V(ACTION_PONG)              \
    T(ACTION_REPORT, SensorReport, "WIRE_REPORT")

// Declare type IDs, structs, and dispatcher prototypes
ZP_DECLARE(APP_TYPES, APP_ACTIONS)

// Generate the runtime schema metadata and dispatch table
ZP_GENERATE_SCHEMA(example_schema, APP_TYPES, APP_ACTIONS)

// -----------------------------------------------------------------------------
// Simulated Loopback Transport (Writes directly into ZeroPack's leased rx
// buffer)
// -----------------------------------------------------------------------------
static zp_t s_zp;
static size_t s_pending_rx_len = 0;

static zp_ssize_t loopback_writev(const zp_iovec_t* iov, int iovcnt) {
    size_t avail = 0;
    uint8_t* rx_buf = zp_rx_get_buffer(&s_zp, &avail);
    if (!rx_buf) return -1;

    zp_ssize_t written = 0;
    for (int i = 0; i < iovcnt; i++) {
        if (s_pending_rx_len + iov[i].len > avail) {
            return -1;  // Overflow
        }
        memcpy(rx_buf + s_pending_rx_len, iov[i].buf, iov[i].len);
        s_pending_rx_len += iov[i].len;
        written += (zp_ssize_t)iov[i].len;
    }
    return written;
}

static void flush_bus_to_receiver(zp_t* zp, const char* label) {
    printf(
        "\n--> Transport: Transmitting %zu bytes [%s]...\n",
        s_pending_rx_len,
        label
    );
    // Commit the leased buffer directly into ZeroPack's receiver state machine
    zp_rx_commit(zp, s_pending_rx_len);
    s_pending_rx_len = 0;
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

static uint8_t s_rx[1024] __attribute__((aligned(8)));
static uint8_t s_pool[1024] __attribute__((aligned(8)));
int main(void) {
    printf("=== ZeroPack Protocol Minimal Loopback Example ===\n");

    // Register event callbacks
    zp_on_action(example_schema, ACTION_PING, on_ping);
    zp_on_action(example_schema, ACTION_REPORT, on_report);

    zp_init(
        &s_zp,
        &(zp_config_t){
            .schema = &example_schema,
            .writev = loopback_writev,
            ZP_SLICE(rx, s_rx),
            ZP_SLICE(pool, s_pool),
        }
    );

    // Emit a void action (no payload)
    printf("\n[Step 1] Emitting ACTION_PING...");
    zp_emit(&s_zp, ACTION_PING);
    flush_bus_to_receiver(&s_zp, "ACTION_PING");

    // Emit a structured action
    SensorReport sample = {
        .temperature = 23.45f,
        .humidity = 58.20f,
        .timestamp = 42000,
    };
    printf("\n[Step 2] Emitting ACTION_REPORT (SensorReport payload)...");
    zp_emit(&s_zp, ACTION_REPORT, &sample);
    flush_bus_to_receiver(&s_zp, "ACTION_REPORT");

    // Send binary handshake schema
    printf("\n[Step 3] Emitting self-describing binary handshake...\n");
    zp_send_handshake(&s_zp);
    printf(
        "  Handshake binary size: %zu bytes (contains complete schema & field "
        "definitions)\n",
        s_pending_rx_len
    );
    s_pending_rx_len = 0;

    printf("\n[Step 4] Verifying Wire Aliases in Runtime Schema Metadata:\n");
    for (int i = 0; i < example_schema.action_count; i++) {
        printf(
            "  Action ID %d -> wire name: \"%s\"\n",
            i,
            example_schema.actions[i].name
        );
    }

    printf("\n=== Execution completed successfully! ===\n");
    return 0;
}
