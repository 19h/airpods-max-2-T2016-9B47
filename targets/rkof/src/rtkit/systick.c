#include "rkof/cpu.h"
#include "rkof/libc.h"
#include "rkof/memmap.h"
#include "rkof/rtkit.h"

extern uint64_t sub_1A3204(void *timer_dev);

/* @provenance RVA 0x18a494 sub_18A494 */
uint32_t sub_18A494(struct rkof_systick_dev *dev, uint32_t unused_r1, uint64_t reload_ticks)
{
    (void)unused_r1;
    if (dev->ticks_per_us == 0u && dev->freq_hz == 0u && dev->ticks_per_100ns == 0u) {
        return 0x80000001u;
    }

    dev->oneshot_mode = 0u;
    if (reload_ticks == 0u) {
        reload_ticks = 1u;
    }
    sub_18A5E0(dev, (uint32_t)reload_ticks, reload_ticks);
    return 0u;
}

/* @provenance RVA 0x18a4d0 sub_18A4D0 */
uint32_t sub_18A4D0(struct rkof_systick_dev *dev, uint32_t unused_r1, uint64_t period_ticks)
{
    (void)unused_r1;
    if (dev->ticks_per_us == 0u && dev->freq_hz == 0u && dev->ticks_per_100ns == 0u) {
        return 0x80000001u;
    }

    if (period_ticks < 2u || period_ticks > 0x01000000ULL) {
        return 0x80000001u;
    }

    dev->oneshot_mode = 1u;
    sub_18A5E0(dev, 0u, period_ticks - 1u);
    return 0u;
}

/* @provenance RVA 0x18a510 sub_18A510 */
uint32_t sub_18A510(struct rkof_systick_dev *dev, uint64_t *remaining_out)
{
    uint32_t saved_basepri = rtkit_enter_critical();
    uint32_t csr = MMIO32_READ(SYST_CSR_ADDR);
    if ((csr & SYST_CSR_ENABLE) == 0u) {
        cpu_write_basepri(saved_basepri);
        return 0x80000007u;
    }

    uint32_t cvr = MMIO32_READ(SYST_CVR_ADDR);
    uint64_t rem = ((uint64_t)dev->rem_ticks_hi << 32u) | (uint64_t)dev->rem_ticks_lo;
    uint64_t total = rem + (uint64_t)cvr;

    if ((dev->oneshot_mode & 1u) != 0u && total == 0u) {
        uint32_t rvr = MMIO32_READ(SYST_RVR_ADDR);
        total = (uint64_t)(rvr + 1u);
    }

    cpu_write_basepri(saved_basepri);
    *remaining_out = total;
    return 0u;
}

/* @provenance RVA 0x18a568 sub_18A568 */
uint64_t sub_18A568(const struct rkof_systick_dev *dev, uint32_t unused_r1, uint64_t ticks)
{
    (void)unused_r1;
    uint32_t denom;
    uint64_t scaled;

    if (dev->ticks_per_100ns != 0u) {
        denom = dev->ticks_per_100ns;
        scaled = ticks * 10000ULL;
    } else if (dev->freq_hz != 0u) {
        denom = dev->freq_hz;
        scaled = ticks * 1000000ULL;
    } else {
        denom = dev->ticks_per_us;
        scaled = ticks;
    }

    return sub_DD68(scaled + (uint64_t)(denom - 1u), (uint64_t)denom);
}

/* @provenance RVA 0x18a59c sub_18A59C */
uint64_t sub_18A59C(const struct rkof_systick_dev *dev, uint32_t unused_r1, uint64_t duration_us)
{
    (void)unused_r1;
    if (dev->ticks_per_100ns != 0u) {
        uint64_t prod = duration_us * (uint64_t)dev->ticks_per_100ns + 9999ULL;
        return sub_DD68(prod, 10000ULL);
    }
    if (dev->freq_hz != 0u) {
        uint64_t prod = duration_us * (uint64_t)dev->freq_hz + 999999ULL;
        return sub_DD68(prod, 1000000ULL);
    }
    return duration_us * (uint64_t)dev->ticks_per_us;
}

/* @provenance RVA 0x18a5e0 sub_18A5E0 */
void sub_18A5E0(struct rkof_systick_dev *dev, uint32_t unused_r1, uint64_t ticks)
{
    (void)unused_r1;
    uint32_t saved_basepri = rtkit_enter_critical();

    MMIO32_WRITE(SYST_CSR_ADDR, 0u);
    uint32_t rvr = (ticks < (uint64_t)SYST_RVR_MAX) ? (uint32_t)ticks : SYST_RVR_MAX;
    MMIO32_WRITE(SYST_RVR_ADDR, rvr);
    MMIO32_WRITE(SYST_CVR_ADDR, 0u);

    uint64_t rem = ticks - (uint64_t)rvr;
    dev->rem_ticks_lo = (uint32_t)rem;
    dev->rem_ticks_hi = (uint32_t)(rem >> 32u);

    uint32_t csr_cfg = ((dev->ext_clk_flag & 1u) != 0u)
        ? (SYST_CSR_ENABLE | SYST_CSR_TICKINT)
        : (SYST_CSR_ENABLE | SYST_CSR_TICKINT | SYST_CSR_CLKSOURCE);
    MMIO32_WRITE(SYST_CSR_ADDR, csr_cfg);

    cpu_write_basepri(saved_basepri);
}

/* @provenance RVA 0x18a134 sub_18A134 */
void sub_18A134(uint64_t delay_ticks)
{
    if ((int64_t)delay_ticks <= 0) {
        return;
    }

    cpu_dsb();
    cpu_isb();
    void *timer_dev = RKOF_PTR(void *, RKOF_DATA_TIMER_DEV_PTR_ADDR);
    uint64_t start = sub_1A3204(timer_dev);
    for (;;) {
        uint64_t now = sub_1A3204(timer_dev);
        uint64_t elapsed = now - start;
        if ((int64_t)(delay_ticks - elapsed) < 0) {
            break;
        }
    }
    cpu_isb();
}
