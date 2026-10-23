#include "zeropack.h"

zp_ctx_t* zp_init(zp_ctx_t* ctx) {
    if (!ctx || !ctx->schema || !ctx->writev || !ctx->buf.data) return NULL;

    uintptr_t rx_addr = (uintptr_t)ctx->buf.data;
    size_t rx_shift = (12 - (rx_addr & 7)) & 7;
    if (ctx->buf.cap < rx_shift + sizeof(zp_header_t)) return NULL;

    ctx->buf.data = (uint8_t*)ctx->buf.data + rx_shift;
    ctx->buf.cap -= rx_shift;
    ctx->buf.head = 0;
    ctx->buf.tail = 0;

    if (ctx->pool.data) {
        uintptr_t pool_addr = (uintptr_t)ctx->pool.data;
        size_t pool_shift = (10 - (pool_addr & 7)) & 7;
        if (ctx->pool.cap < pool_shift + sizeof(zp_pool_slot_t)) return NULL;

        size_t usable_pool = ctx->pool.cap - pool_shift;
        if (usable_pool > 0xFFFE) return NULL;

        ctx->pool.data = (uint8_t*)ctx->pool.data + pool_shift;
        ctx->pool.cap = usable_pool;
    } else {
        ctx->pool.cap = 0;
    }

    ctx->pool.head = 0;
    ctx->pool.tail = 0;
    return ctx;
}
