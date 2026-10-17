#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "zeropack/protocol.h"
#include "zeropack/types.h"

zp_ssize_t zp_send(
    zp_ctx_t* ctx,
    uint8_t action,
    uint16_t count,
    const void* payload,
    size_t size
);

// -----------------------------------------------------------------------------
// Action Descriptors with Optional Wire Alias Support:
//   V(ACTION_NAME)             or V(ACTION_NAME, "wire_name")
//   T(ACTION_NAME, Type)       or T(ACTION_NAME, Type, "wire_name")
//   A(ACTION_NAME, Type)       or A(ACTION_NAME, Type, "wire_name")
// -----------------------------------------------------------------------------
#define _ZP_ACTION_V_1(_name) {.name = #_name, .type = NULL, .is_array = 0},
#define _ZP_ACTION_V_2(_name, _alias) {.name = _alias, .type = NULL, .is_array = 0},
#define _ZP_GET_ACTION_V(_1, _2, NAME, ...) NAME
#define _ZP_ACTION_V(...)                                             \
    _ZP_GET_ACTION_V(__VA_ARGS__, _ZP_ACTION_V_2, _ZP_ACTION_V_1, 0)( \
        __VA_ARGS__                                                   \
    )

#define _ZP_ACTION_T_2(_name, _type) {.name = #_name, .type = &__zp_meta_##_type, .is_array = 0},
#define _ZP_ACTION_T_3(_name, _type, _alias) {.name = _alias, .type = &__zp_meta_##_type, .is_array = 0},
#define _ZP_GET_ACTION_T(_1, _2, _3, NAME, ...) NAME
#define _ZP_ACTION_T(...)                                             \
    _ZP_GET_ACTION_T(__VA_ARGS__, _ZP_ACTION_T_3, _ZP_ACTION_T_2, 0)( \
        __VA_ARGS__                                                   \
    )

#define _ZP_ACTION_A_2(_name, _type) {.name = #_name, .type = &__zp_meta_##_type, .is_array = 1},
#define _ZP_ACTION_A_3(_name, _type, _alias) {.name = _alias, .type = &__zp_meta_##_type, .is_array = 1},
#define _ZP_GET_ACTION_A(_1, _2, _3, NAME, ...) NAME
#define _ZP_ACTION_A(...)                                             \
    _ZP_GET_ACTION_A(__VA_ARGS__, _ZP_ACTION_A_3, _ZP_ACTION_A_2, 0)( \
        __VA_ARGS__                                                   \
    )

#define _ZP_GEN_ACTION_ID_IMPL(name, ...) name,
#define _ZP_GEN_ACTION_ID(...) _ZP_GEN_ACTION_ID_IMPL(__VA_ARGS__, 0)

#define ZP_DECLARE_ACTION_IDS(ActionsMacro) \
    enum { ActionsMacro(_ZP_GEN_ACTION_ID, _ZP_GEN_ACTION_ID, _ZP_GEN_ACTION_ID) };

#define _ZP_DISPATCH_V(name, ...)                          \
    case name:                                             \
        if (__zp_callbacks[name]) {                        \
            ((zp_cb_##name##_t)__zp_callbacks[name])(ctx); \
            return true;                                   \
        }                                                  \
        return false;
#define ZP_DISPATCH_V(...) _ZP_DISPATCH_V(__VA_ARGS__, 0)

#define _ZP_DISPATCH_T(name, type, ...)                                       \
    case name:                                                                \
        if (__zp_callbacks[name]) {                                           \
            ((zp_cb_##name##_t)__zp_callbacks[name])(ctx, (const type*)payload); \
            return true;                                                      \
        }                                                                     \
        return false;
#define ZP_DISPATCH_T(...) _ZP_DISPATCH_T(__VA_ARGS__, 0)

#define _ZP_DISPATCH_A(name, type, ...)                 \
    case name:                                          \
        if (__zp_callbacks[name]) {                     \
            ((zp_cb_##name##_t)__zp_callbacks[name])(   \
                ctx, (const type*)payload, hdr->count   \
            );                                          \
            return true;                                \
        }                                               \
        return false;
#define ZP_DISPATCH_A(...) _ZP_DISPATCH_A(__VA_ARGS__, 0)

#define _ZP_CB_TYPE_V(name, ...) typedef void (*zp_cb_##name##_t)(zp_ctx_t* ctx);
#define ZP_CB_TYPE_V(...) _ZP_CB_TYPE_V(__VA_ARGS__, 0)

#define _ZP_CB_TYPE_T(name, type, ...) \
    typedef void (*zp_cb_##name##_t)(zp_ctx_t* ctx, const type* data);
#define ZP_CB_TYPE_T(...) _ZP_CB_TYPE_T(__VA_ARGS__, 0)

#define _ZP_CB_TYPE_A(name, type, ...) \
    typedef void (*zp_cb_##name##_t)(zp_ctx_t* ctx, const type* data, uint16_t count);
#define ZP_CB_TYPE_A(...) _ZP_CB_TYPE_A(__VA_ARGS__, 0)

#define ZP_DECLARE_CALLBACKS(ActionsMacro) \
    ActionsMacro(ZP_CB_TYPE_V, ZP_CB_TYPE_T, ZP_CB_TYPE_A)

#define ZP_INLINE __attribute__((unused, always_inline)) static inline

#define _ZP_SEND_FN_V(name, ...)                           \
    ZP_INLINE zp_ssize_t __zp_emit_##name(zp_ctx_t* ctx) { \
        return zp_send(ctx, name, 0, NULL, 0);             \
    }
#define ZP_SEND_FN_V(...) _ZP_SEND_FN_V(__VA_ARGS__, 0)

#define _ZP_SEND_FN_T(name, type, ...)                                       \
    ZP_INLINE zp_ssize_t __zp_emit_##name(zp_ctx_t* ctx, const type* data) { \
        return zp_send(ctx, name, 1, data, sizeof(type));                    \
    }
#define ZP_SEND_FN_T(...) _ZP_SEND_FN_T(__VA_ARGS__, 0)

#define _ZP_SEND_FN_A(name, type, ...)                   \
    ZP_INLINE zp_ssize_t __zp_emit_##name(               \
        zp_ctx_t* ctx, const type* data, uint16_t count  \
    ) {                                                  \
        return zp_send(                                  \
            ctx,                                         \
            name,                                        \
            count,                                       \
            data,                                        \
            (size_t)count * sizeof(type)                 \
        );                                               \
    }
#define ZP_SEND_FN_A(...) _ZP_SEND_FN_A(__VA_ARGS__, 0)

#define ZP_DECLARE_SENDERS(ActionsMacro) \
    ActionsMacro(ZP_SEND_FN_V, ZP_SEND_FN_T, ZP_SEND_FN_A)

#define __ZP_EMIT_V(ctx, name) __zp_emit_##name(ctx)
#define __ZP_EMIT_TA(ctx, name, ...) __zp_emit_##name(ctx, __VA_ARGS__)
#define __ZP_EMIT_DISPATCH(_1, _2, _3, _4, NAME, ...) NAME
#define zp_emit(...)                                                            \
    __ZP_EMIT_DISPATCH(__VA_ARGS__, __ZP_EMIT_TA, __ZP_EMIT_TA, __ZP_EMIT_V, 0)( \
        __VA_ARGS__                                                             \
    )

#define zp_on_action(schema, action_name, fn)                       \
    do {                                                            \
        extern void (*__##schema##_callbacks[])(void);              \
        zp_cb_##action_name##_t _chk = (fn);                        \
        __##schema##_callbacks[action_name] = (void (*)(void))_chk; \
    } while (0)

typedef bool (*zp_dispatch_fn_t)(
    zp_ctx_t* ctx, const zp_header_t* hdr, const void* payload
);
