#include "zeropack/types.h"

#include <stdint.h>

#include "zeropack/schema.h"

#define ZP_DEF_PRIM_META(p)                                           \
    const struct_desc_t __zp_meta_##p = {                             \
        .name = #p, .fields = NULL, .size = sizeof(p), .count = TYPE_ID_##p \
    };
ZP_PRIMITIVES(ZP_DEF_PRIM_META)

#define ZP_PRIM_SIZE(type) sizeof(type),
static const uint8_t s_prim_sizes[] = {0, ZP_PRIMITIVES(ZP_PRIM_SIZE)};

#define ZP_PRIM_NAME(type) #type,
static const char* const s_prim_names[] = {"none", ZP_PRIMITIVES(ZP_PRIM_NAME)};

uint8_t zp_get_meta_id(const zp_schema_t* schema, const struct_desc_t* meta) {
    if (!meta) return TYPE_ID_NONE;

    if (!meta->fields) {
        return meta->count; // For primitives, count holds TYPE_ID_##p
    }

    if (schema) {
        for (uint8_t i = 0; i < schema->type_count; i++) {
            if (schema->types[i] == meta) {
                return (uint8_t)(TYPE_ID_START + i);
            }
        }
    }
    return TYPE_ID_NONE;
}

uint8_t zp_get_action_type_id(const zp_schema_t* schema, uint8_t action) {
    if (!schema || action >= schema->action_count) return TYPE_ID_NONE;
    return zp_get_meta_id(schema, schema->actions[action].type);
}

size_t get_type_size(const zp_schema_t* schema, uint8_t type_id) {
    if (type_id == 0) return 0;
    if (type_id < sizeof(s_prim_sizes)) return s_prim_sizes[type_id];
    if (!schema) return 0;

    uint8_t index = type_id - TYPE_ID_START;
    if (index < schema->type_count && schema->types[index]) {
        return schema->types[index]->size;
    }

    return 0;
}

const char* get_type_name(const zp_schema_t* schema, uint8_t type_id) {
    if (type_id == 0) return "none";
    if (type_id < sizeof(s_prim_sizes)) return s_prim_names[type_id];
    if (!schema) return "unknown";

    uint8_t index = type_id - TYPE_ID_START;
    if (index < schema->type_count && schema->types[index]) {
        return schema->types[index]->name;
    }

    return "unknown";
}
