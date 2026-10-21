#include "zeropack.h"

#define ZP_DEF_PRIM_META(p)                                                 \
    const zp_type_meta_t __zp_meta_##p = {                                  \
        .name = #p, .fields = NULL, .size = sizeof(p), .count = TYPE_ID_##p \
    };
ZP_PRIMITIVES(ZP_DEF_PRIM_META)
#undef ZP_DEF_PRIM_META

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

const zp_type_meta_t* zp_get_type_meta(
    const zp_schema_t* schema, uint8_t type_id
) {
#define SW_PRIM(p)    \
    case TYPE_ID_##p: \
        return zp_metaof(p);

    if (type_id == 0) return NULL;
    if (type_id < TYPE_ID_START) {
        switch (type_id) {
            ZP_PRIMITIVES(SW_PRIM)
            default:
                return NULL;
        }
    }
    if (!schema) return NULL;

    uint8_t index = type_id - TYPE_ID_START;
    return (index < schema->type_count) ? schema->types[index] : NULL;

#undef SW_PRIM
}

size_t zp_get_type_size(const zp_schema_t* schema, uint8_t type_id) {
    const zp_type_meta_t* meta = zp_get_type_meta(schema, type_id);
    return meta ? meta->size : 0;
}
