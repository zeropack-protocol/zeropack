#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "zeropack/protocol.h"
#include "zeropack/types.h"

zp_ssize_t zp_send(
    zp_t* zp,
    uint8_t action,
    uint16_t count,
    const void* payload,
    size_t size
);

zp_ssize_t zp_send_raw(
    zp_t* zp,
    uint8_t action,
    uint8_t type_id,
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
#define ACTION_V(...)                                                 \
    _ZP_GET_ACTION_V(__VA_ARGS__, _ZP_ACTION_V_2, _ZP_ACTION_V_1, 0)( \
        __VA_ARGS__                                                   \
    )

#define _ZP_ACTION_T_2(_name, _type) {.name = #_name, .type = &__zp_meta_##_type, .is_array = 0},
#define _ZP_ACTION_T_3(_name, _type, _alias) {.name = _alias, .type = &__zp_meta_##_type, .is_array = 0},
#define _ZP_GET_ACTION_T(_1, _2, _3, NAME, ...) NAME
#define ACTION_T(...)                                                 \
    _ZP_GET_ACTION_T(__VA_ARGS__, _ZP_ACTION_T_3, _ZP_ACTION_T_2, 0)( \
        __VA_ARGS__                                                   \
    )

#define _ZP_ACTION_A_2(_name, _type) {.name = #_name, .type = &__zp_meta_##_type, .is_array = 1},
#define _ZP_ACTION_A_3(_name, _type, _alias) {.name = _alias, .type = &__zp_meta_##_type, .is_array = 1},
#define _ZP_GET_ACTION_A(_1, _2, _3, NAME, ...) NAME
#define ACTION_A(...)                                                 \
    _ZP_GET_ACTION_A(__VA_ARGS__, _ZP_ACTION_A_3, _ZP_ACTION_A_2, 0)( \
        __VA_ARGS__                                                   \
    )

#define _ZP_GEN_ACTION_ID_1(name) name,
#define _ZP_GEN_ACTION_ID_2(name, alias) name,
#define _ZP_GEN_ACTION_ID_3(name, type, ...) name,
#define _ZP_GET_ACTION_ID(_1, _2, _3, NAME, ...) NAME
#define GEN_ACTION_ID(...)                                                                            \
    _ZP_GET_ACTION_ID(__VA_ARGS__, _ZP_GEN_ACTION_ID_3, _ZP_GEN_ACTION_ID_2, _ZP_GEN_ACTION_ID_1, 0)( \
        __VA_ARGS__                                                                                   \
    )

#define ZP_DECLARE_ACTION_IDS(ActionsMacro) \
    enum { ActionsMacro(GEN_ACTION_ID, GEN_ACTION_ID, GEN_ACTION_ID) };

#define _ZP_DISPATCH_V_1(name)                          \
    case name:                                          \
        if (__zp_callbacks[name]) {                     \
            ((zp_cb_##name##_t)__zp_callbacks[name])(); \
            return true;                                \
        }                                               \
        return false;
#define _ZP_DISPATCH_V_2(name, alias) _ZP_DISPATCH_V_1(name)
#define _ZP_GET_DISP_V(_1, _2, NAME, ...) NAME
#define ZP_DISPATCH_V(...)                                              \
    _ZP_GET_DISP_V(__VA_ARGS__, _ZP_DISPATCH_V_2, _ZP_DISPATCH_V_1, 0)( \
        __VA_ARGS__                                                     \
    )

#define _ZP_DISPATCH_T_2(name, type)                                        \
    case name:                                                              \
        if (__zp_callbacks[name] && len >= sizeof(type)) {                  \
            ((zp_cb_##name##_t)__zp_callbacks[name])((const type*)payload); \
            return true;                                                    \
        }                                                                   \
        return false;
#define _ZP_DISPATCH_T_3(name, type, alias) _ZP_DISPATCH_T_2(name, type)
#define _ZP_GET_DISP_T(_1, _2, _3, NAME, ...) NAME
#define ZP_DISPATCH_T(...)                                              \
    _ZP_GET_DISP_T(__VA_ARGS__, _ZP_DISPATCH_T_3, _ZP_DISPATCH_T_2, 0)( \
        __VA_ARGS__                                                     \
    )

#define _ZP_DISPATCH_A_2(name, type)                    \
    case name:                                          \
        if (__zp_callbacks[name] &&                     \
            len >= (size_t)hdr->count * sizeof(type)) { \
            ((zp_cb_##name##_t)__zp_callbacks[name])(   \
                (const type*)payload, hdr->count        \
            );                                          \
            return true;                                \
        }                                               \
        return false;
#define _ZP_DISPATCH_A_3(name, type, alias) _ZP_DISPATCH_A_2(name, type)
#define _ZP_GET_DISP_A(_1, _2, _3, NAME, ...) NAME
#define ZP_DISPATCH_A(...)                                              \
    _ZP_GET_DISP_A(__VA_ARGS__, _ZP_DISPATCH_A_3, _ZP_DISPATCH_A_2, 0)( \
        __VA_ARGS__                                                     \
    )

#define _ZP_CB_TYPE_V_1(name) typedef void (*zp_cb_##name##_t)(void);
#define _ZP_CB_TYPE_V_2(name, alias) _ZP_CB_TYPE_V_1(name)
#define _ZP_GET_CB_V(_1, _2, NAME, ...) NAME
#define ZP_CB_TYPE_V(...) \
    _ZP_GET_CB_V(__VA_ARGS__, _ZP_CB_TYPE_V_2, _ZP_CB_TYPE_V_1, 0)(__VA_ARGS__)

#define _ZP_CB_TYPE_T_2(name, type) \
    typedef void (*zp_cb_##name##_t)(const type* data);
#define _ZP_CB_TYPE_T_3(name, type, alias) _ZP_CB_TYPE_T_2(name, type)
#define _ZP_GET_CB_T(_1, _2, _3, NAME, ...) NAME
#define ZP_CB_TYPE_T(...) \
    _ZP_GET_CB_T(__VA_ARGS__, _ZP_CB_TYPE_T_3, _ZP_CB_TYPE_T_2, 0)(__VA_ARGS__)

#define _ZP_CB_TYPE_A_2(name, type) \
    typedef void (*zp_cb_##name##_t)(const type* data, uint16_t count);
#define _ZP_CB_TYPE_A_3(name, type, alias) _ZP_CB_TYPE_A_2(name, type)
#define _ZP_GET_CB_A(_1, _2, _3, NAME, ...) NAME
#define ZP_CB_TYPE_A(...) \
    _ZP_GET_CB_A(__VA_ARGS__, _ZP_CB_TYPE_A_3, _ZP_CB_TYPE_A_2, 0)(__VA_ARGS__)

#define ZP_DECLARE_CALLBACKS(ActionsMacro) \
    ActionsMacro(ZP_CB_TYPE_V, ZP_CB_TYPE_T, ZP_CB_TYPE_A)

#define ZP_INLINE __attribute__((unused, always_inline)) static inline

#define _ZP_SEND_FN_V_1(name)                         \
    ZP_INLINE zp_ssize_t __zp_emit_##name(zp_t* zp) { \
        return zp_send(zp, name, 0, NULL, 0);         \
    }
#define _ZP_SEND_FN_V_2(name, alias) _ZP_SEND_FN_V_1(name)
#define _ZP_GET_SEND_V(_1, _2, NAME, ...) NAME
#define ZP_SEND_FN_V(...)                                             \
    _ZP_GET_SEND_V(__VA_ARGS__, _ZP_SEND_FN_V_2, _ZP_SEND_FN_V_1, 0)( \
        __VA_ARGS__                                                   \
    )

#define _ZP_SEND_FN_T_2(name, type)                                      \
    ZP_INLINE zp_ssize_t __zp_emit_##name(zp_t* zp, const type* data) {  \
        return zp_send(zp, name, 1, data, sizeof(type));                 \
    }
#define _ZP_SEND_FN_T_3(name, type, alias) _ZP_SEND_FN_T_2(name, type)
#define _ZP_GET_SEND_T(_1, _2, _3, NAME, ...) NAME
#define ZP_SEND_FN_T(...)                                             \
    _ZP_GET_SEND_T(__VA_ARGS__, _ZP_SEND_FN_T_3, _ZP_SEND_FN_T_2, 0)( \
        __VA_ARGS__                                                   \
    )

#define _ZP_SEND_FN_A_2(name, type)                                 \
    ZP_INLINE zp_ssize_t __zp_emit_##name(                          \
        zp_t* zp, const type* data, uint16_t count                  \
    ) {                                                             \
        return zp_send(                                             \
            zp,                                                     \
            name,                                                   \
            count,                                                  \
            data,                                                   \
            (size_t)count * sizeof(type)                            \
        );                                                          \
    }
#define _ZP_SEND_FN_A_3(name, type, alias) _ZP_SEND_FN_A_2(name, type)
#define _ZP_GET_SEND_A(_1, _2, _3, NAME, ...) NAME
#define ZP_SEND_FN_A(...)                                             \
    _ZP_GET_SEND_A(__VA_ARGS__, _ZP_SEND_FN_A_3, _ZP_SEND_FN_A_2, 0)( \
        __VA_ARGS__                                                   \
    )
#define ZP_DECLARE_SENDERS(ActionsMacro) \
    ActionsMacro(ZP_SEND_FN_V, ZP_SEND_FN_T, ZP_SEND_FN_A)

#define __ZP_EMIT_V(zp, name) __zp_emit_##name(zp)
#define __ZP_EMIT_T(zp, name, data) __zp_emit_##name(zp, data)
#define __ZP_EMIT_A(zp, name, data, count) __zp_emit_##name(zp, data, count)
#define __ZP_EMIT_DISPATCH(_1, _2, _3, _4, NAME, ...) NAME
#define zp_emit(...)                                                           \
    __ZP_EMIT_DISPATCH(__VA_ARGS__, __ZP_EMIT_A, __ZP_EMIT_T, __ZP_EMIT_V, 0)( \
        __VA_ARGS__                                                            \
    )
#define zp_on_action(schema, action_name, fn)                       \
    do {                                                            \
        extern void (*__##schema##_callbacks[])(void);              \
        zp_cb_##action_name##_t _chk = (fn);                        \
        __##schema##_callbacks[action_name] = (void (*)(void))_chk; \
    } while (0)

typedef bool (*zp_dispatch_fn_t)(
    const zp_header_t* hdr, const uint8_t* payload, size_t len
);
