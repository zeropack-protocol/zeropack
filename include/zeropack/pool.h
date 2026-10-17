#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "zeropack/config.h"
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
_Static_assert(_Alignof(zp_pool_slot_t) == 2, "zp_pool_slot_t must be 2-byte aligned");

#define ZP_ALIGN4(x) (((x) + 3) & ~3)
#define ZP_ALIGN8(x) (((x) + 7) & ~7)
#define ZP_SLOT_TOTAL_SIZE(data_len) \
    ZP_ALIGN8(sizeof(zp_pool_slot_t) + (data_len))

void zp_pool_reset(zp_ctx_t* ctx);
size_t zp_pool_used(const zp_ctx_t* ctx);
size_t zp_pool_free(const zp_ctx_t* ctx);

poolref_t zp_pool_push(
    zp_ctx_t* ctx, uint8_t type_id, const void* data, uint16_t count
);
bool zp_pool_edit(
    zp_ctx_t* ctx, poolref_t ref, uint8_t type_id, const void* data, uint16_t count
);

#define zp_pool_get(ctx, ref) \
    ((void*)((ctx)->pool_buf + (ref) + sizeof(zp_pool_slot_t)))
#define zp_pool_slot(ctx, ref) ((zp_pool_slot_t*)((ctx)->pool_buf + (ref)))

bool zp_pool_is_valid(const zp_ctx_t* ctx, poolref_t ref);
void* zp_pool_get_safe(zp_ctx_t* ctx, poolref_t ref);

#define zp_pool_str(ctx, s) \
    zp_pool_push((ctx), TYPE_ID_u8, (s), (uint16_t)(zp_strlen(s) + 1))
#define zp_pool_val(ctx, type, v) zp_pool_push((ctx), TYPE_ID_##type, &(v), 1)
#define zp_pool_arr(ctx, type, ptr, count) \
    zp_pool_push((ctx), TYPE_ID_##type, (ptr), (count))
