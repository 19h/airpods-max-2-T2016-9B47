#include "rkof/cpu.h"
#include "rkof/memmap.h"
#include "rkof/rtkit.h"

/* @provenance RVA 0x16f8c rtkit_sched_switch */
uintptr_t rtkit_sched_switch(uintptr_t current_sp)
{
    uint8_t *pending_flag = RKOF_PTR(uint8_t *, STATE_SCHED_PENDING_BASE);
    if ((*pending_flag & 1u) == 0u) {
        return current_sp;
    }

    *pending_flag = 0u;
    uint32_t saved_basepri = rtkit_enter_critical();

    uint32_t ready_bitmap = *RKOF_PTR(const uint32_t *, STATE_SCHED_BITMAP_BASE);
    uintptr_t *cur_thread_slot = RKOF_PTR(uintptr_t *, RKOF_DATA_CURRENT_THREAD_ADDR);
    uint8_t *cur_thread = RKOF_PTR(uint8_t *, *cur_thread_slot);
    uint8_t *sched_ctx = RKOF_PTR(uint8_t *, STATE_SCHED_CTX_BASE);

    uintptr_t *save_sp_slot = NULL;
    const uintptr_t *load_sp_slot = NULL;

    if (cur_thread[0x16] != 0u) {
        sched_ctx[0x7D] = 1u;
        if (ready_bitmap == 0u) {
            cpu_write_basepri(saved_basepri);
            return current_sp;
        }
    }

    uint32_t top_prio = (31u - cpu_clz32(ready_bitmap)) & 0xFFu;
    const uintptr_t *runq_table = RKOF_PTR(const uintptr_t *, STATE_SCHED_BITMAP_BASE + 8u);
    uintptr_t next_thread_addr = runq_table[top_prio * 2u];
    uint8_t *next_thread = RKOF_PTR(uint8_t *, next_thread_addr);

    if (next_thread != cur_thread) {
        sched_ctx[0x7D] = 1u;
        *cur_thread_slot = next_thread_addr;
        uintptr_t save_addr = *(const uintptr_t *)(const void *)(cur_thread + 0x28u);
        uintptr_t load_addr = *(const uintptr_t *)(const void *)(next_thread + 0x28u);
        save_sp_slot = (save_addr != 0u) ? RKOF_PTR(uintptr_t *, save_addr) : NULL;
        load_sp_slot = (load_addr != 0u) ? RKOF_PTR(const uintptr_t *, load_addr) : NULL;
        cur_thread = next_thread;
    }

    if ((sched_ctx[0x7D] & 1u) != 0u && cur_thread[0x16] == 0u) {
        sched_ctx[0x7D] = 0u;
    }

    cpu_write_basepri(saved_basepri);

    if (save_sp_slot != NULL) {
        *save_sp_slot = current_sp;
    }
    if (load_sp_slot != NULL) {
        current_sp = *load_sp_slot;
    }
    return current_sp;
}

/* @provenance RVA 0x1a2994 sub_1A2994 */
uint32_t sub_1A2994(uint8_t *thread_obj)
{
    if (thread_obj[0x16] == 0u) {
        return 0u;
    }

    thread_obj[0x16] = 0u;
    uintptr_t cur_addr = *RKOF_PTR(const uintptr_t *, RKOF_DATA_CURRENT_THREAD_ADDR);
    uint8_t *cur_thread = (cur_addr == 4u) ? (uint8_t *)(uintptr_t)4u : RKOF_PTR(uint8_t *, cur_addr);
    if (cur_thread == thread_obj) {
        return 0u;
    }

    uint8_t cur_yield;
    uint8_t cur_prio;
    if (cur_addr == 4u) {
        cur_yield = *RKOF_PTR(const uint8_t *, 4u + 0x16u);
        if (cur_yield != 0u) {
            return 1u;
        }
        cur_prio = 0u;
    } else {
        cur_yield = cur_thread[0x16];
        if (cur_yield != 0u) {
            return 1u;
        }
        cur_prio = cur_thread[0x14];
    }

    return (thread_obj[0x14] > cur_prio) ? 1u : 0u;
}
