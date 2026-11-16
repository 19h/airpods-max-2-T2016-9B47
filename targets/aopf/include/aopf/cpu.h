/*
 * cpu.h — Portable ARMv7E-M special-register intrinsics and VMA pointer
 * translation for freestanding ARM and native host builds.
 */

#ifndef AOPF_CPU_H
#define AOPF_CPU_H

#include <stdint.h>
#include <stddef.h>

#if defined(__arm__) || defined(__thumb__)

static inline uint32_t aopf_get_primask(void)
{
    uint32_t reg_val;
    __asm__ volatile("mrs %0, primask" : "=r"(reg_val));
    return reg_val;
}

static inline void aopf_set_primask(uint32_t reg_val)
{
    __asm__ volatile("msr primask, %0" :: "r"(reg_val) : "memory");
}

static inline void aopf_cpsid_i(void)
{
    __asm__ volatile("cpsid i" ::: "memory");
}

static inline void aopf_cpsie_i(void)
{
    __asm__ volatile("cpsie i" ::: "memory");
}

static inline uint32_t aopf_get_basepri(void)
{
    uint32_t reg_val;
    __asm__ volatile("mrs %0, basepri" : "=r"(reg_val));
    return reg_val;
}

static inline void aopf_set_basepri(uint32_t reg_val)
{
    __asm__ volatile("msr basepri, %0" :: "r"(reg_val) : "memory");
}

static inline uint32_t aopf_get_ipsr(void)
{
    uint32_t reg_val;
    __asm__ volatile("mrs %0, ipsr" : "=r"(reg_val));
    return reg_val;
}

static inline uint32_t aopf_get_msp(void)
{
    uint32_t reg_val;
    __asm__ volatile("mrs %0, msp" : "=r"(reg_val));
    return reg_val;
}

static inline void aopf_dsb(void)
{
    __asm__ volatile("dsb sy" ::: "memory");
}

static inline void aopf_isb(void)
{
    __asm__ volatile("isb sy" ::: "memory");
}

static inline void aopf_wfi(void)
{
    __asm__ volatile("wfi" ::: "memory");
}

#define AOPF_VMA_PTR(type, vma_addr) ((type)(uintptr_t)(vma_addr))

#else /* Host test environment */

uint32_t aopf_host_get_primask(void);
void aopf_host_set_primask(uint32_t reg_val);
uint32_t aopf_host_get_basepri(void);
void aopf_host_set_basepri(uint32_t reg_val);
uint32_t aopf_host_get_ipsr(void);
uint32_t aopf_host_get_msp(void);
void *aopf_host_translate_vma(uint32_t vma_addr);

static inline uint32_t aopf_get_primask(void) { return aopf_host_get_primask(); }
static inline void aopf_set_primask(uint32_t reg_val) { aopf_host_set_primask(reg_val); }
static inline void aopf_cpsid_i(void) { aopf_host_set_primask(1u); }
static inline void aopf_cpsie_i(void) { aopf_host_set_primask(0u); }
static inline uint32_t aopf_get_basepri(void) { return aopf_host_get_basepri(); }
static inline void aopf_set_basepri(uint32_t reg_val) { aopf_host_set_basepri(reg_val); }
static inline uint32_t aopf_get_ipsr(void) { return aopf_host_get_ipsr(); }
static inline uint32_t aopf_get_msp(void) { return aopf_host_get_msp(); }
static inline void aopf_dsb(void) {}
static inline void aopf_isb(void) {}
static inline void aopf_wfi(void) {}

#define AOPF_VMA_PTR(type, vma_addr) ((type)aopf_host_translate_vma((uint32_t)(vma_addr)))

#endif

static inline uint32_t aopf_bswap32(uint32_t word_val)
{
    return __builtin_bswap32(word_val);
}

static inline uint32_t aopf_clz32(uint32_t word_val)
{
    return word_val == 0u ? 32u : (uint32_t)__builtin_clz(word_val);
}

#endif /* AOPF_CPU_H */
