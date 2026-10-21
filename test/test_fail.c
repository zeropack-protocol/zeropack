#include "zeropack.h"

#define FIELDS(S, F, A) F(S, u32, x)
#define APP_TYPES(X) X(foo_t, FIELDS)
ZP_TYPES(APP_TYPES)

// =============================================================================
#if defined(FAIL_UNDECLARED_ACTION)
// =============================================================================

#define APP_ACTIONS(V, T, A) V(ACTION_UNDECLARED)
// ZP_ACTIONS(APP_ACTIONS) was never called
ZP_SCHEMA(fail_schema, APP_TYPES, APP_ACTIONS)

// =============================================================================
#elif defined(FAIL_DUPLICATE_ACTION_ID)
// =============================================================================

// motor.h
#define MOTOR_ACTIONS(V, T, A) V(ACTION_MOTOR_START)
ZP_ACTIONS(MOTOR_ACTIONS)

// config.h
#define CONFIG_ACTIONS(V, T, A) T(ACTION_CONFIG_WIFI, u8)
ZP_ACTIONS(CONFIG_ACTIONS)

// schema.h
// both ACTION_MOTOR_START and ACTION_CONFIG_WIFI have ID 0
// since both were created independently with ZP_ACTIONS
#define APP_ACTIONS(V, T, A) \
    MOTOR_ACTIONS(V, T, A)   \
    CONFIG_ACTIONS(V, T, A)

// ZP_SCHEMA should fail now
ZP_SCHEMA(fail_schema, APP_TYPES, APP_ACTIONS)
#else
#error "Failure case must be specified with -D<NAME>"
#endif
