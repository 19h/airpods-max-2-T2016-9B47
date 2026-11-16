/*
 * timers.c — Apple AOP 48-bit 1 MHz Timebase (0x42016000) and 3-channel Timer
 * Bank (0x42020000) drivers reconstructed from aopf.i64.
 */

#include <stddef.h>
#include <stdint.h>
#include "aopf/bus.h"
#include "aopf/cpu.h"
#include "aopf/libc.h"
#include "aopf/memmap.h"
#include "aopf/regs.h"
#include "aopf/rtkit.h"

/* @provenance RVA 0x5a75a sub_5A75A */
uint64_t Timebase_readRaw48(rtk_timebase_dev_t *dev)
{
    uint32_t prev_mask = aopf_get_primask();
    if (prev_mask == 0u) {
        aopf_set_primask(1u);
    }
    uintptr_t base = dev->hdr.mmio_base;
    uint32_t lo_word = bus_read32(base + dev->lo_reg_off);
    uint32_t hi_word = bus_read32(base + dev->hi_reg_off) & 0xffffu;
    if (prev_mask == 0u) {
        aopf_set_primask(0u);
    }
    return ((uint64_t)hi_word << 32) | (uint64_t)lo_word;
}

/* @provenance RVA 0x5a5d6 sub_5A5D6 */
void Timebase_setEpochDelta(rtk_timebase_dev_t *dev, uint64_t raw_now, uint64_t target_ticks)
{
    int is_neg = (target_ticks < raw_now) ? 1 : 0;
    uint32_t saved_prio = rtk_enter_critical();

    dev->offset_neg = (uint8_t)is_neg;
    uint64_t mag = is_neg ? (raw_now - target_ticks) : (target_ticks - raw_now);
    dev->offset_lo = (uint32_t)mag;
    dev->offset_hi = (uint32_t)(mag >> 32);

    rtk_exit_critical(saved_prio);
}

/* @shim RVA 0x5a540 sub_5A540 */
uint32_t Timebase_syncOffset(rtk_timebase_dev_t *dev, uint64_t target_ticks)
{
    uint32_t outer_prio = rtk_enter_critical();
    uint32_t inner_prio = rtk_enter_critical();

    uint32_t saved_off_lo = dev->offset_lo;
    uint32_t saved_off_hi = dev->offset_hi;
    uint8_t saved_slew = dev->slew_active;

    dev->offset_lo = 0u;
    dev->offset_hi = 0u;
    dev->slew_active = 0u;

    const rtk_timebase_vtable_t *vt = (const rtk_timebase_vtable_t *)dev->hdr.vt;
    uint64_t raw_ticks = vt->get_ticks(dev);

    dev->offset_hi = saved_off_hi;
    dev->offset_lo = saved_off_lo;
    dev->slew_active = (uint8_t)(saved_slew & 1u);
    rtk_exit_critical(inner_prio);

    uint64_t cur_adj = vt->get_ticks(dev);
    if ((dev->slew_active & 1u) != 0u && target_ticks < cur_adj) {
        uint64_t diff = cur_adj - target_ticks;
        dev->sync_target_lo = (uint32_t)cur_adj;
        dev->sync_target_hi = (uint32_t)(cur_adj >> 32);
        dev->offset_lo = (uint32_t)diff;
        dev->offset_hi = (uint32_t)(diff >> 32);
        dev->sync_raw_lo = (uint32_t)raw_ticks;
        dev->sync_raw_hi = (uint32_t)(raw_ticks >> 32);
    } else {
        Timebase_setEpochDelta(dev, raw_ticks, target_ticks);
    }

    rtk_exit_critical(outer_prio);
    return RTK_OK;
}

/* @provenance RVA 0x5a6ae Timebase_getTicks */
uint64_t Timebase_getTicks(rtk_timebase_dev_t *dev)
{
    uint64_t raw_val = Timebase_readRaw48(dev);
    uint64_t off_mag = ((uint64_t)dev->offset_hi << 32) | (uint64_t)dev->offset_lo;
    uint64_t signed_off = ((dev->offset_neg & 1u) != 0u) ? (0ull - off_mag) : off_mag;
    uint64_t result = raw_val + signed_off;

    if ((dev->slew_active & 1u) != 0u) {
        uint32_t saved_prio = rtk_enter_critical();
        uint64_t sync_target = ((uint64_t)dev->sync_target_hi << 32) | (uint64_t)dev->sync_target_lo;
        uint64_t sync_raw = ((uint64_t)dev->sync_raw_hi << 32) | (uint64_t)dev->sync_raw_lo;
        if (sync_target != 0ull && sync_raw != 0ull) {
            uint64_t elapsed_raw = raw_val - sync_raw;
            uint64_t cur_off = ((uint64_t)dev->offset_hi << 32) | (uint64_t)dev->offset_lo;
            if (cur_off >= elapsed_raw) {
                result = sync_target;
            } else {
                uint64_t new_target = elapsed_raw + sync_target - cur_off;
                Timebase_setEpochDelta(dev, raw_val, new_target);
                dev->sync_raw_hi = 0u;
                dev->sync_raw_lo = 0u;
                dev->sync_target_hi = 0u;
                dev->sync_target_lo = 0u;
                result = ((const rtk_timebase_vtable_t *)dev->hdr.vt)->get_ticks(dev);
            }
        }
        rtk_exit_critical(saved_prio);
    }
    return result;
}

/* @provenance RVA 0x5a678 sub_5A678 */
uint64_t Timebase_getTicksWrap(rtk_timebase_dev_t *dev)
{
    return ((const rtk_timebase_vtable_t *)dev->hdr.vt)->get_ticks(dev);
}

/* @provenance RVA 0x5a630 sub_5A630 */
uint64_t Timebase_usToTicks(rtk_timebase_dev_t *dev, uint64_t duration_us)
{
    if (dev->fast_1mhz_flag != 0u) {
        return rtk_u64_div_u32(duration_us, dev->fast_1mhz_flag);
    }
    uint64_t scaled = duration_us * (uint64_t)dev->freq_hz;
    return rtk_u64_div_u32(scaled, 1000000u);
}

/* @provenance RVA 0x5a65c sub_5A65C */
uint64_t Timebase_ticksToUsSlow(rtk_timebase_dev_t *dev, uint64_t ticks)
{
    uint64_t numer = ticks * (uint64_t)dev->freq_hz;
    return rtk_u64_div_u32(numer, 1000000u);
}

/* @provenance RVA 0x5a654 sub_5A654 */
uint64_t Timebase_ticksToUs(rtk_timebase_dev_t *dev, uint64_t ticks)
{
    if (dev->fast_1mhz_flag != 0u) {
        return ticks * (uint64_t)dev->fast_1mhz_flag;
    }
    return Timebase_ticksToUsSlow(dev, ticks);
}

/* @shim RVA 0x5a450 Timebase_ioctl_impl */
uint32_t Timebase_ioctl_impl(rtk_timebase_dev_t *dev, uint32_t cmd, void *arg0, void *arg1)
{
    (void)arg0;
    (void)arg1;
    if (cmd >= 2u && cmd <= 8u) {
        return RTK_OK;
    }
    if (cmd == 1u) {
        return rtk_device_claim_mmio(&dev->hdr);
    }
    return RTK_ERR_UNSUPPORTED;
}

/* @shim RVA 0x5a67e Timebase_ioctl */
uint32_t Timebase_ioctl(rtk_timebase_dev_t *dev, uint32_t cmd, void *arg0, void *arg1)
{
    if (cmd == 5u) {
        while (Timebase_readRaw48(dev) == 0ull) {
            /* Wait for monotonic counter start */
        }
    }
    return Timebase_ioctl_impl(dev, cmd, arg0, arg1);
}

/* @provenance RVA 0x55984 TimerBank_configChannel */
uint32_t TimerBank_configChannel(rtk_timer_bank_dev_t *dev, uint32_t chan, uint32_t inc_val, uint32_t cmp_val)
{
    if (chan >= dev->num_channels) {
        return RTK_ERR_INVALID_ARG;
    }
    uintptr_t base = dev->hdr.mmio_base;
    uintptr_t ctrl_addr = base + AOP_TIMER_CH_CTRL_OFF(chan);
    uint32_t ctrl = bus_read32(ctrl_addr) | AOP_TIMER_CTRL_CFG_EN;
    bus_write32(ctrl_addr, ctrl);
    bus_write32(base + AOP_TIMER_CH_CMP_OFF(chan), cmp_val);
    bus_write32(base + AOP_TIMER_CH_INC_OFF(chan), inc_val);
    return RTK_OK;
}

/* @shim RVA 0x559bc sub_559BC */
uint32_t TimerBank_enableChannel(rtk_timer_bank_dev_t *dev, uint32_t chan, int periodic)
{
    if (chan >= dev->num_channels) {
        return RTK_ERR_INVALID_ARG;
    }
    uintptr_t ctrl_addr = dev->hdr.mmio_base + AOP_TIMER_CH_CTRL_OFF(chan);
    uint32_t ctrl = (bus_read32(ctrl_addr) | 0x0182u) & ~4u;
    bus_write32(ctrl_addr, ctrl);
    uint32_t run_mode = (periodic != 0) ? 0x4205u : 0x4605u;
    bus_write32(ctrl_addr, run_mode);
    return RTK_OK;
}

/* @shim RVA 0x55a08 sub_55A08 */
uint32_t TimerBank_startChannel(rtk_timer_bank_dev_t *dev, uint32_t chan, uint32_t cb_modulo,
                                void (*cb)(void *), void *ctx)
{
    if (chan >= dev->num_channels) {
        return RTK_ERR_INVALID_ARG;
    }
    if (cb_modulo != 0u && cb == NULL) {
        return RTK_ERR_INVALID_ARG;
    }
    rtk_timer_chan_state_t *slot = &dev->channels[chan];
    slot->fire_count = 0u;
    uint32_t irq_bit = 0u;
    if (cb_modulo != 0u) {
        slot->callback_fn = cb;
        slot->cb_modulo = cb_modulo;
        slot->callback_ctx = ctx;
        irq_bit = AOP_TIMER_CTRL_IRQ_EN;
    }
    uintptr_t ctrl_addr = dev->hdr.mmio_base + AOP_TIMER_CH_CTRL_OFF(chan);
    bus_write32(ctrl_addr, 0x0182u);
    bus_write32(ctrl_addr, 0x4001u | irq_bit);
    return RTK_OK;
}

/* @provenance RVA 0x55a54 sub_55A54 */
uint32_t TimerBank_stopChannel(rtk_timer_bank_dev_t *dev, uint32_t chan)
{
    if (chan >= dev->num_channels) {
        return RTK_ERR_INVALID_ARG;
    }
    uintptr_t ctrl_addr = dev->hdr.mmio_base + AOP_TIMER_CH_CTRL_OFF(chan);
    bus_write32(ctrl_addr, bus_read32(ctrl_addr) & ~AOP_TIMER_CTRL_ENABLE);
    return RTK_OK;
}

/* @shim RVA 0x55a74 sub_55A74 */
uint32_t TimerBank_unimpl28(void)
{
    return RTK_ERR_UNSUPPORTED;
}

/* @shim RVA 0x55a7c sub_55A7C */
uint32_t TimerBank_unimpl32(void)
{
    return RTK_ERR_UNSUPPORTED;
}

/* @provenance RVA 0x55ae4 sub_55AE4 */
void TimerBank_clearAllIrqs(rtk_timer_bank_dev_t *dev)
{
    for (uint32_t chan = 0u; chan < dev->num_channels; chan++) {
        bus_write32(dev->hdr.mmio_base + AOP_TIMER_CH_CTRL_OFF(chan), AOP_TIMER_CTRL_IRQ_CLR);
    }
}

/* @shim RVA 0x55a84 TimerBank_isr */
void TimerBank_isr(rtk_timer_bank_dev_t *dev)
{
    for (uint32_t chan = 0u; chan < dev->num_channels; chan++) {
        uintptr_t ctrl_addr = dev->hdr.mmio_base + AOP_TIMER_CH_CTRL_OFF(chan);
        uint32_t ctrl = bus_read32(ctrl_addr);
        rtk_timer_chan_state_t *slot = &dev->channels[chan];

        if ((ctrl & AOP_TIMER_CTRL_FIRED) != 0u) {
            slot->fire_count += 1u;
            bus_write32(ctrl_addr, ctrl | AOP_TIMER_CTRL_IRQ_CLR);
        }

        uint32_t mod = slot->cb_modulo;
        if (mod != 0u && (slot->fire_count % mod) == 0u && slot->callback_fn != NULL) {
            slot->callback_fn(slot->callback_ctx);
        }
    }
}

/* @shim RVA 0x55914 TimerBank_ioctl */
uint32_t TimerBank_ioctl(rtk_timer_bank_dev_t *dev, uint32_t cmd, void *arg0, void *arg1)
{
    (void)arg0;
    (void)arg1;
    if (cmd == 1u) {
        uint32_t st = rtk_device_claim_mmio(&dev->hdr);
        if (st == RTK_OK) {
            TimerBank_clearAllIrqs(dev);
        }
        return st;
    }
    if (cmd >= 2u && cmd <= 8u) {
        return RTK_OK;
    }
    return RTK_ERR_UNSUPPORTED;
}
