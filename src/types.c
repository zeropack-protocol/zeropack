#include "zeropack.h"

#define ZP_DEF_PRIM_META(p)                                                 \
    const zp_type_meta_t __zp_meta_##p = {                                  \
        .name = #p, .fields = NULL, .size = sizeof(p), .count = TYPE_ID_##p \
    };
ZP_PRIMITIVES(ZP_DEF_PRIM_META)
#undef ZP_DEF_PRIM_META

#define ZP_REF_PRIM_META(p) &__zp_meta_##p,
static const zp_type_meta_t* const s_prim_meta[] = {
    NULL, ZP_PRIMITIVES(ZP_REF_PRIM_META)
};
#undef ZP_REF_PRIM_META

uint8_t zp_get_meta_id(const zp_schema_t* schema, const zp_type_meta_t* meta) {
    if (!meta) return TYPE_ID_NONE;

    if (!meta->fields) {
        return meta->count;  // For primitives, count holds TYPE_ID_##p
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

const zp_type_meta_t* zp_get_type_meta(
    const zp_schema_t* schema, uint8_t type_id
) {
    if (type_id == 0) return NULL;
    if (type_id < TYPE_ID_START) return s_prim_meta[type_id];
    if (!schema) return NULL;

    uint8_t index = type_id - TYPE_ID_START;
    return (index < schema->type_count) ? schema->types[index] : NULL;
}

size_t zp_get_type_size(const zp_schema_t* schema, uint8_t type_id) {
    const zp_type_meta_t* meta = zp_get_type_meta(schema, type_id);
    return meta ? meta->size : 0;
}

const char* zp_get_type_name(const zp_schema_t* schema, uint8_t type_id) {
    if (type_id == 0) return "none";
    const zp_type_meta_t* meta = zp_get_type_meta(schema, type_id);
    return meta ? meta->name : "unknown";
}
