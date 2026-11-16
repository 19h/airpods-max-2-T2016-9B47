/*
 * regs_clk.h -- PHY clock controller (0x4D159000) and the auxiliary timer
 * (0x4D157000).
 */
#ifndef BTP_REGS_CLK_H
#define BTP_REGS_CLK_H

#include "regs.h"

#define CLK_BASE                    0x4D159000u
#define CLK_REG(offset)             MMIO32(CLK_BASE + (offset))

#define CLK_CTRL                    CLK_REG(0x000)
#define CLK_CTRL_EN                 BIT(0)
#define CLK_CTRL_FORCE_ON           BIT(1)
#define CLK_CTRL_MUX_UPDATE         BIT(2)
#define CLK_CTRL_MUX_DIV            RFIELD(4, 3)
#define CLK_CTRL_PRESCALER          RFIELD(12, 3)
#define CLK_CTRL_AUTO_GATE          BIT(16)

#define CLK_TICK                    CLK_REG(0x004)     /* free-running tick counter */

#define CLK_WAKE_IRQ                CLK_REG(0x014)
#define CLK_WAKE_IRQ_ACK            BIT(0)
#define CLK_WAKE_IRQ_EN             BIT(2)

#define AUX_TIMER_BASE              0x4D157000u
#define AUX_TIMER_IRQ               MMIO32(AUX_TIMER_BASE + 0x010)
#define AUX_TIMER_IRQ_EXPIRED       BIT(4)

#endif /* BTP_REGS_CLK_H */
