#include "zeropack.h"

zp_ssize_t zp_send_raw(
    zp_ctx_t* ctx,
    uint8_t action,
    uint8_t type_id,
    uint16_t count,
    const void* payload,
    size_t size
) {
    if (!ctx || !(ctx->state & ZP_STATE_INITIALIZED)) return -1;

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
    zp_ssize_t ret = ctx->writev(ctx, iov, iovcnt);
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
    uint8_t type_id = TYPE_ID_NONE;
    if (ctx && ctx->schema && action < ctx->schema->action_count) {
        type_id =
            zp_get_meta_id(ctx->schema, ctx->schema->actions[action].type);
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
