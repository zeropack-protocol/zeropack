#include "zeropack.h"

zp_ctx_t* zp_init(zp_ctx_t* ctx) {
    if (!ctx) return NULL;
    if (!ctx->schema || !ctx->writev || !ctx->buf.data) return NULL;

    uintptr_t rx_addr = (uintptr_t)ctx->buf.data;
    size_t rx_pad = (-rx_addr) & 7;
    if (ctx->buf.cap < rx_pad + 8) return NULL;

    ctx->buf.data = (uint8_t*)ctx->buf.data + rx_pad + 4;
    ctx->buf.cap -= (rx_pad + 4);
    ctx->buf.head = 0;
    ctx->buf.tail = 0;

    if (ctx->pool.data) {
        uintptr_t pool_addr = (uintptr_t)ctx->pool.data;
        size_t pool_pad = (-pool_addr) & 7;
        if (ctx->pool.cap < pool_pad + 8) return NULL;
        size_t usable_pool = ctx->pool.cap - pool_pad - 2;
        if (usable_pool > 0xFFFE) return NULL;
        ctx->pool.data = (uint8_t*)ctx->pool.data + pool_pad + 2;
        ctx->pool.cap = usable_pool;
        ctx->pool.head = 0;
        ctx->pool.tail = 0;
    } else {
        ctx->pool.cap = 0;
        ctx->pool.head = 0;
        ctx->pool.tail = 0;
    }

    return ctx;
}
