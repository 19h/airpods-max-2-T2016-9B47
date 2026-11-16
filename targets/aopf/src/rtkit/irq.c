/*
 * irq.c — Critical section BASEPRI/PRIMASK control and ARMv7E-M NVIC interrupt
 * controller driver reconstructed from aopf.i64 (0x4E448..0x4EA60).
 */

#include <stddef.h>
#include <stdint.h>
#include "aopf/bus.h"
#include "aopf/cpu.h"
#include "aopf/libc.h"
#include "aopf/memmap.h"
#include "aopf/regs.h"
#include "aopf/rtkit.h"

#define AOPF_CRIT_BASEPRI_VMA        0x2089bed0u
#define AOPF_NVIC_NUM_EXT_IRQS_VMA   0x20819830u
#define AOPF_IRQ_FN_TABLE_VMA        0x2089ba10u
#define AOPF_IRQ_CTX_TABLE_VMA       0x2089bb50u

typedef void (*rtk_irq_fn_t)(void *ctx);

/* @provenance RVA 0x4e448 rtk_enter_critical */
uint32_t rtk_enter_critical(void)
{
    uint32_t prev_basepri = aopf_get_basepri();
    uint32_t ceil_prio = *AOPF_VMA_PTR(const uint32_t *, AOPF_CRIT_BASEPRI_VMA);
    if (prev_basepri == 0u || prev_basepri > ceil_prio) {
        aopf_set_basepri(ceil_prio);
    }
    return prev_basepri;
}

/* @provenance RVA 0x4e460 sub_4E460 */
void rtk_exit_critical(uint32_t saved_basepri)
{
    aopf_set_basepri(saved_basepri);
}

/* @shim RVA 0x4e504 sub_4E504 */
int rtk_in_exception_handler(void)
{
    return (aopf_get_ipsr() != 0u) ? 1 : 0;
}

/* @provenance RVA 0x4e4e4 sub_4E4E4 */
int rtk_are_irqs_enabled(void)
{
    uint32_t cur_basepri = aopf_get_basepri();
    if (cur_basepri != 0u) {
        uint32_t ceil_prio = *AOPF_VMA_PTR(const uint32_t *, AOPF_CRIT_BASEPRI_VMA);
        if (cur_basepri <= ceil_prio) {
            return 0;
        }
    }
    return (aopf_get_primask() == 0u) ? 1 : 0;
}

/* @provenance RVA 0x4e4d8 sub_4E4D8 */
void rtk_irq_enable_all(void)
{
    aopf_set_primask(0u);
    aopf_set_basepri(0u);
}

/* @provenance RVA 0x4e468 rtk_irq_save_if_thread */
int rtk_irq_save_if_thread(void)
{
    if (aopf_get_primask() != 0u) {
        return 0;
    }
    aopf_set_primask(1u);
    return 1;
}

/* @provenance RVA 0x4e4a8 sub_4E4A8 */
void rtk_irq_restore_if_saved(int saved_flag)
{
    if (saved_flag != 0) {
        aopf_set_primask(0u);
    }
}

/* @shim RVA 0x4e50c sub_4E50C */
void rtk_check_msp_guard(void)
{
#if defined(__arm__) || defined(__thumb__)
    uint32_t cur_sp;
    __asm__ volatile("mov %0, sp" : "=r"(cur_sp));
    if (cur_sp < AOPF_MSP_STACK_BOTTOM) {
        for (;;) {
            /* Hardware stack-underflow trap */
        }
    }
#endif
}

/* @shim RVA 0x4e614 nvic_get_cpsr */
uint32_t nvic_get_cpsr(rtk_nvic_dev_t *dev)
{
    (void)dev;
    return aopf_get_ipsr();
}

static uint32_t nvic_max_exc_num(void)
{
    return (*AOPF_VMA_PTR(const uint32_t *, AOPF_NVIC_NUM_EXT_IRQS_VMA)) + 16u;
}

/* @provenance RVA 0x4e61c nvic_set_irq_enabled */
int nvic_set_irq_enabled(rtk_nvic_dev_t *dev, uint32_t irq, int enable)
{
    (void)dev;
    if (irq == 2u) {
        return 1;
    }
    if (irq < 15u || irq >= nvic_max_exc_num()) {
        return 0;
    }
    if (irq == 15u) {
        return 1;
    }

    uint32_t ext_idx = irq - 16u;
    uint32_t word_off = (ext_idx >> 5) * 4u;
    uint32_t bit_mask = 1u << (ext_idx & 31u);

    uint32_t cur_iser = bus_read32(NVIC_ISER_BASE + word_off);
    int was_enabled = ((cur_iser & bit_mask) != 0u) ? 1 : 0;

    int saved = rtk_irq_save_if_thread();
    if (enable != 0 && !was_enabled) {
        bus_write32(NVIC_ICPR_BASE + word_off, bit_mask);
        bus_write32(NVIC_ISER_BASE + word_off, bit_mask);
    } else if (enable == 0 && was_enabled) {
        bus_write32(NVIC_ICER_BASE + word_off, bit_mask);
    }
    rtk_irq_restore_if_saved(saved);
    return was_enabled;
}

/* @provenance RVA 0x4e69c nvic_register_irq */
uint32_t nvic_register_irq(rtk_nvic_dev_t *dev, uint32_t irq, void (*fn)(void *), void *ctx)
{
    (void)dev;
    uint32_t slot;
    if (irq == 2u) {
        slot = 0u;
    } else if (irq >= 15u && irq < nvic_max_exc_num()) {
        slot = irq - 14u;
    } else {
        return RTK_ERR_INVALID_ARG;
    }

    rtk_irq_fn_t *fn_tbl = AOPF_VMA_PTR(rtk_irq_fn_t *, AOPF_IRQ_FN_TABLE_VMA);
    void **ctx_tbl = AOPF_VMA_PTR(void **, AOPF_IRQ_CTX_TABLE_VMA);
    ctx_tbl[slot] = ctx;
    fn_tbl[slot] = fn;
    return RTK_OK;
}

/* @provenance RVA 0x4e6e0 nvic_set_irq_priority */
uint32_t nvic_set_irq_priority(rtk_nvic_dev_t *dev, uint32_t irq, uint32_t prio)
{
    (void)dev;
    if (irq != 2u && (irq < 15u || irq >= nvic_max_exc_num())) {
        return RTK_ERR_INVALID_ARG;
    }
    if (prio > 15u) {
        return RTK_ERR_INVALID_ARG;
    }
    if (irq == 2u) {
        return 0x8000000bu;
    }

    uint32_t group_idx = (prio >> 2) - 1u;
    uint32_t hw_prio = (group_idx < 3u) ? (0x60u - (group_idx << 5)) : 0x80u;

    int saved = rtk_irq_save_if_thread();
    if (irq == 15u) {
        uint32_t shpr3 = bus_read32(SCB_SHPR3_ADDR);
        shpr3 = (shpr3 & 0x00ffffffu) | (hw_prio << 24);
        bus_write32(SCB_SHPR3_ADDR, shpr3);
    } else {
        uint32_t ext_idx = irq - 16u;
        uintptr_t ipr_word_addr = NVIC_IPR_BASE + (ext_idx & ~3u);
        uint32_t shift = (irq & 3u) * 8u;
        uint32_t word_val = bus_read32(ipr_word_addr);
        word_val = (word_val & ~(0xffu << shift)) | (hw_prio << shift);
        bus_write32(ipr_word_addr, word_val);
    }
    rtk_irq_restore_if_saved(saved);
    return RTK_OK;
}

/* @shim RVA 0x4e774 nvic_dispatch_active_irq */
void nvic_dispatch_active_irq(rtk_nvic_dev_t *dev)
{
    (void)dev;
    uint32_t active_exc = aopf_get_ipsr();
    rtk_irq_fn_t *fn_tbl = AOPF_VMA_PTR(rtk_irq_fn_t *, AOPF_IRQ_FN_TABLE_VMA);
    void **ctx_tbl = AOPF_VMA_PTR(void **, AOPF_IRQ_CTX_TABLE_VMA);

    if (active_exc == 2u) {
        if (fn_tbl[0] != NULL) {
            fn_tbl[0](ctx_tbl[0]);
        }
        return;
    }
    if (active_exc >= 15u && active_exc < nvic_max_exc_num()) {
        uint32_t slot = active_exc - 14u;
        if (fn_tbl[slot] != NULL) {
            fn_tbl[slot](ctx_tbl[slot]);
        }
    }
}

/* @provenance RVA 0x4e7b8 nvic_is_irq_pending */
int nvic_is_irq_pending(rtk_nvic_dev_t *dev, uint32_t irq)
{
    (void)dev;
    if (irq == 2u) {
        (void)bus_read32(SCB_ICSR_ADDR);
        return 0;
    }
    if (irq < 15u || irq >= nvic_max_exc_num()) {
        return 0;
    }
    uint32_t icsr = bus_read32(SCB_ICSR_ADDR);
    if (irq == 15u) {
        return (int)((icsr >> 26) & 1u);
    }
    uint32_t ext_idx = irq - 16u;
    uint32_t ispr = bus_read32(NVIC_ISPR_BASE + (ext_idx >> 5) * 4u);
    return (int)((ispr >> (ext_idx & 31u)) & 1u);
}

/* @provenance RVA 0x4e808 sub_4E808 */
uint32_t nvic_clear_irq_pending(rtk_nvic_dev_t *dev, uint32_t irq)
{
    (void)dev;
    if (irq < 15u || irq >= nvic_max_exc_num()) {
        return RTK_ERR_INVALID_ARG;
    }
    if (irq == 15u) {
        bus_write32(SCB_ICSR_ADDR, 0x02000000u);
        return RTK_OK;
    }
    uint32_t ext_idx = irq - 16u;
    bus_write32(NVIC_ICPR_BASE + (ext_idx >> 5) * 4u, 1u << (ext_idx & 31u));
    return RTK_OK;
}

/* @provenance RVA 0x4e850 nvic_trigger_irq */
uint32_t nvic_trigger_irq(rtk_nvic_dev_t *dev, uint32_t irq)
{
    (void)dev;
    if (irq > nvic_max_exc_num()) {
        return RTK_ERR_INVALID_ARG;
    }
    if (irq == 15u) {
        bus_write32(SCB_ICSR_ADDR, 0x04000000u);
        return RTK_OK;
    }
    if (irq == 14u) {
        bus_write32(SCB_ICSR_ADDR, 0x10000000u);
        return RTK_OK;
    }
    if (irq == 2u) {
        bus_write32(SCB_ICSR_ADDR, 0x80000000u);
        return RTK_OK;
    }
    if (irq < 16u) {
        return RTK_ERR_INVALID_ARG;
    }
    uint32_t ext_idx = irq - 16u;
    bus_write32(NVIC_ISPR_BASE + (ext_idx >> 5) * 4u, 1u << (ext_idx & 31u));
    return RTK_OK;
}

/* @shim RVA 0x4e8b4 nvic_nop */
uint32_t nvic_nop(rtk_nvic_dev_t *dev)
{
    (void)dev;
    return RTK_OK;
}

/* @shim RVA 0x4e8b8 nvic_ioctl */
uint32_t nvic_ioctl(rtk_nvic_dev_t *dev, uint32_t cmd, void *arg0, void *arg1)
{
    (void)arg0;
    (void)arg1;
    if (cmd >= 2u && cmd <= 8u) {
        return RTK_OK;
    }
    if (cmd == 1u) {
        return rtk_device_claim_mmio((rtk_dev_hdr_t *)(void *)dev);
    }
    return RTK_ERR_UNSUPPORTED;
}

/* @provenance RVA 0x4ea08 sub_4EA08 */
int nvic_is_irq_active(rtk_nvic_dev_t *dev, uint32_t irq)
{
    (void)dev;
    if (irq == 2u) {
        (void)bus_read32(SCB_SHCSR_ADDR);
        return (aopf_get_ipsr() == 2u) ? 1 : 0;
    }
    if (irq < 15u || irq >= nvic_max_exc_num()) {
        return 0;
    }
    uint32_t shcsr = bus_read32(SCB_SHCSR_ADDR);
    if (irq == 15u) {
        return (int)((shcsr >> 11) & 1u);
    }
    uint32_t ext_idx = irq - 16u;
    uint32_t iabr = bus_read32(NVIC_IABR_BASE + (ext_idx >> 5) * 4u);
    return (int)((iabr >> (ext_idx & 31u)) & 1u);
}

#if defined(__arm__) || defined(__thumb__)
#define AOPF_SYSTICK_IRQ_SEC __attribute__((section(".rva_4e518"), used))
#else
#define AOPF_SYSTICK_IRQ_SEC
#endif

/* @shim RVA 0x4e518 SysTick_IRQ_Handler */
AOPF_SYSTICK_IRQ_SEC
void SysTick_IRQ_Handler(void)
{
    rtk_nvic_dev_t *nvic_dev = AOPF_VMA_PTR(rtk_nvic_dev_t *, AOPF_NVIC_DEV_VMA);
    nvic_dispatch_active_irq(nvic_dev);
}

/*
 * Link-boundary stub for SVCall_PendSV_Handler (RVA 0x4e550 tracked as
 * pending_c in ledger.json until RTKit scheduler context switch is reconstructed).
 */
void rtk_svc_pendsv_dispatch(uint32_t *exc_frame, uint32_t exc_return)
{
    (void)exc_frame;
    (void)exc_return;
}
