#include "zeropack/core.h"

#include <stddef.h>
#include <stdint.h>

#include "zeropack/config.h"
#include "zeropack/pool.h"
#include "zeropack/schema.h"
#include "zeropack/types.h"

void zp_emit_log(
    zp_ctx_t* ctx, zp_log_level_t level, zp_log_code_t code, uint16_t extra
) {
    zp_log_msg_t msg = {
        .level = (uint8_t)level,
        .code = (uint8_t)code,
        .extra = extra,
    };
    zp_send_raw(
        ctx, ZP_SYS_ACTION_LOG, TYPE_ID_u8, sizeof(msg), &msg, sizeof(msg)
    );
}

static void zp_send_pair(
    zp_ctx_t* ctx,
    const void* hdr,
    size_t hdr_len,
    const void* data,
    size_t data_len
) {
    zp_iovec_t iov[2] = {
        {.buf = hdr, .len = hdr_len},
        {.buf = data, .len = data_len},
    };
    ctx->writev(iov, (data && data_len > 0) ? 2 : 1);
}

void zp_send_handshake(zp_ctx_t* ctx) {
    if (!ctx || !ctx->schema || !ctx->writev) return;

    uint16_t pool_item_count = 0;
    size_t offset = 0;
    size_t pool_used = zp_pool_used(ctx);
    while (offset + sizeof(zp_pool_slot_t) <= pool_used) {
        zp_pool_slot_t* slot = zp_pool_slot(ctx, offset);
        size_t total_slot = sizeof(zp_pool_slot_t) + (size_t)slot->cap;
        if (slot->cap == 0 || total_slot > pool_used - offset) break;
        pool_item_count++;
        offset += total_slot;
    }

    zp_handshake_t hs = {
        .magic = 'Z',
        .byteord = (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__),
        .reserved = 0x28,  // 0b101000
        .pool_size = (uint16_t)ctx->pool_cap,
        .type_count = ctx->schema->type_count,
        .action_count = ctx->schema->action_count,
        .pool_item_count = pool_item_count,
    };

    ZP_LOCK(ctx, ZP_LOCK_TX);
    zp_send_pair(ctx, &hs, sizeof(hs), NULL, 0);

    for (uint8_t i = 0; i < ctx->schema->type_count; i++) {
        const zp_type_meta_t* t = ctx->schema->types[i];
        struct __attribute__((packed)) {
            uint8_t type_id;
            uint8_t count;
            uint16_t size;
        } t_hdr = {(uint8_t)(TYPE_ID_START + i), t->count, t->size};

        zp_send_pair(
            ctx, &t_hdr, sizeof(t_hdr), t->name, zp_strlen(t->name) + 1
        );

        for (uint8_t j = 0; j < t->count; j++) {
            const zp_field_meta_t* f = &t->fields[j];
            struct __attribute__((packed)) {
                uint8_t type;
                uint16_t offset;
                uint16_t length;
            } f_hdr = {
                zp_get_meta_id(ctx->schema, f->type), f->offset, f->length
            };

            zp_send_pair(
                ctx, &f_hdr, sizeof(f_hdr), f->name, zp_strlen(f->name) + 1
            );
        }
    }

    for (uint8_t i = 0; i < ctx->schema->action_count; i++) {
        const zp_action_meta_t* a = &ctx->schema->actions[i];
        uint8_t a_hdr[2] = {
            zp_get_action_type_id(ctx->schema, i), a->is_array
        };

        zp_send_pair(
            ctx, a_hdr, sizeof(a_hdr), a->name, zp_strlen(a->name) + 1
        );
    }

    offset = 0;
    while (offset + sizeof(zp_pool_slot_t) <= pool_used) {
        zp_pool_slot_t* slot = zp_pool_slot(ctx, offset);
        size_t total_slot = sizeof(zp_pool_slot_t) + (size_t)slot->cap;
        if (slot->cap == 0 || total_slot > pool_used - offset) break;

        size_t type_size = zp_get_type_size(ctx->schema, slot->type_id);
        size_t data_len = type_size * (size_t)slot->count;
        struct __attribute__((packed)) {
            poolref_t ref;
            uint8_t type_id;
            uint16_t count;
        } p_hdr = {(poolref_t)offset, slot->type_id, slot->count};

        zp_send_pair(
            ctx, &p_hdr, sizeof(p_hdr), zp_pool_get(ctx, offset), data_len
        );

        offset += total_slot;
    }
    ZP_UNLOCK(ctx, ZP_LOCK_TX);
}

static void handle_internal(
    zp_ctx_t* ctx, const zp_header_t* hdr, const uint8_t* payload
) {
    switch (hdr->action) {
        case ZP_SYS_ACTION_POOL_EDIT: {
            const poolref_t ref = *(const poolref_t*)payload;
            const void* data = payload + sizeof(poolref_t);
            zp_pool_edit(ctx, ref, hdr->type_id, data, hdr->count);
            break;
        }
        case ZP_SYS_ACTION_POOL_STORE: {
            poolref_t ref =
                zp_pool_push(ctx, hdr->type_id, payload, hdr->count);

            zp_send_raw(
                ctx,
                ZP_SYS_ACTION_POOL_STORE,
                TYPE_ID_poolref_t,
                1,
                &ref,
                sizeof(poolref_t)
            );
            break;
        }
        case ZP_SYS_ACTION_POOL_RESET:
            zp_pool_reset(ctx);
            zp_send_raw(
                ctx, ZP_SYS_ACTION_POOL_RESET, TYPE_ID_NONE, 0, NULL, 0
            );
            break;
        default:
            zp_emit_log(
                ctx, ZP_LOG_WARN, ZP_ERR_UNKNOWN_INTERNAL_ACTION, hdr->action
            );
            break;
    }
}

uint8_t* zp_rx_get_buffer(zp_ctx_t* ctx, size_t* out_available_len) {
    if (!ctx || !out_available_len) return NULL;
    if (ctx->rx_len >= ctx->rx_cap) {
        *out_available_len = 0;
        return NULL;
    }

    *out_available_len = ctx->rx_cap - ctx->rx_len;
    return ctx->rx_buf + ctx->rx_len;
}

void zp_rx_commit(zp_ctx_t* ctx, size_t len) {
    if (!ctx || !ctx->rx_buf || !ctx->schema) return;

    if (len > ctx->rx_cap - ctx->rx_len) {
        zp_emit_log(
            ctx,
            ZP_LOG_ERROR,
            ZP_ERR_BUFFER_OVERFLOW,
            (uint16_t)(ctx->rx_len + len)
        );
        ctx->rx_len = 0;
        return;
    }

    ctx->rx_len += len;

    while (ctx->rx_len >= sizeof(zp_header_t)) {
        const zp_header_t* hdr = (const zp_header_t*)ctx->rx_buf;
        size_t payload_len = 0;

        if (hdr->action == ZP_SYS_ACTION_POOL_EDIT) {
            size_t type_size = zp_get_type_size(ctx->schema, hdr->type_id);
            if (type_size == 0 || hdr->count == 0 ||
                ctx->rx_cap < sizeof(zp_header_t) + sizeof(poolref_t) ||
                (size_t)hdr->count >
                    (ctx->rx_cap - sizeof(zp_header_t) - sizeof(poolref_t)) /
                        type_size) {
                zp_emit_log(
                    ctx,
                    ZP_LOG_ERROR,
                    type_size == 0 ? ZP_ERR_POOL_TYPE_MISMATCH
                                   : ZP_ERR_BUFFER_OVERFLOW,
                    hdr->action
                );
                ctx->rx_len = 0;
                break;
            }
            payload_len = sizeof(poolref_t) + type_size * (size_t)hdr->count;
        } else if (hdr->action >= ZP_SYS_ACTION_START) {
            size_t type_size = zp_get_type_size(ctx->schema, hdr->type_id);
            if ((hdr->action == ZP_SYS_ACTION_POOL_STORE &&
                 (type_size == 0 || hdr->count == 0)) ||
                (hdr->count > 0 && type_size == 0)) {
                zp_emit_log(
                    ctx, ZP_LOG_WARN, ZP_ERR_INVALID_ACTION, hdr->action
                );
                ctx->rx_len = 0;
                break;
            }
            if (type_size > 0 &&
                (size_t)hdr->count >
                    (ctx->rx_cap - sizeof(zp_header_t)) / type_size) {
                zp_emit_log(
                    ctx, ZP_LOG_ERROR, ZP_ERR_BUFFER_OVERFLOW, hdr->count
                );
                ctx->rx_len = 0;
                break;
            }
            payload_len = type_size * (size_t)hdr->count;
        } else {
            if (hdr->action >= ctx->schema->action_count) {
                zp_emit_log(
                    ctx, ZP_LOG_WARN, ZP_ERR_INVALID_ACTION, hdr->action
                );
                ctx->rx_len = 0;
                break;
            }

            const zp_action_meta_t* act = &ctx->schema->actions[hdr->action];
            uint8_t expected_type =
                zp_get_action_type_id(ctx->schema, hdr->action);
            bool valid_count =
                !act->type
                    ? (hdr->count == 0)
                    : (act->is_array ? (hdr->count >= 1) : (hdr->count == 1));

            if (hdr->type_id != expected_type || !valid_count) {
                zp_emit_log(
                    ctx, ZP_LOG_WARN, ZP_ERR_INVALID_ACTION, hdr->action
                );
                ctx->rx_len = 0;
                break;
            }

            size_t elem_size = act->type ? act->type->size : 0;
            if (elem_size > 0 &&
                (size_t)hdr->count >
                    (ctx->rx_cap - sizeof(zp_header_t)) / elem_size) {
                zp_emit_log(
                    ctx, ZP_LOG_ERROR, ZP_ERR_BUFFER_OVERFLOW, hdr->count
                );
                ctx->rx_len = 0;
                break;
            }

            payload_len = elem_size * (size_t)hdr->count;
        }

        size_t total_packet_len = sizeof(zp_header_t) + payload_len;

        if (ctx->rx_len < total_packet_len) break;

        const uint8_t* payload = ctx->rx_buf + sizeof(zp_header_t);

        if (hdr->action >= ZP_SYS_ACTION_START) {
            handle_internal(ctx, hdr, payload);
        } else if (!ctx->dispatch(ctx, hdr, payload)) {
            zp_emit_log(ctx, ZP_LOG_WARN, ZP_ERR_NO_HOOK, hdr->action);
        }

        size_t remaining = ctx->rx_len - total_packet_len;
        if (remaining > 0) {
            zp_memmove(ctx->rx_buf, ctx->rx_buf + total_packet_len, remaining);
        }
        ctx->rx_len = remaining;
    }
}

int zp_init(zp_ctx_t* ctx, const zp_config_t* cfg) {
    if (!ctx || !cfg) return -1;
    if (!cfg->schema || !cfg->writev || !cfg->rx_buf) return -1;

    uintptr_t rx_addr = (uintptr_t)cfg->rx_buf;
    size_t rx_pad = (8 - (rx_addr & 7)) & 7;
    if (cfg->rx_cap < rx_pad + 8) return -1;

    *ctx = (zp_ctx_t){0};

    ctx->schema = cfg->schema;

    ctx->lock = cfg->lock;
    ctx->unlock = cfg->unlock;

    ctx->writev = cfg->writev;
    ctx->dispatch = cfg->dispatch ? cfg->dispatch : cfg->schema->dispatch;
    ctx->userdata = cfg->userdata;

    ctx->rx_buf = (uint8_t*)cfg->rx_buf + rx_pad + 4;
    ctx->rx_cap = cfg->rx_cap - rx_pad - 4;
    ctx->rx_len = 0;

    if (cfg->pool_buf) {
        uintptr_t pool_addr = (uintptr_t)cfg->pool_buf;
        size_t pool_pad = (8 - (pool_addr & 7)) & 7;
        if (cfg->pool_cap < pool_pad + 8) return -1;
        size_t usable_pool = cfg->pool_cap - pool_pad - 2;
        if (usable_pool > 0xFFFE) return -1;
        ctx->pool_buf = (uint8_t*)cfg->pool_buf + pool_pad + 2;
        ctx->pool_cap = usable_pool;
        ctx->pool_len = 0;
    }

    return 0;
}

zp_ssize_t zp_send_raw(
    zp_ctx_t* ctx,
    uint8_t action,
    uint8_t type_id,
    uint16_t count,
    const void* payload,
    size_t size
) {
    if (!ctx || !ctx->writev) return -1;

    zp_header_t hdr = {
        .action = action,
        .type_id = type_id,
        .count = count,
    };

    zp_iovec_t iov[2] = {
        {.buf = &hdr, .len = sizeof(zp_header_t)},
        {.buf = payload, .len = size},
    };

    int iovcnt = (payload != NULL && size > 0) ? 2 : 1;

    ZP_LOCK(ctx, ZP_LOCK_TX);
    zp_ssize_t ret = ctx->writev(iov, iovcnt);
    ZP_UNLOCK(ctx, ZP_LOCK_TX);
    return ret;
}

zp_ssize_t zp_send(
    zp_ctx_t* ctx,
    uint8_t action,
    uint16_t count,
    const void* payload,
    size_t size
) {
    uint8_t type_id = zp_get_action_type_id(ctx ? ctx->schema : NULL, action);
    return zp_send_raw(ctx, action, type_id, count, payload, size);
}
