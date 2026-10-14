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
#define ZP_GENERATE_SCHEMA(TypesMacro, ActionsMacro)                    \
    void (*__zp_callbacks[ACTION_ID_MAX])(void) = {0};                  \
                                                                        \
    static bool __zp_dispatch(                                          \
        const zp_header_t* hdr, const uint8_t* payload, size_t len      \
    ) {                                                                 \
        switch (hdr->action) {                                          \
            ActionsMacro(ZP_DISPATCH_V, ZP_DISPATCH_T, ZP_DISPATCH_A)   \
            default:                                                    \
                return false;                                           \
            }                                                           \
    }                                                                   \
                                                                        \
    static const struct_desc_t __zp_types[] = {                         \
      TypesMacro(MAKE_STRUCT_META)                                      \
    };                                                                  \
    static const action_desc_t __zp_actions[] = {                       \
        ActionsMacro(ACTION_V, ACTION_T, ACTION_A)                      \
    };                                                                  \
                                                                        \
    const zp_schema_t zp_schema = {                                     \
        .types = __zp_types,                                            \
        .type_count = sizeof(__zp_types) / sizeof(__zp_types[0]),       \
        .actions = __zp_actions,                                        \
        .action_count = sizeof(__zp_actions) / sizeof(__zp_actions[0]), \
        .dispatch = __zp_dispatch,                                      \
    };
// clang-format on

// Instancia global del esquema
// Se declara como 'extern' para que otros modulos puedan consultar
// la tabla de acciones y tipos sin duplicar símbolos
//
// ZP_GENERATE_SCHEMA debe ser llamado una sola vez en el archivo principal
extern const zp_schema_t zp_schema;
