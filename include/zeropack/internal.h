#pragma once
// IWYU pragma: private, include "zeropack.h"

#include <stddef.h>

// freestanding memory builtins
#if defined(__has_builtin)
#if __has_builtin(__builtin_memcpy)
#define zp_memcpy __builtin_memcpy
#endif
#if __has_builtin(__builtin_memmove)
#define zp_memmove __builtin_memmove
#endif
#if __has_builtin(__builtin_strlen)
#define zp_strlen __builtin_strlen
#endif
#elif defined(__GNUC__)
#define zp_memcpy __builtin_memcpy
#define zp_memmove __builtin_memmove
#define zp_strlen __builtin_strlen
#endif

#ifndef zp_memcpy
extern void* memcpy(void* dest, const void* src, size_t n);
#define zp_memcpy memcpy
#endif

#ifndef zp_memmove
extern void* memmove(void* dest, const void* src, size_t n);
#define zp_memmove memmove
#endif

#ifndef zp_strlen
extern size_t strlen(const char* s);
#define zp_strlen strlen
#endif

// internal macro helpers, included only via zeropack.h

#define _ZP_MAKE_FIELD(S, type, name) type name;
#define _ZP_MAKE_ARRAY(S, type, name, len) type name[len];
#define _ZP_DEFINE_TYPE(StructName, FieldsMacro)                \
    typedef struct StructName {                                 \
        FieldsMacro(StructName, _ZP_MAKE_FIELD, _ZP_MAKE_ARRAY) \
    } StructName;

#define _ZP_GEN_TYPE_DECL(TypesMacro) TypesMacro(_ZP_DEFINE_TYPE)

// action descriptors

#define _ZP_ACTION_V_1(_name) {.name = #_name, .type = NULL, .is_array = 0},
#define _ZP_ACTION_V_2(_name, _alias) \
    {.name = _alias, .type = NULL, .is_array = 0},
#define _ZP_GET_ACTION_V(_1, _2, NAME, ...) NAME
#define _ZP_ACTION_V(...)                                             \
    _ZP_GET_ACTION_V(__VA_ARGS__, _ZP_ACTION_V_2, _ZP_ACTION_V_1, 0)( \
        __VA_ARGS__                                                   \
    )

#define _ZP_ACTION_T_2(_name, _type) \
    {.name = #_name, .type = &__zp_meta_##_type, .is_array = 0},
#define _ZP_ACTION_T_3(_name, _type, _alias) \
    {.name = _alias, .type = &__zp_meta_##_type, .is_array = 0},
#define _ZP_GET_ACTION_T(_1, _2, _3, NAME, ...) NAME
#define _ZP_ACTION_T(...)                                             \
    _ZP_GET_ACTION_T(__VA_ARGS__, _ZP_ACTION_T_3, _ZP_ACTION_T_2, 0)( \
        __VA_ARGS__                                                   \
    )

#define _ZP_ACTION_A_2(_name, _type) \
    {.name = #_name, .type = &__zp_meta_##_type, .is_array = 1},
#define _ZP_ACTION_A_3(_name, _type, _alias) \
    {.name = _alias, .type = &__zp_meta_##_type, .is_array = 1},
#define _ZP_GET_ACTION_A(_1, _2, _3, NAME, ...) NAME
#define _ZP_ACTION_A(...)                                             \
    _ZP_GET_ACTION_A(__VA_ARGS__, _ZP_ACTION_A_3, _ZP_ACTION_A_2, 0)( \
        __VA_ARGS__                                                   \
    )

#define _ZP_GEN_ACTION_ID_IMPL(name, ...) name,
#define _ZP_GEN_ACTION_ID(...) _ZP_GEN_ACTION_ID_IMPL(__VA_ARGS__, 0)

#define _ZP_DECLARE_ACTION_IDS(ActionsMacro)                                  \
    enum {                                                                    \
        ActionsMacro(_ZP_GEN_ACTION_ID, _ZP_GEN_ACTION_ID, _ZP_GEN_ACTION_ID) \
    };

// callback signatures and dispatch

#define _ZP_CB_TYPE_V(name, ...) \
    typedef void (*zp_cb_##name##_t)(zp_ctx_t * ctx);
#define ZP_CB_TYPE_V(...) _ZP_CB_TYPE_V(__VA_ARGS__, 0)

#define _ZP_CB_TYPE_T(name, type, ...) \
    typedef void (*zp_cb_##name##_t)(zp_ctx_t * ctx, const type* data);
#define ZP_CB_TYPE_T(...) _ZP_CB_TYPE_T(__VA_ARGS__, 0)

#define _ZP_CB_TYPE_A(name, type, ...)                   \
    typedef void (*zp_cb_##name##_t)(                    \
        zp_ctx_t * ctx, const type* data, uint16_t count \
    );
#define ZP_CB_TYPE_A(...) _ZP_CB_TYPE_A(__VA_ARGS__, 0)

#define _ZP_DECLARE_CALLBACKS(ActionsMacro) \
    ActionsMacro(ZP_CB_TYPE_V, ZP_CB_TYPE_T, ZP_CB_TYPE_A)

#define _ZP_DISPATCH_V(name, ...)                          \
    case name:                                             \
        if (__zp_callbacks[name]) {                        \
            ((zp_cb_##name##_t)__zp_callbacks[name])(ctx); \
            return true;                                   \
        }                                                  \
        return false;
#define ZP_DISPATCH_V(...) _ZP_DISPATCH_V(__VA_ARGS__, 0)

#define _ZP_DISPATCH_T(name, type, ...)               \
    case name:                                        \
        if (__zp_callbacks[name]) {                   \
            ((zp_cb_##name##_t)__zp_callbacks[name])( \
                ctx, (const type*)payload             \
            );                                        \
            return true;                              \
        }                                             \
        return false;
#define ZP_DISPATCH_T(...) _ZP_DISPATCH_T(__VA_ARGS__, 0)

#define _ZP_DISPATCH_A(name, type, ...)               \
    case name:                                        \
        if (__zp_callbacks[name]) {                   \
            ((zp_cb_##name##_t)__zp_callbacks[name])( \
                ctx, (const type*)payload, hdr->count \
            );                                        \
            return true;                              \
        }                                             \
        return false;
#define ZP_DISPATCH_A(...) _ZP_DISPATCH_A(__VA_ARGS__, 0)

// emitters

#define _ZP_INLINE __attribute__((unused, always_inline)) static inline

#define _ZP_SEND_FN_V(name, ...)                            \
    _ZP_INLINE zp_ssize_t __zp_emit_##name(zp_ctx_t* ctx) { \
        return zp_send(ctx, name, 0, NULL, 0);              \
    }
#define ZP_SEND_FN_V(...) _ZP_SEND_FN_V(__VA_ARGS__, 0)

#define _ZP_SEND_FN_T(name, type, ...)                                        \
    _ZP_INLINE zp_ssize_t __zp_emit_##name(zp_ctx_t* ctx, const type* data) { \
        return zp_send(ctx, name, 1, data, sizeof(type));                     \
    }
#define ZP_SEND_FN_T(...) _ZP_SEND_FN_T(__VA_ARGS__, 0)

#define _ZP_SEND_FN_A(name, type, ...)                                        \
    _ZP_INLINE zp_ssize_t __zp_emit_##name(                                   \
        zp_ctx_t* ctx, const type* data, uint16_t count                       \
    ) {                                                                       \
        return zp_send(ctx, name, count, data, (size_t)count * sizeof(type)); \
    }
#define ZP_SEND_FN_A(...) _ZP_SEND_FN_A(__VA_ARGS__, 0)

#define _ZP_DECLARE_SENDERS(ActionsMacro) \
    ActionsMacro(ZP_SEND_FN_V, ZP_SEND_FN_T, ZP_SEND_FN_A)

#define __ZP_EMIT_V(ctx, name) __zp_emit_##name(ctx)
#define __ZP_EMIT_TA(ctx, name, ...) __zp_emit_##name(ctx, __VA_ARGS__)
#define __ZP_EMIT_DISPATCH(_1, _2, _3, _4, NAME, ...) NAME

// type metadata

#define _ZP_FIELD_META(_struct, _type, _name) \
    {.name = #_name,                          \
     .type = &__zp_meta_##_type,              \
     .offset = offsetof(_struct, _name),      \
     .length = 0},

#define _ZP_ARRAY_META(_struct, _type, _name, _len) \
    {.name = #_name,                                \
     .type = &__zp_meta_##_type,                    \
     .offset = offsetof(_struct, _name),            \
     .length = _len},

#define _ZP_MAKE_TYPE_META(StructName, FieldsMacro)             \
    static const zp_field_meta_t __zp_fields_##StructName[] = { \
        FieldsMacro(StructName, _ZP_FIELD_META, _ZP_ARRAY_META) \
    };                                                          \
    static const zp_type_meta_t __zp_meta_##StructName = {      \
        .name = #StructName,                                    \
        .fields = __zp_fields_##StructName,                     \
        .size = sizeof(StructName),                             \
        .count = sizeof(__zp_fields_##StructName) /             \
                 sizeof(__zp_fields_##StructName[0]),           \
    };

#define _ZP_GEN_TYPE_META(TypesMacro) TypesMacro(_ZP_MAKE_TYPE_META)

#define _ZP_GENERATE_TYPES(TypesMacro) \
    _ZP_GEN_TYPE_DECL(TypesMacro)      \
    _ZP_GEN_TYPE_META(TypesMacro)

#define _ZP_DECLARE_ACTIONS(ActionsMacro) \
    _ZP_DECLARE_ACTION_IDS(ActionsMacro)  \
    _ZP_DECLARE_CALLBACKS(ActionsMacro)   \
    _ZP_DECLARE_SENDERS(ActionsMacro)

#define _ZP_REF_TYPE_META(StructName, ...) &__zp_meta_##StructName,

// pool helper

#define _zp_pool_get(ctx, ref) \
    ((void*)((ctx)->pool_buf + (ref) + sizeof(zp_pool_slot_t)))
