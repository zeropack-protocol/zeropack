#include <stdio.h>
#include <string.h>

#include "zeropack.h"

// -----------------------------------------------------------------------------
// Schema Definition (Compile-time macros, no external codegen required)
// -----------------------------------------------------------------------------

#define SENSOR_REPORT_FIELDS(S, F, A) \
    F(S, f32, temperature)            \
    F(S, f32, humidity)               \
    F(S, u32, timestamp)

#define APP_TYPES(X) X(SensorReport, SENSOR_REPORT_FIELDS)

#define APP_ACTIONS(V, T, A)    \
    V(ACTION_PING, "WIRE_PING") \
    V(ACTION_PONG)              \
    T(ACTION_REPORT, SensorReport, "WIRE_REPORT")

ZP_DECLARE(APP_TYPES, APP_ACTIONS)

// Generate the runtime schema metadata and dispatch table
ZP_GENERATE_SCHEMA(example_schema, APP_TYPES, APP_ACTIONS)

// -----------------------------------------------------------------------------
// Simulated Loopback Transport (Writes directly into ZeroPack's leased rx
// buffer)
// -----------------------------------------------------------------------------
static size_t s_pending_rx_len = 0;

static zp_ssize_t loopback_writev(
    zp_ctx_t* ctx, const zp_iovec_t* iov, int iovcnt
) {
    uint8_t* rx_buf = zp_rx_buf(ctx);
    size_t avail = zp_rx_avail(ctx);

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

static void flush_bus_to_receiver(zp_ctx_t* ctx, const char* label) {
    printf(
        "\n--> Transport: Transmitting %zu bytes [%s]...\n",
        s_pending_rx_len,
        label
    );
    ctx->rx_head += s_pending_rx_len;
    s_pending_rx_len = 0;

    zp_msg_t msg;
    while (zp_poll(ctx, &msg)) {
        switch (msg.action) {
            case ACTION_PING:
                printf("  [Poll] Received ACTION_PING (void action)\n");
                break;
            case ACTION_REPORT: {
                const SensorReport* report = (const SensorReport*)msg.payload;
                printf("  [Poll] Received ACTION_REPORT:\n");
                printf(
                    "    Temperature : %.2f C\n", (double)report->temperature
                );
                printf("    Humidity    : %.2f %%\n", (double)report->humidity);
                printf(
                    "    Timestamp   : %u ms\n", (unsigned int)report->timestamp
                );
                break;
            }
            default:
                break;
        }
    }
}

// -----------------------------------------------------------------------------
// Main Entry Point
// -----------------------------------------------------------------------------

static uint8_t s_rx[1024] __attribute__((aligned(8)));
static uint8_t s_pool[1024] __attribute__((aligned(8)));
int main(void) {
    printf("=== ZeroPack Protocol Minimal Loopback Example ===\n");

    zp_ctx_t ctx = {
        .schema = &example_schema,
        .writev = loopback_writev,
        ZP_BUFFER(rx, s_rx),
        ZP_BUFFER(pool, s_pool),
    };
    zp_init(&ctx);

    // Emit a void action (no payload)
    printf("\n[Step 1] Emitting ACTION_PING...");
    zp_send_cmd(&ctx, ACTION_PING);
    flush_bus_to_receiver(&ctx, "ACTION_PING");

    // Emit a structured action
    SensorReport sample = {
        .temperature = 23.45f,
        .humidity = 58.20f,
        .timestamp = 42000,
    };
    printf("\n[Step 2] Emitting ACTION_REPORT (SensorReport payload)...");
    zp_send(&ctx, ACTION_REPORT, &sample);
    flush_bus_to_receiver(&ctx, "ACTION_REPORT");

    // Send binary handshake schema
    printf("\n[Step 3] Emitting self-describing binary handshake...\n");
    zp_send_handshake(&ctx);
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
