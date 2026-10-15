# ZeroPack

[![Standard](https://img.shields.io/badge/standard-ISO%20C11-blue.svg)](#)
[![Environment](https://img.shields.io/badge/environment-freestanding-success.svg)](#)
[![Allocations](https://img.shields.io/badge/heap%20allocations-zero-orange.svg)](#)
[![License](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)

ZeroPack is a freestanding binary communication protocol and wire format for embedded systems.

It features zero dynamic heap allocations, compile-time schema reflection using ISO C11 macros (no external code generator CLI), and an optional binary handshake for runtime schema discovery.

---

## Features

- **No Heap Allocations:** Never calls `malloc` or `free`. Operates on static buffers and caller memory.
- **Freestanding C11:** Zero libc, POSIX, or OS dependencies. Runs on bare-metal microcontrollers (ARM Cortex-M, RISC-V), RTOS (FreeRTOS, Zephyr), and host OSes.
- **No Codegen CLI:** Structs, type IDs, and dispatch tables are generated at compile time using standard C macros.
- **Optional Handshake:** Announce struct layouts and action descriptors dynamically across the wire, or skip entirely for fixed-schema links.
- **Transport Agnostic:** Transmits via a `writev` callback over UART, SPI, I2C, BLE, TCP, or shared memory.

---

## Wire Format

ZeroPack differentiates between standard data packets (`zp_header_t`) and the initial type-negotiation handshake (`zp_handshake_t`).

### Data Packet Header (`zp_header_t` &mdash; 4 bytes)

Every application message or event begins with a fixed 4-byte header:

```text
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|   Action ID   |    Type ID    |         Count (uint16)        |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                                                               |
|             Payload Data (Aligned to 8-byte boundary)         |
|                                                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

- **Action ID (`uint8_t`):** Application event or system command identifier (IDs `0xF0`–`0xFF` reserved for internal system actions).
- **Type ID (`uint8_t`):** Identifier mapping to the primitive type or struct definition.
- **Count (`uint16_t`):** Element count (0 or 1 for single structs, $N$ for array payloads).
- **Payload Data:** Raw, contiguous binary struct memory aligned to an 8-byte boundary.

### Handshake Header (`zp_handshake_t` &mdash; 10 bytes)

When negotiating schemas dynamically, the self-describing handshake frame begins with:

```text
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|   Magic ('Z') |B|  Reserved   |          Pool Size            |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|          Type Count           |         Action Count          |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|        Pool Item Count        |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

- **Magic (`uint8_t`):** Handshake identifier (`'Z'` / `0x5A`).
- **Byte Order (`B`, 1 bit):** Endianness flag (`0` = Little Endian, `1` = Big Endian).
- **Pool Size (`uint16_t`):** Configured shared memory pool size.
- **Type Count (`uint16_t`):** Number of struct type definitions following the header.
- **Action Count (`uint16_t`):** Number of action descriptors following the header.
- **Pool Item Count (`uint16_t`):** Initial number of items stored in the shared pool.

---

## Quickstart

### 1. Configuration (`zp.config.h`)

Create a `zp.config.h` in your project's include path to configure static buffer sizes:

```c
#pragma once

#define ZP_POOL_SIZE    1024  // Memory pool size in bytes (set to 0 to disable)
#define ZP_BUFFER_SIZE  2048  // RX stream assembly buffer size in bytes
```

### 2. Define Types and Actions

Declare your application schema using standard C macros:

```c
#include "zeropack.h"

// Define custom structs and their fields
#define APP_TYPES(X) \
    X(SensorReport, SENSOR_FIELDS)

#define SENSOR_FIELDS(S, F, A) \
    F(S, f32, temperature)     \
    F(S, f32, humidity)        \
    F(S, u32, timestamp)

// Define actions: V = Void, T = Single Struct, A = Array of Structs
#define APP_ACTIONS(V, T, A) \
    V(ACTION_PING)           \
    V(ACTION_PONG)           \
    T(ACTION_REPORT, SensorReport)

// Declare type IDs, struct layouts, and inline senders
ZP_DECLARE(APP_TYPES, APP_ACTIONS)

// Generate runtime schema metadata and dispatcher (invoke once in main translation unit)
ZP_GENERATE_SCHEMA(APP_TYPES, APP_ACTIONS)
```

### 3. Implement Transport

ZeroPack requires only a simple scatter-gather `writev` implementation:

```c
static zp_ssize_t my_uart_writev(const zp_iovec_t* iov, int iovcnt) {
    zp_ssize_t total = 0;
    for (int i = 0; i < iovcnt; i++) {
        uart_send_blocking(iov[i].buf, iov[i].len);
        total += iov[i].len;
    }
    return total;
}

static const zp_transport_t my_transport = {
    .writev = my_uart_writev,
};

const zp_transport_t* zp_transport = &my_transport;
```

### 4. Register Event Callbacks & Emit Actions

```c
static void on_report(const SensorReport* report) {
    printf("Received: Temp=%.2f C, Humidity=%.2f %%\n",
           report->temperature, report->humidity);
}

int main(void) {
    // Register callback for incoming ACTION_REPORT events
    zp_on_action(ACTION_REPORT, on_report);

    // Emit an event over the wire
    SensorReport data = {
        .temperature = 24.5f,
        .humidity = 60.2f,
        .timestamp = 12000,
    };
    zp_emit(ACTION_REPORT, &data);

    // Emit void actions with zero payload
    zp_emit(ACTION_PING);

    return 0;
}
```

### 5. Feed Ingress Bytes

Feed incoming bytes from UART DMA, interrupts, or socket `recv()` directly into the parser state machine:

```c
void on_uart_rx_chunk(const uint8_t* buffer, size_t length) {
    zp_process_rx_bytes(buffer, length);
}
```

---

## Comparison Matrix

| Feature | ZeroPack | Protocol Buffers (NanoPB) | MessagePack | FlatBuffers | CBOR |
| :--- | :---: | :---: | :---: | :---: | :---: |
| **Freestanding C11** | **Yes** | Limited | Requires libc | C++ Only | Requires libc |
| **Heap Allocations** | **Zero (0)** | Arena / Stack | Dynamic | Zero (0) | Dynamic |
| **External Codegen CLI** | **None (Pure C)** | Required (`protoc`) | None | Required (`flatc`) | None |
| **Self-Describing Schema** | **Yes (Handshake)** | No (Static `.proto`) | Partial (Types only) | No (Static `.fbs`) | Partial |
| **Wire Overhead** | **4 bytes** | Variable | 1–5 bytes/field | 4–8 bytes offset | 1–5 bytes/field |
| **Zero-Copy Deserialization** | **Direct Cast** | Requires Unpack | Requires Unpack | VTable pointer | Requires Unpack |

---

## Building and Running the Example

A self-contained loopback example demonstrating action emission, typed callbacks, and binary handshake transmission is provided under `examples/`.

Build and run using `make`:

```bash
make example
```

Output:
```text
=== ZeroPack Protocol Minimal Loopback Example ===

[Step 1] Emitting ACTION_PING...
--> Transport: Transmitting 4 bytes [ACTION_PING]...
  [Callback] Handled ACTION_PING (void action)

[Step 2] Emitting ACTION_REPORT (SensorReport payload)...
--> Transport: Transmitting 16 bytes [ACTION_REPORT]...
  [Callback] Handled ACTION_REPORT:
    Temperature : 23.45 C
    Humidity    : 58.20 %
    Timestamp   : 42000 ms

[Step 3] Emitting self-describing binary handshake...
  Handshake binary size: 117 bytes (contains complete schema & field definitions)

=== Execution completed successfully! ===
```

---

## License

This project is licensed under the [MIT License](LICENSE).
