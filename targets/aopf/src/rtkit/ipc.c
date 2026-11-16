/*
 * ipc.c — Apple AKF 128-bit hardware FIFO mailbox driver routines
 * (g_mbox_ap_42220000_dev, g_mbox_4002c000_dev, g_mbox_42218000_dev,
 * g_mbox_440c4000_dev) reconstructed from aopf.i64.
 */

#include <stddef.h>
#include <stdint.h>
#include "aopf/bus.h"
#include "aopf/cpu.h"
#include "aopf/libc.h"
#include "aopf/memmap.h"
#include "aopf/regs.h"
#include "aopf/rtkit.h"

/* @shim RVA 0x5223c sub_5223C */
uint32_t Mbox_unimpl04(void)
{
    return RTK_ERR_UNSUPPORTED;
}

/* @provenance RVA 0x52244 sub_52244 */
uint32_t Mbox_setIrqHandler(rtk_mbox_dev_t *dev, void (*cb)(void *), void *ctx)
{
    rtk_nvic_dev_t *nvic = dev->irq_ctrl;
    (void)nvic->vt->register_irq(nvic, dev->rx_irq, cb, ctx);
    int en = (cb != NULL) ? 1 : 0;
    (void)nvic->vt->set_irq_enabled(nvic, dev->rx_irq, en);
    return RTK_OK;
}

/* @provenance RVA 0x5247c sub_5247C */
void Mbox_read128(uintptr_t mmio_base, uint32_t fifo_off, rtk_mbox_msg128_t *out_msg)
{
    bus_read128(mmio_base + fifo_off, out_msg->words);
}

/* @provenance RVA 0x5248e sub_5248E */
void Mbox_armRxEmptyIrq(rtk_mbox_dev_t *dev)
{
    if (dev->rx_empty_arm_en != 0u &&
        dev->rx_defer_count == 0u &&
        (dev->rx_int_en_flags & 1u) != 0u) {
        bus_write32(dev->rx_int_set_mmio, 1u << dev->rx_int_bit_shift);
    }
}

/* @provenance RVA 0x524b0 sub_524B0 */
uint32_t Mbox_popOne(rtk_mbox_dev_t *dev, rtk_mbox_msg128_t *out_msg)
{
    uint32_t saved_prio = rtk_enter_critical();
    uint32_t stat_reg = bus_read32(dev->hdr.mmio_base + dev->err_reg_off);
    uint32_t ret;

    if ((stat_reg & AKF_MBOX_RX_EMPTY_BIT) == 0u) {
        Mbox_read128(dev->hdr.mmio_base, dev->rx_fifo_off, out_msg);
        ret = RTK_OK;
    } else {
        out_msg->words[1] = 0u;
        out_msg->words[0] = 0u;
        out_msg->words[3] = 0u;
        out_msg->words[2] = 0u;
        Mbox_armRxEmptyIrq(dev);
        ret = RTK_ERR_NO_MMIO;
    }

    rtk_exit_critical(saved_prio);
    dev->rx_drained_flag = 1u;
    return ret;
}

/* @provenance RVA 0x523b4 sub_523B4 */
uint32_t Mbox_tryReadOne(rtk_mbox_dev_t *dev, rtk_mbox_msg128_t *out_msg)
{
    if ((dev->hdr.claimed_flags & 1u) == 0u) {
        return 0x80000008u;
    }
    return Mbox_popOne(dev, out_msg);
}

/* @shim RVA 0x52910 Mbox_checkError */
void Mbox_checkError(rtk_mbox_dev_t *dev, const char *op_name)
{
    uint32_t err_reg = bus_read32(dev->hdr.mmio_base + dev->err_reg_off);
    if ((err_reg & AKF_MBOX_ERR_MASK) == 0u) {
        return;
    }
    dev->hdr.claimed_flags &= 0xfeu;
    rtk_panic("Mbox:%s (h=%p %x=%x)",
              (op_name != NULL) ? op_name : "?",
              (const void *)dev,
              dev->err_reg_off,
              err_reg);
}

/* @shim RVA 0x520a4 Mbox_ioctl */
uint32_t Mbox_ioctl(rtk_mbox_dev_t *dev, uint32_t cmd, void *arg0, void *arg1)
{
    (void)arg0;
    (void)arg1;
    if (cmd == 1u) {
        return rtk_device_claim_mmio(&dev->hdr);
    }
    if (cmd >= 2u && cmd <= 8u) {
        return RTK_OK;
    }
    return RTK_ERR_UNSUPPORTED;
}
