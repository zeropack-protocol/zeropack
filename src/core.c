#include "zeropack.h"

zp_ctx_t* zp_init(zp_ctx_t* ctx) {
    if (!ctx) return NULL;
    if (!ctx->schema || !ctx->writev || !ctx->rx_buf) return NULL;

    uintptr_t rx_addr = (uintptr_t)ctx->rx_buf;
    size_t rx_pad = (-rx_addr) & 7;
    if (ctx->rx_cap < rx_pad + 8) return NULL;

    ctx->rx_buf = (uint8_t*)ctx->rx_buf + rx_pad + 4;
    ctx->rx_cap -= (rx_pad + 4);
    ctx->rx_head = 0;
    ctx->rx_tail = 0;
    ctx->pool_len = 0;

    if (ctx->pool_buf) {
        uintptr_t pool_addr = (uintptr_t)ctx->pool_buf;
        size_t pool_pad = (-pool_addr) & 7;
        if (ctx->pool_cap < pool_pad + 8) return NULL;
        size_t usable_pool = ctx->pool_cap - pool_pad - 2;
        if (usable_pool > 0xFFFE) return NULL;
        ctx->pool_buf = (uint8_t*)ctx->pool_buf + pool_pad + 2;
        ctx->pool_cap = (uint16_t)usable_pool;
    } else {
        ctx->pool_cap = 0;
    }

    return ctx;
}
