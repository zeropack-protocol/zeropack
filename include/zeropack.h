#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Types =======================================================================
// clang-format off
typedef int8_t   s8;
typedef uint8_t  u8;
typedef int16_t  s16;
typedef uint16_t u16;
typedef int32_t  s32;
typedef uint32_t u32;
typedef int64_t  s64;
typedef uint64_t u64;
typedef float    f32;
typedef double   f64;
typedef uint16_t poolref_t;
typedef ptrdiff_t zp_ssize_t;

typedef struct {
    const void* buf;
    size_t len;
} zp_iovec_t;

#define ZP_PRIMITIVES(X) \
    X(u8) X(u16) X(u32) X(u64) \
    X(s8) X(s16) X(s32) X(s64) \
    X(f32) X(f64) X(bool) X(poolref_t)
// clang-format on

#define _ZP_GEN_PRIM_TYPE_ID(name) TYPE_ID_##name,
enum {
    TYPE_ID_NONE = 0,
    ZP_PRIMITIVES(_ZP_GEN_PRIM_TYPE_ID) TYPE_ID_START,
};
#undef _ZP_GEN_PRIM_TYPE_ID
#define TYPE_ID__Bool TYPE_ID_bool

#ifndef zp_container_of
#define zp_container_of(ptr, type, member) \
    ((type*)((char*)(ptr) - offsetof(type, member)))
#endif

// Wire format =================================================================
typedef struct __attribute__((packed)) {
    uint8_t action;
    uint8_t type_id;
    uint16_t count;
} zp_header_t;

_Static_assert(sizeof(zp_header_t) == 4, "zp_header_t must be 4 bytes");

typedef struct __attribute__((packed)) {
    uint8_t magic;
    uint8_t byteord : 1;
    uint8_t reserved : 7;
    uint16_t pool_size;
    uint8_t type_count;
    uint8_t action_count;
    uint16_t pool_item_count;
} zp_handshake_t;

_Static_assert(sizeof(zp_handshake_t) == 8, "zp_handshake_t must be 8 bytes");

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

// Context & configuration =====================================================
typedef enum {
    ZP_LOCK_TX = 0,
    ZP_LOCK_POOL = 1,
} zp_lock_target_t;

typedef struct zp_ctx zp_ctx_t;
typedef struct zp_schema zp_schema_t;

typedef void (*zp_lock_fn_t)(zp_ctx_t* ctx, zp_lock_target_t target);
typedef zp_ssize_t (*zp_writev_fn_t)(
    zp_ctx_t* ctx, const zp_iovec_t* iov, int iovcnt
);
typedef bool (*zp_dispatch_fn_t)(
    zp_ctx_t* ctx, const zp_header_t* hdr, const void* payload
);

typedef enum {
    ZP_STATE_UNINITIALIZED = 0,
    ZP_STATE_INITIALIZED = (1 << 0),
    ZP_STATE_HANDSHAKE_SENT = (1 << 1),
    ZP_STATE_CONNECTED = (1 << 2),
} zp_state_t;

struct zp_ctx {
    const zp_schema_t* schema;
    zp_writev_fn_t writev;
    zp_dispatch_fn_t dispatch;
    zp_lock_fn_t lock;
    zp_lock_fn_t unlock;
    uint8_t* rx_buf;
    uint8_t* pool_buf;
    void* userdata;

    size_t rx_cap;
    size_t rx_len;

    uint32_t state;
    uint16_t pool_cap;
    uint16_t pool_len;
};

#define ZP_BUFFER(n, s) .n##_buf = (uint8_t*)(s), .n##_cap = sizeof(s)

#define ZP_LOCK(ctx, target)                           \
    do {                                               \
        if ((ctx)->lock) (ctx)->lock((ctx), (target)); \
    } while (0)

#define ZP_UNLOCK(ctx, target)                             \
    do {                                                   \
        if ((ctx)->unlock) (ctx)->unlock((ctx), (target)); \
    } while (0)

#define zp_rx_buf(ctx) ((ctx)->rx_buf + (ctx)->rx_len)
#define zp_rx_avail(ctx) ((ctx)->rx_cap - (ctx)->rx_len)

// Schema metadata =============================================================
typedef struct zp_field_meta zp_field_meta_t;
typedef struct zp_type_meta zp_type_meta_t;

struct zp_field_meta {
    const char* name;
    const zp_type_meta_t* type;
    uint16_t offset;
    uint16_t length;
};

struct zp_type_meta {
    const char* name;
    const zp_field_meta_t* fields;
    uint16_t size;
    uint8_t count;
};

#define _ZP_DECL_PRIM_META(p) extern const zp_type_meta_t __zp_meta_##p;
ZP_PRIMITIVES(_ZP_DECL_PRIM_META)
#undef _ZP_DECL_PRIM_META
#define __zp_meta__Bool __zp_meta_bool

#define zp_type_meta(T) (&__zp_meta_##T)
#define zp_type_name(T) (__zp_meta_##T.name)
#define zp_type_size(T) (__zp_meta_##T.size)

typedef struct {
    const char* name;
    const zp_type_meta_t* type;
    uint8_t is_array;
} zp_action_meta_t;

struct zp_schema {
    const zp_type_meta_t* const* types;
    const zp_action_meta_t* actions;
    zp_dispatch_fn_t dispatch;
    uint8_t type_count;
    uint8_t action_count;
};

uint8_t zp_get_meta_id(const zp_schema_t* schema, const zp_type_meta_t* meta);
uint8_t zp_get_action_type_id(const zp_schema_t* schema, uint8_t action);
const zp_type_meta_t* zp_get_type_meta(
    const zp_schema_t* schema, uint8_t type_id
);
size_t zp_get_type_size(const zp_schema_t* schema, uint8_t type_id);
const char* zp_get_type_name(const zp_schema_t* schema, uint8_t type_id);

#define zp_type_id(schema, T) zp_get_meta_id((schema), zp_type_meta(T))

// Memory pool =================================================================
typedef enum {
    ZP_POOL_FLAG_NONE = 0x00,
    ZP_POOL_FLAG_READONLY = (1 << 0),
} zp_pool_flags_t;

typedef struct {
    uint8_t type_id;
    uint8_t flags;
    uint16_t count;
    uint16_t cap;
} zp_pool_slot_t;

_Static_assert(sizeof(zp_pool_slot_t) == 6, "zp_pool_slot_t must be 6 bytes");
_Static_assert(
    _Alignof(zp_pool_slot_t) == 2, "zp_pool_slot_t must be 2-byte aligned"
);

#define ZP_ALIGN8(x) (((x) + 7) & ~7)
#define ZP_SLOT_TOTAL_SIZE(data_len) \
    ZP_ALIGN8(sizeof(zp_pool_slot_t) + (data_len))

#define zp_pool_avail(ctx) ((size_t)(ctx)->pool_cap - (size_t)(ctx)->pool_len)
#define zp_pool_slot(ctx, ref) ((zp_pool_slot_t*)((ctx)->pool_buf + (ref)))

bool zp_pool_is_valid(const zp_ctx_t* ctx, poolref_t ref);
void* zp_pool_get_checked(
    const zp_ctx_t* ctx, uint8_t type, poolref_t ref, uint16_t* out_count
);
poolref_t zp_pool_push(
    zp_ctx_t* ctx, uint8_t type_id, const void* data, uint16_t count
);
zp_status_t zp_pool_edit(
    zp_ctx_t* ctx,
    poolref_t ref,
    uint8_t type_id,
    const void* data,
    uint16_t count
);
void zp_pool_reset(zp_ctx_t* ctx);

#define zp_pool_get_typed(ctx, type, ref)                   \
    ((type*)zp_pool_get_checked(                            \
        (ctx), zp_type_id((ctx)->schema, type), (ref), NULL \
    ))

#define zp_pool_get_slice(ctx, type, ref, out_count)               \
    ((type*)zp_pool_get_checked(                                   \
        (ctx), zp_type_id((ctx)->schema, type), (ref), (out_count) \
    ))

#define zp_pool_get_str(ctx, ref) \
    ((char*)zp_pool_get_checked((ctx), TYPE_ID_u8, (ref), NULL))

#define zp_pool_push_val(ctx, type, v) \
    zp_pool_push((ctx), zp_type_id((ctx)->schema, type), &(v), 1)

#define zp_pool_push_str(ctx, s) \
    zp_pool_push((ctx), TYPE_ID_u8, (s), (uint16_t)(zp_strlen(s) + 1))

#define zp_pool_push_arr(ctx, type, ptr, count) \
    zp_pool_push((ctx), zp_type_id((ctx)->schema, type), (ptr), (count))

// Core runtime ================================================================
int zp_init(zp_ctx_t* ctx);
void zp_rx_commit(zp_ctx_t* ctx, size_t len);
void zp_send_handshake(zp_ctx_t* ctx);
void zp_emit_log(
    zp_ctx_t* ctx, zp_log_level_t level, zp_status_t code, uint16_t extra
);
zp_ssize_t zp_send_raw(
    zp_ctx_t* ctx,
    uint8_t action,
    uint8_t type_id,
    uint16_t count,
    const void* payload,
    size_t size
);
zp_ssize_t zp_send(
    zp_ctx_t* ctx,
    uint8_t action,
    uint16_t count,
    const void* payload,
    size_t size
);

// Schema & action macros ======================================================
#define ZP_DECLARE(TypesMacro, ActionsMacro) \
    _ZP_GENERATE_TYPES(TypesMacro)           \
    _ZP_DECLARE_ACTIONS(ActionsMacro)

// clang-format off
#define ZP_GENERATE_SCHEMA(Name, TypesMacro, ActionsMacro)                  \
    static const zp_action_meta_t Name##_actions[] = {                      \
        ActionsMacro(_ZP_ACTION_V, _ZP_ACTION_T, _ZP_ACTION_A)              \
    };                                                                      \
                                                                            \
    _Static_assert(                                                         \
        (sizeof(Name##_actions) / sizeof(Name##_actions[0])) <=             \
            ZP_SYS_ACTION_START,                                            \
        "Action limit exceeded"                                             \
    );                                                                      \
                                                                            \
    void (*__##Name##_callbacks[sizeof(Name##_actions) /                    \
                             sizeof(Name##_actions[0])])(void) = {0};       \
                                                                            \
    static bool Name##_dispatch(                                            \
        zp_ctx_t* ctx, const zp_header_t* hdr, const void* payload          \
    ) {                                                                     \
        (void)ctx; (void)hdr; (void)payload;                                \
        void (** const __zp_callbacks)(void) = __##Name##_callbacks;        \
        switch (hdr->action) {                                              \
            ActionsMacro(ZP_DISPATCH_V, ZP_DISPATCH_T, ZP_DISPATCH_A)       \
            default:                                                        \
                return false;                                               \
        }                                                                   \
    }                                                                       \
                                                                            \
    static const zp_type_meta_t* const Name##_types[] = {                   \
        TypesMacro(_ZP_REF_TYPE_META)                                       \
    };                                                                      \
                                                                            \
    _Static_assert(                                                         \
        (sizeof(Name##_types) / sizeof(Name##_types[0])) <=                 \
            (255 - TYPE_ID_START),                                          \
        "Struct type limit exceeded"                                        \
    );                                                                      \
                                                                            \
    const zp_schema_t Name = {                                              \
        .types = Name##_types,                                              \
        .type_count = sizeof(Name##_types) / sizeof(Name##_types[0]),       \
        .actions = Name##_actions,                                          \
        .action_count = sizeof(Name##_actions) / sizeof(Name##_actions[0]), \
        .dispatch = Name##_dispatch,                                        \
    };
// clang-format on

#define ZP_SUBSCRIBE(schema, action_name, fn)                       \
    do {                                                            \
        extern void (*__##schema##_callbacks[])(void);              \
        zp_cb_##action_name##_t _chk = (fn);                        \
        __##schema##_callbacks[action_name] = (void (*)(void))_chk; \
    } while (0)

#define zp_emit(...)                                                             \
    __ZP_EMIT_DISPATCH(__VA_ARGS__, __ZP_EMIT_TA, __ZP_EMIT_TA, __ZP_EMIT_V, 0)( \
        __VA_ARGS__                                                              \
    )

#include "zeropack/internal.h"
