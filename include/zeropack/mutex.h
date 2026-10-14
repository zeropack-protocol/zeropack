#pragma once

typedef struct zp_mutex zp_mutex_t;

void zp_mutex_lock(zp_mutex_t* m);
void zp_mutex_unlock(zp_mutex_t* m);

#define ZP_LOCK(name) zp_mutex_lock(zp_mutex_##name)
#define ZP_UNLOCK(name) zp_mutex_unlock(zp_mutex_##name)

extern zp_mutex_t* zp_mutex_tx;
extern zp_mutex_t* zp_mutex_pool;
