#pragma once

#include <stddef.h>
#include <stdint.h>

#include "zeropack/action.h"
#include "zeropack/transport.h"

int zp_start(void);

zp_ssize_t zp_send(
    uint8_t action,
    uint8_t type_id,
    uint16_t count,
    const void* payload,
    size_t size
);

void zp_send_handshake(void);
uint8_t* zp_rx_get_buffer(size_t* out_available_len);
void zp_rx_commit(size_t len);
void zp_set_dispatch_hook(zp_dispatch_fn_t hook);
void zp_emit_log(zp_log_level_t level, zp_log_code_t code, uint16_t extra);
