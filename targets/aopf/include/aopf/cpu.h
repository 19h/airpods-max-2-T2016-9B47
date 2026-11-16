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

#define AOPF_NOINLINE __attribute__((noinline))

/*
 * Alignment-safe, strict-aliasing-clean memory accessors.
 * Under clang -Oz on ARMv7E-M (UNALIGN_TRP=0), __builtin_memcpy of 1/2/4/8
 * bytes lowers to a single Thumb-2 ldrb/ldrh/ldr/ldrd/vldr or str* instruction.
 */
static inline uint8_t aopf_rd_u8(const void *base, size_t byte_off)
{
    return *((const uint8_t *)base + byte_off);
}

static inline int8_t aopf_rd_i8(const void *base, size_t byte_off)
{
    return (int8_t)*((const uint8_t *)base + byte_off);
}

static inline uint16_t aopf_rd_u16(const void *base, size_t byte_off)
{
    uint16_t out_val;
    __builtin_memcpy(&out_val, (const uint8_t *)base + byte_off, sizeof(out_val));
    return out_val;
}

static inline int16_t aopf_rd_i16(const void *base, size_t byte_off)
{
    int16_t out_val;
    __builtin_memcpy(&out_val, (const uint8_t *)base + byte_off, sizeof(out_val));
    return out_val;
}

static inline uint32_t aopf_rd_u32(const void *base, size_t byte_off)
{
    uint32_t out_val;
    __builtin_memcpy(&out_val, (const uint8_t *)base + byte_off, sizeof(out_val));
    return out_val;
}

static inline int32_t aopf_rd_i32(const void *base, size_t byte_off)
{
    int32_t out_val;
    __builtin_memcpy(&out_val, (const uint8_t *)base + byte_off, sizeof(out_val));
    return out_val;
}

static inline uint64_t aopf_rd_u64(const void *base, size_t byte_off)
{
    uint64_t out_val;
    __builtin_memcpy(&out_val, (const uint8_t *)base + byte_off, sizeof(out_val));
    return out_val;
}

static inline float aopf_rd_f32(const void *base, size_t byte_off)
{
    float out_val;
    __builtin_memcpy(&out_val, (const uint8_t *)base + byte_off, sizeof(out_val));
    return out_val;
}

static inline void aopf_wr_u8(void *base, size_t byte_off, uint8_t val)
{
    *((uint8_t *)base + byte_off) = val;
}

static inline void aopf_wr_u16(void *base, size_t byte_off, uint16_t val)
{
    __builtin_memcpy((uint8_t *)base + byte_off, &val, sizeof(val));
}

static inline void aopf_wr_u32(void *base, size_t byte_off, uint32_t val)
{
    __builtin_memcpy((uint8_t *)base + byte_off, &val, sizeof(val));
}

static inline void aopf_wr_u64(void *base, size_t byte_off, uint64_t val)
{
    __builtin_memcpy((uint8_t *)base + byte_off, &val, sizeof(val));
}

static inline void aopf_wr_f32(void *base, size_t byte_off, float val)
{
    __builtin_memcpy((uint8_t *)base + byte_off, &val, sizeof(val));
}

static inline uint32_t aopf_f32_to_bits(float val)
{
    uint32_t bits;
    __builtin_memcpy(&bits, &val, sizeof(bits));
    return bits;
}

static inline float aopf_f32_from_bits(uint32_t bits)
{
    float val;
    __builtin_memcpy(&val, &bits, sizeof(val));
    return val;
}

static inline float aopf_fabsf(float val)
{
    return __builtin_fabsf(val);
}

static inline float aopf_sqrtf(float val)
{
#if defined(__arm__) || defined(__thumb__)
    float res;
    __asm__("vsqrt.f32 %0, %1" : "=t"(res) : "t"(val));
    return res;
#else
    return __builtin_sqrtf(val);
#endif
}

static inline void aopf_dmb_sy(void)
{
#if defined(__arm__) || defined(__thumb__)
    __asm__ volatile("dmb sy" ::: "memory");
#endif
}

/*
 * Portable Embedded C++ / RTKit C virtual & indirect Thumb function dispatch.
 * On ARMv7E-M, emits direct blx to the 32-bit Thumb function pointer VMA.
 * On 64-bit host builds, evaluates all arguments warning-cleanly.
 */
static inline uint32_t aopf_vt_slot(const void *obj_self, size_t vt_byte_off)
{
    uint32_t vt_vma = aopf_rd_u32(obj_self, 0u);
    const void *vt_ptr = AOPF_VMA_PTR(const void *, vt_vma);
    return aopf_rd_u32(vt_ptr, vt_byte_off);
}

#if defined(__arm__) || defined(__thumb__)
#define AOPF_CALL0(ret_t, fn_vma) \
    (((ret_t (*)(void))(uintptr_t)(fn_vma))())
#define AOPF_CALL1(ret_t, fn_vma, t0, x0) \
    (((ret_t (*)(t0))(uintptr_t)(fn_vma))(x0))
#define AOPF_CALL2(ret_t, fn_vma, t0, x0, t1, x1) \
    (((ret_t (*)(t0, t1))(uintptr_t)(fn_vma))((x0), (x1)))
#define AOPF_CALL3(ret_t, fn_vma, t0, x0, t1, x1, t2, x2) \
    (((ret_t (*)(t0, t1, t2))(uintptr_t)(fn_vma))((x0), (x1), (x2)))
#define AOPF_CALL4(ret_t, fn_vma, t0, x0, t1, x1, t2, x2, t3, x3) \
    (((ret_t (*)(t0, t1, t2, t3))(uintptr_t)(fn_vma))((x0), (x1), (x2), (x3)))
#define AOPF_CALL5(ret_t, fn_vma, t0, x0, t1, x1, t2, x2, t3, x3, t4, x4) \
    (((ret_t (*)(t0, t1, t2, t3, t4))(uintptr_t)(fn_vma))((x0), (x1), (x2), (x3), (x4)))
#define AOPF_CALL6(ret_t, fn_vma, t0, x0, t1, x1, t2, x2, t3, x3, t4, x4, t5, x5) \
    (((ret_t (*)(t0, t1, t2, t3, t4, t5))(uintptr_t)(fn_vma))((x0), (x1), (x2), (x3), (x4), (x5)))

#define AOPF_VCALL0(fn_vma) \
    (((void (*)(void))(uintptr_t)(fn_vma))())
#define AOPF_VCALL1(fn_vma, t0, x0) \
    (((void (*)(t0))(uintptr_t)(fn_vma))(x0))
#define AOPF_VCALL2(fn_vma, t0, x0, t1, x1) \
    (((void (*)(t0, t1))(uintptr_t)(fn_vma))((x0), (x1)))
#define AOPF_VCALL3(fn_vma, t0, x0, t1, x1, t2, x2) \
    (((void (*)(t0, t1, t2))(uintptr_t)(fn_vma))((x0), (x1), (x2)))
#define AOPF_VCALL4(fn_vma, t0, x0, t1, x1, t2, x2, t3, x3) \
    (((void (*)(t0, t1, t2, t3))(uintptr_t)(fn_vma))((x0), (x1), (x2), (x3)))
#define AOPF_VCALL5(fn_vma, t0, x0, t1, x1, t2, x2, t3, x3, t4, x4) \
    (((void (*)(t0, t1, t2, t3, t4))(uintptr_t)(fn_vma))((x0), (x1), (x2), (x3), (x4)))
#define AOPF_VCALL6(fn_vma, t0, x0, t1, x1, t2, x2, t3, x3, t4, x4, t5, x5) \
    (((void (*)(t0, t1, t2, t3, t4, t5))(uintptr_t)(fn_vma))((x0), (x1), (x2), (x3), (x4), (x5)))
#else
#define AOPF_CALL0(ret_t, fn_vma) ((void)(fn_vma), (ret_t)0)
#define AOPF_CALL1(ret_t, fn_vma, t0, x0) ((void)(fn_vma), (void)(t0)(x0), (ret_t)0)
#define AOPF_CALL2(ret_t, fn_vma, t0, x0, t1, x1) \
    ((void)(fn_vma), (void)(t0)(x0), (void)(t1)(x1), (ret_t)0)
#define AOPF_CALL3(ret_t, fn_vma, t0, x0, t1, x1, t2, x2) \
    ((void)(fn_vma), (void)(t0)(x0), (void)(t1)(x1), (void)(t2)(x2), (ret_t)0)
#define AOPF_CALL4(ret_t, fn_vma, t0, x0, t1, x1, t2, x2, t3, x3) \
    ((void)(fn_vma), (void)(t0)(x0), (void)(t1)(x1), (void)(t2)(x2), (void)(t3)(x3), (ret_t)0)
#define AOPF_CALL5(ret_t, fn_vma, t0, x0, t1, x1, t2, x2, t3, x3, t4, x4) \
    ((void)(fn_vma), (void)(t0)(x0), (void)(t1)(x1), (void)(t2)(x2), (void)(t3)(x3), (void)(t4)(x4), (ret_t)0)
#define AOPF_CALL6(ret_t, fn_vma, t0, x0, t1, x1, t2, x2, t3, x3, t4, x4, t5, x5) \
    ((void)(fn_vma), (void)(t0)(x0), (void)(t1)(x1), (void)(t2)(x2), (void)(t3)(x3), (void)(t4)(x4), (void)(t5)(x5), (ret_t)0)

#define AOPF_VCALL0(fn_vma) ((void)(fn_vma))
#define AOPF_VCALL1(fn_vma, t0, x0) ((void)(fn_vma), (void)(t0)(x0))
#define AOPF_VCALL2(fn_vma, t0, x0, t1, x1) ((void)(fn_vma), (void)(t0)(x0), (void)(t1)(x1))
#define AOPF_VCALL3(fn_vma, t0, x0, t1, x1, t2, x2) \
    ((void)(fn_vma), (void)(t0)(x0), (void)(t1)(x1), (void)(t2)(x2))
#define AOPF_VCALL4(fn_vma, t0, x0, t1, x1, t2, x2, t3, x3) \
    ((void)(fn_vma), (void)(t0)(x0), (void)(t1)(x1), (void)(t2)(x2), (void)(t3)(x3))
#define AOPF_VCALL5(fn_vma, t0, x0, t1, x1, t2, x2, t3, x3, t4, x4) \
    ((void)(fn_vma), (void)(t0)(x0), (void)(t1)(x1), (void)(t2)(x2), (void)(t3)(x3), (void)(t4)(x4))
#define AOPF_VCALL6(fn_vma, t0, x0, t1, x1, t2, x2, t3, x3, t4, x4, t5, x5) \
    ((void)(fn_vma), (void)(t0)(x0), (void)(t1)(x1), (void)(t2)(x2), (void)(t3)(x3), (void)(t4)(x4), (void)(t5)(x5))
#endif

#endif /* AOPF_CPU_H */
