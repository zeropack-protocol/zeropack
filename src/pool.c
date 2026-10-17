#include "zeropack/pool.h"

#include <stdint.h>

#include "zeropack/config.h"
#include "zeropack/core.h"
#include "zeropack/types.h"

bool zp_pool_is_valid(const zp_ctx_t* ctx, poolref_t ref) {
    if (!ctx || !ctx->pool_buf || !ctx->schema) return false;
    if ((ref & 7) != 0) return false;

    size_t offset = 0;
    while (offset + sizeof(zp_pool_slot_t) <= ctx->pool_len) {
        zp_pool_slot_t* slot = zp_pool_slot(ctx, offset);
        if (slot->cap == 0) return false;

        size_t total_slot = sizeof(zp_pool_slot_t) + (size_t)slot->cap;
        if ((total_slot & 7) != 0 || total_slot > ctx->pool_len - offset) {
            return false;
        }

        if (offset == ref) return true;
        if (offset > ref) return false;

        offset += total_slot;
    }
    return false;
}

void* zp_pool_get_safe(zp_ctx_t* ctx, poolref_t ref) {
    return zp_pool_is_valid(ctx, ref) ? zp_pool_get(ctx, ref) : NULL;
}

poolref_t zp_pool_push(
    zp_ctx_t* ctx, uint8_t type_id, const void* data, uint16_t count
) {
    if (!ctx || !ctx->pool_buf || !ctx->schema || !data || count == 0 ||
        ctx->pool_cap == 0) {
        return 0xFFFF;
    }

    size_t type_size = zp_get_type_size(ctx->schema, type_id);
    if (type_size == 0) return 0xFFFF;

    if (ctx->pool_cap <= sizeof(zp_pool_slot_t) ||
        (size_t)count > (ctx->pool_cap - sizeof(zp_pool_slot_t)) / type_size) {
        zp_emit_log(ctx, ZP_LOG_ERROR, ZP_ERR_POOL_FULL, count);
        return 0xFFFF;
    }

    size_t total_len = type_size * (size_t)count;
    size_t total_slot = ZP_SLOT_TOTAL_SIZE(total_len);

    ZP_LOCK(ctx, ZP_LOCK_POOL);

    if (total_slot > zp_pool_avail(ctx)) {
        ZP_UNLOCK(ctx, ZP_LOCK_POOL);
        zp_emit_log(ctx, ZP_LOG_ERROR, ZP_ERR_POOL_FULL, (uint16_t)total_slot);
        return 0xFFFF;
    }

    zp_pool_slot_t* slot = (zp_pool_slot_t*)(ctx->pool_buf + ctx->pool_len);
    slot->type_id = type_id;
    slot->flags = ZP_POOL_FLAG_NONE;
    slot->count = count;
    slot->cap = (uint16_t)(total_slot - sizeof(zp_pool_slot_t));

    poolref_t ref = (poolref_t)ctx->pool_len;
    zp_memcpy(
        ctx->pool_buf + ctx->pool_len + sizeof(zp_pool_slot_t), data, total_len
    );

    ctx->pool_len += total_slot;
    ZP_UNLOCK(ctx, ZP_LOCK_POOL);

    return ref;
}

bool zp_pool_edit(
    zp_ctx_t* ctx, poolref_t ref, uint8_t type_id, const void* data, uint16_t count
) {
    if (!ctx || !ctx->pool_buf || !ctx->schema || !data || count == 0 ||
        ctx->pool_cap == 0) {
        return false;
    }

    ZP_LOCK(ctx, ZP_LOCK_POOL);

    if (!zp_pool_is_valid(ctx, ref)) {
        ZP_UNLOCK(ctx, ZP_LOCK_POOL);
        return false;
    }

    zp_pool_slot_t* slot = zp_pool_slot(ctx, ref);
    if (slot->flags & ZP_POOL_FLAG_READONLY) {
        ZP_UNLOCK(ctx, ZP_LOCK_POOL);
        zp_emit_log(ctx, ZP_LOG_ERROR, ZP_ERR_POOL_READONLY, ref);
        return false;
    }
    if (type_id != slot->type_id) {
        ZP_UNLOCK(ctx, ZP_LOCK_POOL);
        zp_emit_log(ctx, ZP_LOG_ERROR, ZP_ERR_POOL_TYPE_MISMATCH, ref);
        return false;
    }

    size_t type_size = zp_get_type_size(ctx->schema, type_id);
    if (type_size == 0 || (size_t)count > (size_t)slot->cap / type_size) {
        ZP_UNLOCK(ctx, ZP_LOCK_POOL);
        zp_emit_log(ctx, ZP_LOG_ERROR, ZP_ERR_POOL_SLOT_EXCEEDED, count);
        return false;
    }

    size_t data_len = type_size * (size_t)count;
    slot->count = count;

    zp_memcpy(ctx->pool_buf + ref + sizeof(zp_pool_slot_t), data, data_len);
    ZP_UNLOCK(ctx, ZP_LOCK_POOL);

    return true;
}

void zp_pool_reset(zp_ctx_t* ctx) {
    if (!ctx) return;
    ZP_LOCK(ctx, ZP_LOCK_POOL);
    ctx->pool_len = 0;
    ZP_UNLOCK(ctx, ZP_LOCK_POOL);
}
