#include "zeropack.h"

int zp_init(zp_ctx_t* ctx) {
    if (!ctx) return -1;
    if (!ctx->schema || !ctx->writev || !ctx->rx_buf) return -1;

    uintptr_t rx_addr = (uintptr_t)ctx->rx_buf;
    size_t rx_pad = (8 - (rx_addr & 7)) & 7;
    if (ctx->rx_cap < rx_pad + 8) return -1;

    if (!ctx->dispatch) {
        ctx->dispatch = ctx->schema->dispatch;
    }

    ctx->rx_buf = (uint8_t*)ctx->rx_buf + rx_pad + 4;
    ctx->rx_cap -= (rx_pad + 4);
    ctx->rx_len = 0;

    if (ctx->pool_buf) {
        uintptr_t pool_addr = (uintptr_t)ctx->pool_buf;
        size_t pool_pad = (8 - (pool_addr & 7)) & 7;
        if (ctx->pool_cap < pool_pad + 8) return -1;
        size_t usable_pool = ctx->pool_cap - pool_pad - 2;
        if (usable_pool > 0xFFFE) return -1;
        ctx->pool_buf = (uint8_t*)ctx->pool_buf + pool_pad + 2;
        ctx->pool_cap = (uint16_t)usable_pool;
        ctx->pool_len = 0;
    } else {
        ctx->pool_cap = 0;
        ctx->pool_len = 0;
    }

    ctx->state = ZP_STATE_INITIALIZED;
    return 0;
}
