#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "zeropack/action.h"
#include "zeropack/types.h"

typedef struct {
    const char* name;
    const zp_type_meta_t* type;
    uint16_t offset;
    uint16_t length;
} zp_field_meta_t;

struct zp_type_meta_s {
    const char* name;
    const zp_field_meta_t* fields;
    uint16_t size;
    uint8_t count;
};

typedef struct {
    const char* name;
    const zp_type_meta_t* type;
    uint8_t is_array;
} zp_action_meta_t;

typedef struct {
    const zp_type_meta_t* const* types;
    const zp_action_meta_t* actions;
    zp_dispatch_fn_t dispatch;
    uint8_t type_count;
    uint8_t action_count;
} zp_schema_t;

uint8_t zp_get_meta_id(const zp_schema_t* schema, const zp_type_meta_t* meta);
uint8_t zp_get_action_type_id(const zp_schema_t* schema, uint8_t action);
const zp_type_meta_t* zp_get_type_meta(
    const zp_schema_t* schema, uint8_t type_id
);
size_t zp_get_type_size(const zp_schema_t* schema, uint8_t type_id);
const char* zp_get_type_name(const zp_schema_t* schema, uint8_t type_id);

#define zp_type_id(schema, T) zp_get_meta_id((schema), zp_type_meta(T))

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

#define _ZP_MAKE_TYPE_META(StructName, FieldsMacro)               \
    static const zp_field_meta_t __zp_fields_##StructName[] = {   \
        FieldsMacro(StructName, _ZP_FIELD_META, _ZP_ARRAY_META)   \
    };                                                            \
    static const zp_type_meta_t __zp_meta_##StructName = {        \
        .name = #StructName,                                      \
        .fields = __zp_fields_##StructName,                       \
        .size = sizeof(StructName),                               \
        .count = sizeof(__zp_fields_##StructName) /               \
                 sizeof(__zp_fields_##StructName[0]),             \
    };

#define ZP_GEN_TYPE_META(TypesMacro) TypesMacro(_ZP_MAKE_TYPE_META)

#define ZP_GENERATE_TYPES(TypesMacro) \
    ZP_GEN_TYPE_DECL(TypesMacro)      \
    ZP_GEN_TYPE_META(TypesMacro)

#define ZP_DECLARE_ACTIONS(ActionsMacro) \
    ZP_DECLARE_ACTION_IDS(ActionsMacro)  \
    ZP_DECLARE_CALLBACKS(ActionsMacro)   \
    ZP_DECLARE_SENDERS(ActionsMacro)

#define ZP_DECLARE(TypesMacro, ActionsMacro) \
    ZP_GENERATE_TYPES(TypesMacro)            \
    ZP_DECLARE_ACTIONS(ActionsMacro)

#define _ZP_REF_TYPE_META(StructName, ...) &__zp_meta_##StructName,

// clang-format off
#define ZP_GENERATE_SCHEMA(Name, TypesMacro, ActionsMacro)                    \
    static const zp_action_meta_t Name##_actions[] = {                        \
        ActionsMacro(_ZP_ACTION_V, _ZP_ACTION_T, _ZP_ACTION_A)                \
    };                                                                        \
                                                                              \
    _Static_assert(                                                           \
        (sizeof(Name##_actions) / sizeof(Name##_actions[0])) <=               \
            ZP_SYS_ACTION_START,                                              \
        "Action limit exceeded"                                               \
    );                                                                        \
                                                                              \
    void (*__##Name##_callbacks[sizeof(Name##_actions) /                      \
                            sizeof(Name##_actions[0])])(void) = {0};          \
                                                                              \
    static bool Name##_dispatch(                                              \
        zp_ctx_t* ctx, const zp_header_t* hdr, const void* payload            \
    ) {                                                                       \
        (void)ctx; (void)hdr; (void)payload;                                  \
        void (** const __zp_callbacks)(void) = __##Name##_callbacks;          \
        switch (hdr->action) {                                                \
            ActionsMacro(ZP_DISPATCH_V, ZP_DISPATCH_T, ZP_DISPATCH_A)         \
            default:                                                          \
                return false;                                                 \
        }                                                                     \
    }                                                                         \
                                                                              \
    static const zp_type_meta_t* const Name##_types[] = {                     \
        TypesMacro(_ZP_REF_TYPE_META)                                         \
    };                                                                        \
                                                                              \
    _Static_assert(                                                           \
        (sizeof(Name##_types) / sizeof(Name##_types[0])) <=                   \
            (255 - TYPE_ID_START),                                            \
        "Struct type limit exceeded"                                          \
    );                                                                        \
                                                                              \
    const zp_schema_t Name = {                                                \
        .types = Name##_types,                                                \
        .type_count = sizeof(Name##_types) / sizeof(Name##_types[0]),         \
        .actions = Name##_actions,                                            \
        .action_count = sizeof(Name##_actions) / sizeof(Name##_actions[0]),   \
        .dispatch = Name##_dispatch,                                          \
    };
// clang-format on
