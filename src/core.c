#include "zeropack/core.h"

#include <stddef.h>
#include <stdint.h>

#include "zeropack/config.h"
#include "zeropack/mutex.h"
#include "zeropack/pool.h"
#include "zeropack/schema.h"
#include "zeropack/transport.h"

void zp_emit_log(zp_log_level_t level, zp_log_code_t code, uint16_t extra) {
    zp_log_msg_t msg = {
        .level = (uint8_t)level,
        .code = (uint8_t)code,
        .extra = extra,
    };
    zp_send(ZP_SYS_ACTION_LOG, TYPE_ID_u8, sizeof(msg), &msg, sizeof(msg));
}

// Se le agrega un + 4 al tamaño del buffer para dejar un espacio al comienzo,
// de esta manera aseguramos que el struct que llegue estará perfectamente
// alineado en 8 bytes soportando tipos u64/f64
//
// layout:
//  __zp_buf_raw
//  |        zp_rxbuf
//  v        v
//  [4 bytes][zp_header_t (4 bytes)][payload (alineado 8 bytes)]
static uint8_t __zp_buf_raw[ZP_BUFFER_SIZE + 4] __attribute__((aligned(8)));
static uint8_t* zp_rxbuf = &__zp_buf_raw[4];
static size_t s_rx_buffered = 0;

static zp_dispatch_fn_t s_dispatch_hook = NULL;

static inline zp_ssize_t zp_transport_send_buf(const void* buf, size_t len) {
    if (!zp_transport || !zp_transport->writev) return -1;
    zp_iovec_t iov = {.buf = buf, .len = len};
    return zp_transport->writev(&iov, 1);
}

void zp_send_handshake(void) {
    uint16_t pool_item_count = 0;
    uint16_t offset = 0;
    size_t pool_used = zp_pool_used();
    while (offset < pool_used) {
        zp_pool_slot_t* slot = zp_pool_slot(offset);
        size_t type_size = get_type_size(slot->type_id);
        size_t data_len = type_size * slot->count;
        size_t total_slot = ZP_SLOT_TOTAL_SIZE(data_len);
        pool_item_count++;
        offset += total_slot;
    }

    zp_handshake_t hs = {0};
    hs.magic = 'Z';
    hs.byteord = (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__);
    hs.reserved = 0x28;  // 0b101000
    hs.pool_size = ZP_POOL_SIZE;
    hs.type_count = zp_schema.type_count;
    hs.action_count = zp_schema.action_count;
    hs.pool_item_count = pool_item_count;

    ZP_LOCK(tx);
    zp_transport_send_buf(&hs, sizeof(hs));

    for (int i = 0; i < zp_schema.type_count; i++) {
        const struct_desc_t* t = &zp_schema.types[i];

        zp_transport_send_buf(&t->id, sizeof(t->id));
        zp_transport_send_buf(&t->count, sizeof(t->count));
        zp_transport_send_buf(&t->size, sizeof(t->size));
        zp_transport_send_buf(t->name, zp_strlen(t->name) + 1);

        for (int j = 0; j < t->count; j++) {
            const field_desc_t* f = &t->fields[j];

            zp_transport_send_buf(&f->type, sizeof(f->type));
            zp_transport_send_buf(&f->offset, sizeof(f->offset));
            zp_transport_send_buf(&f->length, sizeof(f->length));
            zp_transport_send_buf(f->name, zp_strlen(f->name) + 1);
        }
    }

    for (int i = 0; i < zp_schema.action_count; i++) {
        const action_desc_t* a = &zp_schema.actions[i];

        zp_transport_send_buf(&a->type, sizeof(a->type));
        zp_transport_send_buf(&a->is_array, sizeof(a->is_array));
        zp_transport_send_buf(a->name, zp_strlen(a->name) + 1);
    }

    offset = 0;
    while (offset < pool_used) {
        zp_pool_slot_t* slot = zp_pool_slot(offset);
        size_t type_size = get_type_size(slot->type_id);
        size_t data_len = type_size * slot->count;
        size_t total_slot = ZP_SLOT_TOTAL_SIZE(data_len);

        poolref_t ref = offset;
        zp_transport_send_buf(&ref, sizeof(ref));
        zp_transport_send_buf(&slot->type_id, sizeof(slot->type_id));
        zp_transport_send_buf(&slot->count, sizeof(slot->count));
        zp_transport_send_buf(zp_pool_get(offset), data_len);

        offset += total_slot;
    }
    ZP_UNLOCK(tx);
}

static void handle_internal(
    const zp_header_t* hdr, const uint8_t* payload, size_t len
) {
    switch (hdr->action) {
        case ZP_SYS_ACTION_POOL_EDIT: {
            if (len < sizeof(poolref_t)) break;

            const poolref_t ref = *(const poolref_t*)payload;
            const void* data = payload + sizeof(poolref_t);
            zp_pool_edit(ref, hdr->type_id, data, hdr->count);
            break;
        }
        case ZP_SYS_ACTION_POOL_STORE: {
            uint16_t count = hdr->count ? hdr->count : 1;
            poolref_t ref = zp_pool_push(hdr->type_id, payload, count);

            zp_send(
                ZP_SYS_ACTION_POOL_STORE,
                TYPE_ID_poolref_t,
                1,
                &ref,
                sizeof(poolref_t)
            );
            break;
        }
        case ZP_SYS_ACTION_POOL_RESET:
            zp_pool_reset();
            zp_send(ZP_SYS_ACTION_POOL_RESET, TYPE_ID_NONE, 0, NULL, 0);
            break;
        default:
            zp_emit_log(
                ZP_LOG_WARN, ZP_ERR_UNKNOWN_INTERNAL_ACTION, hdr->action
            );
            break;
    }
}

uint8_t* zp_rx_get_buffer(size_t* out_available_len) {
    if (s_rx_buffered >= ZP_BUFFER_SIZE) {
        *out_available_len = 0;
        return NULL;
    }

    *out_available_len = ZP_BUFFER_SIZE - s_rx_buffered;
    return zp_rxbuf + s_rx_buffered;
}

void zp_rx_commit(size_t len) {
    if (s_rx_buffered + len > ZP_BUFFER_SIZE) {
        zp_emit_log(
            ZP_LOG_ERROR,
            ZP_ERR_BUFFER_OVERFLOW,
            (uint16_t)(s_rx_buffered + len)
        );
        s_rx_buffered = 0;
        return;
    }

    s_rx_buffered += len;

    while (s_rx_buffered >= sizeof(zp_header_t)) {
        const zp_header_t* hdr = (const zp_header_t*)zp_rxbuf;
        size_t payload_len = 0;
        const action_desc_t* act = NULL;

        if (hdr->action == ZP_SYS_ACTION_POOL_EDIT) {
            size_t data_bytes = get_type_size(hdr->type_id) * hdr->count;
            payload_len = sizeof(poolref_t) + data_bytes;
        } else if (hdr->action >= ZP_SYS_ACTION_START) {
            payload_len = get_type_size(hdr->type_id) * hdr->count;
        } else {
            if (hdr->action >= zp_schema.action_count) {
                zp_emit_log(ZP_LOG_WARN, ZP_ERR_INVALID_ACTION, hdr->action);
                s_rx_buffered = 0;
                break;
            }

            act = &zp_schema.actions[hdr->action];
            payload_len = get_type_size(act->type) * hdr->count;
        }

        size_t total_packet_len = sizeof(zp_header_t) + payload_len;

        if (total_packet_len > ZP_BUFFER_SIZE) {
            zp_emit_log(
                ZP_LOG_ERROR, ZP_ERR_BUFFER_OVERFLOW, (uint16_t)total_packet_len
            );
            s_rx_buffered = 0;
            break;
        }

        if (s_rx_buffered < total_packet_len) break;

        const uint8_t* payload = zp_rxbuf + sizeof(zp_header_t);

        if (hdr->action >= ZP_SYS_ACTION_START) {
            handle_internal(hdr, payload, payload_len);
            goto advance;
        }

        zp_dispatch_fn_t dispatch =
            s_dispatch_hook ? s_dispatch_hook : zp_schema.dispatch;

        if (!dispatch(hdr, payload, payload_len)) {
            zp_emit_log(ZP_LOG_WARN, ZP_ERR_NO_HOOK, hdr->action);
        }

    advance:;
        size_t remaining = s_rx_buffered - total_packet_len;
        if (remaining > 0) {
            zp_memmove(zp_rxbuf, zp_rxbuf + total_packet_len, remaining);
        }
        s_rx_buffered = remaining;
    }
}

void zp_set_dispatch_hook(zp_dispatch_fn_t hook) { s_dispatch_hook = hook; }

int zp_start(void) {
    if (!zp_transport) return -1;
    if (zp_transport->start) {
        return zp_transport->start();
    }
    return 0;
}

zp_ssize_t zp_send(
    uint8_t action,
    uint8_t type_id,
    uint16_t count,
    const void* payload,
    size_t size
) {
    if (!zp_transport || !zp_transport->writev) return -1;

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

    ZP_LOCK(tx);
    zp_ssize_t ret = zp_transport->writev(iov, iovcnt);
    ZP_UNLOCK(tx);
    return ret;
}
