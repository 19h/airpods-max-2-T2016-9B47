/*
 * systick.c — ARMv7E-M SysTick one-shot / periodic timer driver and busy-wait
 * delay helper reconstructed from aopf.i64 (0x5152C..0x51810, 0x5DF5E).
 */

#include <stddef.h>
#include <stdint.h>
#include "aopf/bus.h"
#include "aopf/cpu.h"
#include "aopf/libc.h"
#include "aopf/memmap.h"
#include "aopf/regs.h"
#include "aopf/rtkit.h"

/* @provenance RVA 0x5df5e rtk_timer_get_us */
uint64_t rtk_timer_get_us(rtk_timebase_dev_t *tb)
{
    uint64_t ticks = tb->hdr.vt != NULL
        ? ((const rtk_timebase_vtable_t *)tb->hdr.vt)->get_ticks(tb)
        : 0ull;
    return ((const rtk_timebase_vtable_t *)tb->hdr.vt)->ticks_to_us(tb, ticks);
}

/* @shim RVA 0x5152c sub_5152C */
void rtk_delay_us(uint64_t delay_us)
{
    if ((int64_t)delay_us <= 0) {
        return;
    }
    aopf_dsb();
    aopf_isb();
    rtk_timebase_dev_t *tb = AOPF_VMA_PTR(rtk_timebase_dev_t *, AOPF_TIMEBASE_DEV_VMA);
    uint64_t start_us = rtk_timer_get_us(tb);
    for (;;) {
        uint64_t elapsed = rtk_timer_get_us(tb) - start_us;
        if ((int64_t)(delay_us - elapsed) < 0) {
            break;
        }
    }
    aopf_isb();
}

/* @provenance RVA 0x51640 sub_51640 */
void SysTick_setCallback(rtk_systick_dev_t *dev, void (*cb)(void *), void *ctx)
{
    dev->callback_ctx = ctx;
    dev->callback_fn = cb;
}

/* @provenance RVA 0x5178c sub_5178C */
void SysTick_programReload(rtk_systick_dev_t *dev, uint64_t reload_ticks)
{
    uint32_t saved_prio = rtk_enter_critical();

    bus_write32(SYSTICK_CSR_ADDR, 0u);

    uint32_t hw_reload = SYSTICK_MAX_RELOAD;
    if ((reload_ticks >> 32) == 0ull && (uint32_t)reload_ticks < SYSTICK_MAX_RELOAD) {
        hw_reload = (uint32_t)reload_ticks;
    }
    bus_write32(SYSTICK_RVR_ADDR, hw_reload);
    bus_write32(SYSTICK_CVR_ADDR, 0u);

    uint64_t rem = reload_ticks - (uint64_t)hw_reload;
    dev->rem_ticks_lo = (uint32_t)rem;
    dev->rem_ticks_hi = (uint32_t)(rem >> 32);

    uint32_t csr_val = ((dev->use_ext_clk & 1u) != 0u) ? 3u : 7u;
    bus_write32(SYSTICK_CSR_ADDR, csr_val);

    rtk_exit_critical(saved_prio);
}

static int systick_has_clock(const rtk_systick_dev_t *dev)
{
    return (dev->cpu_clk_hz != 0u || dev->ext_clk_hz != 0u || dev->ten_khz_ticks != 0u) ? 1 : 0;
}

/* @provenance RVA 0x51648 sub_51648 */
uint32_t SysTick_armTicks(rtk_systick_dev_t *dev, uint64_t ticks)
{
    if (!systick_has_clock(dev)) {
        return RTK_ERR_INVALID_ARG;
    }
    dev->periodic_mode = 0u;
    uint64_t eff_ticks = (ticks > 1ull) ? ticks : 1ull;
    SysTick_programReload(dev, eff_ticks);
    return RTK_OK;
}

/* @provenance RVA 0x51684 sub_51684 */
uint32_t SysTick_armUs(rtk_systick_dev_t *dev, uint64_t delay_us)
{
    if (!systick_has_clock(dev)) {
        return RTK_ERR_INVALID_ARG;
    }
    if (delay_us < 2ull || delay_us > 0x01000000ull) {
        return RTK_ERR_INVALID_ARG;
    }
    dev->periodic_mode = 1u;
    SysTick_programReload(dev, delay_us - 1ull);
    return RTK_OK;
}

/* @provenance RVA 0x516c4 sub_516C4 */
uint32_t SysTick_cancel(rtk_systick_dev_t *dev, uint64_t *rem_ticks_out)
{
    uint32_t saved_prio = rtk_enter_critical();
    uint32_t csr = bus_read32(SYSTICK_CSR_ADDR);
    if ((csr & SYSTICK_CSR_ENABLE) == 0u) {
        rtk_exit_critical(saved_prio);
        return RTK_ERR_NOT_RUNNING;
    }

    uint32_t cur_cvr = bus_read32(SYSTICK_CVR_ADDR);
    uint64_t rem = ((uint64_t)dev->rem_ticks_hi << 32)
                 | (uint64_t)dev->rem_ticks_lo;
    uint64_t total = rem + (uint64_t)cur_cvr;
    if ((dev->periodic_mode & 1u) != 0u && total == 0ull) {
        total = (uint64_t)bus_read32(SYSTICK_RVR_ADDR) + 1ull;
    }

    rtk_exit_critical(saved_prio);
    *rem_ticks_out = total;
    return RTK_OK;
}

/* @provenance RVA 0x5171c sub_5171C */
uint64_t SysTick_usToTicks(rtk_systick_dev_t *dev, uint64_t delay_us)
{
    uint32_t div_val;
    uint64_t numer;

    if (dev->ten_khz_ticks != 0u) {
        div_val = dev->ten_khz_ticks;
        numer = delay_us * 10000ull;
    } else if (dev->ext_clk_hz != 0u) {
        div_val = dev->ext_clk_hz;
        numer = delay_us * 1000000ull;
    } else {
        div_val = dev->cpu_clk_hz;
        numer = delay_us;
    }
    return rtk_u64_div_u32(numer + (uint64_t)(div_val - 1u), div_val);
}

/* @provenance RVA 0x51748 sub_51748 */
uint64_t SysTick_ticksToUs(rtk_systick_dev_t *dev, uint64_t ticks)
{
    if (dev->ten_khz_ticks != 0u) {
        uint64_t numer = ticks * (uint64_t)dev->ten_khz_ticks + 9999ull;
        return rtk_u64_div_u32(numer, 10000u);
    }
    if (dev->ext_clk_hz != 0u) {
        uint64_t numer = ticks * (uint64_t)dev->ext_clk_hz + 999999ull;
        return rtk_u64_div_u32(numer, 1000000u);
    }
    return ticks * (uint64_t)dev->cpu_clk_hz;
}

/* @provenance RVA 0x517e8 SysTick_isr */
void SysTick_isr(rtk_systick_dev_t *dev)
{
    if ((dev->periodic_mode & 1u) == 0u) {
        bus_write32(SYSTICK_CSR_ADDR, 0u);
        bus_write32(SYSTICK_CVR_ADDR, 0u);
        uint64_t rem = ((uint64_t)dev->rem_ticks_hi << 32)
                     | (uint64_t)dev->rem_ticks_lo;
        if (rem != 0ull) {
            SysTick_programReload(dev, rem);
            return;
        }
    }
    if (dev->callback_fn != NULL) {
        dev->callback_fn(dev->callback_ctx);
    }
}

/* @shim RVA 0x51574 SysTick_ioctl */
uint32_t SysTick_ioctl(rtk_systick_dev_t *dev, uint32_t cmd, void *arg0, void *arg1)
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
