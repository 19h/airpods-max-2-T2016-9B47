/*
 * cpu.h -- ARMv7-M core access: system-control block, NVIC, FPU and the
 * special-register intrinsics the RTKit runtime relies on.
 *
 * Everything here is architectural (ARM DDI 0403); nothing is W3 specific.
 * The host build replaces the MRS/MSR intrinsics with a register file in
 * tests/host_bus.c so the boot and fault paths can be exercised natively.
 */
#ifndef BTP_CPU_H
#define BTP_CPU_H

#include <stdint.h>
#include "bus.h"

/* ---- System Control Space ------------------------------------------------ */

#define SCS_BASE            0xE000E000u

#define SYST_CSR            MMIO32(SCS_BASE + 0x010)   /* SysTick control/status */
#define SYST_RVR            MMIO32(SCS_BASE + 0x014)   /* SysTick reload */
#define SYST_CVR            MMIO32(SCS_BASE + 0x018)   /* SysTick current value */
#define NVIC_ISER0          MMIO32(SCS_BASE + 0x100)
#define NVIC_ICER0          MMIO32(SCS_BASE + 0x180)
#define NVIC_ICPR0          MMIO32(SCS_BASE + 0x280)
#define SCB_CCR             MMIO32(SCS_BASE + 0xD14)
#define SCB_SHPR1           MMIO32(SCS_BASE + 0xD18)
#define SCB_SHPR2           MMIO32(SCS_BASE + 0xD1C)
#define SCB_SHPR3           MMIO32(SCS_BASE + 0xD20)
#define SCB_SHCSR           MMIO32(SCS_BASE + 0xD24)
#define SCB_CFSR            MMIO32(SCS_BASE + 0xD28)
#define SCB_HFSR            MMIO32(SCS_BASE + 0xD2C)
#define SCB_DFSR            MMIO32(SCS_BASE + 0xD30)
#define SCB_MMFAR           MMIO32(SCS_BASE + 0xD34)
#define SCB_BFAR            MMIO32(SCS_BASE + 0xD38)
#define SCB_CPACR           MMIO32(SCS_BASE + 0xD88)
#define MPU_TYPE            MMIO32(SCS_BASE + 0xD90)
#define MPU_CTRL            MMIO32(SCS_BASE + 0xD94)
#define MPU_RBAR            MMIO32(SCS_BASE + 0xD9C)
#define MPU_RASR            MMIO32(SCS_BASE + 0xDA0)
#define FPU_FPCCR           MMIO32(SCS_BASE + 0xF34)
#define FPU_FPDSCR          MMIO32(SCS_BASE + 0xF3C)

#define SYST_CSR_ENABLE         (1u << 0)
#define SYST_CSR_TICKINT        (1u << 1)
#define SYST_CSR_CLKSOURCE      (1u << 2)

#define SCB_CCR_UNALIGN_TRP     (1u << 3)
#define SCB_CCR_DIV_0_TRP       (1u << 4)
#define SCB_CCR_STKALIGN        (1u << 9)

#define SCB_SHCSR_MEMFAULTENA   (1u << 16)
#define SCB_SHCSR_BUSFAULTENA   (1u << 17)
#define SCB_SHCSR_USGFAULTENA   (1u << 18)

#define SCB_CFSR_BUSFAULT_MASK  0x0000FF00u    /* BFSR byte within CFSR */

#define SCB_CPACR_CP10_CP11_FULL 0x00F00000u
#define FPU_FPCCR_ASPEN         (1u << 31)
#define FPU_FPCCR_LSPEN         (1u << 30)

#define MPU_TYPE_SEPARATE       (1u << 0)
#define MPU_TYPE_DREGION_SHIFT  8
#define MPU_TYPE_DREGION_MASK   0xFFu
#define MPU_RBAR_ADDR_MASK      0xFFFFFFE0u

/* Exception numbers as read from IPSR. */
enum arm_exception_number {
    ARM_EXC_RESET        = 1,
    ARM_EXC_NMI          = 2,
    ARM_EXC_HARDFAULT    = 3,
    ARM_EXC_MEMMANAGE    = 4,
    ARM_EXC_BUSFAULT     = 5,
    ARM_EXC_USAGEFAULT   = 6,
    ARM_EXC_SVCALL       = 11,
    ARM_EXC_DEBUGMONITOR = 12,
    ARM_EXC_PENDSV       = 14,
    ARM_EXC_SYSTICK      = 15,
    ARM_EXC_IRQ0         = 16,
};

/* EXC_RETURN bits. */
#define EXC_RETURN_NO_FPU_FRAME (1u << 4)      /* set: basic (no S0-S15) frame */
#define EXC_RETURN_PSP          (1u << 2)      /* set: return to thread/PSP */
#define EXC_RETURN_THREAD       (1u << 3)

#define CONTROL_SPSEL           (1u << 1)

/* xPSR bits visible in the stacked frame. */
#define XPSR_STKALIGN_PAD       (1u << 9)

/* ---- Special-register intrinsics ----------------------------------------- */

#if defined(__arm__) || defined(__thumb__)

#define CPU_SYSREG_READ(name)                                   \
    static inline uint32_t cpu_get_##name(void)                 \
    {                                                           \
        uint32_t v;                                             \
        __asm volatile ("mrs %0, " #name : "=r" (v));           \
        return v;                                               \
    }
#define CPU_SYSREG_WRITE(name)                                  \
    static inline void cpu_set_##name(uint32_t v)               \
    {                                                           \
        __asm volatile ("msr " #name ", %0" :: "r" (v) : "memory"); \
    }

CPU_SYSREG_READ(primask)   CPU_SYSREG_WRITE(primask)
CPU_SYSREG_READ(basepri)   CPU_SYSREG_WRITE(basepri)
CPU_SYSREG_READ(faultmask)
CPU_SYSREG_READ(control)   CPU_SYSREG_WRITE(control)
CPU_SYSREG_READ(ipsr)
CPU_SYSREG_READ(msp)       CPU_SYSREG_WRITE(msp)
CPU_SYSREG_READ(psp)       CPU_SYSREG_WRITE(psp)

#undef CPU_SYSREG_READ
#undef CPU_SYSREG_WRITE

static inline void cpu_disable_irq(void) { __asm volatile ("cpsid i" ::: "memory"); }
static inline void cpu_enable_irq(void)  { __asm volatile ("cpsie i" ::: "memory"); }
static inline void cpu_dsb(void)         { __asm volatile ("dsb sy" ::: "memory"); }
static inline void cpu_isb(void)         { __asm volatile ("isb sy" ::: "memory"); }
static inline void cpu_wfi(void)         { __asm volatile ("wfi" ::: "memory"); }

#else /* host model */

enum cpu_host_sysreg {
    CPU_HOST_PRIMASK = 0, CPU_HOST_BASEPRI, CPU_HOST_FAULTMASK,
    CPU_HOST_CONTROL, CPU_HOST_IPSR, CPU_HOST_MSP, CPU_HOST_PSP,
    CPU_HOST_REG_COUNT
};

uint32_t cpu_host_sysreg_get(unsigned index);
void     cpu_host_sysreg_set(unsigned index, uint32_t value);
void     cpu_host_wfi(void);

#define cpu_get_primask()    cpu_host_sysreg_get(CPU_HOST_PRIMASK)
#define cpu_set_primask(v)   cpu_host_sysreg_set(CPU_HOST_PRIMASK, (v))
#define cpu_get_basepri()    cpu_host_sysreg_get(CPU_HOST_BASEPRI)
#define cpu_set_basepri(v)   cpu_host_sysreg_set(CPU_HOST_BASEPRI, (v))
#define cpu_get_faultmask()  cpu_host_sysreg_get(CPU_HOST_FAULTMASK)
#define cpu_get_control()    cpu_host_sysreg_get(CPU_HOST_CONTROL)
#define cpu_set_control(v)   cpu_host_sysreg_set(CPU_HOST_CONTROL, (v))
#define cpu_get_ipsr()       cpu_host_sysreg_get(CPU_HOST_IPSR)
#define cpu_get_msp()        cpu_host_sysreg_get(CPU_HOST_MSP)
#define cpu_set_msp(v)       cpu_host_sysreg_set(CPU_HOST_MSP, (v))
#define cpu_get_psp()        cpu_host_sysreg_get(CPU_HOST_PSP)
#define cpu_set_psp(v)       cpu_host_sysreg_set(CPU_HOST_PSP, (v))
#define cpu_disable_irq()    cpu_host_sysreg_set(CPU_HOST_PRIMASK, 1u)
#define cpu_enable_irq()     cpu_host_sysreg_set(CPU_HOST_PRIMASK, 0u)
#define cpu_dsb()            ((void)0)
#define cpu_isb()            ((void)0)
#define cpu_wfi()            cpu_host_wfi()

#endif

/* CLZ with the ARM semantics (clz(0) == 32). */
static inline uint32_t cpu_clz(uint32_t v)
{
    return v ? (uint32_t)__builtin_clz(v) : 32u;
}

/* Signed saturate to `bits` bits (SSAT). */
static inline int32_t cpu_ssat(int32_t v, unsigned bits)
{
    const int32_t hi = (int32_t)((1u << (bits - 1)) - 1u);
    const int32_t lo = -hi - 1;
    return v > hi ? hi : (v < lo ? lo : v);
}

#endif /* BTP_CPU_H */
