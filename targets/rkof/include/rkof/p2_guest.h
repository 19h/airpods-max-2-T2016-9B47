#ifndef RKOF_P2_GUEST_H
#define RKOF_P2_GUEST_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "rkof/bus.h"
#include "rkof/cpu.h"
#include "rkof/libc.h"
#include "rkof/memmap.h"
#include "rkof/rtkit.h"

/*
 * Portable ARMv7E-M + 64-bit Host C11 helpers for Phase-2 reconstructed modules.
 *
 * All pointer-sized firmware fields in 32-bit ARM SRAM/RODATA/MMIO are 32-bit
 * addresses (`uint32_t`). Using `RKOF_PTR(type, addr)` compiles to a zero-cost
 * cast `(type)(uintptr_t)(addr)` on ARMv7E-M (`sizeof(void *) == 4`) while
 * routing through `rkof_host_translate_ptr` on 64-bit host builds (`-DRKOF_HOST_BUILD=1`).
 */

#define RK_MEM_PTR(type, addr)   RKOF_PTR(type, (uintptr_t)(uint32_t)(addr))
#define RK_RODATA_STR(rva, doc)  RKOF_PTR(const char *, (uintptr_t)(uint32_t)(rva))

#if defined(RKOF_HOST_BUILD)
uint32_t rkof_host_scratch_frame(uint32_t byte_size);
uint32_t rkof_host_vcall(uint32_t fn_addr, uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3);
#define RK_PTR_TO_U32(p)         ((uint32_t)(uintptr_t)(p))
#define RK_VOL_U32(addr)         (*RKOF_PTR(volatile uint32_t *, (uintptr_t)(uint32_t)(addr)))
#define RK_VOL_U16(addr)         (*RKOF_PTR(volatile uint16_t *, (uintptr_t)(uint32_t)(addr)))
#define RK_VOL_U8(addr)          (*RKOF_PTR(volatile uint8_t *, (uintptr_t)(uint32_t)(addr)))
#define RK_FRAME_ALLOC(sz)       uint32_t rk_fp = rkof_host_scratch_frame(sz)
#define RK_VCALL0(rt, fn)        ((rt)rkof_host_vcall((uint32_t)(fn), 0u, 0u, 0u, 0u))
#define RK_VCALL1(rt, fn, x0)    ((rt)rkof_host_vcall((uint32_t)(fn), (uint32_t)(x0), 0u, 0u, 0u))
#define RK_VCALL2(rt, fn, x0, x1) ((rt)rkof_host_vcall((uint32_t)(fn), (uint32_t)(x0), (uint32_t)(x1), 0u, 0u))
#define RK_VCALL3(rt, fn, x0, x1, x2) ((rt)rkof_host_vcall((uint32_t)(fn), (uint32_t)(x0), (uint32_t)(x1), (uint32_t)(x2), 0u))
#define RK_VCALL4(rt, fn, x0, x1, x2, x3) ((rt)rkof_host_vcall((uint32_t)(fn), (uint32_t)(x0), (uint32_t)(x1), (uint32_t)(x2), (uint32_t)(x3)))
#define RK_VCALLV0(fn)           ((void)rkof_host_vcall((uint32_t)(fn), 0u, 0u, 0u, 0u))
#define RK_VCALLV1(fn, x0)       ((void)rkof_host_vcall((uint32_t)(fn), (uint32_t)(x0), 0u, 0u, 0u))
#define RK_VCALLV2(fn, x0, x1)   ((void)rkof_host_vcall((uint32_t)(fn), (uint32_t)(x0), (uint32_t)(x1), 0u, 0u))
#define RK_VCALLV3(fn, x0, x1, x2) ((void)rkof_host_vcall((uint32_t)(fn), (uint32_t)(x0), (uint32_t)(x1), (uint32_t)(x2), 0u))
#define RK_VCALLV4(fn, x0, x1, x2, x3) ((void)rkof_host_vcall((uint32_t)(fn), (uint32_t)(x0), (uint32_t)(x1), (uint32_t)(x2), (uint32_t)(x3)))
#else
#define RK_PTR_TO_U32(p)         ((uint32_t)(uintptr_t)(p))
#define RK_VOL_U32(addr)         (*(volatile uint32_t *)(uintptr_t)(uint32_t)(addr))
#define RK_VOL_U16(addr)         (*(volatile uint16_t *)(uintptr_t)(uint32_t)(addr))
#define RK_VOL_U8(addr)          (*(volatile uint8_t *)(uintptr_t)(uint32_t)(addr))
#define RK_FRAME_ALLOC(sz)       _Alignas(8) uint8_t rk_local_frame[(sz)]; uint32_t rk_fp = (uint32_t)(uintptr_t)rk_local_frame
#define RK_VCALL0(rt, fn)        (((rt (*)(void))(uintptr_t)((uint32_t)(fn) | 1u))())
#define RK_VCALL1(rt, fn, x0)    (((rt (*)(uint32_t))(uintptr_t)((uint32_t)(fn) | 1u))((uint32_t)(x0)))
#define RK_VCALL2(rt, fn, x0, x1) (((rt (*)(uint32_t, uint32_t))(uintptr_t)((uint32_t)(fn) | 1u))((uint32_t)(x0), (uint32_t)(x1)))
#define RK_VCALL3(rt, fn, x0, x1, x2) (((rt (*)(uint32_t, uint32_t, uint32_t))(uintptr_t)((uint32_t)(fn) | 1u))((uint32_t)(x0), (uint32_t)(x1), (uint32_t)(x2)))
#define RK_VCALL4(rt, fn, x0, x1, x2, x3) (((rt (*)(uint32_t, uint32_t, uint32_t, uint32_t))(uintptr_t)((uint32_t)(fn) | 1u))((uint32_t)(x0), (uint32_t)(x1), (uint32_t)(x2), (uint32_t)(x3)))
#define RK_VCALLV0(fn)           (((void (*)(void))(uintptr_t)((uint32_t)(fn) | 1u))())
#define RK_VCALLV1(fn, x0)       (((void (*)(uint32_t))(uintptr_t)((uint32_t)(fn) | 1u))((uint32_t)(x0)))
#define RK_VCALLV2(fn, x0, x1)   (((void (*)(uint32_t, uint32_t))(uintptr_t)((uint32_t)(fn) | 1u))((uint32_t)(x0), (uint32_t)(x1)))
#define RK_VCALLV3(fn, x0, x1, x2) (((void (*)(uint32_t, uint32_t, uint32_t))(uintptr_t)((uint32_t)(fn) | 1u))((uint32_t)(x0), (uint32_t)(x1), (uint32_t)(x2)))
#define RK_VCALLV4(fn, x0, x1, x2, x3) (((void (*)(uint32_t, uint32_t, uint32_t, uint32_t))(uintptr_t)((uint32_t)(fn) | 1u))((uint32_t)(x0), (uint32_t)(x1), (uint32_t)(x2), (uint32_t)(x3)))
#endif

#define RK_U32_AT(addr)          (*RK_MEM_PTR(uint32_t *, (addr)))
#define RK_I32_AT(addr)          (*RK_MEM_PTR(int32_t *, (addr)))
#define RK_U16_AT(addr)          (*RK_MEM_PTR(uint16_t *, (addr)))
#define RK_I16_AT(addr)          (*RK_MEM_PTR(int16_t *, (addr)))
#define RK_U8_AT(addr)           (*RK_MEM_PTR(uint8_t *, (addr)))
#define RK_I8_AT(addr)           (*RK_MEM_PTR(int8_t *, (addr)))
#define RK_U64_AT(addr)          (*RK_MEM_PTR(uint64_t *, (addr)))
#define RK_I64_AT(addr)          (*RK_MEM_PTR(int64_t *, (addr)))
#define RK_F32_AT(addr)          (*RK_MEM_PTR(float *, (addr)))
#define RK_F64_AT(addr)          (*RK_MEM_PTR(double *, (addr)))

static inline float rk_f32_from_u32(uint32_t bits)
{
    union {
        uint32_t u;
        float f;
    } conv;
    conv.u = bits;
    return conv.f;
}

static inline uint32_t rk_u32_from_f32(float val)
{
    union {
        uint32_t u;
        float f;
    } conv;
    conv.f = val;
    return conv.u;
}

static inline double rk_f64_from_u64(uint64_t bits)
{
    union {
        uint64_t u;
        double d;
    } conv;
    conv.u = bits;
    return conv.d;
}

static inline uint64_t rk_u64_from_f64(double val)
{
    union {
        uint64_t u;
        double d;
    } conv;
    conv.d = val;
    return conv.u;
}

/* ARMv7E-M shift semantics without C11 shift-width undefined behavior */
static inline uint32_t rk_shl32(uint32_t val, uint32_t shift_amt)
{
    uint32_t amt = shift_amt & 0xFFu;
    return (amt >= 32u) ? 0u : (val << amt);
}

static inline uint32_t rk_shr32(uint32_t val, uint32_t shift_amt)
{
    uint32_t amt = shift_amt & 0xFFu;
    return (amt >= 32u) ? 0u : (val >> amt);
}

static inline int32_t rk_sar32(int32_t val, uint32_t shift_amt)
{
    uint32_t amt = shift_amt & 0xFFu;
    if (amt >= 32u) {
        return (val < 0) ? -1 : 0;
    }
    if (amt == 0u) {
        return val;
    }
    return (val < 0)
        ? (int32_t)(~((~((uint32_t)val)) >> amt))
        : (int32_t)(((uint32_t)val) >> amt);
}

static inline uint32_t rk_ror32(uint32_t val, uint32_t shift_amt)
{
    uint32_t amt = shift_amt & 31u;
    return (amt == 0u) ? val : ((val >> amt) | (val << (32u - amt)));
}

static inline uint32_t rk_rbit32(uint32_t val)
{
#if !defined(RKOF_HOST_BUILD)
    uint32_t out;
    __asm__("rbit %0, %1" : "=r"(out) : "r"(val));
    return out;
#else
    uint32_t x = val;
    x = ((x & 0x55555555u) << 1u) | ((x >> 1u) & 0x55555555u);
    x = ((x & 0x33333333u) << 2u) | ((x >> 2u) & 0x33333333u);
    x = ((x & 0x0F0F0F0Fu) << 4u) | ((x >> 4u) & 0x0F0F0F0Fu);
    return __builtin_bswap32(x);
#endif
}

static inline uint16_t rk_bswap16(uint16_t val)
{
    return (uint16_t)((val >> 8u) | (val << 8u));
}

/* External pending_boundary.S & host_bus.c prototypes */
void rtkit_app_init(void);
void *rtkit_heap_alloc(void *heap, size_t size);
void rtkit_heap_free(void *heap, void *ptr, uint32_t mode);
void sub_141B0(void *log_pack);
void sub_141B8(void *log_pack, uint32_t tag_id);
int sub_15830(void);
void sub_167E8(uint8_t *heap, uint32_t enable);
void sub_17C98(void);
void sub_17CEC(void);
void sub_1878C(void *log_pack, uint32_t media_id, uint32_t off, uint32_t exp_crc, uint32_t got_crc);
void sub_189C4(void *log_pack, const char *arg0);
void sub_18EC0(void *log_pack, uint32_t tag_id, uint32_t len);
void sub_1A5A8(uint32_t coproc_id);
void sub_1A5F4(uintptr_t log_token, const void *log_pack);
void sub_1A646(uintptr_t log_token, const void *log_pack);
void *sub_1AA14(void *heap_desc);
int sub_1BA50(void);
int sub_1BA72(void);
int sub_22F14(uint32_t ep, uintptr_t cb_fn, uint32_t arg);
void sub_23B78(uint32_t dst_id, uint32_t op, uint32_t subop, uint32_t len, const void *payload);
void sub_C7A8C(uint32_t err_code, uint32_t subcode);
void sub_F1C94(void);
void sub_F1D1C(void);
void sub_186A8C(uintptr_t region_base, size_t region_size);
void *sub_186BE4(void *old_ptr, size_t new_size);
void sub_187222(const char *str);
void sub_187240(const char *fmt, va_list ap);
int sub_18726C(void *sink_ctx, const char *fmt, va_list ap);
uint32_t sub_187A64(const char *str, char **endptr, int base);
int sub_1880F8(uint32_t tag_fourcc, uint32_t flags, uintptr_t cb_fn, uint32_t ctx);
void sub_188300(uint32_t code, uint32_t subcode, const char *fmt, va_list ap);
void sub_1887D8(void);
int sub_18E930(uint32_t *word_out, uint16_t aligned_off);
uint32_t sub_19CBB4(uint32_t *out_a, uint32_t *out_b);
uint32_t sub_19CC9C(uint32_t *out_val);
RKOF_NORETURN void sub_1A234C(void);
void sub_1A295C(void *lock_obj, uint32_t token);
uint32_t sub_1A2A3C(void *lock_obj);
uint64_t sub_1A3204(void *timer_dev);
uint32_t sub_1A375C(uintptr_t addr);
void sub_13328(uint32_t coproc_slot);

#endif /* RKOF_P2_GUEST_H */
