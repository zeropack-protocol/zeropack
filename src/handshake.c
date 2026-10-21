#include "zeropack.h"

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
    ctx->writev(ctx, iov, (data && data_len > 0) ? 2 : 1);
}

void zp_send_handshake(zp_ctx_t* ctx) {
    if (!ctx || !ctx->schema || !ctx->writev) return;

    uint16_t pool_item_count = 0;
    size_t offset = 0;
    while (offset + sizeof(zp_pool_slot_t) <= ctx->pool_len) {
        zp_pool_slot_t* slot = zp_pool_slot(ctx, offset);
        size_t total_slot = sizeof(zp_pool_slot_t) + (size_t)slot->cap;
        if (slot->cap == 0 || total_slot > ctx->pool_len - offset) break;
        pool_item_count++;
        offset += total_slot;
    }

    zp_handshake_t hs = {
        .magic = 'Z',
        .byteord = (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__),
        .reserved = 0x28,  // 0b101000
        .pool_size = ctx->pool_cap,
        .type_count = ctx->schema->type_count,
        .action_count = ctx->schema->action_count,
        .pool_item_count = pool_item_count,
    };

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
        uint8_t a_hdr[2] = {zp_get_meta_id(ctx->schema, a->type), a->is_array};

        zp_send_pair(
            ctx, a_hdr, sizeof(a_hdr), a->name, zp_strlen(a->name) + 1
        );
    }

    offset = 0;
    while (offset + sizeof(zp_pool_slot_t) <= ctx->pool_len) {
        zp_pool_slot_t* slot = zp_pool_slot(ctx, offset);
        size_t total_slot = sizeof(zp_pool_slot_t) + (size_t)slot->cap;
        if (slot->cap == 0 || total_slot > ctx->pool_len - offset) break;

        size_t type_size = zp_get_type_size(ctx->schema, slot->type_id);
        size_t data_len = type_size * (size_t)slot->count;

        struct __attribute__((packed)) {
            poolref_t ref;
            uint8_t type_id;
            uint16_t count;
        } p_hdr = {(poolref_t)offset, slot->type_id, slot->count};

        zp_send_pair(
            ctx, &p_hdr, sizeof(p_hdr), _zp_pool_get(ctx, offset), data_len
        );

        offset += total_slot;
    }
}
