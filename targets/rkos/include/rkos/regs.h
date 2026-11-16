#ifndef RKOS_REGS_H
#define RKOS_REGS_H

#include "rkos/memmap.h"

/*
 * ARMv7E-M System Control Space (0xE000E000..0xE000F000) & Apple H2 MMIO registers.
 */

/* Interrupt Controller Type & SysTick */
#define RKOS_SCS_ICTR           0xE000E004u
#define RKOS_SYST_CSR           0xE000E010u
#define RKOS_SYST_RVR           0xE000E014u
#define RKOS_SYST_CVR           0xE000E018u
#define RKOS_SYST_CALIB         0xE000E01Cu

/* NVIC register banks (72 external IRQs -> 3 words) */
#define RKOS_NVIC_ISER_BASE     0xE000E100u
#define RKOS_NVIC_ICER_BASE     0xE000E180u
#define RKOS_NVIC_ISPR_BASE     0xE000E200u
#define RKOS_NVIC_ICPR_BASE     0xE000E280u
#define RKOS_NVIC_IABR_BASE     0xE000E300u
#define RKOS_NVIC_IPR_BASE      0xE000E400u

/* System Control Block (SCB) */
#define RKOS_SCB_CPUID          0xE000ED00u
#define RKOS_SCB_ICSR           0xE000ED04u
#define RKOS_SCB_VTOR           0xE000ED08u
#define RKOS_SCB_AIRCR          0xE000ED0Cu
#define RKOS_SCB_SCR            0xE000ED10u
#define RKOS_SCB_CCR            0xE000ED14u
#define RKOS_SCB_SHPR1          0xE000ED18u
#define RKOS_SCB_SHPR2          0xE000ED1Cu
#define RKOS_SCB_SHPR3          0xE000ED20u
#define RKOS_SCB_SHCSR          0xE000ED24u
#define RKOS_SCB_CFSR           0xE000ED28u
#define RKOS_SCB_HFSR           0xE000ED2Cu
#define RKOS_SCB_DFSR           0xE000ED30u
#define RKOS_SCB_MMFAR          0xE000ED34u
#define RKOS_SCB_BFAR           0xE000ED38u
#define RKOS_SCB_CPACR          0xE000ED88u

/* SCB_ICSR bit masks */
#define RKOS_ICSR_VECTACTIVE_MASK 0x000001FFu
#define RKOS_ICSR_PENDSTCLR       0x02000000u
#define RKOS_ICSR_PENDSTSET       0x04000000u
#define RKOS_ICSR_PENDSVCLR       0x08000000u
#define RKOS_ICSR_PENDSVSET       0x10000000u
#define RKOS_ICSR_NMIPENDSET      0x80000000u

/* SCB_SHCSR bit masks */
#define RKOS_SHCSR_SYSTICKACT     0x00000800u
#define RKOS_SHCSR_SVCALLPENDED   0x00008000u
#define RKOS_SHCSR_MEMFAULTENA    0x00010000u
#define RKOS_SHCSR_BUSFAULTENA    0x00020000u
#define RKOS_SHCSR_USGFAULTENA    0x00040000u
#define RKOS_SHCSR_ALL_FAULTS_ENA 0x00070000u

/* Memory Protection Unit (MPU) */
#define RKOS_MPU_TYPE           0xE000ED90u
#define RKOS_MPU_CTRL           0xE000ED94u
#define RKOS_MPU_RNR            0xE000ED98u
#define RKOS_MPU_RBAR           0xE000ED9Cu
#define RKOS_MPU_RASR           0xE000EDA0u

/* Floating-Point Extension */
#define RKOS_FPCCR              0xE000EF34u
#define RKOS_FPCAR              0xE000EF38u

/* Boot / Remap / Security Fuse registers */
#define RKOS_BOOT_REMAP_LOCK    0x00800000u
#define RKOS_FUSE_CHIP_CFG0     0x40406000u
#define RKOS_FUSE_CHIP_CFG1     0x4040607Cu
#define RKOS_FUSE_BOOT_MODE     0x4040800Cu

/* ARM core special register helpers (portable across ARMv7E-M and RKOS_HOST) */
#ifdef RKOS_HOST
uint32_t rkos_arch_get_ipsr(void);
uint32_t rkos_arch_get_primask(void);
void rkos_arch_set_primask(uint32_t val);
uint32_t rkos_arch_get_basepri(void);
void rkos_arch_set_basepri(uint32_t val);
uint32_t rkos_arch_get_control(void);
uint32_t rkos_arch_get_msp(void);
uint32_t rkos_arch_get_psp(void);
void rkos_arch_dsb(void);
void rkos_arch_isb(void);
void rkos_arch_wfi(void);
void rkos_arch_svc0(void);
#else
static inline uint32_t rkos_arch_get_ipsr(void) {
    uint32_t v;
    __asm volatile("mrs %0, ipsr" : "=r"(v));
    return v;
}
static inline uint32_t rkos_arch_get_primask(void) {
    uint32_t v;
    __asm volatile("mrs %0, primask" : "=r"(v));
    return v;
}
static inline void rkos_arch_set_primask(uint32_t val) {
    __asm volatile("msr primask, %0" :: "r"(val) : "memory");
}
static inline uint32_t rkos_arch_get_basepri(void) {
    uint32_t v;
    __asm volatile("mrs %0, basepri" : "=r"(v));
    return v;
}
static inline void rkos_arch_set_basepri(uint32_t val) {
    __asm volatile("msr basepri, %0" :: "r"(val) : "memory");
}
static inline uint32_t rkos_arch_get_control(void) {
    uint32_t v;
    __asm volatile("mrs %0, control" : "=r"(v));
    return v;
}
static inline uint32_t rkos_arch_get_msp(void) {
    uint32_t v;
    __asm volatile("mrs %0, msp" : "=r"(v));
    return v;
}
static inline uint32_t rkos_arch_get_psp(void) {
    uint32_t v;
    __asm volatile("mrs %0, psp" : "=r"(v));
    return v;
}
static inline void rkos_arch_dsb(void) {
    __asm volatile("dsb sy" ::: "memory");
}
static inline void rkos_arch_isb(void) {
    __asm volatile("isb sy" ::: "memory");
}
static inline void rkos_arch_wfi(void) {
    __asm volatile("wfi" ::: "memory");
}
static inline void rkos_arch_svc0(void) {
    __asm volatile("svc #0" ::: "memory");
}
#endif

#endif /* RKOS_REGS_H */
