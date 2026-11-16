/*
 * platform.c — Apple AOP Fuse/OTP (0x4201C000), PMGR (0x42440000), and
 * Always-On Watchdog (0x42200000) drivers reconstructed from aopf.i64.
 */

#include <stddef.h>
#include <stdint.h>
#include "aopf/bus.h"
#include "aopf/cpu.h"
#include "aopf/memmap.h"
#include "aopf/regs.h"
#include "aopf/rtkit.h"

/* @shim RVA 0x52e40 Fuse_readWords */
uint32_t Fuse_readWords(rtk_fuse_dev_t *dev, uint32_t addr_cmd,
                        uint32_t *out_words, uint32_t *io_count)
{
    if ((addr_cmd & 1u) == 0u) {
        (void)rtk_fastlock_unlock(&dev->lock_word);
        return 0x80000003u;
    }
    if (((uintptr_t)out_words & 3u) != 0u) {
        (void)rtk_fastlock_unlock(&dev->lock_word);
        return 0x80000002u;
    }

    uint32_t lock_st = rtk_fastlock_trylock(&dev->lock_word);
    if (lock_st != RTK_OK) {
        return lock_st;
    }

    uintptr_t mmio = dev->hdr.mmio_base;
    uint32_t status_reg = bus_read32(mmio + AOP_FUSE_STATUS_OFF);
    if ((status_reg & 0x40000000u) != 0u) {
        (void)rtk_fastlock_unlock(&dev->lock_word);
        return 0x80000006u;
    }

    uint32_t req_words = *io_count;
    uint32_t word_idx_start = addr_cmd >> 16;
    if (((req_words + word_idx_start) >> 13) != 0u) {
        (void)rtk_fastlock_unlock(&dev->lock_word);
        return 0x80000018u;
    }

    bus_write32(mmio + AOP_FUSE_CMD_OFF, 0xf8000000u);
    uint32_t read_trig = 1u | ((word_idx_start & 0x1fffu) << 1);
    bus_write32(mmio + AOP_FUSE_CMD_OFF, read_trig);

    uint32_t completed = 0u;
    uint32_t ret = RTK_OK;
    *io_count = 0u;

    while (completed < req_words) {
        uint32_t val = bus_read32(mmio + AOP_FUSE_DATA_OFF);
        uint32_t cmd_state = bus_read32(mmio + AOP_FUSE_CMD_OFF);
        if ((cmd_state & 0x10000000u) != 0u || (cmd_state >> 29) > 2u) {
            ret = 0x80000014u;
            break;
        }
        out_words[completed] = val;
        completed++;
    }

    if (ret == RTK_OK) {
        completed = req_words;
    }
    *io_count = completed;
    bus_write32(mmio + AOP_FUSE_CMD_OFF, 0u);
    (void)rtk_fastlock_unlock(&dev->lock_word);
    return ret;
}

/* @shim RVA 0x52da0 Fuse_ioctl */
uint32_t Fuse_ioctl(rtk_fuse_dev_t *dev, uint32_t cmd, void *arg0, void *arg1)
{
    (void)arg0;
    (void)arg1;
    if (cmd == 1u) {
        uint32_t st = rtk_device_claim_mmio(&dev->hdr);
        if (st != RTK_OK) {
            return st;
        }
        uint32_t lock_st = rtk_fastlock_trylock(&dev->lock_word);
        if (lock_st != RTK_OK) {
            return lock_st;
        }
        uintptr_t mmio = dev->hdr.mmio_base;
        if (bus_read32(mmio + AOP_FUSE_STATUS_OFF) != AOP_FUSE_READY_MAGIC) {
            (void)rtk_fastlock_unlock(&dev->lock_word);
            return RTK_ERR_NO_MMIO;
        }
        bus_set32(mmio + AOP_FUSE_STATUS_OFF, 0x80000000u);
        if ((bus_read32(mmio + AOP_FUSE_LOCK_OFF) & 1u) == 0u) {
            bus_set32(mmio + AOP_FUSE_LOCK_OFF, 1u);
        }
        (void)rtk_fastlock_unlock(&dev->lock_word);
        return RTK_OK;
    }
    if (cmd >= 2u && cmd <= 8u) {
        return RTK_OK;
    }
    return 0x80000000u;
}

/* @shim RVA 0x52d80 Fuse_readOTP */
uint32_t Fuse_readOTP(uint32_t *out_word)
{
    rtk_fuse_dev_t *fuse_dev = AOPF_VMA_PTR(rtk_fuse_dev_t *, AOPF_FUSE_OTP_DEV_VMA);
    uint32_t count = 1u;
    return Fuse_readWords(fuse_dev, 0x1f200001u, out_word, &count);
}

/* @provenance RVA 0x53518 sub_53518 */
uint32_t PMGR_lookupRegOffset(rtk_pmgr_dev_t *dev, uint32_t dev_id,
                              uint32_t idx, uint32_t out_pair[2])
{
    if ((dev->locked_flag & 1u) != 0u) {
        out_pair[0] = 0u;
        return 0x8000000bu;
    }
    if (dev_id == 0u) {
        out_pair[0] = 0u;
        return 0x8000000cu;
    }

    const rtk_pmgr_entry_t *cur = dev->entries;
    while (cur != NULL && cur->dev_id != 0u) {
        if (cur->dev_id == dev_id) {
            uint32_t off = cur->reg_offsets[idx];
            out_pair[0] = dev_id;
            out_pair[1] = off;
            return (off == 0xffffffffu) ? 0x80000003u : RTK_OK;
        }
        cur++;
    }
    out_pair[0] = 0u;
    return 0x8000000cu;
}

/* @shim RVA 0x52ef8 PMGR_ioctl */
uint32_t PMGR_ioctl(rtk_pmgr_dev_t *dev, uint32_t cmd, void *arg0, void *arg1)
{
    (void)arg0;
    (void)arg1;
    if (cmd >= 2u && cmd <= 5u) {
        return RTK_OK;
    }
    if (cmd == 8u) {
        return RTK_OK;
    }
    if (cmd == 1u) {
        uint32_t st = rtk_device_claim_mmio(&dev->hdr);
        if (st == RTK_OK) {
            dev->locked_flag = 0u;
        }
        return st;
    }
    return RTK_ERR_UNSUPPORTED;
}

/* @provenance RVA 0x52f3c sub_52F3C */
uint32_t PMGR_getPowerState(rtk_pmgr_dev_t *dev, uint32_t dev_id, uint32_t *out_state)
{
    uint32_t pair[2] = {0u, 0u};
    uint32_t st = PMGR_lookupRegOffset(dev, dev_id, 1u, pair);
    if (st != RTK_OK) {
        return st;
    }
    if (pair[1] == 0xffffffffu) {
        return 0x8000000bu;
    }

    uint32_t reg_val = bus_read32(dev->hdr.mmio_base + pair[1]);
    uint32_t ps_nibble = (reg_val >> 4) & 0x0fu;
    if (ps_nibble == 0u) {
        *out_state = 0u;
        return RTK_OK;
    }
    if (ps_nibble == 0x0fu) {
        *out_state = 2u;
        return RTK_OK;
    }
    if (ps_nibble == 4u) {
        *out_state = 1u;
        return RTK_OK;
    }
    *out_state = 3u;
    return 0x8000000au;
}

/* @provenance RVA 0x5a9e4 sub_5A9E4 */
uint32_t WDT_setRunning(rtk_wdt_dev_t *dev, int running)
{
    uintptr_t mmio = dev->hdr.mmio_base;
    uint32_t ctrl = bus_read32(mmio + AOP_WDT_CTRL_OFF) & ~AOP_WDT_CTRL_STOP_MASK;
    if (running == 0) {
        ctrl |= AOP_WDT_CTRL_STOP_MASK;
    }
    bus_write32(mmio + AOP_WDT_CTRL_OFF, ctrl);
    const rtk_wdt_vtable_t *vt = (const rtk_wdt_vtable_t *)dev->hdr.vt;
    return vt->kick(dev);
}

/* @provenance RVA 0x5aa02 sub_5AA02 */
uint32_t WDT_kick(rtk_wdt_dev_t *dev)
{
    uintptr_t mmio = dev->hdr.mmio_base;
    bus_set32(mmio + AOP_WDT_CTRL_OFF, AOP_WDT_CTRL_KICK);
    return RTK_OK;
}

/* @provenance RVA 0x5abec sub_5ABEC */
uint32_t WDT_updateBarkPrescaler(rtk_wdt_dev_t *dev, uint32_t prescaler_idx)
{
    uintptr_t mmio = dev->hdr.mmio_base;
    uint32_t ctrl = bus_read32(mmio + AOP_WDT_CTRL_OFF);
    const rtk_wdt_vtable_t *vt = (const rtk_wdt_vtable_t *)dev->hdr.vt;

    if ((ctrl & 0x8000u) == 0u) {
        (void)vt->set_running(dev, 0);
        ctrl = bus_read32(mmio + AOP_WDT_CTRL_OFF);
        ctrl = (ctrl & ~AOP_WDT_CTRL_BARK_MASK) | ((prescaler_idx & 7u) << AOP_WDT_CTRL_BARK_SHIFT);
        bus_write32(mmio + AOP_WDT_CTRL_OFF, ctrl);
        return vt->set_running(dev, 1);
    }

    ctrl = (ctrl & ~AOP_WDT_CTRL_BARK_MASK) | ((prescaler_idx & 7u) << AOP_WDT_CTRL_BARK_SHIFT);
    bus_write32(mmio + AOP_WDT_CTRL_OFF, ctrl);
    return vt->kick(dev);
}

/* @provenance RVA 0x5aa10 sub_5AA10 */
uint32_t WDT_setBarkTimeoutUs(rtk_wdt_dev_t *dev, uint64_t bark_us)
{
    for (uint32_t idx = 0u; idx < 8u; idx++) {
        uint64_t step_us = (uint64_t)(AOP_WDT_UNIT_US << idx);
        if (bark_us == step_us) {
            return WDT_updateBarkPrescaler(dev, idx);
        }
    }
    return RTK_ERR_INVALID_ARG;
}

/* @provenance RVA 0x5aaa8 sub_5AAA8 */
uint64_t WDT_getBarkTimeoutUs(rtk_wdt_dev_t *dev)
{
    uint32_t ctrl = bus_read32(dev->hdr.mmio_base + AOP_WDT_CTRL_OFF);
    uint32_t shift = (ctrl >> AOP_WDT_CTRL_BARK_SHIFT) & 7u;
    return (uint64_t)(AOP_WDT_UNIT_US << shift);
}

/* @provenance RVA 0x5aac0 sub_5AAC0 */
uint32_t WDT_setBiteEnable(rtk_wdt_dev_t *dev, int enable)
{
    if (dev->bark_cb == NULL && (dev->allow_bite_no_cb & 1u) == 0u) {
        return (enable != 0) ? RTK_ERR_NOT_RUNNING : RTK_OK;
    }
    uint32_t bits = (enable != 0)
        ? ((dev->bite_prescaler << AOP_WDT_CTRL_BITE_SHIFT) & AOP_WDT_CTRL_BITE_MASK)
        : 0u;
    uintptr_t mmio = dev->hdr.mmio_base;
    uint32_t ctrl = (bus_read32(mmio + AOP_WDT_CTRL_OFF) & ~AOP_WDT_CTRL_BITE_MASK) | bits;
    bus_write32(mmio + AOP_WDT_CTRL_OFF, ctrl);
    return RTK_OK;
}

/* @provenance RVA 0x5aaf8 sub_5AAF8 */
uint32_t WDT_setBiteTimeoutUs(rtk_wdt_dev_t *dev, uint64_t bite_us)
{
    uint32_t idx_found = 8u;
    for (uint32_t idx = 0u; idx < 8u; idx++) {
        if (bite_us == (uint64_t)(AOP_WDT_UNIT_US * idx)) {
            idx_found = idx;
            break;
        }
    }
    if (idx_found >= 8u) {
        return RTK_ERR_INVALID_ARG;
    }
    dev->bite_prescaler = idx_found;
    uintptr_t mmio = dev->hdr.mmio_base;
    uint32_t ctrl = bus_read32(mmio + AOP_WDT_CTRL_OFF) & ~AOP_WDT_CTRL_BITE_MASK;
    ctrl |= (idx_found << AOP_WDT_CTRL_BITE_SHIFT);
    bus_write32(mmio + AOP_WDT_CTRL_OFF, ctrl);
    return RTK_OK;
}

/* @provenance RVA 0x5ab8c sub_5AB8C */
uint64_t WDT_getBiteTimeoutUs(rtk_wdt_dev_t *dev)
{
    uint32_t ctrl = bus_read32(dev->hdr.mmio_base + AOP_WDT_CTRL_OFF);
    uint32_t idx = (ctrl >> AOP_WDT_CTRL_BITE_SHIFT) & 7u;
    return (uint64_t)(AOP_WDT_UNIT_US * idx);
}

/* @provenance RVA 0x5aba0 WDT_setCallback */
uint32_t WDT_setCallback(rtk_wdt_dev_t *dev, void (*cb)(void *), void *ctx)
{
    dev->bark_cb = cb;
    dev->bark_ctx = ctx;

    rtk_nvic_dev_t *nvic_dev = AOPF_VMA_PTR(rtk_nvic_dev_t *, AOPF_NVIC_DEV_VMA);
    if (cb == NULL) {
        (void)nvic_dev->vt->set_irq_enabled(nvic_dev, dev->bark_irq, 0);
    }
    (void)nvic_dev->vt->register_irq(nvic_dev, dev->bark_irq, dev->bark_cb, dev->bark_ctx);
    if (dev->bark_cb != NULL) {
        (void)nvic_dev->vt->set_irq_enabled(nvic_dev, dev->bark_irq, 1);
    }
    return RTK_OK;
}

/* @shim RVA 0x5a928 WDT_ioctl */
uint32_t WDT_ioctl(rtk_wdt_dev_t *dev, uint32_t cmd, void *arg0, void *arg1)
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
