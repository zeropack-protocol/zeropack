#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

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

typedef struct zp_ctx_s zp_ctx_t;

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

#define _ZP_MAKE_FIELD(S, type, name) type name;
#define _ZP_MAKE_ARRAY(S, type, name, len) type name[len];
#define _ZP_DEFINE_TYPE(StructName, FieldsMacro)                \
    typedef struct StructName {                                 \
        FieldsMacro(StructName, _ZP_MAKE_FIELD, _ZP_MAKE_ARRAY) \
    } StructName;

#define ZP_GEN_TYPE_DECL(TypesMacro) TypesMacro(_ZP_DEFINE_TYPE)

typedef struct zp_type_meta_s zp_type_meta_t;

#define _ZP_DECL_PRIM_META(p) extern const zp_type_meta_t __zp_meta_##p;
ZP_PRIMITIVES(_ZP_DECL_PRIM_META)
#undef _ZP_DECL_PRIM_META

#define zp_type_meta(T) (&__zp_meta_##T)
#define zp_type_name(T) (__zp_meta_##T.name)
#define zp_type_size(T) (__zp_meta_##T.size)

