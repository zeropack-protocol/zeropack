#include "zeropack/core.h"

#include <stddef.h>
#include <stdint.h>

#include "zeropack/config.h"
#include "zeropack/pool.h"
#include "zeropack/schema.h"
#include "zeropack/types.h"

void zp_emit_log(
    zp_t* zp, zp_log_level_t level, zp_log_code_t code, uint16_t extra
) {
    zp_log_msg_t msg = {
        .level = (uint8_t)level,
        .code = (uint8_t)code,
        .extra = extra,
    };
    zp_send_raw(
        zp, ZP_SYS_ACTION_LOG, TYPE_ID_u8, sizeof(msg), &msg, sizeof(msg)
    );
}

static inline zp_ssize_t zp_send_buf(zp_t* zp, const void* buf, size_t len) {
    if (!zp || !zp->writev) return -1;
    zp_iovec_t iov = {.buf = buf, .len = len};
    return zp->writev(&iov, 1);
}

void zp_send_handshake(zp_t* zp) {
    if (!zp || !zp->schema) return;

    uint16_t pool_item_count = 0;
    uint16_t offset = 0;
    size_t pool_used = zp_pool_used(zp);
    while (offset < pool_used) {
        zp_pool_slot_t* slot = zp_pool_slot(zp, offset);
        size_t type_size = get_type_size(zp->schema, slot->type_id);
        size_t data_len = type_size * slot->count;
        size_t total_slot = ZP_SLOT_TOTAL_SIZE(data_len);
        pool_item_count++;
        offset += total_slot;
    }

    zp_handshake_t hs = {0};
    hs.magic = 'Z';
    hs.byteord = (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__);
    hs.reserved = 0x28;  // 0b101000
    hs.pool_size = (uint16_t)zp->pool_cap;
    hs.type_count = zp->schema->type_count;
    hs.action_count = zp->schema->action_count;
    hs.pool_item_count = pool_item_count;

    ZP_LOCK(zp, ZP_LOCK_TX);
    zp_send_buf(zp, &hs, sizeof(hs));

    for (int i = 0; i < zp->schema->type_count; i++) {
        const struct_desc_t* t = zp->schema->types[i];

        uint8_t type_id = (uint8_t)(TYPE_ID_START + i);
        zp_send_buf(zp, &type_id, sizeof(type_id));
        zp_send_buf(zp, &t->count, sizeof(t->count));
        zp_send_buf(zp, &t->size, sizeof(t->size));
        zp_send_buf(zp, t->name, zp_strlen(t->name) + 1);

        for (int j = 0; j < t->count; j++) {
            const field_desc_t* f = &t->fields[j];

            uint8_t f_type = zp_get_meta_id(zp->schema, f->type);
            zp_send_buf(zp, &f_type, sizeof(f_type));
            zp_send_buf(zp, &f->offset, sizeof(f->offset));
            zp_send_buf(zp, &f->length, sizeof(f->length));
            zp_send_buf(zp, f->name, zp_strlen(f->name) + 1);
        }
    }

    for (int i = 0; i < zp->schema->action_count; i++) {
        const action_desc_t* a = &zp->schema->actions[i];

        uint8_t type_id = zp_get_action_type_id(zp->schema, (uint8_t)i);
        zp_send_buf(zp, &type_id, sizeof(type_id));
        zp_send_buf(zp, &a->is_array, sizeof(a->is_array));
        zp_send_buf(zp, a->name, zp_strlen(a->name) + 1);
    }

    offset = 0;
    while (offset < pool_used) {
        zp_pool_slot_t* slot = zp_pool_slot(zp, offset);
        size_t type_size = get_type_size(zp->schema, slot->type_id);
        size_t data_len = type_size * slot->count;
        size_t total_slot = ZP_SLOT_TOTAL_SIZE(data_len);

        poolref_t ref = offset;
        zp_send_buf(zp, &ref, sizeof(ref));
        zp_send_buf(zp, &slot->type_id, sizeof(slot->type_id));
        zp_send_buf(zp, &slot->count, sizeof(slot->count));
        zp_send_buf(zp, zp_pool_get(zp, offset), data_len);

        offset += total_slot;
    }
    ZP_UNLOCK(zp, ZP_LOCK_TX);
}

static void handle_internal(
    zp_t* zp, const zp_header_t* hdr, const uint8_t* payload, size_t len
) {
    switch (hdr->action) {
        case ZP_SYS_ACTION_POOL_EDIT: {
            if (len < sizeof(poolref_t)) break;

            const poolref_t ref = *(const poolref_t*)payload;
            const void* data = payload + sizeof(poolref_t);
            zp_pool_edit(zp, ref, hdr->type_id, data, hdr->count);
            break;
        }
        case ZP_SYS_ACTION_POOL_STORE: {
            uint16_t count = hdr->count ? hdr->count : 1;
            poolref_t ref = zp_pool_push(zp, hdr->type_id, payload, count);

            zp_send_raw(
                zp,
                ZP_SYS_ACTION_POOL_STORE,
                TYPE_ID_poolref_t,
                1,
                &ref,
                sizeof(poolref_t)
            );
            break;
        }
        case ZP_SYS_ACTION_POOL_RESET:
            zp_pool_reset(zp);
            zp_send_raw(zp, ZP_SYS_ACTION_POOL_RESET, TYPE_ID_NONE, 0, NULL, 0);
            break;
        default:
            zp_emit_log(
                zp, ZP_LOG_WARN, ZP_ERR_UNKNOWN_INTERNAL_ACTION, hdr->action
            );
            break;
    }
}

uint8_t* zp_rx_get_buffer(zp_t* zp, size_t* out_available_len) {
    if (!zp || !out_available_len) return NULL;
    if (zp->rx_len >= zp->rx_cap) {
        *out_available_len = 0;
        return NULL;
    }

    *out_available_len = zp->rx_cap - zp->rx_len;
    return zp->rx_buf + zp->rx_len;
}

void zp_rx_commit(zp_t* zp, size_t len) {
    if (zp->rx_len + len > zp->rx_cap) {
        zp_emit_log(
            zp,
            ZP_LOG_ERROR,
            ZP_ERR_BUFFER_OVERFLOW,
            (uint16_t)(zp->rx_len + len)
        );
        zp->rx_len = 0;
        return;
    }

    zp->rx_len += len;

    while (zp->rx_len >= sizeof(zp_header_t)) {
        const zp_header_t* hdr = (const zp_header_t*)zp->rx_buf;
        size_t payload_len = 0;
        const action_desc_t* act = NULL;

        if (hdr->action == ZP_SYS_ACTION_POOL_EDIT) {
            size_t data_bytes =
                get_type_size(zp->schema, hdr->type_id) * hdr->count;
            payload_len = sizeof(poolref_t) + data_bytes;
        } else if (hdr->action >= ZP_SYS_ACTION_START) {
            payload_len = get_type_size(zp->schema, hdr->type_id) * hdr->count;
        } else {
            if (hdr->action >= zp->schema->action_count) {
                zp_emit_log(
                    zp, ZP_LOG_WARN, ZP_ERR_INVALID_ACTION, hdr->action
                );
                zp->rx_len = 0;
                break;
            }

            act = &zp->schema->actions[hdr->action];
            payload_len = (act->type ? act->type->size : 0) * hdr->count;
        }

        size_t total_packet_len = sizeof(zp_header_t) + payload_len;

        if (total_packet_len > zp->rx_cap) {
            zp_emit_log(
                zp,
                ZP_LOG_ERROR,
                ZP_ERR_BUFFER_OVERFLOW,
                (uint16_t)total_packet_len
            );
            zp->rx_len = 0;
            break;
        }

        if (zp->rx_len < total_packet_len) break;

        const uint8_t* payload = zp->rx_buf + sizeof(zp_header_t);

        if (hdr->action >= ZP_SYS_ACTION_START) {
            handle_internal(zp, hdr, payload, payload_len);
            goto advance;
        }

        if (!zp->dispatch(hdr, payload, payload_len)) {
            zp_emit_log(zp, ZP_LOG_WARN, ZP_ERR_NO_HOOK, hdr->action);
        }

    advance:;
        size_t remaining = zp->rx_len - total_packet_len;
        if (remaining > 0) {
            zp_memmove(zp->rx_buf, zp->rx_buf + total_packet_len, remaining);
        }
        zp->rx_len = remaining;
    }
}

int zp_init(zp_t* zp, const zp_config_t* cfg) {
    if (!zp || !cfg) return -1;
    if (!cfg->schema || !cfg->writev || !cfg->rx_buf) return -1;

    uintptr_t rx_addr = (uintptr_t)cfg->rx_buf;
    size_t rx_pad = (8 - (rx_addr & 7)) & 7;
    if (cfg->rx_cap < rx_pad + 8) return -1;

    *zp = (zp_t){0};

    zp->schema = cfg->schema;

    zp->lock = cfg->lock;
    zp->unlock = cfg->unlock;

    zp->writev = cfg->writev;
    zp->dispatch = cfg->dispatch ? cfg->dispatch : cfg->schema->dispatch;
    zp->userdata = cfg->userdata;

    zp->rx_buf = (uint8_t*)cfg->rx_buf + rx_pad + 4;
    zp->rx_cap = cfg->rx_cap - rx_pad - 4;
    zp->rx_len = 0;

    if (cfg->pool_buf) {
        uintptr_t pool_addr = (uintptr_t)cfg->pool_buf;
        size_t pool_pad = (8 - (pool_addr & 7)) & 7;
        if (cfg->pool_cap < pool_pad + 8) return -1;
        zp->pool_buf = (uint8_t*)cfg->pool_buf + pool_pad + 4;
        zp->pool_cap = cfg->pool_cap - pool_pad - 4;
        zp->pool_len = 0;
    }

    return 0;
}

zp_ssize_t zp_send_raw(
    zp_t* zp,
    uint8_t action,
    uint8_t type_id,
    uint16_t count,
    const void* payload,
    size_t size
) {
    if (!zp || !zp->writev) return -1;

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

    ZP_LOCK(zp, ZP_LOCK_TX);
    zp_ssize_t ret = zp->writev(iov, iovcnt);
    ZP_UNLOCK(zp, ZP_LOCK_TX);
    return ret;
}

zp_ssize_t zp_send(
    zp_t* zp,
    uint8_t action,
    uint16_t count,
    const void* payload,
    size_t size
) {
    uint8_t type_id = zp_get_action_type_id(zp ? zp->schema : NULL, action);
    return zp_send_raw(zp, action, type_id, count, payload, size);
}
