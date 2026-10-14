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
#define ZP_PRIMITIVES(X) \
    X(u8) X(u16) X(u32) X(u64) \
    X(s8) X(s16) X(s32) X(s64) \
    X(f32) X(f64) X(bool) X(poolref_t)
// clang-format on

#define GEN_PRIM_TYPE_ID(name) TYPE_ID_##name,
#define GEN_STRUCT_TYPE_ID(name, fields) TYPE_ID_##name,
typedef enum {
    TYPE_ID_NONE = 0,
    ZP_PRIMITIVES(GEN_PRIM_TYPE_ID) __PRIM_TYPE_END,
} primtype_id_t;

#define ZP_DECLARE_TYPE_IDS(TypesMacro)             \
    typedef enum {                                  \
        TYPE_ID_START = __PRIM_TYPE_END,            \
        TypesMacro(GEN_STRUCT_TYPE_ID) TYPE_ID_MAX, \
    } type_id_t;

#define MAKE_FIELD(S, type, name) type name;
#define MAKE_ARRAY(S, type, name, len) type name[len];
#define DEFINE_TYPE(StructName, FieldsMacro)            \
    typedef struct StructName {                         \
        FieldsMacro(StructName, MAKE_FIELD, MAKE_ARRAY) \
    } StructName;

#define ZP_DECLARE_STRUCTS(TypesMacro) TypesMacro(DEFINE_TYPE)

const char* get_type_name(uint8_t type_id);
size_t get_type_size(uint8_t type_id);
