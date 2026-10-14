#include "zeropack/types.h"

#include "zeropack/schema.h"

#define ZP_PRIM_SIZE(type) sizeof(type),
static const uint8_t s_prim_sizes[] = {0, ZP_PRIMITIVES(ZP_PRIM_SIZE)};

#define ZP_PRIM_NAME(type) #type,
static const char* const s_prim_names[] = {"none", ZP_PRIMITIVES(ZP_PRIM_NAME)};

size_t get_type_size(uint8_t type_id) {
    if (type_id == 0) return 0;
    if (type_id < sizeof(s_prim_sizes)) return s_prim_sizes[type_id];

    for (size_t i = 0; i < zp_schema.type_count; i++) {
        if (zp_schema.types[i].id == type_id) return zp_schema.types[i].size;
    }

    return 0;
}

const char* get_type_name(uint8_t type_id) {
    if (type_id == 0) return "none";
    if (type_id < sizeof(s_prim_sizes)) return s_prim_names[type_id];

    for (size_t i = 0; i < zp_schema.type_count; i++) {
        if (zp_schema.types[i].id == type_id) return zp_schema.types[i].name;
    }

    return "unknown";
}
