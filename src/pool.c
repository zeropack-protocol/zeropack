#include "zeropack/pool.h"

#include <stdint.h>

#include "zeropack/config.h"
#include "zeropack/core.h"
#include "zeropack/types.h"

bool zp_pool_is_valid(const zp_ctx_t* ctx, poolref_t ref) {
    return ctx && (ref + sizeof(zp_pool_slot_t)) <= zp_pool_used(ctx);
}

void* zp_pool_get_safe(zp_ctx_t* ctx, poolref_t ref) {
    return zp_pool_is_valid(ctx, ref) ? zp_pool_get(ctx, ref) : NULL;
}

poolref_t zp_pool_push(
    zp_ctx_t* ctx, uint8_t type_id, const void* data, uint16_t count
) {
    if (!ctx || !ctx->pool_buf || !ctx->schema) return 0xFFFF;

    size_t type_size = get_type_size(ctx->schema, type_id);
    if (type_size == 0) return 0xFFFF;
    if (count == 0) count = 1;

    size_t total_len = type_size * count;
    size_t total_slot = ZP_SLOT_TOTAL_SIZE(total_len);

    if (ctx->pool_cap == 0) return 0xFFFF;

    ZP_LOCK(ctx, ZP_LOCK_POOL);

    if (ctx->pool_len + total_slot > ctx->pool_cap) {
        ZP_UNLOCK(ctx, ZP_LOCK_POOL);
        zp_emit_log(ctx, ZP_LOG_ERROR, ZP_ERR_POOL_FULL, (uint16_t)total_slot);
        return 0xFFFF;
    }

    zp_pool_slot_t* slot = (zp_pool_slot_t*)(ctx->pool_buf + ctx->pool_len);
    slot->type_id = type_id;
    slot->flags = ZP_POOL_FLAG_NONE;
    slot->count = count;

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
    if (!ctx || !ctx->pool_buf || !ctx->schema || ctx->pool_cap == 0) {
        return false;
    }

    if (count == 0) count = 1;

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

    if (count > slot->count) {
        ZP_UNLOCK(ctx, ZP_LOCK_POOL);
        zp_emit_log(ctx, ZP_LOG_ERROR, ZP_ERR_POOL_SLOT_EXCEEDED, count);
        return false;
    }

    bool shrunk = (count < slot->count);
    slot->type_id = type_id;
    slot->count = count;

    size_t data_len = get_type_size(ctx->schema, type_id) * count;
    zp_memcpy(ctx->pool_buf + ref + sizeof(zp_pool_slot_t), data, data_len);
    ZP_UNLOCK(ctx, ZP_LOCK_POOL);

    if (shrunk) {
        zp_emit_log(ctx, ZP_LOG_WARN, ZP_ERR_POOL_SLOT_SHRUNK, count);
    }

    return true;
}

void zp_pool_reset(zp_ctx_t* ctx) {
    if (!ctx) return;
    ZP_LOCK(ctx, ZP_LOCK_POOL);
    ctx->pool_len = 0;
    ZP_UNLOCK(ctx, ZP_LOCK_POOL);
}

size_t zp_pool_used(const zp_ctx_t* ctx) { return ctx ? ctx->pool_len : 0; }

size_t zp_pool_free(const zp_ctx_t* ctx) {
    if (!ctx || ctx->pool_cap < ctx->pool_len) return 0;
    return ctx->pool_cap - ctx->pool_len;
}
