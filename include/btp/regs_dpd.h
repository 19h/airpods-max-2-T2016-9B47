/*
 * regs_dpd.h -- digital pre-distortion engine and loopback correlators
 * (0x4D141000), with the DPD sample RAM aperture at 0x4D148000.
 */
#ifndef BTP_REGS_DPD_H
#define BTP_REGS_DPD_H

#include "regs.h"

#define DPD_BASE                    0x4D141000u
#define DPD_REG(offset)             MMIO32(DPD_BASE + (offset))

#define DPD_CTRL                    DPD_REG(0x000)
#define DPD_CTRL_EN                 BIT(0)
#define DPD_CTRL_MODE               RFIELD(1, 3)

#define DPD_IRQ                     DPD_REG(0x01C)
#define DPD_IRQ_CAPTURE_DONE        BIT(1)
#define DPD_IRQ_CORR0_ACK           (BIT(2) | BIT(8))
#define DPD_IRQ_CORR1_ACK           (BIT(5) | BIT(9))

/* Loopback correlator capture windows (24-bit sample positions). */
#define CORR_CAPTURE_START(n)       DPD_REG((n) ? 0x118 : 0x0E8)
#define CORR_CAPTURE_END(n)         DPD_REG((n) ? 0x11C : 0x0EC)
#define CORR_CAPTURE_POS            RFIELD(0, 24)
#define CORR_CAPTURE_EN             BIT(24)

/* Correlator n control block: ctrl, start, end. */
#define CORR_CTRL(n)                DPD_REG((n) ? 0x104 : 0x0F4)
#define CORR_START(n)               DPD_REG((n) ? 0x108 : 0x0F8)
#define CORR_END(n)                 DPD_REG((n) ? 0x10C : 0x0FC)
#define CORR_CTRL_WINDOW            RFIELD(4, 7)
#define CORR_POS                    RFIELD(0, 24)

#define CORR_TRIGGER                DPD_REG(0x1CC)
#define CORR_TRIGGER_CORR(n)        BIT(2 + (n))

#define CORR_RESULT_I(n)            DPD_REG(0x1E0 + 8 * (n))
#define CORR_RESULT_Q(n)            DPD_REG(0x1E4 + 8 * (n))
#define CORR_RESULT_BITS            24

#define LB_IQMM_DCOC                DPD_REG(0x300)
#define LB_IQMM_DCOC_I              RFIELD(4, 10)
#define LB_IQMM_DCOC_Q              RFIELD(16, 10)

#define LB_IQMM_AB                  DPD_REG(0x328)
#define LB_IQMM_AB_A                RFIELD(0, 12)
#define LB_IQMM_AB_B                RFIELD(16, 12)

#define LB_SEQ_CTRL                 DPD_REG(0x500)
#define LB_SEQ_CTRL_MODE            RFIELD(4, 4)
#define LB_SEQ_WINDOW0_START        DPD_REG(0x540)
#define LB_SEQ_WINDOW0_END          DPD_REG(0x544)
#define LB_SEQ_WINDOW1_START        DPD_REG(0x548)
#define LB_SEQ_WINDOW1_END          DPD_REG(0x54C)
#define LB_SEQ_WINDOW_POS           RFIELD(0, 24)

#define DPD_GTB                     DPD_REG(0x604)     /* gain-table-base counter */
#define DPD_STATUS                  DPD_REG(0x608)
#define DPD_STATUS_CPU_ACCESS_ERR   RFIELD(0, 10)

/* Sample RAM: 752 words of capture data; the actuation table window starts
 * 0x200 words in. */
#define DPD_RAM_BASE                0x4D148000u
#define DPD_RAM_WORDS               752
#define DPD_ACTUATION_RAM_BASE      0x4D148800u
#define DPD_ACTUATION_RAM_WORDS     240
#define DPD_RAM                     BTP_ARR(volatile uint32_t, DPD_RAM_WORDS, DPD_RAM_BASE)
#define DPD_ACTUATION_RAM           BTP_ARR(volatile uint32_t, DPD_ACTUATION_RAM_WORDS, \
                                            DPD_ACTUATION_RAM_BASE)

#endif /* BTP_REGS_DPD_H */
