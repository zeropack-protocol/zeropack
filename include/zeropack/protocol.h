#pragma once

#include <stdint.h>

typedef struct __attribute__((packed)) {
    uint8_t magic;
    uint8_t byteord : 1;
    uint8_t reserved : 7;

    uint16_t pool_size;
    uint8_t type_count;
    uint8_t action_count;
    uint16_t pool_item_count;
} zp_handshake_t;

typedef struct __attribute__((packed)) {
    uint8_t action;
    uint8_t type_id;
    uint16_t count;
} zp_header_t;

// Acciones internas
#define ZP_SYS_ACTION_START 0xF0
#define ZP_SYS_ACTION_LOG 0xF1

#define ZP_SYS_ACTION_POOL_EDIT 0xFD
#define ZP_SYS_ACTION_POOL_STORE 0xFE
#define ZP_SYS_ACTION_POOL_RESET 0xFF

typedef enum {
    ZP_LOG_OK = 0,
    ZP_LOG_WARN = 1,
    ZP_LOG_ERROR = 2,
} zp_log_level_t;

typedef enum {
    ZP_OK = 0,
    ZP_BUFFER_OVERFLOW,
    ZP_INVALID_ACTION,
    ZP_NO_HOOK,
    ZP_POOL_FULL,
    ZP_POOL_READONLY,
    ZP_POOL_SLOT_STORED,
    ZP_POOL_TYPE_MISMATCH,
    ZP_POOL_SLOT_EXCEEDED,
    ZP_UNKNOWN_INTERNAL_ACTION,
} zp_status_t;

typedef struct {
    uint8_t level;
    uint8_t code;
    uint16_t extra;
} zp_log_msg_t;
