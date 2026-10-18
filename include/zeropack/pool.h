#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "zeropack/config.h"
#include "zeropack/protocol.h"
#include "zeropack/types.h"

typedef enum {
    ZP_POOL_FLAG_NONE = 0x00,
    ZP_POOL_FLAG_READONLY = (1 << 0),
} zp_pool_flags_t;

typedef struct {
    uint8_t type_id;
    uint8_t flags;
    uint16_t count;
    uint16_t cap;
} zp_pool_slot_t;

_Static_assert(sizeof(zp_pool_slot_t) == 6, "zp_pool_slot_t must be 6 bytes");
_Static_assert(
    _Alignof(zp_pool_slot_t) == 2, "zp_pool_slot_t must be 2-byte aligned"
);

#define ZP_ALIGN8(x) (((x) + 7) & ~7)
#define ZP_SLOT_TOTAL_SIZE(data_len) \
    ZP_ALIGN8(sizeof(zp_pool_slot_t) + (data_len))

#define zp_pool_avail(ctx) ((ctx)->pool_cap - (ctx)->pool_len)

void zp_pool_reset(zp_ctx_t* ctx);

poolref_t zp_pool_push(
    zp_ctx_t* ctx, uint8_t type_id, const void* data, uint16_t count
);

zp_status_t zp_pool_edit(
    zp_ctx_t* ctx,
    poolref_t ref,
    uint8_t type_id,
    const void* data,
    uint16_t count
);

#define zp_pool_slot(ctx, ref) ((zp_pool_slot_t*)((ctx)->pool_buf + (ref)))
#define _zp_pool_get(ctx, ref) \
    ((void*)((ctx)->pool_buf + (ref) + sizeof(zp_pool_slot_t)))

bool zp_pool_is_valid(const zp_ctx_t* ctx, poolref_t ref);
void* zp_pool_get_checked(
    const zp_ctx_t* ctx, uint8_t type, poolref_t ref, uint16_t* out_count
);

#define zp_pool_get_str(ctx, ref) \
    ((char*)zp_pool_get_checked((ctx), TYPE_ID_u8, (ref), NULL))

#define zp_pool_get_typed(ctx, type, ref)                   \
    ((type*)zp_pool_get_checked(                            \
        (ctx), zp_type_id((ctx)->schema, type), (ref), NULL \
    ))

#define zp_pool_get_slice(ctx, type, ref, out_count)               \
    ((type*)zp_pool_get_checked(                                   \
        (ctx), zp_type_id((ctx)->schema, type), (ref), (out_count) \
    ))

#define zp_pool_push_str(ctx, s) \
    zp_pool_push((ctx), TYPE_ID_u8, (s), (uint16_t)(zp_strlen(s) + 1))
#define zp_pool_push_val(ctx, type, v) \
    zp_pool_push((ctx), zp_type_id((ctx)->schema, type), &(v), 1)
#define zp_pool_push_arr(ctx, type, ptr, count) \
    zp_pool_push((ctx), zp_type_id((ctx)->schema, type), (ptr), (count))
