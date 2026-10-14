#include "zeropack/pool.h"

#include <stdint.h>

#include "zeropack/config.h"
#include "zeropack/core.h"
#include "zeropack/mutex.h"
#include "zeropack/types.h"

#if ZP_POOL_SIZE > 0
static uint8_t __zp_pool_raw[ZP_POOL_SIZE + 4] __attribute__((aligned(8)));
uint8_t* __zp_pool = &__zp_pool_raw[4];
#else
uint8_t* __zp_pool = NULL;
#endif

static uint16_t s_pool_head = 0;

poolref_t zp_pool_push(uint8_t type_id, const void* data, uint16_t count) {
    size_t type_size = get_type_size(type_id);
    if (type_size == 0) return 0xFFFF;
    if (count == 0) count = 1;

    size_t total_len = type_size * count;
    size_t total_slot = ZP_SLOT_TOTAL_SIZE(total_len);

#if ZP_POOL_SIZE == 0
    return 0xFFFF;
#else
    ZP_LOCK(pool);

    if (s_pool_head + total_slot > ZP_POOL_SIZE) {
        ZP_UNLOCK(pool);
        zp_emit_log(ZP_LOG_ERROR, ZP_ERR_POOL_FULL, (uint16_t)total_slot);
        return 0xFFFF;
    }

    zp_pool_slot_t* slot = (zp_pool_slot_t*)(__zp_pool + s_pool_head);
    slot->type_id = type_id;
    slot->flags = ZP_POOL_FLAG_NONE;
    slot->count = count;

    poolref_t ref = s_pool_head;
    zp_memcpy(
        __zp_pool + s_pool_head + sizeof(zp_pool_slot_t), data, total_len
    );

    s_pool_head += total_slot;
    ZP_UNLOCK(pool);
#endif

    return ref;
}

bool zp_pool_edit(
    poolref_t ref, uint8_t type_id, const void* data, uint16_t count
) {
#if ZP_POOL_SIZE == 0
    return false;
#else
    if (count == 0) count = 1;

    ZP_LOCK(pool);

    if (!zp_pool_is_valid(ref)) {
        ZP_UNLOCK(pool);
        return false;
    }

    zp_pool_slot_t* slot = zp_pool_slot(ref);
    if (slot->flags & ZP_POOL_FLAG_READONLY) {
        ZP_UNLOCK(pool);
        zp_emit_log(ZP_LOG_ERROR, ZP_ERR_POOL_READONLY, ref);
        return false;
    }
    if (type_id != slot->type_id) {
        ZP_UNLOCK(pool);
        zp_emit_log(ZP_LOG_ERROR, ZP_ERR_POOL_TYPE_MISMATCH, ref);
        return false;
    }

    if (count > slot->count) {
        ZP_UNLOCK(pool);
        zp_emit_log(ZP_LOG_ERROR, ZP_ERR_POOL_SLOT_EXCEEDED, count);
        return false;
    }

    bool shrunk = (count < slot->count);
    slot->type_id = type_id;
    slot->count = count;

    size_t data_len = get_type_size(type_id) * count;
    zp_memcpy(__zp_pool + ref + sizeof(zp_pool_slot_t), data, data_len);
    ZP_UNLOCK(pool);

    if (shrunk) {
        zp_emit_log(ZP_LOG_WARN, ZP_ERR_POOL_SLOT_SHRUNK, count);
    }

    return true;
#endif
}

void zp_pool_reset(void) {
    ZP_LOCK(pool);
    s_pool_head = 0;
    ZP_UNLOCK(pool);
}
size_t zp_pool_used(void) { return s_pool_head; }
size_t zp_pool_free(void) { return ZP_POOL_SIZE - s_pool_head; }
