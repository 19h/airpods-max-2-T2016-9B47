#ifndef ACIB_ACIB_H
#define ACIB_ACIB_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if defined(ACIB_HOST_TEST)
#define ACIB_SECTION(name)
#define ACIB_USED
#define ACIB_NAKED
#define ACIB_NORETURN
#define ACIB_WEAK
#else
#define ACIB_SECTION(name) __attribute__((section(name)))
#define ACIB_USED          __attribute__((used))
#define ACIB_NAKED         __attribute__((naked))
#define ACIB_NORETURN      __attribute__((noreturn))
#define ACIB_WEAK          __attribute__((weak))
#endif

#define ACIB_PACKED        __attribute__((packed))
#define ACIB_ALIGNED(n)    __attribute__((aligned(n)))
#define ACIB_ARRAY_LEN(a)  (sizeof(a) / sizeof((a)[0]))

#endif /* ACIB_ACIB_H */
