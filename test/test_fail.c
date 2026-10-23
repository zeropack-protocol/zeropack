#define ZP_IMPLEMENTATION
#include "zeropack.h"

#define FIELDS(S, F, A) F(S, u32, x)
#define APP_TYPES(X) X(foo_t, FIELDS)
ZP_TYPES(APP_TYPES)

// =============================================================================
#if defined(FAIL_DUPLICATE_ACTION)
// =============================================================================

#define APP_ACTIONS(V, T, A) \
    V(ACTION_PING)           \
    V(ACTION_PING)

ZP_SCHEMA(fail_schema, APP_TYPES, APP_ACTIONS)

// =============================================================================
#elif defined(FAIL_UNDECLARED_TYPE)
// =============================================================================

#define APP_ACTIONS(V, T, A) T(ACTION_REPORT, ghost_type_t)

ZP_SCHEMA(fail_schema, APP_TYPES, APP_ACTIONS)

#else
#error "Failure case must be specified with -D<NAME>"
#endif
