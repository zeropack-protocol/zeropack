#pragma once

#include <stddef.h>

typedef ptrdiff_t zp_ssize_t;

typedef struct {
    const void* buf;
    size_t len;
} zp_iovec_t;

typedef struct {
    int (*start)(void);  // Optional (NULL for event-driven / static transports)
    zp_ssize_t (*writev)(
        const zp_iovec_t* iov, int iovcnt
    );  // Required: scatter-gather transmission
    zp_ssize_t (*recv)(
        void* buf, size_t max_len
    );                    // Optional (only for polling/threaded transports)
    void (*close)(void);  // Optional
} zp_transport_t;

extern const zp_transport_t* zp_transport;
