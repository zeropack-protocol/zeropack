#include "zeropack.h"

zp_ctx_t* zp_init(zp_ctx_t* ctx) {
    if (!ctx) return NULL;
    if (!ctx->schema || !ctx->writev || !ctx->buf) return NULL;

    uintptr_t rx_addr = (uintptr_t)ctx->buf;
    size_t rx_pad = (-rx_addr) & 7;
    if (ctx->cap < rx_pad + 8) return NULL;

    ctx->buf = (uint8_t*)ctx->buf + rx_pad + 4;
    ctx->cap -= (rx_pad + 4);
    ctx->head = 0;
    ctx->tail = 0;
    ctx->pool_len = 0;

    if (ctx->pool) {
        uintptr_t pool_addr = (uintptr_t)ctx->pool;
        size_t pool_pad = (-pool_addr) & 7;
        if (ctx->pool_cap < pool_pad + 8) return NULL;
        size_t usable_pool = ctx->pool_cap - pool_pad - 2;
        if (usable_pool > 0xFFFE) return NULL;
        ctx->pool = (uint8_t*)ctx->pool + pool_pad + 2;
        ctx->pool_cap = (uint16_t)usable_pool;
    } else {
        ctx->pool_cap = 0;
    }

    return ctx;
}
