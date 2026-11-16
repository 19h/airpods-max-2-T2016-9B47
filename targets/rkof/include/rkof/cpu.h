#ifndef RKOF_CPU_H
#define RKOF_CPU_H

#include <stdint.h>
#include "rkof/bus.h"

/* ARMv7E-M System Control Space (0xE000E000) */
#define SCS_BASE            0xE000E000u
#define SCS_ICTR_ADDR       0xE000E004u

/* SysTick (0xE000E010) */
#define SYST_CSR_ADDR       0xE000E010u
#define SYST_RVR_ADDR       0xE000E014u
#define SYST_CVR_ADDR       0xE000E018u
#define SYST_CALIB_ADDR     0xE000E01Cu

#define SYST_CSR_ENABLE     (1u << 0)
#define SYST_CSR_TICKINT    (1u << 1)
#define SYST_CSR_CLKSOURCE  (1u << 2)
#define SYST_CSR_COUNTFLAG  (1u << 16)
#define SYST_RVR_MAX        0x00FFFFFFu

/* NVIC (0xE000E100) */
#define NVIC_ISER_BASE      0xE000E100u
#define NVIC_ICER_BASE      0xE000E180u
#define NVIC_ISPR_BASE      0xE000E200u
#define NVIC_ICPR_BASE      0xE000E280u
#define NVIC_IABR_BASE      0xE000E300u
#define NVIC_IPR_BASE       0xE000E400u

/* SCB (0xE000ED00) */
#define SCB_BASE            0xE000ED00u
#define SCB_ICSR_ADDR       0xE000ED04u
#define SCB_VTOR_ADDR       0xE000ED08u
#define SCB_AIRCR_ADDR      0xE000ED0Cu
#define SCB_SCR_ADDR        0xE000ED10u
#define SCB_CCR_ADDR        0xE000ED14u
#define SCB_SHPR1_ADDR      0xE000ED18u
#define SCB_SHPR2_ADDR      0xE000ED1Cu
#define SCB_SHPR3_ADDR      0xE000ED20u
#define SCB_SHCSR_ADDR      0xE000ED24u
#define SCB_CFSR_ADDR       0xE000ED28u
#define SCB_HFSR_ADDR       0xE000ED2Cu
#define SCB_DFSR_ADDR       0xE000ED30u
#define SCB_MMFAR_ADDR      0xE000ED34u
#define SCB_BFAR_ADDR       0xE000ED38u
#define SCB_AFSR_ADDR       0xE000ED3Cu
#define SCB_CPACR_ADDR      0xE000ED88u

/* MPU (0xE000ED90) */
#define MPU_TYPE_ADDR       0xE000ED90u
#define MPU_CTRL_ADDR       0xE000ED94u
#define MPU_RNR_ADDR        0xE000ED98u
#define MPU_RBAR_ADDR       0xE000ED9Cu
#define MPU_RASR_ADDR       0xE000EDA0u

/* FPCCR / FPDSCR (0xE000EF34) */
#define FPU_FPCCR_ADDR      0xE000EF34u
#define FPU_FPCAR_ADDR      0xE000EF38u
#define FPU_FPDSCR_ADDR     0xE000EF3Cu

#if defined(RKOF_HOST_BUILD)

struct rkof_host_cpu_state {
    uint32_t primask;
    uint32_t basepri;
    uint32_t faultmask;
    uint32_t control;
    uint32_t ipsr;
    uint32_t wfi_count;
};

extern struct rkof_host_cpu_state g_rkof_host_cpu;

static inline uint32_t cpu_read_primask(void)
{
    return g_rkof_host_cpu.primask;
}

static inline void cpu_write_primask(uint32_t val)
{
    g_rkof_host_cpu.primask = val & 1u;
}

static inline uint32_t cpu_read_basepri(void)
{
    return g_rkof_host_cpu.basepri;
}

static inline void cpu_write_basepri(uint32_t val)
{
    g_rkof_host_cpu.basepri = val & 0xFFu;
}

static inline uint32_t cpu_read_faultmask(void)
{
    return g_rkof_host_cpu.faultmask;
}

static inline uint32_t cpu_read_control(void)
{
    return g_rkof_host_cpu.control;
}

static inline void cpu_dsb(void) {}
static inline void cpu_isb(void) {}
static inline void cpu_wfi(void)
{
    g_rkof_host_cpu.wfi_count++;
}

#else /* ARMv7E-M freestanding build */

static inline uint32_t cpu_read_primask(void)
{
    uint32_t val;
    __asm__ volatile("mrs %0, primask" : "=r"(val));
    return val;
}

static inline void cpu_write_primask(uint32_t val)
{
    __asm__ volatile("msr primask, %0" : : "r"(val) : "memory");
}

static inline uint32_t cpu_read_basepri(void)
{
    uint32_t val;
    __asm__ volatile("mrs %0, basepri" : "=r"(val));
    return val;
}

static inline void cpu_write_basepri(uint32_t val)
{
    __asm__ volatile("msr basepri, %0" : : "r"(val) : "memory");
}

static inline uint32_t cpu_read_faultmask(void)
{
    uint32_t val;
    __asm__ volatile("mrs %0, faultmask" : "=r"(val));
    return val;
}

static inline uint32_t cpu_read_control(void)
{
    uint32_t val;
    __asm__ volatile("mrs %0, control" : "=r"(val));
    return val;
}

static inline void cpu_dsb(void)
{
    __asm__ volatile("dsb sy" : : : "memory");
}

static inline void cpu_isb(void)
{
    __asm__ volatile("isb sy" : : : "memory");
}

static inline void cpu_wfi(void)
{
    __asm__ volatile("wfi" : : : "memory");
}

#endif /* RKOF_HOST_BUILD */

static inline uint32_t cpu_clz32(uint32_t val)
{
    return val == 0u ? 32u : (uint32_t)__builtin_clz(val);
}

static inline uint32_t cpu_bswap32(uint32_t val)
{
    return __builtin_bswap32(val);
}

static inline uint32_t cpu_ror16_1(uint16_t val)
{
    return ((uint32_t)val >> 1u) | (((uint32_t)val & 1u) << 15u);
}

#endif /* RKOF_CPU_H */
