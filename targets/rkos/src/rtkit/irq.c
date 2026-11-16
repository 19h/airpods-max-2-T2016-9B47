#include "rkos/rkos.h"

/*
 * Reconstructed ARMv7E-M NVIC, BASEPRI/PRIMASK masking, IRQ vector table
 * registration/dispatch, and recursive heap critical section routines
 * (`0x3470..0x3988`, `0x50B4`, `0x5124`).
 */

static inline uint32_t rkos_irq_count_read(void)
{
    return *RKOS_VMA_PTR(const uint32_t *, RKOS_IRQ_COUNT_VA);
}

/* @provenance RVA 0x3470 sub_3470 */
uint32_t rkos_irq_save_and_raise_basepri(void)
{
    uint32_t prev = rkos_arch_get_basepri();
    uint32_t target = *RKOS_VMA_PTR(const uint32_t *, 0x2001F060u);
    if (prev == 0u || prev > target) {
        rkos_arch_set_basepri(target);
    }
    return prev;
}

/* @shim RVA 0x3488 sub_3488 (pending_c: not shown equivalent to raw) */
int rkos_primask_disable_if_enabled(void)
{
    if (rkos_arch_get_primask() != 0u) {
        return 0;
    }
    rkos_arch_set_primask(1u);
    return 1;
}

/* @shim RVA 0x349a sub_349A (pending_c: not shown equivalent to raw) */
int rkos_irq_enable_all(void)
{
    rkos_arch_set_primask(0u);
    rkos_arch_set_basepri(0u);
    return 0;
}

/* @provenance RVA 0x34a8 sub_34A8 */
int rkos_irq_are_enabled(void)
{
    uint32_t bpri = rkos_arch_get_basepri();
    if (bpri != 0u && bpri <= *RKOS_VMA_PTR(const uint32_t *, 0x2001F060u)) {
        return 0;
    }
    return (rkos_arch_get_primask() == 0u) ? 1 : 0;
}

/* @shim RVA 0x357c sub_357C (pending_c: not shown equivalent to raw) */
void rkos_cpu_wfi_idle(void)
{
    for (;;) {
        rkos_arch_dsb();
        rkos_arch_isb();
        rkos_arch_wfi();
        rkos_arch_isb();
    }
}

/* @provenance RVA 0x3594 sub_3594 */
uint32_t rkos_get_ipsr_num(void)
{
    return rkos_arch_get_ipsr();
}

/* @shim RVA 0x35bc sub_35BC (pending_c: not shown equivalent to raw) */
int rkos_nvic_set_enable_state(int self_unused, uint32_t vec, int enable)
{
    (void)self_unused;
    uint32_t ext_irq = vec - 16u;
    uint32_t word_idx = ext_irq >> 5;
    uint32_t bit_mask = 1u << (ext_irq & 31u);

    uint32_t cur_iser = RKOS_REG32_READ(RKOS_NVIC_ISER_BASE + word_idx * 4u) & bit_mask;
    int was_enabled = (cur_iser != 0u) ? 1 : 0;

    uint32_t pmask = rkos_arch_get_primask();
    if (pmask == 0u) {
        rkos_arch_set_primask(1u);
    }

    if (enable != 0 && cur_iser == 0u) {
        RKOS_REG32_WRITE(RKOS_NVIC_ICPR_BASE + word_idx * 4u, bit_mask);
        RKOS_REG32_WRITE(RKOS_NVIC_ISER_BASE + word_idx * 4u, bit_mask);
    } else if (enable == 0 && cur_iser != 0u) {
        RKOS_REG32_WRITE(RKOS_NVIC_ICER_BASE + word_idx * 4u, bit_mask);
    }

    if (pmask == 0u) {
        rkos_arch_set_primask(0u);
    }
    return was_enabled;
}

/* @shim RVA 0x359c sub_359C (pending_c: not shown equivalent to raw) */
int rkos_nvic_is_enabled(uint32_t self_va, uint32_t vec)
{
    if (vec == 2u) {
        return 1;
    }
    uint32_t irq_cnt = rkos_irq_count_read();
    if (vec >= 15u && vec < irq_cnt + 16u) {
        if (vec == 15u) {
            return 1;
        }
        uint32_t ext_irq = vec - 16u;
        uint32_t word = RKOS_REG32_READ(RKOS_NVIC_ISER_BASE + (ext_irq >> 5) * 4u);
        (void)self_va;
        return ((word >> (ext_irq & 31u)) & 1u) != 0u ? 1 : 0;
    }
    return 0;
}

/* @provenance RVA 0x3620 sub_3620 */
uint32_t rkos_nvic_register_handler(uint32_t self_va, uint32_t vec,
                                    uint32_t fn_va, uint32_t ctx_va)
{
    (void)self_va;
    uint32_t irq_cnt = rkos_irq_count_read();
    if (vec != 2u && (vec < 15u || vec >= irq_cnt + 16u)) {
        return 0x80000001u;
    }

    uint32_t slot = (vec == 2u) ? 0u : (vec - 14u);
    uint32_t *handlers = RKOS_VMA_PTR(uint32_t *, 0x20000D50u);
    uint32_t *contexts = RKOS_VMA_PTR(uint32_t *, 0x20000E78u);
    contexts[slot] = ctx_va;
    handlers[slot] = fn_va;
    return 0u;
}

/* @provenance RVA 0x3664 sub_3664 */
uint32_t rkos_nvic_set_priority(uint32_t self_va, uint32_t vec, uint32_t prio_level)
{
    (void)self_va;
    uint32_t irq_cnt = rkos_irq_count_read();
    if ((vec != 2u && (vec < 15u || vec >= irq_cnt + 16u)) || prio_level > 15u) {
        return 0x80000001u;
    }
    if (vec == 2u) {
        return 0x8000000Bu;
    }

    uint32_t tier = (prio_level >> 2) - 1u;
    uint32_t hw_prio = (tier < 3u) ? (0x60u - 0x20u * tier) : 0x80u;

    uint32_t pmask = rkos_arch_get_primask();
    if (pmask == 0u) {
        rkos_arch_set_primask(1u);
    }

    if (vec == 15u) {
        uint32_t shpr3 = RKOS_REG32_READ(RKOS_SCB_SHPR3);
        RKOS_REG32_WRITE(RKOS_SCB_SHPR3, (shpr3 & 0x00FFFFFFu) | (hw_prio << 24));
    } else {
        uint32_t ext_irq = vec - 16u;
        uint32_t ipr_addr = RKOS_NVIC_IPR_BASE + (ext_irq & ~3u);
        uint32_t shift = (ext_irq & 3u) * 8u;
        uint32_t old_val = RKOS_REG32_READ(ipr_addr);
        uint32_t new_val = (old_val & ~(0xFFu << shift)) | (hw_prio << shift);
        RKOS_REG32_WRITE(ipr_addr, new_val);
    }

    if (pmask == 0u) {
        rkos_arch_set_primask(0u);
    }
    return 0u;
}

#ifdef RKOS_HOST
uint32_t rkos_host_invoke_irq_cb(uint32_t fn_va, uint32_t ctx_va);
#endif

/* @shim RVA 0x36f8 IRQ_CommonHandler (pending_c: not shown equivalent to raw) */
uint32_t IRQ_CommonHandler(void)
{
    uint32_t ipsr = rkos_arch_get_ipsr();
    uint32_t *handlers = RKOS_VMA_PTR(uint32_t *, 0x20000D50u);
    uint32_t *contexts = RKOS_VMA_PTR(uint32_t *, 0x20000E78u);

    if (ipsr == 2u) {
        uint32_t fn_va = handlers[0];
        if (fn_va != 0u) {
#ifdef RKOS_HOST
            return rkos_host_invoke_irq_cb(fn_va, contexts[0]);
#else
            int (*fn)(uint32_t) = (int (*)(uint32_t))(uintptr_t)fn_va;
            return (uint32_t)fn(contexts[0]);
#endif
        }
        return 0x20000D50u;
    }

    uint32_t irq_cnt = rkos_irq_count_read();
    if (ipsr >= 15u && ipsr < irq_cnt + 16u) {
        uint32_t slot = ipsr - 14u;
        uint32_t fn_va = handlers[slot];
        if (fn_va != 0u) {
#ifdef RKOS_HOST
            return rkos_host_invoke_irq_cb(fn_va, contexts[slot]);
#else
            int (*fn)(uint32_t) = (int (*)(uint32_t))(uintptr_t)fn_va;
            return (uint32_t)fn(contexts[slot]);
#endif
        }
        return slot;
    }
    return ipsr;
}

/* @provenance RVA 0x373c sub_373C */
int rkos_nvic_is_pending(uint32_t self_va, uint32_t vec)
{
    (void)self_va;
    if (vec == 2u) {
        return 0;
    }
    uint32_t irq_cnt = rkos_irq_count_read();
    if (vec >= 15u && vec < irq_cnt + 16u) {
        if (vec == 15u) {
            return (RKOS_REG32_READ(RKOS_SCB_ICSR) & RKOS_ICSR_PENDSTSET) != 0u ? 1 : 0;
        }
        uint32_t ext_irq = vec - 16u;
        uint32_t ispr = RKOS_REG32_READ(RKOS_NVIC_ISPR_BASE + (ext_irq >> 5) * 4u);
        return ((ispr >> (ext_irq & 31u)) & 1u) != 0u ? 1 : 0;
    }
    return 0;
}

/* @provenance RVA 0x3788 sub_3788 */
uint32_t rkos_nvic_clear_pending(uint32_t self_va, uint32_t vec)
{
    (void)self_va;
    uint32_t irq_cnt = rkos_irq_count_read();
    if (vec < 15u || vec >= irq_cnt + 16u) {
        return 0x80000001u;
    }
    if (vec == 15u) {
        RKOS_REG32_WRITE(RKOS_SCB_ICSR, RKOS_ICSR_PENDSTCLR);
    } else {
        uint32_t ext_irq = vec - 16u;
        RKOS_REG32_WRITE(RKOS_NVIC_ICPR_BASE + (ext_irq >> 5) * 4u,
                         1u << (ext_irq & 31u));
    }
    return 0u;
}

/* @provenance RVA 0x37d0 sub_37D0 */
uint32_t rkos_nvic_set_pending(uint32_t self_va, uint32_t vec)
{
    (void)self_va;
    uint32_t irq_cnt = rkos_irq_count_read();
    if (vec > irq_cnt + 16u) {
        return 0x80000001u;
    }
    if (vec == 2u) {
        RKOS_REG32_WRITE(RKOS_SCB_ICSR, RKOS_ICSR_NMIPENDSET);
        return 0u;
    }
    if (vec == 14u) {
        RKOS_REG32_WRITE(RKOS_SCB_ICSR, RKOS_ICSR_PENDSVSET);
        return 0u;
    }
    if (vec == 15u) {
        RKOS_REG32_WRITE(RKOS_SCB_ICSR, RKOS_ICSR_PENDSTSET);
        return 0u;
    }
    if (vec >= 16u) {
        uint32_t ext_irq = vec - 16u;
        RKOS_REG32_WRITE(RKOS_NVIC_ISPR_BASE + (ext_irq >> 5) * 4u,
                         1u << (ext_irq & 31u));
        return 0u;
    }
    return 0x80000001u;
}

/* @provenance RVA 0x3834 sub_3834 */
int rkos_nvic_get_noop(void)
{
    return 0;
}

/* @provenance RVA 0x3988 sub_3988 */
int rkos_nvic_is_active(uint32_t self_va, uint32_t vec)
{
    (void)self_va;
    if (vec == 2u) {
        return (rkos_arch_get_ipsr() == 2u) ? 1 : 0;
    }
    uint32_t irq_cnt = rkos_irq_count_read();
    if (vec >= 15u && vec < irq_cnt + 16u) {
        if (vec == 15u) {
            return (RKOS_REG32_READ(RKOS_SCB_SHCSR) & RKOS_SHCSR_SYSTICKACT) != 0u ? 1 : 0;
        }
        uint32_t ext_irq = vec - 16u;
        uint32_t iabr = RKOS_REG32_READ(RKOS_NVIC_IABR_BASE + (ext_irq >> 5) * 4u);
        return ((iabr >> (ext_irq & 31u)) & 1u) != 0u ? 1 : 0;
    }
    return 0;
}

/* @shim RVA 0x50b4 crit_enter (pending_c: not shown equivalent to raw) */
uint32_t crit_enter(void)
{
    uint32_t lock_res = mutex_lock(0x2001DE24u);
    uint32_t *kheap_ptr = RKOS_VMA_PTR(uint32_t *, RKOS_STATE_META_BASE);

    if (*kheap_ptr == 0u) {
        *kheap_ptr = rkos_heap_init_struct(0x2001DC38u);
        (void)rkos_kernel_heap_add_region(0x20000BA4u, 0u);
        (void)rkos_kernel_heap_add_region(RKOS_STATE_ARENA_BASE, 0x1C000u);
    }
    return lock_res;
}

/* @shim RVA 0x5124 crit_exit (pending_c: not shown equivalent to raw) */
void crit_exit(uint32_t state)
{
    rkos_spinlock_release(0x2001DE24u, state);
}
