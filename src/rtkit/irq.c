/*
 * irq.c -- interrupt masking, NVIC helpers and CPU idle/halt.
 * The firmware's threading.c hooks live in the adjacent threading.c.
 */
#include "btp/btp.h"
#include "btp/rtkit.h"

struct rtk_irq_state rtk_irq BTP_STATE(rtk_irq);
struct rtk_state     rtk     BTP_STATE(rtk);
struct rtk_mpu_state rtk_mpu BTP_STATE(mpu);

/* ---- priority configuration ------------------------------------------------ */

/* @provenance RVA 0x6210 rtk_irq_set_basepri_level */
uint32_t rtk_set_irq_priority_level(uint32_t level)
{
    uint32_t step = (level >> 2) - 1;       /* levels 4, 8, 12 */

    if (step > 2)
        return RTK_ERR_INVALID_PRIORITY;
    rtk_irq.priority_level = level;
    rtk_irq.basepri_level = RTK_BASEPRI_LEVEL_DEFAULT - 0x20 * step;
    return 0;
}

/* @provenance RVA 0x01A4 rtk_irq_configure_basepri */
uint32_t rtk_configure_irq_priority(uint32_t level)
{
    if (rtk_set_irq_priority_level(level) != 0)
        return BTP_ERR_INVALID;
    return 0;
}

/* ---- NVIC ------------------------------------------------------------------------ */

/* @provenance RVA 0x01B4 nvic_enable_irq */
uint32_t nvic_enable_irq(unsigned irq)
{
    uint32_t mask = 1u << irq;

    NVIC_ISER0 = mask;
    return mask;
}

/* @provenance RVA 0x01C4 nvic_disable_irq */
uint32_t nvic_disable_irq(unsigned irq)
{
    uint32_t mask = 1u << irq;

    NVIC_ICER0 = mask;
    return mask;
}

/* @provenance RVA 0x01D4 nvic_clear_pending_irq */
uint32_t nvic_clear_pending_irq(unsigned irq)
{
    uint32_t mask = 1u << (irq & 0x1F);

    NVIC_ICPR0 = mask;
    return mask;
}

/* ---- global masking ------------------------------------------------------------ */

/* @provenance RVA 0x01E8 irq_save_and_disable */
bool irq_save(void)
{
    uint32_t primask = cpu_get_primask();

    cpu_disable_irq();
    return (primask & 1u) == 0;
}

/* @provenance RVA 0x01F6 irq_restore */
bool irq_restore(bool enabled)
{
    if (enabled)
        cpu_enable_irq();
    return enabled;
}

/* @provenance RVA 0x01FC irq_enable */
void irq_enable(void)
{
    cpu_enable_irq();
}

/* ---- idle and halt --------------------------------------------------------------- */

static void rtk_wfi(void)
{
    cpu_dsb();
    cpu_isb();
    cpu_wfi();
    cpu_isb();
}

/* @provenance RVA 0x62D0 _rtk_cpu_idle_wfi */
uint32_t rtk_cpu_idle(void)
{
    uint32_t primask = cpu_get_primask();

    if (primask == 0) {
        /* Mask around WFI so a pending interrupt cannot be lost. */
        cpu_set_primask(1);
        rtk_wfi();
        cpu_set_primask(0);
    } else {
        rtk_wfi();
    }
    return primask;
}

/* @provenance RVA 0x020C rtk_cpu_idle */
uint32_t rtk_platform_idle(uint32_t unused)
{
    return rtk_cpu_idle();
}

/* @provenance RVA 0x6B22 _rtk_cpu_halt_wfi */
void rtk_cpu_halt(void)
{
    if (cpu_get_primask() == 0)
        cpu_set_primask(1);
    for (;;)
        rtk_wfi();
}

/* @provenance RVA 0x0108 _rtk_platform_halt */
void rtk_platform_halt(void)
{
    rtk_cpu_halt();
}

/* @provenance RVA 0x0110 _rtk_panic_abort */
void rtk_abort(void)
{
    rtk_platform_halt();
}

/* @provenance RVA 0x6430 _rtk_assert_fail */
void rtk_assert_failed(void)
{
    rtk_abort();
}

