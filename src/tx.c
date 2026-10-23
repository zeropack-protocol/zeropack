#include "zeropack.h"

zp_ssize_t zp_send_raw(
    zp_ctx_t* ctx,
    uint8_t action,
    uint8_t type_id,
    uint16_t count,
    const void* payload,
    size_t size
) {
    if (!ctx || !ctx->writev) return -1;
    if (size > 0 && !payload) return -1;

    zp_header_t hdr = {
        .action = action,
        .type_id = type_id,
        .count = count,
    };

    zp_iovec_t iov[2] = {
        {.buf = &hdr, .len = sizeof(zp_header_t)},
        {.buf = payload, .len = size},
    };

    int iovcnt = (payload && size > 0) ? 2 : 1;
    return ctx->writev(ctx, iov, iovcnt);
}

zp_ssize_t zp_send(
    zp_ctx_t* ctx, uint8_t action, const void* payload, uint16_t count
) {
    if (!ctx || !ctx->schema || action >= ctx->schema->action_count) {
        return -1;
    }

    const zp_action_meta_t* act = &ctx->schema->actions[action];
    uint8_t type_id = TYPE_ID_NONE;
    size_t size = 0;

    if (act->type) {
        if (!payload) return -1;

        type_id = zp_get_id(ctx->schema, act->type);
        if (type_id == TYPE_ID_NONE) return -1;

        if (!act->is_array) {
            count = 1;
        } else if (count == 0) {
            return -1;
        }

        size = (size_t)count * act->type->size;
    } else {
        count = 0;
        payload = NULL;
    }

    return zp_send_raw(ctx, action, type_id, count, payload, size);
}

void zp_emit_log(
    zp_ctx_t* ctx, zp_log_level_t level, zp_status_t code, uint16_t extra
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
