#include "zeropack.h"

typedef struct {
    size_t elem_size;
    size_t prefix_len;
    size_t data_offset;
    size_t payload_len;
} zp_rx_meta_t;

static zp_status_t zp_parse_packet_meta(
    const zp_ctx_t* ctx, const zp_header_t* hdr, zp_rx_meta_t* out
) {
    *out = (zp_rx_meta_t){0};

    if (hdr->action >= ZP_SYS_ACTION_START) {
        size_t type_size = zp_get_type_size(ctx->schema, hdr->type_id);
        switch (hdr->action) {
            case ZP_SYS_ACTION_POOL_EDIT:
                if (type_size == 0) return ZP_POOL_TYPE_MISMATCH;
                if (hdr->count == 0) return ZP_INVALID_ACTION;
                out->prefix_len = sizeof(poolref_t);
                out->data_offset = sizeof(poolref_t);
                out->elem_size = type_size;
                break;

            case ZP_SYS_ACTION_POOL_STORE:
                if (type_size == 0 || hdr->count == 0) return ZP_INVALID_ACTION;
                out->elem_size = type_size;
                break;

            case ZP_SYS_ACTION_POOL_RESET:
                if (hdr->count != 0) return ZP_INVALID_ACTION;
                break;

            case ZP_SYS_ACTION_LOG:
                out->elem_size = 1;
                break;

            default:
                return ZP_UNKNOWN_INTERNAL_ACTION;
        }
    } else {
        if (hdr->action >= ctx->schema->action_count) {
            return ZP_INVALID_ACTION;
        }
        if (hdr->type_id != zp_get_action_type_id(ctx->schema, hdr->action)) {
            return ZP_INVALID_ACTION;
        }

        const zp_action_meta_t* act = &ctx->schema->actions[hdr->action];
        bool valid_count = !act->type ? (hdr->count == 0)
                                      : (act->is_array ? (hdr->count >= 1)
                                                       : (hdr->count == 1));
        if (!valid_count) return ZP_INVALID_ACTION;

        out->elem_size = act->type ? act->type->size : 0;
    }

    size_t max_payload = ctx->rx_cap - sizeof(zp_header_t);
    if (out->prefix_len > max_payload) return ZP_BUFFER_OVERFLOW;
    if (out->elem_size > 0 &&
        (size_t)hdr->count > (max_payload - out->prefix_len) / out->elem_size) {
        return ZP_BUFFER_OVERFLOW;
    }

    out->payload_len = out->prefix_len + out->elem_size * (size_t)hdr->count;
    return ZP_OK;
}

static void handle_internal(
    zp_ctx_t* ctx, const zp_header_t* hdr, const uint8_t* payload
) {
    switch (hdr->action) {
        case ZP_SYS_ACTION_POOL_EDIT: {
            const poolref_t ref = *(const poolref_t*)payload;
            const void* data = payload + sizeof(poolref_t);
            zp_status_t status =
                zp_pool_edit(ctx, ref, hdr->type_id, data, hdr->count);

            if (status != ZP_OK) {
                zp_emit_log(ctx, ZP_LOG_ERROR, status, ref);
            }
            break;
        }
        case ZP_SYS_ACTION_POOL_STORE: {
            poolref_t ref =
                zp_pool_push(ctx, hdr->type_id, payload, hdr->count);

            if (ref == 0xFFFF) {
                zp_emit_log(ctx, ZP_LOG_ERROR, ZP_POOL_FULL, ctx->pool_len);
                break;
            }

            zp_emit_log(ctx, ZP_LOG_OK, ZP_POOL_SLOT_STORED, ref);
            break;
        }
        case ZP_SYS_ACTION_POOL_RESET:
            zp_pool_reset(ctx);
            break;
        case ZP_SYS_ACTION_LOG:
            break;
        default:
            zp_emit_log(
                ctx, ZP_LOG_WARN, ZP_UNKNOWN_INTERNAL_ACTION, hdr->action
            );
            break;
    }
}

void zp_rx_commit(zp_ctx_t* ctx, size_t len) {
    if (!ctx || !(ctx->state & ZP_STATE_INITIALIZED)) return;

    if (len > ctx->rx_cap - ctx->rx_len) {
        zp_emit_log(
            ctx, ZP_LOG_ERROR, ZP_BUFFER_OVERFLOW, (uint16_t)(ctx->rx_len + len)
        );
        ctx->rx_len = 0;
        return;
    }

    ctx->rx_len += len;

    while (ctx->rx_len >= sizeof(zp_header_t)) {
        const zp_header_t* hdr = (const zp_header_t*)ctx->rx_buf;
        zp_rx_meta_t meta;
        zp_status_t status = zp_parse_packet_meta(ctx, hdr, &meta);

        if (status != ZP_OK) {
            zp_log_level_t level = (status == ZP_BUFFER_OVERFLOW ||
                                    status == ZP_POOL_TYPE_MISMATCH)
                                       ? ZP_LOG_ERROR
                                       : ZP_LOG_WARN;
            uint16_t extra =
                (status == ZP_BUFFER_OVERFLOW) ? hdr->count : hdr->action;
            zp_emit_log(ctx, level, status, extra);
            ctx->rx_len = 0;
            break;
        }

        size_t total_packet_len = sizeof(zp_header_t) + meta.payload_len;
        if (ctx->rx_len < total_packet_len) break;

        uint8_t* payload = ctx->rx_buf + sizeof(zp_header_t);

        // normalize bool values to 0 or 1
        if (hdr->type_id == TYPE_ID_bool) {
            uint8_t* data = payload + meta.data_offset;
            for (uint16_t i = 0; i < hdr->count; i++) {
                data[i] = (data[i] != 0);
            }
        }

        if (hdr->action >= ZP_SYS_ACTION_START) {
            handle_internal(ctx, hdr, payload);
        } else if (!ctx->dispatch(ctx, hdr, payload)) {
            zp_emit_log(ctx, ZP_LOG_WARN, ZP_NO_HOOK, hdr->action);
        }

        size_t remaining = ctx->rx_len - total_packet_len;
        if (remaining > 0) {
            zp_memmove(ctx->rx_buf, ctx->rx_buf + total_packet_len, remaining);
        }
        ctx->rx_len = remaining;
    }
}
