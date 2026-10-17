#pragma once

#include <stddef.h>
#include <stdint.h>

#include "zeropack/action.h"
#include "zeropack/schema.h"
#include "zeropack/types.h"

#define ZP_SLICE(n, s) .n##_buf = (s), .n##_cap = sizeof(s)

#define ZP_LOCK(ctx, target)                                     \
    do {                                                         \
        if ((ctx)->lock) (ctx)->lock(target, (ctx)->userdata);   \
    } while (0)

#define ZP_UNLOCK(ctx, target)                                       \
    do {                                                             \
        if ((ctx)->unlock) (ctx)->unlock(target, (ctx)->userdata);   \
    } while (0)

typedef enum {
    ZP_LOCK_TX = 0,
    ZP_LOCK_POOL = 1,
} zp_lock_target_t;

typedef void (*zp_lock_fn_t)(zp_lock_target_t target, void* userdata);
typedef zp_ssize_t (*zp_writev_fn_t)(const zp_iovec_t* iov, int iovcnt);

struct zp_ctx_s {
    const zp_schema_t* schema;

    zp_lock_fn_t lock;
    zp_lock_fn_t unlock;
    zp_writev_fn_t writev;
    zp_dispatch_fn_t dispatch;

    uint8_t* rx_buf;
    size_t rx_cap;
    size_t rx_len;

    uint8_t* pool_buf;
    size_t pool_cap;
    size_t pool_len;

    void* userdata;
};

typedef struct {
    const zp_schema_t* schema;

    zp_lock_fn_t lock;
    zp_lock_fn_t unlock;
    zp_writev_fn_t writev;
    zp_dispatch_fn_t dispatch;

    void* rx_buf;
    size_t rx_cap;

    void* pool_buf;
    size_t pool_cap;

    void* userdata;
} zp_config_t;

int zp_init(zp_ctx_t* ctx, const zp_config_t* cfg);

zp_ssize_t zp_send_raw(
    zp_ctx_t* ctx,
    uint8_t action,
    uint8_t type_id,
    uint16_t count,
    const void* payload,
    size_t size
);

void zp_send_handshake(zp_ctx_t* ctx);
uint8_t* zp_rx_get_buffer(zp_ctx_t* ctx, size_t* out_available_len);
void zp_rx_commit(zp_ctx_t* ctx, size_t len);
void zp_emit_log(
    zp_ctx_t* ctx, zp_log_level_t level, zp_log_code_t code, uint16_t extra
);
