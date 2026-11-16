#include "rkof/cpu.h"
#include "rkof/memmap.h"
#include "rkof/rtkit.h"

/* @provenance RVA 0x16f08 rtkit_enter_critical */
uint32_t rtkit_enter_critical(void)
{
    uint32_t prev_basepri = cpu_read_basepri();
    uint32_t mask_prio = *RKOF_PTR(const uint32_t *, STATE_IRQ_PRIO_BASE);
    if (prev_basepri == 0u || prev_basepri > mask_prio) {
        cpu_write_basepri(mask_prio);
    }
    return prev_basepri;
}

/* @provenance RVA 0x16f20 sub_16F20 */
void sub_16F20(uint32_t saved_basepri)
{
    cpu_write_basepri(saved_basepri);
}

/* @provenance RVA 0x16f26 sub_16F26 */
uint32_t sub_16F26(void)
{
    uint32_t pm = cpu_read_primask();
    if (pm != 0u) {
        return 0u;
    }
    cpu_write_primask(1u);
    return 1u;
}

/* @provenance RVA 0x16f38 sub_16F38 */
void sub_16F38(void)
{
    cpu_write_primask(0u);
    cpu_write_basepri(0u);
}

/* @provenance RVA 0x16f44 sub_16F44 */
uint32_t sub_16F44(void)
{
    uint32_t basepri = cpu_read_basepri();
    if (basepri != 0u) {
        uint32_t mask_prio = *RKOF_PTR(const uint32_t *, STATE_IRQ_PRIO_BASE);
        if (basepri <= mask_prio) {
            return 0u;
        }
    }
    return (cpu_read_primask() == 0u) ? 1u : 0u;
}

/* @provenance RVA 0x17024 sub_17024 */
RKOF_NORETURN void sub_17024(void)
{
    for (;;) {
        cpu_dsb();
        cpu_isb();
        cpu_wfi();
        cpu_isb();
#if defined(RKOF_HOST_BUILD)
        if (g_rkof_host_cpu.wfi_count >= 1u) {
            __builtin_unreachable();
        }
#endif
    }
}

/* @provenance RVA 0x17064 sub_17064 */
uint32_t sub_17064(uint32_t bit_seed, uint32_t exc_num, uint32_t enable)
{
    uint32_t irq_idx = exc_num - 16u;
    uint32_t bit_pos = irq_idx & 0x1Fu;
    uint32_t word_idx = irq_idx >> 5u;
    uint32_t bit_mask = bit_seed << bit_pos;

    uintptr_t iser_addr = NVIC_ISER_BASE + (word_idx << 2u);
    uint32_t cur_iser = MMIO32_READ(iser_addr);
    uint32_t was_set = cur_iser & bit_mask;
    uint32_t ret = (was_set != 0u) ? 1u : 0u;

    uint32_t saved_pm = cpu_read_primask();
    if (saved_pm == 0u) {
        cpu_write_primask(1u);
    }

    if (enable != 0u && was_set == 0u) {
        MMIO32_WRITE(iser_addr + 0x180u, bit_mask); /* NVIC_ICPR */
        MMIO32_WRITE(iser_addr, bit_mask);          /* NVIC_ISER */
    } else if (enable == 0u && was_set != 0u) {
        MMIO32_WRITE(iser_addr + 0x80u, bit_mask);  /* NVIC_ICER */
    }

    if (saved_pm == 0u) {
        cpu_write_primask(0u);
    }
    return ret;
}

/* @provenance RVA 0x170c8 sub_170C8 */
uint32_t sub_170C8(void *unused_ctx, uint32_t exc_num, uintptr_t handler_fn, uintptr_t handler_arg)
{
    (void)unused_ctx;
    uint32_t slot;
    if (exc_num == 2u) {
        slot = 0u;
    } else {
        if (exc_num < 15u) {
            return 0x80000001u;
        }
        uint32_t max_ext = *RKOF_PTR(const uint32_t *, RKOF_DATA_MAX_EXT_IRQS_ADDR);
        if (exc_num >= max_ext + 16u) {
            return 0x80000001u;
        }
        slot = exc_num - 14u;
    }

    uintptr_t *arg_table = RKOF_PTR(uintptr_t *, STATE_IRQ_ARG_BASE);
    uintptr_t *fn_table = RKOF_PTR(uintptr_t *, STATE_IRQ_FN_BASE);
    arg_table[slot] = handler_arg;
    fn_table[slot] = handler_fn;
    return 0u;
}

/* @provenance RVA 0x1710c sub_1710C */
uint32_t sub_1710C(void *unused_ctx, uint32_t exc_num, uint32_t logical_prio)
{
    (void)unused_ctx;
    if (exc_num != 2u) {
        if (exc_num < 15u) {
            return 0x80000001u;
        }
        uint32_t max_ext = *RKOF_PTR(const uint32_t *, RKOF_DATA_MAX_EXT_IRQS_ADDR);
        if (exc_num >= max_ext + 16u) {
            return 0x80000001u;
        }
    }

    if (logical_prio > 15u) {
        return 0x80000001u;
    }

    uint32_t band = (logical_prio >> 2u) - 1u;
    uint32_t hw_prio = (band < 3u) ? (0x60u - (band << 5u)) : 0x80u;

    if (exc_num == 2u) {
        return 0x8000000Bu;
    }

    uint32_t saved_pm = cpu_read_primask();
    if (saved_pm == 0u) {
        cpu_write_primask(1u);
    }

    if (exc_num == 15u) {
        uint32_t shpr3 = MMIO32_READ(SCB_SHPR3_ADDR);
        shpr3 = (shpr3 & 0x00FFFFFFu) | (hw_prio << 24u);
        MMIO32_WRITE(SCB_SHPR3_ADDR, shpr3);
    } else {
        uint32_t irq_idx = exc_num - 16u;
        uint32_t shift = (exc_num & 3u) << 3u;
        uintptr_t ipr_word_addr = NVIC_IPR_BASE + (irq_idx & ~3u);
        uint32_t ipr_val = MMIO32_READ(ipr_word_addr);
        ipr_val = (ipr_val & ~(0xFFu << shift)) | (hw_prio << shift);
        MMIO32_WRITE(ipr_word_addr, ipr_val);
    }

    if (saved_pm == 0u) {
        cpu_write_primask(0u);
    }
    return 0u;
}

/* @provenance RVA 0x175f0 sub_175F0 */
uint32_t sub_175F0(uint32_t *lock_word)
{
    uint32_t prev_basepri = rtkit_enter_critical();
    if (*lock_word != 0u) {
        return 0x00020002u;
    }
    *lock_word = prev_basepri + 1u;
    return 0u;
}

/* @provenance RVA 0x1760c sub_1760C */
void sub_1760C(uint32_t *lock_word)
{
    uint32_t held_val = *lock_word;
    *lock_word = 0u;
    cpu_write_basepri(held_val - 1u);
}
