#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "zeropack/action.h"
#include "zeropack/types.h"

typedef struct {
    uint8_t type;
    uint16_t offset;
    uint16_t length;
    const char* name;
} field_desc_t;

typedef struct {
    uint8_t id;
    uint8_t count;
    uint16_t size;
    const char* name;
    const field_desc_t* fields;
} struct_desc_t;

typedef struct {
    uint8_t type;
    uint8_t is_array;
    const char* name;
} action_desc_t;

typedef struct {
    const struct_desc_t* types;
    uint16_t type_count;
    const action_desc_t* actions;
    uint16_t action_count;
    zp_dispatch_fn_t dispatch;
} zp_schema_t;

const char* get_type_name(const zp_schema_t* schema, uint8_t type_id);
size_t get_type_size(const zp_schema_t* schema, uint8_t type_id);

#define FIELD_DESC(S, type, name) {TYPE_ID_##type, offsetof(S, name), 0, #name},
#define ARRAY_DESC(S, type, name, len) \
    {TYPE_ID_##type, offsetof(S, name), len, #name},

#define MAKE_STRUCT_META(StructName, FieldsMacro)                 \
    {.id = TYPE_ID_##StructName,                                  \
     .size = sizeof(StructName),                                  \
     .count = sizeof((const field_desc_t[]){                      \
                  FieldsMacro(StructName, FIELD_DESC, ARRAY_DESC) \
              }) /                                                \
              sizeof(field_desc_t),                               \
     .name = #StructName,                                         \
     .fields = (const field_desc_t[]){                            \
         FieldsMacro(StructName, FIELD_DESC, ARRAY_DESC)          \
     }},

#define ZP_DECLARE(TypesMacro, ActionsMacro) \
    ZP_DECLARE_STRUCTS(TypesMacro)           \
    ZP_DECLARE_TYPE_IDS(TypesMacro)          \
    ZP_DECLARE_ACTION_IDS(ActionsMacro)      \
    ZP_DECLARE_CALLBACKS(ActionsMacro)       \
    ZP_DECLARE_SENDERS(ActionsMacro)

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
        const zp_header_t* hdr, const uint8_t* payload, size_t len            \
    ) {                                                                       \
        void (** const __zp_callbacks)(void) = __##Name##_callbacks;          \
        switch (hdr->action) {                                                \
            ActionsMacro(ZP_DISPATCH_V, ZP_DISPATCH_T, ZP_DISPATCH_A)         \
            default:                                                          \
                return false;                                                 \
        }                                                                     \
    }                                                                         \
                                                                              \
    static const struct_desc_t Name##_types[] = {                             \
        TypesMacro(MAKE_STRUCT_META)                                          \
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
