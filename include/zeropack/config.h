#pragma once

#include <stddef.h>

/*
 * Abstracción de memoria freestanding:
 * Si el compilador soporta built-ins (GCC / Clang), los usamos directamente.
 * Si no (MSVC u otros compiladores antiguos), declaramos externamente las
 * funciones sin necesidad de arrastrar <string.h>.
 */
#if defined(__has_builtin)
#if __has_builtin(__builtin_memcpy)
#define zp_memcpy __builtin_memcpy
#endif
#if __has_builtin(__builtin_memmove)
#define zp_memmove __builtin_memmove
#endif
#if __has_builtin(__builtin_strlen)
#define zp_strlen __builtin_strlen
#endif
#elif defined(__GNUC__)
#define zp_memcpy __builtin_memcpy
#define zp_memmove __builtin_memmove
#define zp_strlen __builtin_strlen
#endif

#ifndef zp_memcpy
extern void* memcpy(void* dest, const void* src, size_t n);
#define zp_memcpy memcpy
#endif

#ifndef zp_memmove
extern void* memmove(void* dest, const void* src, size_t n);
#define zp_memmove memmove
#endif

#ifndef zp_strlen
extern size_t strlen(const char* s);
#define zp_strlen strlen
#endif
