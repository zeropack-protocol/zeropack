#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "zeropack/protocol.h"
#include "zeropack/transport.h"

// Forward declaration of zp_send so action.h doesn't depend on core.h
zp_ssize_t zp_send(
    uint8_t action,
    uint8_t type_id,
    uint16_t count,
    const void* payload,
    size_t size
);

#define ACTION_V(name) {TYPE_ID_NONE, 0, #name},
#define ACTION_T(name, type) {TYPE_ID_##type, 0, #name},
#define ACTION_A(name, type) {TYPE_ID_##type, 1, #name},

#define GEN_ACTION_ID_V(name) name,
#define GEN_ACTION_ID_T(name, type) name,
#define GEN_ACTION_ID_A(name, type) name,
#define ZP_DECLARE_ACTION_IDS(ActionsMacro)                                \
    typedef enum {                                                         \
        ActionsMacro(GEN_ACTION_ID_V, GEN_ACTION_ID_T, GEN_ACTION_ID_A)    \
            ACTION_ID_MAX                                                  \
    } action_id_t;                                                         \
                                                                           \
    _Static_assert(                                                        \
        ACTION_ID_MAX <= ZP_SYS_ACTION_START,                              \
        "Action limit exceeded: ZeroPack reserves action IDs 0xF0 .. 0xFF" \
    );

#define ZP_DISPATCH_V(name)                             \
    case name:                                          \
        if (__zp_callbacks[name]) {                     \
            ((zp_cb_##name##_t)__zp_callbacks[name])(); \
            return true;                                \
        }                                               \
        return false;

#define ZP_DISPATCH_T(name, type)                                           \
    case name:                                                              \
        if (__zp_callbacks[name] && len >= sizeof(type)) {                  \
            ((zp_cb_##name##_t)__zp_callbacks[name])((const type*)payload); \
            return true;                                                    \
        }                                                                   \
        return false;

#define ZP_DISPATCH_A(name, type)                       \
    case name:                                          \
        if (__zp_callbacks[name] &&                     \
            len >= (size_t)hdr->count * sizeof(type)) { \
            ((zp_cb_##name##_t)__zp_callbacks[name])(   \
                (const type*)payload, hdr->count        \
            );                                          \
            return true;                                \
        }                                               \
        return false;

#define ZP_CB_TYPE_V(name) typedef void (*zp_cb_##name##_t)(void);
#define ZP_CB_TYPE_T(name, type) \
    typedef void (*zp_cb_##name##_t)(const type* data);
#define ZP_CB_TYPE_A(name, type) \
    typedef void (*zp_cb_##name##_t)(const type* data, uint16_t count);

#define ZP_DECLARE_CALLBACKS(ActionsMacro)       \
    ActionsMacro(                                \
        ZP_CB_TYPE_V, ZP_CB_TYPE_T, ZP_CB_TYPE_A \
    ) extern void (*__zp_callbacks[ACTION_ID_MAX])(void);

#define ZP_INLINE __attribute__((unused, always_inline)) static inline
#define ZP_SEND_FN_V(name)                              \
    ZP_INLINE zp_ssize_t __zp_emit_##name(void) {       \
        return zp_send(name, TYPE_ID_NONE, 0, NULL, 0); \
    }

#define ZP_SEND_FN_T(name, type)                                     \
    ZP_INLINE zp_ssize_t __zp_emit_##name(const type* data) {        \
        return zp_send(name, TYPE_ID_##type, 1, data, sizeof(type)); \
    }

#define ZP_SEND_FN_A(name, type)                                              \
    ZP_INLINE zp_ssize_t __zp_emit_##name(const type* data, uint16_t count) { \
        return zp_send(                                                       \
            name, TYPE_ID_##type, count, data, (size_t)count * sizeof(type)   \
        );                                                                    \
    }
#define ZP_DECLARE_SENDERS(ActionsMacro) \
    ActionsMacro(ZP_SEND_FN_V, ZP_SEND_FN_T, ZP_SEND_FN_A)

#define __ZP_EMIT_V(name) __zp_emit_##name()
#define __ZP_EMIT_T(name, data) __zp_emit_##name(data)
#define __ZP_EMIT_A(name, data, count) __zp_emit_##name(data, count)
#define __ZP_EMIT_DISPATCH(_1, _2, _3, NAME, ...) NAME
#define zp_emit(...)                                                           \
    __ZP_EMIT_DISPATCH(__VA_ARGS__, __ZP_EMIT_A, __ZP_EMIT_T, __ZP_EMIT_V, 0)( \
        __VA_ARGS__                                                            \
    )
#define zp_on_action(action_name, fn)                       \
    do {                                                    \
        zp_cb_##action_name##_t _chk = (fn);                \
        __zp_callbacks[action_name] = (void (*)(void))_chk; \
    } while (0)

typedef bool (*zp_dispatch_fn_t)(
    const zp_header_t* hdr, const uint8_t* payload, size_t len
);
