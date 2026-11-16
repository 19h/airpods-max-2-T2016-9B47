/*
 * regs.h — ARMv7E-M System Control Space (SCS) and Apple AOP peripheral
 * register definitions verified from aopf.i64 MMIO cross-references.
 */

#ifndef AOPF_REGS_H
#define AOPF_REGS_H

#include <stdint.h>
#include "aopf/memmap.h"

/* ARMv7E-M System Control Space (0xE000E000) */
#define SCB_ICTR_ADDR                0xe000e004u

/* SysTick (0xE000E010) */
#define SYSTICK_BASE_ADDR            0xe000e010u
#define SYSTICK_CSR_ADDR             0xe000e010u
#define SYSTICK_RVR_ADDR             0xe000e014u
#define SYSTICK_CVR_ADDR             0xe000e018u
#define SYSTICK_CALIB_ADDR           0xe000e01cu

#define SYSTICK_CSR_ENABLE           (1u << 0)
#define SYSTICK_CSR_TICKINT          (1u << 1)
#define SYSTICK_CSR_CLKSOURCE        (1u << 2)
#define SYSTICK_CSR_COUNTFLAG        (1u << 16)
#define SYSTICK_MAX_RELOAD           0x00ffffffu

/* NVIC (0xE000E100..0xE000EF00) */
#define NVIC_ISER_BASE               0xe000e100u
#define NVIC_ICER_BASE               0xe000e180u
#define NVIC_ISPR_BASE               0xe000e200u
#define NVIC_ICPR_BASE               0xe000e280u
#define NVIC_IABR_BASE               0xe000e300u
#define NVIC_IPR_BASE                0xe000e400u
#define NVIC_STIR_ADDR               0xe000ef00u

/* System Control Block (0xE000ED00) */
#define SCB_CPUID_ADDR               0xe000ed00u
#define SCB_ICSR_ADDR                0xe000ed04u
#define SCB_VTOR_ADDR                0xe000ed08u
#define SCB_AIRCR_ADDR               0xe000ed0cu
#define SCB_SCR_ADDR                 0xe000ed10u
#define SCB_CCR_ADDR                 0xe000ed14u
#define SCB_SHPR1_ADDR               0xe000ed18u
#define SCB_SHPR2_ADDR               0xe000ed1cu
#define SCB_SHPR3_ADDR               0xe000ed20u
#define SCB_SHCSR_ADDR               0xe000ed24u
#define SCB_CFSR_ADDR                0xe000ed28u
#define SCB_HFSR_ADDR                0xe000ed2cu
#define SCB_DFSR_ADDR                0xe000ed30u
#define SCB_MMFAR_ADDR               0xe000ed34u
#define SCB_BFAR_ADDR                0xe000ed38u
#define SCB_AFSR_ADDR                0xe000ed3cu
#define SCB_CPACR_ADDR               0xe000ed88u

#define SCB_ICSR_PENDSVSET           (1u << 28)
#define SCB_ICSR_VECTACTIVE_MASK     0x000001ffu

/* MPU (0xE000ED90) & FPU (0xE000EF30) */
#define MPU_TYPE_ADDR                0xe000ed90u
#define MPU_CTRL_ADDR                0xe000ed94u
#define MPU_RNR_ADDR                 0xe000ed98u
#define MPU_RBAR_ADDR                0xe000ed9cu
#define MPU_RASR_ADDR                0xe000eda0u
#define FPU_FPCCR_ADDR               0xe000ef34u

/* DWT (0xE0001000) */
#define DWT_CTRL_ADDR                0xe0001000u
#define DWT_CYCCNT_ADDR              0xe0001004u

/* Apple AOP Timebase (0x42016000, 1 MHz 48-bit monotonic counter) */
#define AOP_TIMEBASE_LO_OFF          0x08u
#define AOP_TIMEBASE_HI_OFF          0x0cu
#define AOP_TIMEBASE_FREQ_HZ         1000000u

/* Apple AOP Timer Bank (0x42020000, 3 channels) */
#define AOP_TIMER_CH_CMP_OFF(ch)     ((uint32_t)(ch) * 8u + 0x00u)
#define AOP_TIMER_CH_INC_OFF(ch)     ((uint32_t)(ch) * 8u + 0x04u)
#define AOP_TIMER_CH_CTRL_OFF(ch)    ((uint32_t)(ch) * 4u + 0x18u)
#define AOP_TIMER_CTRL_ENABLE        (1u << 0)
#define AOP_TIMER_CTRL_CFG_EN        (1u << 1)
#define AOP_TIMER_CTRL_IRQ_EN        (1u << 4)
#define AOP_TIMER_CTRL_FIRED         (1u << 6)
#define AOP_TIMER_CTRL_IRQ_CLR       0x00000180u

/* Apple AOP Fuse / OTP Aperture (0x4201C000) */
#define AOP_FUSE_STATUS_OFF          0x00u
#define AOP_FUSE_CMD_OFF             0x10u
#define AOP_FUSE_DATA_OFF            0x14u
#define AOP_FUSE_LOCK_OFF            0x140u
#define AOP_FUSE_READY_MAGIC         0x00000461u

/* Apple AOP Watchdog Aperture (0x42200000) */
#define AOP_WDT_CTRL_OFF             0x00u
#define AOP_WDT_CTRL_STOP_MASK       0x00008002u
#define AOP_WDT_CTRL_KICK            (1u << 2)
#define AOP_WDT_CTRL_BARK_SHIFT      4u
#define AOP_WDT_CTRL_BARK_MASK       0x00000070u
#define AOP_WDT_CTRL_BITE_SHIFT      12u
#define AOP_WDT_CTRL_BITE_MASK       0x00007000u
#define AOP_WDT_UNIT_US              0x0001e848u /* 125,000 us = 125 ms */

/* Apple AKF 128-bit Mailbox FIFO Status Bits */
#define AKF_MBOX_ERR_MASK            0x000c0000u
#define AKF_MBOX_RX_EMPTY_BIT        (1u << 17)

/* Standard RTKit Status / Error Codes */
#define RTK_OK                       0x00000000u
#define RTK_ERR_INVALID_ARG          0x80000001u
#define RTK_ERR_NOT_FOUND            0x80000002u
#define RTK_ERR_UNSUPPORTED          0x80000003u
#define RTK_ERR_BAD_STATE            0x80000004u
#define RTK_ERR_NO_MMIO              0x80000005u
#define RTK_ERR_NOT_RUNNING          0x80000007u
#define RTK_ERR_BUSY_STATE           0x80000008u
#define RTK_ERR_NULL_KEY             0x80000009u
#define RTK_ERR_IO_FAULT             0x80000012u
#define RTK_ERR_OUT_OF_RANGE         0x80000016u

/* Boot & Heap Magic Constants */
#define RTK_BOOT_IN_PROGRESS_MAGIC   0xbaadf1acu
#define RTK_WARM_RESUME_MAGIC        0xcafe4b0bu
#define RTK_STACK_PAINT_LO           0x534b5452u /* "RTKS" */
#define RTK_STACK_PAINT_HI           0x4b434154u /* "TACK" */
#define RTK_STACK_PAINT_U64          0x4b434154534b5452ull
#define RTK_MEMALIGN_MAGIC           0xab5c155au

#endif /* AOPF_REGS_H */
