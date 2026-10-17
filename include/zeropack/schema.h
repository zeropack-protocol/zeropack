#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "zeropack/action.h"
#include "zeropack/types.h"

typedef struct {
    const char* name;
    const struct_desc_t* type;
    uint16_t offset;
    uint16_t length;
} field_desc_t;

struct struct_desc_s {
    const char* name;
    const field_desc_t* fields;
    uint16_t size;
    uint8_t count;
};

typedef struct {
    const char* name;
    const struct_desc_t* type;
    uint8_t is_array;
} action_desc_t;

typedef struct {
    const struct_desc_t* const* types;
    const action_desc_t* actions;
    zp_dispatch_fn_t dispatch;
    uint16_t type_count;
    uint16_t action_count;
} zp_schema_t;

uint8_t zp_get_meta_id(const zp_schema_t* schema, const struct_desc_t* meta);
uint8_t zp_get_action_type_id(const zp_schema_t* schema, uint8_t action);
const char* get_type_name(const zp_schema_t* schema, uint8_t type_id);
size_t get_type_size(const zp_schema_t* schema, uint8_t type_id);

#define FIELD_DESC(_struct, _type, _name) \
    {.name = #_name,                      \
     .type = &__zp_meta_##_type,          \
     .offset = offsetof(_struct, _name),  \
     .length = 0},
#define ARRAY_DESC(_struct, _type, _name, _len) \
    {.name = #_name,                            \
     .type = &__zp_meta_##_type,                \
     .offset = offsetof(_struct, _name),        \
     .length = _len},

#define MAKE_STRUCT_META(StructName, FieldsMacro)            \
    static const field_desc_t __zp_fields_##StructName[] = { \
        FieldsMacro(StructName, FIELD_DESC, ARRAY_DESC)      \
    };                                                       \
    static const struct_desc_t __zp_meta_##StructName = {    \
        .name = #StructName,                                 \
        .fields = __zp_fields_##StructName,                  \
        .size = sizeof(StructName),                          \
        .count = sizeof(__zp_fields_##StructName) /          \
                 sizeof(__zp_fields_##StructName[0]),        \
    };

#define ZP_GEN_TYPE_META(TypesMacro) TypesMacro(MAKE_STRUCT_META)

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

#define REF_STRUCT_META(StructName, ...) &__zp_meta_##StructName,

// clang-format off
#define ZP_GENERATE_SCHEMA(Name, TypesMacro, ActionsMacro)                    \
    static const action_desc_t Name##_actions[] = {                           \
        ActionsMacro(ACTION_V, ACTION_T, ACTION_A)                            \
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
        zp_ctx_t* ctx, const zp_header_t* hdr, const uint8_t* payload, size_t len \
    ) {                                                                       \
        (void)ctx; (void)hdr; (void)payload; (void)len;                       \
        void (** const __zp_callbacks)(void) = __##Name##_callbacks;          \
        switch (hdr->action) {                                                \
            ActionsMacro(ZP_DISPATCH_V, ZP_DISPATCH_T, ZP_DISPATCH_A)         \
            default:                                                          \
                return false;                                                 \
        }                                                                     \
    }                                                                         \
                                                                              \
    static const struct_desc_t* const Name##_types[] = {                             \
        TypesMacro(REF_STRUCT_META)                                           \
    };                                                                        \
                                                                              \
    const zp_schema_t Name = {                                                \
        .types = Name##_types,                                                \
        .type_count = sizeof(Name##_types) / sizeof(Name##_types[0]),         \
        .actions = Name##_actions,                                            \
        .action_count = sizeof(Name##_actions) / sizeof(Name##_actions[0]),   \
        .dispatch = Name##_dispatch,                                          \
    };
// clang-format on
