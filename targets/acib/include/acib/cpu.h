#ifndef ACIB_CPU_H
#define ACIB_CPU_H

#include <stdint.h>

#if defined(ACIB_HOST_TEST)

struct acib_host_cpu_state {
    uint32_t ipsr;
    uint32_t primask;
    uint32_t basepri;
    uint32_t faultmask;
    uint32_t control;
    uint32_t msp;
    uint32_t psp;
};

extern struct acib_host_cpu_state g_acib_host_cpu;

static inline uint32_t acib_cpu_get_ipsr(void)      { return g_acib_host_cpu.ipsr; }
static inline uint32_t acib_cpu_get_primask(void)   { return g_acib_host_cpu.primask; }
static inline void     acib_cpu_set_primask(uint32_t v) { g_acib_host_cpu.primask = v & 1u; }
static inline uint32_t acib_cpu_get_basepri(void)   { return g_acib_host_cpu.basepri; }
static inline void     acib_cpu_set_basepri(uint32_t v) { g_acib_host_cpu.basepri = v & 0xFFu; }
static inline void     acib_cpu_cpsid_i(void)       { g_acib_host_cpu.primask = 1u; }
static inline void     acib_cpu_cpsie_i(void)       { g_acib_host_cpu.primask = 0u; }
static inline void     acib_cpu_dmb(void)           {}
static inline void     acib_cpu_isb(void)           {}

#else

static inline uint32_t acib_cpu_get_ipsr(void)
{
    uint32_t v;
    __asm__ volatile("mrs %0, ipsr" : "=r"(v));
    return v;
}

static inline uint32_t acib_cpu_get_primask(void)
{
    uint32_t v;
    __asm__ volatile("mrs %0, primask" : "=r"(v));
    return v;
}

static inline void acib_cpu_set_primask(uint32_t v)
{
    __asm__ volatile("msr primask, %0" :: "r"(v) : "memory");
}

static inline uint32_t acib_cpu_get_basepri(void)
{
    uint32_t v;
    __asm__ volatile("mrs %0, basepri" : "=r"(v));
    return v;
}

static inline void acib_cpu_set_basepri(uint32_t v)
{
    __asm__ volatile("msr basepri, %0" :: "r"(v) : "memory");
}

static inline void acib_cpu_cpsid_i(void)
{
    __asm__ volatile("cpsid i" ::: "memory");
}

static inline void acib_cpu_cpsie_i(void)
{
    __asm__ volatile("cpsie i" ::: "memory");
}

static inline void acib_cpu_dmb(void)
{
    __asm__ volatile("dmb sy" ::: "memory");
}

static inline void acib_cpu_isb(void)
{
    __asm__ volatile("isb sy" ::: "memory");
}

#endif /* ACIB_HOST_TEST */

static inline uint32_t acib_clz32(uint32_t v)
{
    return (v == 0u) ? 32u : (uint32_t)__builtin_clz(v);
}

#endif /* ACIB_CPU_H */
