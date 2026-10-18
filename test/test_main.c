#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "zeropack.h"
#include "zeropack/core.h"
#include "zeropack/pool.h"
#include "zeropack/protocol.h"
#include "zeropack/schema.h"
#include "zeropack/types.h"

// =============================================================================
// Minimalist Test Harness
// =============================================================================

static int s_tests_run = 0;
static int s_tests_failed = 0;

#define TEST_SUITE_BEGIN() \
    printf("\n=== Running ZeroPack Core Test Suite ===\n\n")

#define TEST_SUITE_END()                                              \
    do {                                                              \
        printf("\n========================================\n");        \
        if (s_tests_failed == 0) {                                    \
            printf("PASSED (%d/%d tests successful)\n",               \
                   s_tests_run, s_tests_run);                         \
        } else {                                                      \
            printf("FAILED (%d/%d tests failed)\n",                   \
                   s_tests_failed, s_tests_run);                      \
        }                                                             \
        printf("========================================\n");        \
        return s_tests_failed == 0 ? 0 : 1;                           \
    } while (0)

#define RUN_TEST(fn)                                                  \
    do {                                                              \
        s_tests_run++;                                                \
        printf("  [RUN]  %s ... ", #fn);                              \
        bool _ok = fn();                                              \
        if (_ok) {                                                    \
            printf("\033[32mOK\033[0m\n");                            \
        } else {                                                      \
            s_tests_failed++;                                         \
            printf("\033[31mFAIL\033[0m\n");                          \
        }                                                             \
    } while (0)

#define ASSERT(expr)                                                  \
    do {                                                              \
        if (!(expr)) {                                                \
            printf("\n    Assert failed at %s:%d: %s\n",              \
                   __FILE__, __LINE__, #expr);                        \
            return false;                                             \
        }                                                             \
    } while (0)

#define ASSERT_EQ(a, b) ASSERT((a) == (b))
#define ASSERT_STR_EQ(a, b) ASSERT(strcmp((a), (b)) == 0)
#define ASSERT_NULL(p) ASSERT((p) == NULL)
#define ASSERT_NOT_NULL(p) ASSERT((p) != NULL)

// =============================================================================
// Test Schema Definition
// =============================================================================

#define SENSOR_FIELDS(S, F, A) \
    F(S, u16, sensor_id)       \
    F(S, s32, temp_milli)      \
    F(S, bool, is_active)

#define COORD_FIELDS(S, F, A)  \
    F(S, f32, x)               \
    F(S, f32, y)               \
    F(S, f32, z)

#define TEST_TYPES(X)          \
    X(sensor_t, SENSOR_FIELDS) \
    X(coord_t, COORD_FIELDS)

#define TEST_ACTIONS(V, T, A)               \
    V(ACTION_PING, "WIRE_PING")             \
    V(ACTION_RESET_DEV)                     \
    T(ACTION_UPDATE_DATA, sensor_t)         \
    T(ACTION_SET_COORD, coord_t)            \
    A(ACTION_SEND_BUFFER, u8)

ZP_DECLARE(TEST_TYPES, TEST_ACTIONS)
ZP_GENERATE_SCHEMA(test_schema, TEST_TYPES, TEST_ACTIONS)

// =============================================================================
// Test Context & Dummy Transport
// =============================================================================

static zp_ctx_t s_ctx;
static uint8_t s_rx_buf[512] __attribute__((aligned(8)));
static uint8_t s_pool_buf[512] __attribute__((aligned(8)));

static uint8_t s_tx_capture[512];
static size_t s_tx_capture_len = 0;

static zp_ssize_t test_writev(
    zp_ctx_t* ctx, const zp_iovec_t* iov, int iovcnt
) {
    (void)ctx;
    zp_ssize_t written = 0;
    for (int i = 0; i < iovcnt; i++) {
        if (s_tx_capture_len + iov[i].len > sizeof(s_tx_capture)) {
            return -1;
        }
        memcpy(s_tx_capture + s_tx_capture_len, iov[i].buf, iov[i].len);
        s_tx_capture_len += iov[i].len;
        written += (zp_ssize_t)iov[i].len;
    }
    return written;
}

static int s_lock_pool_count = 0;
static int s_unlock_pool_count = 0;
static int s_lock_tx_count = 0;
static int s_unlock_tx_count = 0;

static void test_lock(zp_ctx_t* ctx, zp_lock_target_t target) {
    (void)ctx;
    if (target == ZP_LOCK_POOL) s_lock_pool_count++;
    if (target == ZP_LOCK_TX) s_lock_tx_count++;
}

static void test_unlock(zp_ctx_t* ctx, zp_lock_target_t target) {
    (void)ctx;
    if (target == ZP_LOCK_POOL) s_unlock_pool_count++;
    if (target == ZP_LOCK_TX) s_unlock_tx_count++;
}

static void test_setup(void) {
    s_tx_capture_len = 0;
    s_lock_pool_count = 0;
    s_unlock_pool_count = 0;
    s_lock_tx_count = 0;
    s_unlock_tx_count = 0;
    memset(s_rx_buf, 0, sizeof(s_rx_buf));
    memset(s_pool_buf, 0, sizeof(s_pool_buf));
    memset(s_tx_capture, 0, sizeof(s_tx_capture));

    zp_init(
        &s_ctx,
        &(zp_config_t){
            .schema = &test_schema,
            .writev = test_writev,
            .lock = test_lock,
            .unlock = test_unlock,
            ZP_BUFFER(rx, s_rx_buf),
            ZP_BUFFER(pool, s_pool_buf),
        }
    );
}

// =============================================================================
// Tests: Schema & Metadata
// =============================================================================

static bool test_schema_metadata(void) {
    test_setup();

    // Struct sizes & names
    ASSERT_EQ(sizeof(sensor_t), zp_type_size(sensor_t));
    ASSERT_STR_EQ("sensor_t", zp_type_name(sensor_t));
    ASSERT_EQ(sizeof(coord_t), zp_type_size(coord_t));
    ASSERT_STR_EQ("coord_t", zp_type_name(coord_t));

    // Field offsets match ABI
    ASSERT_EQ(offsetof(sensor_t, sensor_id), zp_type_meta(sensor_t)->fields[0].offset);
    ASSERT_EQ(offsetof(sensor_t, temp_milli), zp_type_meta(sensor_t)->fields[1].offset);
    ASSERT_EQ(offsetof(sensor_t, is_active), zp_type_meta(sensor_t)->fields[2].offset);

    // Actions count & wire names
    ASSERT_EQ(5, test_schema.action_count);
    ASSERT_STR_EQ("WIRE_PING", test_schema.actions[0].name);
    ASSERT_STR_EQ("ACTION_RESET_DEV", test_schema.actions[1].name);
    ASSERT_STR_EQ("ACTION_UPDATE_DATA", test_schema.actions[2].name);

    // Test container_of macro
    struct dummy_transport {
        int dummy_fd;
        zp_ctx_t ctx;
    } transport = {.dummy_fd = 99};

    struct dummy_transport* parent = zp_container_of(&transport.ctx, struct dummy_transport, ctx);
    ASSERT_EQ(99, parent->dummy_fd);
    ASSERT_EQ(&transport, parent);

    return true;
}

// =============================================================================
// Tests: Pool Typed API (Strings, Values, Slices, Safety)
// =============================================================================

static bool test_pool_strings(void) {
    test_setup();

    const char* original = "Hello ZeroPack!";
    poolref_t ref = zp_pool_push_str(&s_ctx, original);
    ASSERT_EQ(0, ref);

    // Retrieve via typed string getter
    char* str = zp_pool_get_str(&s_ctx, ref);
    ASSERT_NOT_NULL(str);
    ASSERT_STR_EQ(original, str);

    // Type checking: attempting to retrieve string as a struct returns NULL
    sensor_t* wrong = zp_pool_get_typed(&s_ctx, sensor_t, ref);
    ASSERT_NULL(wrong);

    // Check slot capacity and alignment (8-byte aligned)
    size_t expected_slot_size = ZP_SLOT_TOTAL_SIZE(strlen(original) + 1);
    ASSERT_EQ(expected_slot_size, s_ctx.pool_len);

    return true;
}

static bool test_pool_values_and_mutation(void) {
    test_setup();

    sensor_t s1 = {
        .sensor_id = 42,
        .temp_milli = 25000,
        .is_active = true,
    };

    poolref_t ref_s1 = zp_pool_push_val(&s_ctx, sensor_t, s1);
    ASSERT_EQ(0, ref_s1);

    // Typed retrieval
    sensor_t* got = zp_pool_get_typed(&s_ctx, sensor_t, ref_s1);
    ASSERT_NOT_NULL(got);
    ASSERT_EQ(42, got->sensor_id);
    ASSERT_EQ(25000, got->temp_milli);
    ASSERT_EQ(true, got->is_active);

    // Type mismatch check: coord_t is not sensor_t
    coord_t* wrong_coord = zp_pool_get_typed(&s_ctx, coord_t, ref_s1);
    ASSERT_NULL(wrong_coord);

    // In-place mutation
    got->temp_milli = 27500;
    sensor_t* mutated = zp_pool_get_typed(&s_ctx, sensor_t, ref_s1);
    ASSERT_EQ(27500, mutated->temp_milli);

    return true;
}

static bool test_pool_slices_and_arrays(void) {
    test_setup();

    u32 raw_data[4] = {10, 20, 30, 40};
    poolref_t ref_arr = zp_pool_push_arr(&s_ctx, u32, raw_data, 4);
    ASSERT_EQ(0, ref_arr);

    uint16_t out_count = 0;
    u32* slice = zp_pool_get_slice(&s_ctx, u32, ref_arr, &out_count);
    ASSERT_NOT_NULL(slice);
    ASSERT_EQ(4, out_count);
    ASSERT_EQ(10, slice[0]);
    ASSERT_EQ(20, slice[1]);
    ASSERT_EQ(30, slice[2]);
    ASSERT_EQ(40, slice[3]);

    // Type mismatch check: u16 slice instead of u32
    u16* wrong_slice = zp_pool_get_slice(&s_ctx, u16, ref_arr, &out_count);
    ASSERT_NULL(wrong_slice);

    return true;
}

static bool test_pool_validation_and_bounds(void) {
    test_setup();

    sensor_t s = {.sensor_id = 1};
    poolref_t ref = zp_pool_push_val(&s_ctx, sensor_t, s);
    ASSERT_EQ(0, ref);

    // Valid ref
    ASSERT_EQ(true, zp_pool_is_valid(&s_ctx, ref));

    // Invalid misaligned ref
    ASSERT_EQ(false, zp_pool_is_valid(&s_ctx, ref + 1));
    ASSERT_EQ(false, zp_pool_is_valid(&s_ctx, ref + 3));

    // Out of bounds ref
    ASSERT_EQ(false, zp_pool_is_valid(&s_ctx, 1000));

    // Overflow protection: requesting push larger than pool capacity
    uint8_t large_blob[600];
    poolref_t fail_ref = zp_pool_push(&s_ctx, TYPE_ID_u8, large_blob, 600);
    ASSERT_EQ(0xFFFF, fail_ref);

    return true;
}

static bool test_pool_edit_and_reset(void) {
    test_setup();

    sensor_t s1 = {.sensor_id = 10, .temp_milli = 1000, .is_active = true};
    poolref_t ref = zp_pool_push_val(&s_ctx, sensor_t, s1);
    ASSERT_EQ(0, ref);

    // Valid edit
    sensor_t s2 = {.sensor_id = 20, .temp_milli = 2000, .is_active = false};
    zp_status_t st = zp_pool_edit(&s_ctx, ref, zp_type_id(&test_schema, sensor_t), &s2, 1);
    ASSERT_EQ(ZP_OK, st);

    sensor_t* got = zp_pool_get_typed(&s_ctx, sensor_t, ref);
    ASSERT_EQ(20, got->sensor_id);
    ASSERT_EQ(2000, got->temp_milli);
    ASSERT_EQ(false, got->is_active);

    // Edit with mismatched type rejected
    coord_t c = {.x = 1.0f, .y = 2.0f, .z = 3.0f};
    st = zp_pool_edit(&s_ctx, ref, zp_type_id(&test_schema, coord_t), &c, 1);
    ASSERT_EQ(ZP_POOL_TYPE_MISMATCH, st);

    // Pool reset restores capacity from 0
    zp_pool_reset(&s_ctx);
    ASSERT_EQ(0, s_ctx.pool_len);

    poolref_t new_ref = zp_pool_push_str(&s_ctx, "fresh");
    ASSERT_EQ(0, new_ref);
    ASSERT_STR_EQ("fresh", zp_pool_get_str(&s_ctx, new_ref));

    return true;
}

// =============================================================================
// Tests: Core RX & Dispatch Pipeline
// =============================================================================

static bool s_ping_handled = false;
static void on_test_ping(zp_ctx_t* ctx) {
    (void)ctx;
    s_ping_handled = true;
}

static sensor_t s_sensor_received = {0};
static void on_test_sensor(zp_ctx_t* ctx, const sensor_t* data) {
    (void)ctx;
    s_sensor_received = *data;
}

static bool test_rx_pipeline_and_dispatch(void) {
    test_setup();
    s_ping_handled = false;
    memset(&s_sensor_received, 0, sizeof(s_sensor_received));

    zp_on_action(test_schema, ACTION_PING, on_test_ping);
    zp_on_action(test_schema, ACTION_UPDATE_DATA, on_test_sensor);

    // 1. Dispatch void action
    zp_header_t ping_hdr = {
        .action = ACTION_PING,
        .type_id = TYPE_ID_NONE,
        .count = 0,
    };
    uint8_t* buf = zp_rx_buf(&s_ctx);
    memcpy(buf, &ping_hdr, sizeof(ping_hdr));
    zp_rx_commit(&s_ctx, sizeof(ping_hdr));
    ASSERT_EQ(true, s_ping_handled);

    // 2. Dispatch structured action
    sensor_t tx_sensor = {
        .sensor_id = 999,
        .temp_milli = -4000,
        .is_active = true,
    };
    zp_header_t sensor_hdr = {
        .action = ACTION_UPDATE_DATA,
        .type_id = zp_type_id(&test_schema, sensor_t),
        .count = 1,
    };
    buf = zp_rx_buf(&s_ctx);
    memcpy(buf, &sensor_hdr, sizeof(sensor_hdr));
    memcpy(buf + sizeof(sensor_hdr), &tx_sensor, sizeof(tx_sensor));
    zp_rx_commit(&s_ctx, sizeof(sensor_hdr) + sizeof(tx_sensor));

    ASSERT_EQ(999, s_sensor_received.sensor_id);
    ASSERT_EQ(-4000, s_sensor_received.temp_milli);
    ASSERT_EQ(true, s_sensor_received.is_active);

    return true;
}

static bool test_rx_fragmentation(void) {
    test_setup();
    memset(&s_sensor_received, 0, sizeof(s_sensor_received));
    zp_on_action(test_schema, ACTION_UPDATE_DATA, on_test_sensor);

    sensor_t tx_sensor = {
        .sensor_id = 1234,
        .temp_milli = 18500,
        .is_active = true,
    };
    zp_header_t hdr = {
        .action = ACTION_UPDATE_DATA,
        .type_id = zp_type_id(&test_schema, sensor_t),
        .count = 1,
    };

    uint8_t packet[sizeof(zp_header_t) + sizeof(sensor_t)];
    memcpy(packet, &hdr, sizeof(hdr));
    memcpy(packet + sizeof(hdr), &tx_sensor, sizeof(tx_sensor));

    // Deliver byte-by-byte into the RX pipeline
    for (size_t i = 0; i < sizeof(packet); i++) {
        uint8_t* buf = zp_rx_buf(&s_ctx);
        ASSERT_NOT_NULL(buf);
        buf[0] = packet[i];
        zp_rx_commit(&s_ctx, 1);
        if (i < sizeof(packet) - 1) {
            // Callback must not have fired yet
            ASSERT_EQ(0, s_sensor_received.sensor_id);
        }
    }

    // After last byte, callback must have fired
    ASSERT_EQ(1234, s_sensor_received.sensor_id);
    ASSERT_EQ(18500, s_sensor_received.temp_milli);
    ASSERT_EQ(true, s_sensor_received.is_active);

    return true;
}

static bool test_boolean_normalization(void) {
    test_setup();
    memset(&s_sensor_received, 0, sizeof(s_sensor_received));
    zp_on_action(test_schema, ACTION_UPDATE_DATA, on_test_sensor);

    // Set non-standard boolean value (e.g. 0x42 instead of 0 or 1)
    sensor_t tx_sensor = {
        .sensor_id = 77,
        .temp_milli = 100,
        .is_active = false,
    };
    uint8_t* raw_bytes = (uint8_t*)&tx_sensor;
    raw_bytes[offsetof(sensor_t, is_active)] = 0x5A; // dirty non-zero boolean

    zp_header_t hdr = {
        .action = ACTION_UPDATE_DATA,
        .type_id = zp_type_id(&test_schema, sensor_t),
        .count = 1,
    };

    uint8_t* buf = zp_rx_buf(&s_ctx);
    memcpy(buf, &hdr, sizeof(hdr));
    memcpy(buf + sizeof(hdr), &tx_sensor, sizeof(tx_sensor));
    zp_rx_commit(&s_ctx, sizeof(hdr) + sizeof(tx_sensor));

    // Protocol must normalize boolean to exactly 1 (true)
    ASSERT_EQ(true, s_sensor_received.is_active);
    ASSERT_EQ(1, (int)s_sensor_received.is_active);

    return true;
}

static bool test_internal_pool_sys_actions(void) {
    test_setup();

    // 1. Store value in pool
    sensor_t s = {.sensor_id = 55};
    zp_pool_push_val(&s_ctx, sensor_t, s);
    ASSERT_EQ(true, s_ctx.pool_len > 0);

    // 2. Receive internal POOL_RESET packet (wire action 0xFFFD)
    zp_header_t reset_hdr = {
        .action = ZP_SYS_ACTION_POOL_RESET,
        .type_id = TYPE_ID_NONE,
        .count = 0,
    };
    uint8_t* buf = zp_rx_buf(&s_ctx);
    memcpy(buf, &reset_hdr, sizeof(reset_hdr));
    zp_rx_commit(&s_ctx, sizeof(reset_hdr));

    // Pool must be cleanly reset
    ASSERT_EQ(0, s_ctx.pool_len);

    return true;
}

static bool test_lock_callbacks(void) {
    test_setup();

    // Pool push must lock and unlock pool mutex symmetrically
    sensor_t s = {.sensor_id = 123};
    zp_pool_push_val(&s_ctx, sensor_t, s);
    ASSERT_EQ(1, s_lock_pool_count);
    ASSERT_EQ(1, s_unlock_pool_count);

    // TX send must lock and unlock TX mutex symmetrically
    zp_emit(&s_ctx, ACTION_PING);
    ASSERT_EQ(1, s_lock_tx_count);
    ASSERT_EQ(1, s_unlock_tx_count);

    // Pool edit must lock and unlock pool mutex
    sensor_t s2 = {.sensor_id = 456};
    zp_pool_edit(&s_ctx, 0, zp_type_id(&test_schema, sensor_t), &s2, 1);
    ASSERT_EQ(2, s_lock_pool_count);
    ASSERT_EQ(2, s_unlock_pool_count);

    // Pool reset must lock and unlock pool mutex
    zp_pool_reset(&s_ctx);
    ASSERT_EQ(3, s_lock_pool_count);
    ASSERT_EQ(3, s_unlock_pool_count);

    return true;
}

// =============================================================================
// Main Runner
// =============================================================================

int main(void) {
    TEST_SUITE_BEGIN();

    RUN_TEST(test_schema_metadata);
    RUN_TEST(test_pool_strings);
    RUN_TEST(test_pool_values_and_mutation);
    RUN_TEST(test_pool_slices_and_arrays);
    RUN_TEST(test_pool_validation_and_bounds);
    RUN_TEST(test_pool_edit_and_reset);
    RUN_TEST(test_rx_pipeline_and_dispatch);
    RUN_TEST(test_rx_fragmentation);
    RUN_TEST(test_boolean_normalization);
    RUN_TEST(test_internal_pool_sys_actions);
    RUN_TEST(test_lock_callbacks);

    TEST_SUITE_END();
}
