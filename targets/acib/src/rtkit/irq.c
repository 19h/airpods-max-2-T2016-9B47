#include "acib/rtkit.h"
#include "acib/bus.h"
#include "acib/cpu.h"
#include "acib/memmap.h"
#include "acib/regs.h"

/*
 * RTKit interrupt masking, NVIC controller methods, SysTick dispatch,
 * and priority-bitmap thread scheduler reconstructed from 0x0e5844..0x0e6120.
 */

#define ACIB_RAM_NVIC_MAX_IRQS_ADDR   0x100002ECu
#define ACIB_RAM_SYSTICK_DISP_ADDR    0x10002530u
#define ACIB_RAM_CUR_THREAD_ADDR      0x10002568u

#define ACIB_RTK_ERR_INVALID_ARG      0x80000001u
#define ACIB_RTK_ERR_UNSUPPORTED      0x8000000Bu
#define ACIB_RTK_ERR_SPINLOCK_BUSY    0x00020002u

struct acib_rtk_thread_hdr {
    uint8_t  pad0[0x16];
    uint8_t  preemptible;
    uint8_t  pad1[0x11];
    void   **sp_slot;
};

struct acib_rtk_systick_vtbl {
    uintptr_t method0;
    uintptr_t method1;
    uintptr_t method2;
    int (*on_systick)(void *obj, uint32_t arg);
};

static inline uint32_t acib_read_max_irqs(void)
{
    return *(uint32_t *)acib_addr_ptr(ACIB_RAM_NVIC_MAX_IRQS_ADDR);
}

/* @provenance RVA 0x0e5844 sub_E5844 */
uint32_t rtk_irq_mask_save(void)
{
    uint32_t prev = acib_cpu_get_basepri();
    uint32_t mask = g_rtk_irq_mask_basepri;
    if (prev == 0u || prev > mask) {
        acib_cpu_set_basepri(mask);
    }
    return prev;
}

/* @provenance RVA 0x0e585c sub_E585C */
void rtk_irq_mask_restore(uint32_t prev_basepri)
{
    acib_cpu_set_basepri(prev_basepri);
}

/* @provenance RVA 0x0e5862 sub_E5862 */
uint32_t rtk_primask_disable_save(void)
{
    if (acib_cpu_get_primask() == 0u) {
        acib_cpu_set_primask(1u);
        return 1u;
    }
    return 0u;
}

/* @provenance RVA 0x0e5874 sub_E5874 */
void rtk_primask_restore(uint32_t was_enabled)
{
    if (was_enabled != 0u) {
        acib_cpu_set_primask(0u);
    }
}

/* @provenance RVA 0x0e587e sub_E587E */
void rtk_irq_enable_all(void)
{
    acib_cpu_set_primask(0u);
    acib_cpu_set_basepri(0u);
}

/* @provenance RVA 0x0e588c sub_E588C */
uint32_t rtk_irq_are_enabled(void)
{
    uint32_t basepri = acib_cpu_get_basepri();
    if (basepri != 0u && basepri <= g_rtk_irq_mask_basepri) {
        return 0u;
    }
    return (acib_cpu_get_primask() == 0u) ? 1u : 0u;
}

/* @provenance RVA 0x0e58ac sub_E58AC */
uint32_t rtk_irq_set_prio_group(uint32_t prio_bits)
{
    if (prio_bits < 4u || prio_bits > 15u) {
        return ACIB_RTK_ERR_INVALID_ARG;
    }
    uint32_t idx = (prio_bits >> 2) - 1u;
    g_rtk_irq_prio_group = prio_bits;
    g_rtk_irq_mask_basepri = 0x80u - (idx << 5);
    return 0u;
}

/* @provenance RVA 0x0e58dc sub_E58DC */
uint32_t rtk_cpu_in_thread_mode(void)
{
    return (acib_cpu_get_ipsr() == 0u) ? 1u : 0u;
}

/* @provenance RVA 0x0ef0dc outlined_sub_EF0DC */
uint32_t outlined_sub_EF0DC(uint32_t val)
{
    return acib_clz32(val) >> 5;
}

/* @provenance RVA 0x0e6104 sub_E6104 */
uint32_t rtk_spinlock_try_enter(uint32_t *lock_word)
{
    uint32_t prev_basepri = rtk_irq_mask_save();
    if (*lock_word != 0u) {
        return ACIB_RTK_ERR_SPINLOCK_BUSY;
    }
    *lock_word = prev_basepri + 1u;
    return 0u;
}

/* @provenance RVA 0x0e58f0 SysTick_Handler */
ACIB_SECTION(".pinned.systick")
int SysTick_Handler(void *exc_frame, uint32_t ipsr)
{
    (void)exc_frame;
    (void)ipsr;
#if !defined(ACIB_HOST_TEST)
    uintptr_t cur_sp;
    __asm__ volatile("mov %0, sp" : "=r"(cur_sp));
    while (cur_sp < ACIB_MSP_STACK_BASE) {
    }
#endif
    const struct acib_rtk_systick_vtbl **disp_obj =
        (const struct acib_rtk_systick_vtbl **)acib_addr_ptr(ACIB_RAM_SYSTICK_DISP_ADDR);
    return (*disp_obj)->on_systick(disp_obj, 0u);
}

/* @provenance RVA 0x0e5908 rtk_sched_switch */
void *rtk_sched_switch(void *saved_sp)
{
    if ((g_rtk_sched_need_resched & 1u) == 0u) {
        return saved_sp;
    }

    g_rtk_sched_need_resched = 0u;
    uint32_t prev_basepri = rtk_irq_mask_save();

    struct acib_rtk_thread_hdr **cur_slot =
        (struct acib_rtk_thread_hdr **)acib_addr_ptr(ACIB_RAM_CUR_THREAD_ADDR);
    struct acib_rtk_thread_hdr *cur = *cur_slot;
    void **old_sp_slot = NULL;
    void **new_sp_slot = NULL;

    if (cur->preemptible != 0u && (g_rtk_sched_preempted = 1u, g_rtk_ready_mask == 0u)) {
        old_sp_slot = NULL;
        new_sp_slot = NULL;
    } else {
        uint32_t top_prio = 31u - acib_clz32(g_rtk_ready_mask);
        struct acib_rtk_thread_hdr *next =
            (struct acib_rtk_thread_hdr *)g_rtk_ready_queues[2u * top_prio];
        if (next != cur) {
            g_rtk_sched_preempted = 1u;
            *cur_slot = next;
            old_sp_slot = cur->sp_slot;
            new_sp_slot = next->sp_slot;
            cur = next;
        }
        if ((g_rtk_sched_preempted & 1u) != 0u && cur->preemptible == 0u) {
            g_rtk_sched_preempted = 0u;
        }
    }

    rtk_irq_mask_restore(prev_basepri);

    if (old_sp_slot != NULL) {
        *old_sp_slot = saved_sp;
    }
    if (new_sp_slot != NULL) {
        return *new_sp_slot;
    }
    return saved_sp;
}

/* @provenance RVA 0x0e59a8 rtk_nvic_method_4 */
uint32_t rtk_nvic_method_4(void *self)
{
    (void)self;
    return acib_cpu_get_ipsr();
}

/* @provenance RVA 0x0e59b0 rtk_nvic_method_5 */
uint32_t rtk_nvic_method_5(void *self, uint32_t vec, int enable)
{
    (void)self;
    if (vec == 2u) {
        return 1u;
    }
    uint32_t max_vec = acib_read_max_irqs() + 16u;
    if (vec < 15u || vec >= max_vec) {
        return 0u;
    }
    if (vec == 15u) {
        return 1u;
    }

    uint32_t ext_irq = vec - 16u;
    uint32_t word_idx = ext_irq >> 5;
    uint32_t bit_mask = 1u << (ext_irq & 31u);
    uint32_t iser_addr = ACIB_NVIC_ISER0_ADDR + 4u * word_idx;
    uint32_t was_enabled = (acib_mmio_read32(iser_addr) & bit_mask) != 0u ? 1u : 0u;

    uint32_t primask = acib_cpu_get_primask();
    if (primask == 0u) {
        acib_cpu_set_primask(1u);
    }

    if (enable != 0 && was_enabled == 0u) {
        acib_mmio_write32(ACIB_NVIC_ICPR0_ADDR + 4u * word_idx, bit_mask);
        acib_mmio_write32(iser_addr, bit_mask);
    } else if (enable == 0 && was_enabled != 0u) {
        acib_mmio_write32(ACIB_NVIC_ICER0_ADDR + 4u * word_idx, bit_mask);
    }

    if (primask == 0u) {
        acib_cpu_set_primask(0u);
    }
    return was_enabled;
}

/* @provenance RVA 0x0e5a34 rtk_nvic_method_7 */
uint32_t rtk_nvic_method_7(void *self, uint32_t vec, acib_irq_handler_fn fn, uintptr_t ctx)
{
    (void)self;
    uint32_t max_vec = acib_read_max_irqs() + 16u;
    if (vec != 2u && (vec < 15u || vec >= max_vec)) {
        return ACIB_RTK_ERR_INVALID_ARG;
    }
    uint32_t slot = (vec == 2u) ? 0u : (vec - 14u);
    g_rtk_irq_contexts[slot] = ctx;
    g_rtk_irq_handlers[slot] = fn;
    return 0u;
}

/* @provenance RVA 0x0e5a78 rtk_nvic_method_8 */
uint32_t rtk_nvic_method_8(void *self, uint32_t vec, uint32_t prio)
{
    (void)self;
    uint32_t max_vec = acib_read_max_irqs() + 16u;
    if ((vec != 2u && (vec < 15u || vec >= max_vec)) || prio > 15u) {
        return ACIB_RTK_ERR_INVALID_ARG;
    }
    if (vec == 2u) {
        return ACIB_RTK_ERR_UNSUPPORTED;
    }

    uint32_t grp = (prio >> 2) - 1u;
    uint32_t hw_prio = (grp < 3u) ? (0x60u - 0x20u * grp) : 0x80u;

    uint32_t primask = acib_cpu_get_primask();
    if (primask == 0u) {
        acib_cpu_set_primask(1u);
    }

    if (vec == 15u) {
        uint32_t shpr3 = acib_mmio_read32(ACIB_SCB_SHPR3_ADDR);
        acib_mmio_write32(ACIB_SCB_SHPR3_ADDR, (shpr3 & 0x00FFFFFFu) | (hw_prio << 24));
    } else {
        uint32_t ext_irq = vec - 16u;
        uint32_t ipr_addr = ACIB_NVIC_IPR0_ADDR + (ext_irq & ~3u);
        uint32_t shift = (ext_irq & 3u) * 8u;
        uint32_t old_val = acib_mmio_read32(ipr_addr);
        uint32_t new_val = (old_val & ~(0xFFu << shift)) | (hw_prio << shift);
        acib_mmio_write32(ipr_addr, new_val);
    }

    if (primask == 0u) {
        acib_cpu_set_primask(0u);
    }
    return 0u;
}

/* @provenance RVA 0x0e5b0c rtk_nvic_dispatch_irq */
uint32_t rtk_nvic_dispatch_irq(void)
{
    uint32_t vec = acib_cpu_get_ipsr();
    if (vec == 2u) {
        if (g_rtk_irq_handlers[0] != NULL) {
            return g_rtk_irq_handlers[0](g_rtk_irq_contexts[0]);
        }
        return (uint32_t)(uintptr_t)g_rtk_irq_handlers;
    }
    uint32_t max_vec = acib_read_max_irqs() + 16u;
    if (vec >= 15u && vec < max_vec) {
        uint32_t slot = vec - 14u;
        acib_irq_handler_fn fn = g_rtk_irq_handlers[slot];
        if (fn != NULL) {
            return fn(g_rtk_irq_contexts[slot]);
        }
        return slot;
    }
    return vec;
}

/* @provenance RVA 0x0e5b50 IRQ16_Handler */
bool IRQ16_Handler(void *self, uint32_t vec)
{
    (void)self;
    if (vec == 2u) {
        (void)acib_mmio_read32(ACIB_SCB_ICSR_ADDR);
        return false;
    }
    uint32_t max_vec = acib_read_max_irqs() + 16u;
    if (vec < 15u || vec >= max_vec) {
        return false;
    }
    uint32_t icsr = acib_mmio_read32(ACIB_SCB_ICSR_ADDR);
    if (vec == 15u) {
        return (icsr & ACIB_SCB_ICSR_PENDSTSET) != 0u;
    }
    uint32_t ext_irq = vec - 16u;
    uint32_t ispr = acib_mmio_read32(ACIB_NVIC_ISPR0_ADDR + 4u * (ext_irq >> 5));
    return ((ispr >> (ext_irq & 31u)) & 1u) != 0u;
}

/* @provenance RVA 0x0e5ba0 rtk_nvic_method_15 */
uint32_t rtk_nvic_method_15(void *self, uint32_t vec)
{
    (void)self;
    uint32_t max_vec = acib_read_max_irqs() + 16u;
    if (vec < 15u || vec >= max_vec) {
        return ACIB_RTK_ERR_INVALID_ARG;
    }
    if (vec == 15u) {
        acib_mmio_write32(ACIB_SCB_ICSR_ADDR, ACIB_SCB_ICSR_PENDSTCLR);
    } else {
        uint32_t ext_irq = vec - 16u;
        acib_mmio_write32(ACIB_NVIC_ICPR0_ADDR + 4u * (ext_irq >> 5), 1u << (ext_irq & 31u));
    }
    return 0u;
}

/* @provenance RVA 0x0e5be8 rtk_nvic_method_6 */
uint32_t rtk_nvic_method_6(void *self, uint32_t vec)
{
    (void)self;
    uint32_t max_vec = acib_read_max_irqs() + 16u;
    if (vec > max_vec) {
        return ACIB_RTK_ERR_INVALID_ARG;
    }
    if (vec == 15u) {
        acib_mmio_write32(ACIB_SCB_ICSR_ADDR, ACIB_SCB_ICSR_PENDSTSET);
        return 0u;
    }
    if (vec == 14u) {
        acib_mmio_write32(ACIB_SCB_ICSR_ADDR, ACIB_SCB_ICSR_PENDSVSET);
        return 0u;
    }
    if (vec == 2u) {
        acib_mmio_write32(ACIB_SCB_ICSR_ADDR, ACIB_SCB_ICSR_NMIPENDSET);
        return 0u;
    }
    if (vec >= 16u) {
        uint32_t ext_irq = vec - 16u;
        acib_mmio_write32(ACIB_NVIC_ISPR0_ADDR + 4u * (ext_irq >> 5), 1u << (ext_irq & 31u));
        return 0u;
    }
    return ACIB_RTK_ERR_INVALID_ARG;
}

/* @provenance RVA 0x0e5c4c rtk_nvic_method_9 */
uint32_t rtk_nvic_method_9(void *self)
{
    (void)self;
    return 0u;
}
