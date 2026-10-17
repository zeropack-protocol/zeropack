#include "zeropack/pool.h"

#include <stdint.h>

#include "zeropack/config.h"
#include "zeropack/core.h"
#include "zeropack/types.h"

bool zp_pool_is_valid(const zp_t* zp, poolref_t ref) {
    return zp && (ref + sizeof(zp_pool_slot_t)) <= zp_pool_used(zp);
}

void* zp_pool_get_safe(zp_t* zp, poolref_t ref) {
    return zp_pool_is_valid(zp, ref) ? zp_pool_get(zp, ref) : NULL;
}

poolref_t zp_pool_push(
    zp_t* zp, uint8_t type_id, const void* data, uint16_t count
) {
    if (!zp || !zp->pool_buf || !zp->schema) return 0xFFFF;

    size_t type_size = get_type_size(zp->schema, type_id);
    if (type_size == 0) return 0xFFFF;
    if (count == 0) count = 1;

    size_t total_len = type_size * count;
    size_t total_slot = ZP_SLOT_TOTAL_SIZE(total_len);

    if (zp->pool_cap == 0) return 0xFFFF;

    ZP_LOCK(zp, ZP_LOCK_POOL);

    if (zp->pool_len + total_slot > zp->pool_cap) {
        ZP_UNLOCK(zp, ZP_LOCK_POOL);
        zp_emit_log(zp, ZP_LOG_ERROR, ZP_ERR_POOL_FULL, (uint16_t)total_slot);
        return 0xFFFF;
    }

    zp_pool_slot_t* slot = (zp_pool_slot_t*)(zp->pool_buf + zp->pool_len);
    slot->type_id = type_id;
    slot->flags = ZP_POOL_FLAG_NONE;
    slot->count = count;

    poolref_t ref = (poolref_t)zp->pool_len;
    zp_memcpy(
        zp->pool_buf + zp->pool_len + sizeof(zp_pool_slot_t), data, total_len
    );

    zp->pool_len += total_slot;
    ZP_UNLOCK(zp, ZP_LOCK_POOL);

    return ref;
}

bool zp_pool_edit(
    zp_t* zp, poolref_t ref, uint8_t type_id, const void* data, uint16_t count
) {
    if (!zp || !zp->pool_buf || !zp->schema || zp->pool_cap == 0) {
        return false;
    }

    if (count == 0) count = 1;

    ZP_LOCK(zp, ZP_LOCK_POOL);

    if (!zp_pool_is_valid(zp, ref)) {
        ZP_UNLOCK(zp, ZP_LOCK_POOL);
        return false;
    }

    zp_pool_slot_t* slot = zp_pool_slot(zp, ref);
    if (slot->flags & ZP_POOL_FLAG_READONLY) {
        ZP_UNLOCK(zp, ZP_LOCK_POOL);
        zp_emit_log(zp, ZP_LOG_ERROR, ZP_ERR_POOL_READONLY, ref);
        return false;
    }
    if (type_id != slot->type_id) {
        ZP_UNLOCK(zp, ZP_LOCK_POOL);
        zp_emit_log(zp, ZP_LOG_ERROR, ZP_ERR_POOL_TYPE_MISMATCH, ref);
        return false;
    }

    if (count > slot->count) {
        ZP_UNLOCK(zp, ZP_LOCK_POOL);
        zp_emit_log(zp, ZP_LOG_ERROR, ZP_ERR_POOL_SLOT_EXCEEDED, count);
        return false;
    }

    bool shrunk = (count < slot->count);
    slot->type_id = type_id;
    slot->count = count;

    size_t data_len = get_type_size(zp->schema, type_id) * count;
    zp_memcpy(zp->pool_buf + ref + sizeof(zp_pool_slot_t), data, data_len);
    ZP_UNLOCK(zp, ZP_LOCK_POOL);

    if (shrunk) {
        zp_emit_log(zp, ZP_LOG_WARN, ZP_ERR_POOL_SLOT_SHRUNK, count);
    }

    return true;
}

void zp_pool_reset(zp_t* zp) {
    if (!zp) return;
    ZP_LOCK(zp, ZP_LOCK_POOL);
    zp->pool_len = 0;
    ZP_UNLOCK(zp, ZP_LOCK_POOL);
}

size_t zp_pool_used(const zp_t* zp) { return zp ? zp->pool_len : 0; }

size_t zp_pool_free(const zp_t* zp) {
    if (!zp || zp->pool_cap < zp->pool_len) return 0;
    return zp->pool_cap - zp->pool_len;
}
