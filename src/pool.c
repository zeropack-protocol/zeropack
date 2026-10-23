#include "zeropack.h"

bool zp_pool_is_valid(const zp_ctx_t* ctx, poolref_t ref) {
    if (!ctx || !ctx->pool.data || ctx->pool.head > ctx->pool.cap) return false;
    if ((ref & 7) != 0) return false;

    size_t offset = 0;
    while (offset + sizeof(zp_pool_slot_t) <= ctx->pool.head) {
        zp_pool_slot_t* slot = zp_pool_slot(ctx, offset);
        if (slot->cap == 0) return false;

        size_t total_slot = sizeof(zp_pool_slot_t) + (size_t)slot->cap;
        if ((total_slot & 7) != 0 || total_slot > ctx->pool.head - offset) {
            return false;
        }

        if (offset == ref) return true;
        if (offset > ref) return false;

        offset += total_slot;
    }
    return false;
}

void* zp_pool_get_checked(
    const zp_ctx_t* ctx, uint8_t type_id, poolref_t ref, uint16_t* out_count
) {
    if (!zp_pool_is_valid(ctx, ref)) return NULL;

    const zp_pool_slot_t* slot = zp_pool_slot(ctx, ref);
    if (slot->type_id != type_id) return NULL;

    if (out_count) *out_count = slot->count;
    return (void*)(slot + 1);
}

poolref_t zp_pool_push_raw(
    zp_ctx_t* ctx, uint8_t type_id, const void* data, uint16_t count
) {
    if (!ctx || !ctx->pool.data || ctx->pool.cap == 0 || !data || count == 0) {
        return 0xFFFF;
    }

    const zp_type_meta_t* meta = zp_get_meta(ctx->schema, type_id);
    if (!meta || meta->size == 0) return 0xFFFF;
    size_t type_size = meta->size;

    if (ctx->pool.cap <= sizeof(zp_pool_slot_t) ||
        (size_t)count > (ctx->pool.cap - sizeof(zp_pool_slot_t)) / type_size) {
        return 0xFFFF;
    }

    size_t total_len = type_size * (size_t)count;
    size_t total_slot = ZP_SLOT_TOTAL_SIZE(total_len);

    if (total_slot > zp_pool_avail(ctx)) {
        return 0xFFFF;
    }

    zp_pool_slot_t* slot = (zp_pool_slot_t*)(ctx->pool.data + ctx->pool.head);
    slot->type_id = type_id;
    slot->flags = ZP_POOL_FLAG_NONE;
    slot->count = count;
    slot->cap = (uint16_t)(total_slot - sizeof(zp_pool_slot_t));

    poolref_t ref = (poolref_t)ctx->pool.head;
    zp_memcpy(slot + 1, data, total_len);

    ctx->pool.head += total_slot;

    return ref;
}

zp_status_t zp_pool_edit(
    zp_ctx_t* ctx,
    poolref_t ref,
    uint8_t type_id,
    const void* data,
    uint16_t count
) {
    if (!ctx || !ctx->pool.data || ctx->pool.cap == 0 || !data || count == 0) {
        return ZP_INVALID_ACTION;
    }

    if (!zp_pool_is_valid(ctx, ref)) {
        return ZP_INVALID_ACTION;
    }

    zp_pool_slot_t* slot = zp_pool_slot(ctx, ref);
    if (slot->flags & ZP_POOL_FLAG_READONLY) {
        return ZP_POOL_READONLY;
    }
    if (type_id != slot->type_id) {
        return ZP_POOL_TYPE_MISMATCH;
    }

    const zp_type_meta_t* meta = zp_get_meta(ctx->schema, type_id);
    if (!meta || meta->size == 0 ||
        (size_t)count > (size_t)slot->cap / meta->size) {
        return ZP_POOL_SLOT_EXCEEDED;
    }
    size_t type_size = meta->size;

    size_t data_len = type_size * (size_t)count;
    slot->count = count;

    zp_memcpy(slot + 1, data, data_len);

    return ZP_OK;
}

void zp_pool_reset(zp_ctx_t* ctx) {
    if (!ctx) return;
    ctx->pool.head = 0;
    ctx->pool.tail = 0;
}
