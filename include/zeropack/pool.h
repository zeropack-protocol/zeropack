#pragma once

#include <stdbool.h>
#include <stddef.h>

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
} zp_pool_slot_t;

#define ZP_ALIGN4(x) (((x) + 3) & ~3)
#define ZP_ALIGN8(x) (((x) + 7) & ~7)
#define ZP_SLOT_TOTAL_SIZE(data_len) \
    ZP_ALIGN8(sizeof(zp_pool_slot_t) + (data_len))

extern uint8_t* __zp_pool;

void zp_pool_reset(void);
size_t zp_pool_used(void);
size_t zp_pool_free(void);

poolref_t zp_pool_push(uint8_t type_id, const void* data, uint16_t count);
bool zp_pool_edit(
    poolref_t ref, uint8_t type_id, const void* data, uint16_t count
);

#define zp_pool_get(ref) ((void*)(__zp_pool + (ref) + sizeof(zp_pool_slot_t)))
#define zp_pool_slot(ref) ((zp_pool_slot_t*)(__zp_pool + (ref)))

static inline bool zp_pool_is_valid(poolref_t ref) {
    return (ref + sizeof(zp_pool_slot_t)) <= zp_pool_used();
}

static inline void* zp_pool_get_safe(poolref_t ref) {
    return zp_pool_is_valid(ref) ? zp_pool_get(ref) : NULL;
}

#define zp_pool_str(s) zp_pool_push(TYPE_ID_u8, (s), zp_strlen(s) + 1)
#define zp_pool_val(type, v) zp_pool_push(TYPE_ID_##type, &(v), 1)
#define zp_pool_arr(type, ptr, count) \
    zp_pool_push(TYPE_ID_##type, (ptr), (count))
