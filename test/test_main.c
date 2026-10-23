#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ZP_IMPLEMENTATION
#include "zeropack.h"

// =============================================================================
// Minimalist Test Harness
// =============================================================================

static int s_tests_run = 0;
static int s_tests_failed = 0;

#define TEST_SUITE_BEGIN() \
    printf("\n=== Running ZeroPack Core Test Suite ===\n\n")

#define TEST_SUITE_END()                                                      \
    do {                                                                      \
        printf("\n========================================\n");               \
        if (s_tests_failed == 0) {                                            \
            printf(                                                           \
                "PASSED (%d/%d tests successful)\n", s_tests_run, s_tests_run \
            );                                                                \
        } else {                                                              \
            printf(                                                           \
                "FAILED (%d/%d tests failed)\n", s_tests_failed, s_tests_run  \
            );                                                                \
        }                                                                     \
        printf("========================================\n");                 \
        return s_tests_failed == 0 ? 0 : 1;                                   \
    } while (0)

#define RUN_TEST(fn)                         \
    do {                                     \
        s_tests_run++;                       \
        printf("  [RUN]  %s ... ", #fn);     \
        bool _ok = fn();                     \
        if (_ok) {                           \
            printf("\033[32mOK\033[0m\n");   \
        } else {                             \
            s_tests_failed++;                \
            printf("\033[31mFAIL\033[0m\n"); \
        }                                    \
    } while (0)

#define ASSERT(expr)                                  \
    do {                                              \
        if (!(expr)) {                                \
            printf(                                   \
                "\n    Assert failed at %s:%d: %s\n", \
                __FILE__,                             \
                __LINE__,                             \
                #expr                                 \
            );                                        \
            return false;                             \
        }                                             \
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

#define COORD_FIELDS(S, F, A) \
    F(S, f32, x)              \
    F(S, f32, y)              \
    F(S, f32, z)

#define SENSOR_TYPES(X) X(sensor_t, SENSOR_FIELDS)
#define COORD_TYPES(X) X(coord_t, COORD_FIELDS)

// Composed types list demonstrates modular concatenation
#define TEST_TYPES(X) \
    SENSOR_TYPES(X)   \
    COORD_TYPES(X)

#define BASE_ACTIONS(V, T, A)   \
    V(ACTION_PING, "WIRE_PING") \
    V(ACTION_RESET_DEV)

#define DATA_ACTIONS(V, T, A)       \
    T(ACTION_UPDATE_DATA, sensor_t) \
    T(ACTION_SET_COORD, coord_t)    \
    A(ACTION_SEND_BUFFER, u8)

// Composed actions list demonstrates modular concatenation
#define TEST_ACTIONS(V, T, A) \
    BASE_ACTIONS(V, T, A)     \
    DATA_ACTIONS(V, T, A)

#define SINGLE_FIELDS(S, F, A) F(S, u32, val)
#define SINGLE_TYPES(X) X(single_item_t, SINGLE_FIELDS)
ZP_TYPES(SINGLE_TYPES)

ZP_TYPES(TEST_TYPES)
ZP_SCHEMA(test_schema, TEST_TYPES, TEST_ACTIONS)

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

static void test_setup(void) {
    s_tx_capture_len = 0;
    memset(s_rx_buf, 0, sizeof(s_rx_buf));
    memset(s_pool_buf, 0, sizeof(s_pool_buf));
    memset(s_tx_capture, 0, sizeof(s_tx_capture));

    s_ctx = (zp_ctx_t){
        .schema = &test_schema,
        .writev = test_writev,
        .buf = {.data = s_rx_buf, .cap = sizeof(s_rx_buf)},
        .pool = {.data = s_pool_buf, .cap = sizeof(s_pool_buf)},
    };
    zp_init(&s_ctx);
}

// =============================================================================
// Tests: Schema & Metadata
// =============================================================================

static bool test_schema_metadata(void) {
    test_setup();

    // Struct sizes & names
    ASSERT_EQ(sizeof(sensor_t), zp_metaof(sensor_t)->size);
    ASSERT_STR_EQ("sensor_t", zp_metaof(sensor_t)->name);
    ASSERT_EQ(sizeof(coord_t), zp_metaof(coord_t)->size);
    ASSERT_STR_EQ("coord_t", zp_metaof(coord_t)->name);

    // Field offsets match ABI
    ASSERT_EQ(
        offsetof(sensor_t, sensor_id), zp_metaof(sensor_t)->fields[0].offset
    );
    ASSERT_EQ(
        offsetof(sensor_t, temp_milli), zp_metaof(sensor_t)->fields[1].offset
    );
    ASSERT_EQ(
        offsetof(sensor_t, is_active), zp_metaof(sensor_t)->fields[2].offset
    );

    // Actions count & wire names
    ASSERT_EQ(5, test_schema.action_count);
    ASSERT_STR_EQ("WIRE_PING", test_schema.actions[0].name);
    ASSERT_STR_EQ("ACTION_RESET_DEV", test_schema.actions[1].name);
    ASSERT_STR_EQ("ACTION_UPDATE_DATA", test_schema.actions[2].name);

    // Modular ZP_TYPES verification
    ASSERT_EQ(sizeof(single_item_t), zp_metaof(single_item_t)->size);
    ASSERT_STR_EQ("single_item_t", zp_metaof(single_item_t)->name);

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
    sensor_t* wrong = zp_pool_get(&s_ctx, sensor_t, ref, NULL);
    ASSERT_NULL(wrong);

    // Check slot capacity and alignment (8-byte aligned)
    size_t expected_slot_size = ZP_SLOT_TOTAL_SIZE(strlen(original) + 1);
    ASSERT_EQ(expected_slot_size, s_ctx.pool.head);

    return true;
}

static bool test_pool_values_and_mutation(void) {
    test_setup();

    sensor_t s1 = {
        .sensor_id = 42,
        .temp_milli = 25000,
        .is_active = true,
    };

    poolref_t ref_s1 = zp_pool_push(&s_ctx, sensor_t, &s1, 1);
    ASSERT_EQ(0, ref_s1);

    // Typed retrieval
    sensor_t* got = zp_pool_get(&s_ctx, sensor_t, ref_s1, NULL);
    ASSERT_NOT_NULL(got);
    ASSERT_EQ(42, got->sensor_id);
    ASSERT_EQ(25000, got->temp_milli);
    ASSERT_EQ(true, got->is_active);

    // Type mismatch check: coord_t is not sensor_t
    coord_t* wrong_coord = zp_pool_get(&s_ctx, coord_t, ref_s1, NULL);
    ASSERT_NULL(wrong_coord);

    // In-place mutation
    got->temp_milli = 27500;
    sensor_t* mutated = zp_pool_get(&s_ctx, sensor_t, ref_s1, NULL);
    ASSERT_EQ(27500, mutated->temp_milli);

    return true;
}

static bool test_pool_slices_and_arrays(void) {
    test_setup();

    u32 raw_data[4] = {10, 20, 30, 40};
    poolref_t ref_arr = zp_pool_push(&s_ctx, u32, raw_data, 4);
    ASSERT_EQ(0, ref_arr);

    uint16_t out_count = 0;
    u32* slice = zp_pool_get(&s_ctx, u32, ref_arr, &out_count);
    ASSERT_NOT_NULL(slice);
    ASSERT_EQ(4, out_count);
    ASSERT_EQ(10, slice[0]);
    ASSERT_EQ(20, slice[1]);
    ASSERT_EQ(30, slice[2]);
    ASSERT_EQ(40, slice[3]);

    // Type mismatch check: u16 slice instead of u32
    u16* wrong_slice = zp_pool_get(&s_ctx, u16, ref_arr, &out_count);
    ASSERT_NULL(wrong_slice);

    return true;
}

static bool test_pool_validation_and_bounds(void) {
    test_setup();

    sensor_t s = {.sensor_id = 1};
    poolref_t ref = zp_pool_push(&s_ctx, sensor_t, &s, 1);
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
    poolref_t fail_ref = zp_pool_push(&s_ctx, u8, large_blob, 600);
    ASSERT_EQ(0xFFFF, fail_ref);

    return true;
}

static bool test_pool_edit_and_reset(void) {
    test_setup();

    sensor_t s1 = {.sensor_id = 10, .temp_milli = 1000, .is_active = true};
    poolref_t ref = zp_pool_push(&s_ctx, sensor_t, &s1, 1);
    ASSERT_EQ(0, ref);

    // Valid edit
    sensor_t s2 = {.sensor_id = 20, .temp_milli = 2000, .is_active = false};
    zp_status_t st =
        zp_pool_edit(&s_ctx, ref, zp_type_id(&test_schema, sensor_t), &s2, 1);
    ASSERT_EQ(ZP_OK, st);

    sensor_t* got = zp_pool_get(&s_ctx, sensor_t, ref, NULL);
    ASSERT_EQ(20, got->sensor_id);
    ASSERT_EQ(2000, got->temp_milli);
    ASSERT_EQ(false, got->is_active);

    // Edit with mismatched type rejected
    coord_t c = {.x = 1.0f, .y = 2.0f, .z = 3.0f};
    st = zp_pool_edit(&s_ctx, ref, zp_type_id(&test_schema, coord_t), &c, 1);
    ASSERT_EQ(ZP_POOL_TYPE_MISMATCH, st);

    // Pool reset restores capacity from 0
    zp_pool_reset(&s_ctx);
    ASSERT_EQ(0, s_ctx.pool.head);

    poolref_t new_ref = zp_pool_push_str(&s_ctx, "fresh");
    ASSERT_EQ(0, new_ref);
    ASSERT_STR_EQ("fresh", zp_pool_get_str(&s_ctx, new_ref));

    return true;
}

// =============================================================================
// Tests: Core RX & Dispatch Pipeline
// =============================================================================

static bool test_rx_pipeline_and_dispatch(void) {
    test_setup();

    // 1. Dispatch void action
    zp_header_t ping_hdr = {
        .action = ACTION_PING,
        .type_id = TYPE_ID_NONE,
        .count = 0,
    };
    uint8_t* buf = zp_buffer(&s_ctx, NULL);
    memcpy(buf, &ping_hdr, sizeof(ping_hdr));
    zp_commit(&s_ctx, sizeof(ping_hdr));

    zp_msg_t msg;
    ASSERT(zp_recv(&s_ctx, &msg));

    ASSERT_EQ(msg.action, ACTION_PING);
    ASSERT_EQ(msg.type_id, TYPE_ID_NONE);
    ASSERT_EQ(msg.count, 0);
    ASSERT_EQ(msg.payload, NULL);

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
    buf = zp_buffer(&s_ctx, NULL);
    memcpy(buf, &sensor_hdr, sizeof(sensor_hdr));
    memcpy(buf + sizeof(sensor_hdr), &tx_sensor, sizeof(tx_sensor));
    zp_commit(&s_ctx, sizeof(sensor_hdr) + sizeof(tx_sensor));

    ASSERT(zp_recv(&s_ctx, &msg));
    ASSERT_EQ(msg.action, ACTION_UPDATE_DATA);
    ASSERT_EQ(msg.type_id, zp_type_id(&test_schema, sensor_t));
    ASSERT_EQ(msg.count, 1);
    ASSERT_NOT_NULL(msg.payload);

    const sensor_t* rx_sensor = (const sensor_t*)msg.payload;
    ASSERT_EQ(999, rx_sensor->sensor_id);
    ASSERT_EQ(-4000, rx_sensor->temp_milli);
    ASSERT_EQ(true, rx_sensor->is_active);

    return true;
}

static bool test_rx_fragmentation(void) {
    test_setup();

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
        uint8_t* buf = zp_buffer(&s_ctx, NULL);
        ASSERT_NOT_NULL(buf);
        buf[0] = packet[i];
        s_ctx.buf.head += 1;
        if (i < sizeof(packet) - 1) {
            zp_msg_t msg;
            ASSERT(!zp_recv(&s_ctx, &msg));
        }
    }

    // After last byte, message must be ready
    zp_msg_t msg;
    ASSERT(zp_recv(&s_ctx, &msg));
    ASSERT_EQ(msg.action, ACTION_UPDATE_DATA);
    const sensor_t* rx_sensor = (const sensor_t*)msg.payload;
    ASSERT_NOT_NULL(rx_sensor);
    ASSERT_EQ(1234, rx_sensor->sensor_id);
    ASSERT_EQ(18500, rx_sensor->temp_milli);
    ASSERT_EQ(true, rx_sensor->is_active);

    return true;
}

static bool test_boolean_normalization(void) {
    test_setup();

    // Send a dirty boolean (0x5A instead of 0 or 1) via TYPE_ID_bool to the pool
    uint8_t dirty_bool = 0x5A;
    zp_header_t hdr = {
        .action = ZP_SYS_ACTION_POOL_STORE,
        .type_id = TYPE_ID_bool,
        .count = 1,
    };

    uint8_t* buf = zp_buffer(&s_ctx, NULL);
    memcpy(buf, &hdr, sizeof(hdr));
    memcpy(buf + sizeof(hdr), &dirty_bool, sizeof(dirty_bool));
    zp_commit(&s_ctx, sizeof(hdr) + sizeof(dirty_bool));

    zp_msg_t msg;
    ASSERT(!zp_recv(&s_ctx, &msg));

    bool* b = zp_pool_get(&s_ctx, bool, 0, NULL);
    ASSERT_NOT_NULL(b);

    // Protocol must normalize boolean to exactly 1 (true)
    ASSERT_EQ(true, *b);
    ASSERT_EQ(1, (int)*b);

    return true;
}

static bool test_internal_pool_sys_actions(void) {
    test_setup();

    // 1. Store boolean value in pool via wire SYS_ACTION_POOL_STORE
    uint8_t raw_bool = 0x55;
    zp_header_t store_hdr = {
        .action = ZP_SYS_ACTION_POOL_STORE,
        .type_id = TYPE_ID_bool,
        .count = 1,
    };
    uint8_t* buf = zp_buffer(&s_ctx, NULL);
    memcpy(buf, &store_hdr, sizeof(store_hdr));
    memcpy(buf + sizeof(store_hdr), &raw_bool, sizeof(raw_bool));
    zp_commit(&s_ctx, sizeof(store_hdr) + sizeof(raw_bool));

    zp_msg_t msg;
    ASSERT(!zp_recv(&s_ctx, &msg));

    ASSERT_EQ(true, s_ctx.pool.head > 0);
    bool* b_val = zp_pool_get(&s_ctx, bool, 0, NULL);
    ASSERT_NOT_NULL(b_val);
    ASSERT_EQ(true, *b_val);

    // 2. Edit boolean value in pool via wire SYS_ACTION_POOL_EDIT (ref = 0,
    // new_val = 0)
    struct __attribute__((packed)) {
        zp_header_t hdr;
        poolref_t ref;
        uint8_t val;
    } edit_pkt = {
        .hdr =
            {
                .action = ZP_SYS_ACTION_POOL_EDIT,
                .type_id = TYPE_ID_bool,
                .count = 1,
            },
        .ref = 0,
        .val = 0x00,
    };
    buf = zp_buffer(&s_ctx, NULL);
    memcpy(buf, &edit_pkt, sizeof(edit_pkt));
    zp_commit(&s_ctx, sizeof(edit_pkt));
    ASSERT(!zp_recv(&s_ctx, &msg));

    b_val = zp_pool_get(&s_ctx, bool, 0, NULL);
    ASSERT_NOT_NULL(b_val);
    ASSERT_EQ(false, *b_val);

    // 3. Receive internal POOL_RESET packet
    zp_header_t reset_hdr = {
        .action = ZP_SYS_ACTION_POOL_RESET,
        .type_id = TYPE_ID_NONE,
        .count = 0,
    };
    buf = zp_buffer(&s_ctx, NULL);
    memcpy(buf, &reset_hdr, sizeof(reset_hdr));
    zp_commit(&s_ctx, sizeof(reset_hdr));
    ASSERT(!zp_recv(&s_ctx, &msg));

    // Pool must be cleanly reset
    ASSERT_EQ(0, s_ctx.pool.head);

    return true;
}

static bool test_uninitialized_guard(void) {
    zp_ctx_t null_ctx = {0};

    // Any operation on null/empty context must be safely rejected without
    // crashing
    ASSERT_EQ(-1, zp_send_raw(&null_ctx, 1, 1, 0, NULL, 0));
    ASSERT_EQ(-1, zp_send(&null_ctx, 0, NULL, 0));
    size_t avail = 99;
    ASSERT_NULL(zp_buffer(&null_ctx, &avail));
    ASSERT_EQ(0, avail);
    ASSERT_NULL(zp_buffer(NULL, &avail));
    ASSERT_EQ(0, avail);
    ASSERT_EQ(0xFFFF, zp_pool_push(&null_ctx, u8, "test", 4));
    ASSERT_EQ(
        ZP_INVALID_ACTION, zp_pool_edit(&null_ctx, 0, TYPE_ID_u8, "test", 4)
    );
    ASSERT_EQ(false, zp_pool_is_valid(&null_ctx, 0));
    ASSERT_EQ(false, zp_recv(&null_ctx, NULL));

    // Reject out-of-bounds actions
    ASSERT_EQ(-1, zp_send(&s_ctx, 99, NULL, 0));

    // Sending void action via zp_send normalizes count to 0 even if passed > 0
    s_tx_capture_len = 0;
    ASSERT_EQ(4, zp_send(&s_ctx, ACTION_PING, NULL, 5));
    const zp_header_t* sent_hdr = (const zp_header_t*)s_tx_capture;
    ASSERT_EQ(ACTION_PING, sent_hdr->action);
    ASSERT_EQ(0, sent_hdr->count);

    zp_ctx_t uninit_ctx = {
        .schema = &test_schema,
        .writev = test_writev,
        .buf = {.data = s_rx_buf, .cap = sizeof(s_rx_buf)},
        .pool = {.data = s_pool_buf, .cap = sizeof(s_pool_buf)},
    };
    // Calling zp_init activates the context
    ASSERT_EQ(&uninit_ctx, zp_init(&uninit_ctx));
    ASSERT(uninit_ctx.buf.data != NULL);

    return true;
}

static bool test_rx_compaction_on_boundary(void) {
    test_setup();

    // 1. Consume a ping packet so tail > 0
    zp_header_t ping_hdr = {
        .action = ACTION_PING,
        .type_id = TYPE_ID_NONE,
        .count = 0,
    };
    uint8_t* buf = zp_buffer(&s_ctx, NULL);
    memcpy(buf, &ping_hdr, sizeof(ping_hdr));
    s_ctx.buf.head += sizeof(ping_hdr);

    zp_msg_t msg;
    ASSERT(zp_recv(&s_ctx, &msg));
    ASSERT_EQ(ACTION_PING, msg.action);

    // 2. Prepare a sensor packet
    sensor_t s = {.sensor_id = 42, .temp_milli = 25000, .is_active = true};
    zp_header_t s_hdr = {
        .action = ACTION_UPDATE_DATA,
        .type_id = zp_type_id(&test_schema, sensor_t),
        .count = 1,
    };
    uint8_t pkt[sizeof(zp_header_t) + sizeof(sensor_t)];
    memcpy(pkt, &s_hdr, sizeof(s_hdr));
    memcpy(pkt + sizeof(s_hdr), &s, sizeof(s));

    // Place only part1 (6 bytes) at the very end of buf
    size_t part1_len = 6;
    s_ctx.buf.tail = s_ctx.buf.cap - part1_len;
    s_ctx.buf.head = s_ctx.buf.cap;  // hits the ceiling!
    memcpy(s_ctx.buf.data + s_ctx.buf.tail, pkt, part1_len);

    // 3. zp_recv detects incomplete packet and boundary collision, compacts to
    // 0
    ASSERT(!zp_recv(&s_ctx, &msg));
    ASSERT_EQ(0, s_ctx.buf.tail);
    ASSERT_EQ(part1_len, s_ctx.buf.head);

    // 4. Now zp_buffer has free space
    size_t avail = 0;
    uint8_t* rx_dest = zp_buffer(&s_ctx, &avail);
    ASSERT(avail > sizeof(pkt));
    ASSERT_NOT_NULL(rx_dest);

    // 5. Append part2 into the freed space
    size_t part2_len = sizeof(pkt) - part1_len;
    memcpy(rx_dest, pkt + part1_len, part2_len);
    s_ctx.buf.head += part2_len;

    // 6. zp_recv successfully decodes the complete packet
    ASSERT(zp_recv(&s_ctx, &msg));
    ASSERT_EQ(ACTION_UPDATE_DATA, msg.action);
    const sensor_t* rx_s = (const sensor_t*)msg.payload;
    ASSERT_NOT_NULL(rx_s);
    ASSERT_EQ(42, rx_s->sensor_id);
    ASSERT_EQ(25000, rx_s->temp_milli);
    ASSERT_EQ(true, rx_s->is_active);

    return true;
}

static bool test_security_pool_and_handshake(void) {
    test_setup();

    // 1. Handshake with NULL pool data must not crash
    zp_ctx_t no_pool_ctx = {
        .schema = &test_schema,
        .writev = test_writev,
        .buf = {.data = s_rx_buf, .cap = sizeof(s_rx_buf)},
        .pool = {.data = NULL, .cap = 0},
    };
    ASSERT_NOT_NULL(zp_init(&no_pool_ctx));
    s_tx_capture_len = 0;
    zp_send_handshake(&no_pool_ctx);
    ASSERT(s_tx_capture_len > 0);

    // 2. Raw u8 bytes without NUL terminator must be rejected by
    // zp_pool_get_str
    uint8_t raw_bytes[] = {'A', 'B', 'C'};
    poolref_t raw_ref =
        zp_pool_push_raw(&s_ctx, TYPE_ID_u8, raw_bytes, sizeof(raw_bytes));
    ASSERT(raw_ref != 0xFFFF);
    ASSERT_NULL(zp_pool_get_str(&s_ctx, raw_ref));

    // 3. Defensive check against corrupted head > cap in pool
    s_ctx.pool.head = s_ctx.pool.cap + 8;
    ASSERT(!zp_pool_is_valid(&s_ctx, 0));
    ASSERT_EQ(0xFFFF, zp_pool_push_raw(&s_ctx, TYPE_ID_u8, raw_bytes, 1));

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
    RUN_TEST(test_uninitialized_guard);
    RUN_TEST(test_rx_compaction_on_boundary);
    RUN_TEST(test_security_pool_and_handshake);

    TEST_SUITE_END();
}
