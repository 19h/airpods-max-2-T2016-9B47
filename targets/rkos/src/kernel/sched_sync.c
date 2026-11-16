#include "rkos/rkos.h"

/*
 * Reconstructed RTKitOS scheduler switch (`0x34F4`), priority tail-queues
 * (`0x10668`, `0x109D0`), stack watermark checker (`0x108E4`), thread sanity
 * check (`0x106A0`), fast/recursive mutex lock & unlock (`0x10930`, `0x10938`,
 * `0x10A10`, `0x10A30`), memory region attribute table (`0x11376..0x11634`),
 * and canonical nullsub stubs (`0x42E0..0x11374`).
 */

#define RKOS_STACK_CANARY_U64 0x4B434154534B5452ULL /* "RTKSTACK" */

static inline uint32_t rkos_clz32(uint32_t val)
{
    if (val == 0u) {
        return 32u;
    }
    uint32_t n = 0u;
    while ((val & 0x80000000u) == 0u) {
        n++;
        val <<= 1;
    }
    return n;
}

/* @shim RVA 0x34f4 rtkit_sched_switch (pending_c: not shown equivalent to raw) */
uint32_t rtkit_sched_switch(uint32_t current_sp)
{
    uint32_t *resched_va = RKOS_VMA_PTR(uint32_t *, 0x2001DDE8u);
    if ((*resched_va & 1u) == 0u) {
        return current_sp;
    }

    *(uint8_t *)(void *)resched_va = 0u;
    uint32_t saved_basepri = rkos_irq_save_and_raise_basepri();

    uint32_t *cur_task_slot = RKOS_VMA_PTR(uint32_t *, RKOS_CURRENT_TASK_VA);
    uint32_t cur_va = *cur_task_slot;
    uint32_t *preempt_slot = RKOS_VMA_PTR(uint32_t *, 0x2001DDFCu);
    uint32_t ready_mask = *RKOS_VMA_PTR(const uint32_t *, 0x2001DCD0u);

    uint32_t save_sp_va = 0u;
    uint32_t load_sp_va = 0u;

    uint8_t cur_state = (cur_va != 0u) ? *RKOS_VMA_PTR(const uint8_t *, cur_va + 0x16u) : 0u;
    if (cur_state != 0u && (*preempt_slot = 1u, ready_mask == 0u)) {
        save_sp_va = 0u;
        load_sp_va = 0u;
    } else if (ready_mask != 0u) {
        uint32_t top_prio = 31u - rkos_clz32(ready_mask);
        uint32_t next_va = *RKOS_VMA_PTR(const uint32_t *, 0x2001DCD8u + 8u * top_prio);

        if (next_va != 0u && next_va != cur_va) {
            *preempt_slot = 1u;
            *cur_task_slot = next_va;
            if (cur_va != 0u) {
                save_sp_va = *RKOS_VMA_PTR(const uint32_t *, cur_va + 0x28u);
            }
            load_sp_va = *RKOS_VMA_PTR(const uint32_t *, next_va + 0x28u);
            cur_va = next_va;
        }
        uint8_t next_state = (cur_va != 0u) ? *RKOS_VMA_PTR(const uint8_t *, cur_va + 0x16u) : 0u;
        if ((*preempt_slot & 1u) != 0u && next_state == 0u) {
            *preempt_slot = 0u;
        }
    }

    rkos_arch_set_basepri(saved_basepri);
    if (save_sp_va != 0u) {
        *RKOS_VMA_PTR(uint32_t *, save_sp_va) = current_sp;
    }
    if (load_sp_va != 0u) {
        return *RKOS_VMA_PTR(const uint32_t *, load_sp_va);
    }
    return current_sp;
}

/* @provenance RVA 0x10668 sub_10668 */
uint32_t rkos_sched_tailq_enqueue(uint32_t node_va, uint32_t prio)
{
    uint32_t *node = RKOS_VMA_PTR(uint32_t *, node_va);
    uint32_t *ready_mask = RKOS_VMA_PTR(uint32_t *, 0x2001DCD0u);
    uint32_t *ready_cnt = RKOS_VMA_PTR(uint32_t *, 0x2001DCD4u);
    uint32_t head_slot_va = 0x2001DCD8u + 8u * prio;
    uint32_t *tail_ptr_slot = RKOS_VMA_PTR(uint32_t *, head_slot_va + 4u);

    node[0] = 0u;
    *ready_mask |= (1u << prio);

    uint32_t prev_tail_va = *tail_ptr_slot;
    *tail_ptr_slot = node_va;
    node[1] = prev_tail_va;
    *RKOS_VMA_PTR(uint32_t *, prev_tail_va) = node_va;
    (*ready_cnt)++;
    return 0x2001DCD4u;
}

/* @shim RVA 0x109d0 sub_109D0 (pending_c: not shown equivalent to raw) */
uint32_t rkos_sched_tailq_remove(uint32_t next_va, uint32_t prev_link_va, uint32_t prio)
{
    uint32_t *ready_mask = RKOS_VMA_PTR(uint32_t *, 0x2001DCD0u);
    uint32_t *ready_cnt = RKOS_VMA_PTR(uint32_t *, 0x2001DCD4u);
    uint32_t head_slot_va = 0x2001DCD8u + 8u * prio;

    if (next_va != 0u) {
        *RKOS_VMA_PTR(uint32_t *, next_va + 4u) = prev_link_va;
    } else {
        *RKOS_VMA_PTR(uint32_t *, head_slot_va + 4u) = prev_link_va;
    }

    *RKOS_VMA_PTR(uint32_t *, prev_link_va) = next_va;
    if (*RKOS_VMA_PTR(const uint32_t *, head_slot_va) == 0u) {
        *ready_mask &= ~(1u << prio);
    }
    (*ready_cnt)--;
    return 0x2001DCD4u;
}

/* @provenance RVA 0x108e4 sub_108E4 */
uint32_t rkos_stack_scan_unused(uint32_t stack_base_va, uint32_t stack_size)
{
    const uint64_t *words = (const uint64_t *)RKOS_VMA_RAW(stack_base_va, stack_size);
    while (stack_size >= 8u) {
        if (*words != RKOS_STACK_CANARY_U64) {
            return stack_size;
        }
        stack_size -= 8u;
        words++;
    }
    return 0u;
}

/* @shim RVA 0x106a0 thread_check_intr_or_stovfl (pending_c: not shown equivalent to raw) */
void thread_check_intr_or_stovfl(uint32_t thread_va)
{
    uint32_t ipsr = rkos_arch_get_ipsr();
    if (ipsr != 0u) {
        rtkit_panic("thintr:%u/%u/%u/%u", ipsr, thread_va, 0u, 0u);
    }
    if (thread_va != 0u) {
        const uint32_t *th = RKOS_VMA_PTR(const uint32_t *, thread_va);
        uint32_t stk_base = th[1];
        uint32_t stk_size = th[2];
        if (stk_base != 0u && stk_size >= 8u) {
            if (*RKOS_VMA_PTR(const uint64_t *, stk_base) != RKOS_STACK_CANARY_U64) {
                rtkit_panic("thstovfl:%u", thread_va);
            }
        }
    }
}

/* @provenance RVA 0x10a30 sub_10A30 */
int rkos_mutex_trylock(uint32_t mutex_va)
{
    uint32_t *word_ptr = RKOS_VMA_PTR(uint32_t *, mutex_va);
    uint32_t cur_task = *RKOS_VMA_PTR(const uint32_t *, RKOS_CURRENT_TASK_VA);
    if (cur_task == 0u) {
        cur_task = 4u;
    }

    uint32_t cur_val = *word_ptr;
    uint32_t low_flags = cur_val & 3u;
    if (cur_val == low_flags) {
        *word_ptr = low_flags | cur_task;
        rkos_arch_dsb();
        return 0;
    }
    if ((cur_val & ~3u) == cur_task) {
        return 0x20002;
    }
    return (int)0x80000006u;
}

/* @shim RVA 0x12f7c mutex_lock_slow (pending_c: not shown equivalent to raw) */
void mutex_lock_slow(uint32_t mutex_va)
{
    uint32_t prev = rkos_irq_save_and_raise_basepri();
    while (rkos_mutex_trylock(mutex_va) == (int)0x80000006u) {
        rkos_arch_set_basepri(prev);
        prev = rkos_irq_save_and_raise_basepri();
    }
    rkos_arch_set_basepri(prev);
}

/* @provenance RVA 0x10a10 mutex_lock */
uint32_t mutex_lock(uint32_t mutex_va)
{
    int res = rkos_mutex_trylock(mutex_va);
    if ((uint32_t)res == 0x80000006u) {
        mutex_lock_slow(mutex_va);
        return 0u;
    }
    return (uint32_t)res;
}

/* @shim RVA 0x12e38 sub_12E38 (pending_c: not shown equivalent to raw) */
int rkos_mutex_unlock_slow_impl(uint32_t mutex_va)
{
    uint32_t prev = rkos_irq_save_and_raise_basepri();
    uint32_t *word_ptr = RKOS_VMA_PTR(uint32_t *, mutex_va);
    *word_ptr &= 2u;
    rkos_arch_set_basepri(prev);
    return 0;
}

/* @provenance RVA 0x10938 mutex_unlock */
int mutex_unlock(uint32_t mutex_va)
{
    uint32_t *word_ptr = RKOS_VMA_PTR(uint32_t *, mutex_va);
    uint32_t cur_task = *RKOS_VMA_PTR(const uint32_t *, RKOS_CURRENT_TASK_VA);
    if (cur_task == 0u) {
        cur_task = 4u;
    }

    uint32_t keep_bit = *word_ptr & 2u;
    if (*word_ptr == (cur_task | keep_bit)) {
        *word_ptr = keep_bit;
        return 0;
    }
    return rkos_mutex_unlock_slow_impl(mutex_va);
}

/* @shim RVA 0x10930 sub_10930 (pending_c: not shown equivalent to raw) */
void rkos_spinlock_release(uint32_t lock_va, uint32_t prev_state)
{
    if (prev_state == 0u) {
        (void)mutex_unlock(lock_va);
    }
}

/* @provenance RVA 0x11634 sub_11634 */
uint32_t rkos_mem_region_lookup(uint32_t addr, uint32_t *out_idx)
{
    const rkos_mem_region_t *regions = RKOS_VMA_PTR(const rkos_mem_region_t *, 0x20001088u);
    for (uint32_t idx = 0u; idx < 9u; idx++) {
        uint32_t base = regions[idx].base;
        uint32_t size = regions[idx].size;
        if (size != 0u && addr >= base && addr < base + size) {
            *out_idx = idx;
            return 0u;
        }
    }
    return 0x8000000Cu;
}

/* @provenance RVA 0x1137a sub_1137A */
bool rkos_mem_region_is_ram(uint32_t addr)
{
    uint32_t idx = 0u;
    return rkos_mem_region_lookup(addr, &idx) == 0u && (idx & ~1u) == 6u;
}

/* @provenance RVA 0x11398 sub_11398 */
bool rkos_mem_region_is_ro(uint32_t addr)
{
    uint32_t idx = 0u;
    if (rkos_mem_region_lookup(addr, &idx) != 0u || idx > 8u) {
        return false;
    }
    return ((0x135u >> idx) & 1u) != 0u;
}

/* @provenance RVA 0x113bc sub_113BC */
uint32_t rkos_alloc_tagged_handle(uint32_t val0, uint32_t tag_flags)
{
    uint32_t obj_va = rtkit_malloc(0x14u);
    if (obj_va != 0u) {
        uint32_t *obj = RKOS_VMA_PTR(uint32_t *, obj_va);
        obj[0] = val0;
        obj[1] = (tag_flags & 0x0FFFFFFFu) | 0x80000000u;
        obj[3] = 0u;
        obj[4] = 0u;
    }
    return obj_va;
}

/* @provenance RVA 0x11528 sub_11528 */
uint32_t rkos_mem_region_ensure_init(void)
{
    uint32_t *init_state = RKOS_VMA_PTR(uint32_t *, 0x2001DE44u);
    uint32_t cur = *init_state;
    if ((cur - 1u) < 2u) {
        return cur - 1u;
    }

    *init_state = 1u;
    uint32_t table_va = *RKOS_VMA_PTR(const uint32_t *, 0x2001DE48u);
    uint32_t rc;
    if (table_va != 0u) {
        rc = rkos_mem_region_apply_table();
    } else {
        rc = rkos_mem_region_register(7u, 0x00000000u, 0x00017590u);
        if (rc == 0u) {
            rc = rkos_mem_region_register(8u, 0x20000000u, 0x0001F068u);
        }
    }

    if (rc == 0u) {
        for (uint32_t i = 0u; i < 8u; i++) {
            uint32_t b_i = *RKOS_VMA_PTR(const uint32_t *, 0x20001088u + 16u * i);
            uint32_t s_i = *RKOS_VMA_PTR(const uint32_t *, 0x2000108Cu + 16u * i);
            if (s_i == 0u) {
                continue;
            }
            for (uint32_t j = i + 1u; j < 9u; j++) {
                uint32_t b_j = *RKOS_VMA_PTR(const uint32_t *, 0x20001088u + 16u * j);
                uint32_t s_j = *RKOS_VMA_PTR(const uint32_t *, 0x2000108Cu + 16u * j);
                if (s_j != 0u && b_i + s_i > b_j && b_j + s_j > b_i) {
                    rtkit_panic("VmOv %d %d", i, j);
                }
            }
        }
    }

    *init_state = 2u;
    return 2u;
}

/* @shim RVA 0x113e0 sub_113E0 (pending_c: not shown equivalent to raw) */
uint32_t rkos_mem_region_register(uint32_t slot, uint32_t base, uint32_t size)
{
    (void)rkos_mem_region_ensure_init();
    if (slot > 8u) {
        return 0x80000001u;
    }

    uint32_t lock_state;
    if (rkos_arch_get_ipsr() != 0u || rkos_irq_are_enabled() == 0) {
        int tr = rkos_mutex_trylock(0x2001DDDCu);
        if (((uint32_t)tr & 0x90000000u) != 0u) {
            return (uint32_t)tr;
        }
        lock_state = (uint32_t)tr;
    } else {
        lock_state = mutex_lock(0x2001DDDCu);
    }

    uint32_t end_addr = base + size;
    for (uint32_t idx = 0u; idx < 9u; idx++) {
        uint32_t r_base = *RKOS_VMA_PTR(const uint32_t *, 0x20001088u + 16u * idx);
        uint32_t r_size = *RKOS_VMA_PTR(const uint32_t *, 0x2000108Cu + 16u * idx);
        uint32_t r_head = *RKOS_VMA_PTR(const uint32_t *, 0x20001090u + 16u * idx);
        if (r_head != 0u && end_addr > r_base && base < r_base + r_size) {
            uint32_t cur_va = r_head;
            do {
                const uint32_t *node = RKOS_VMA_PTR(const uint32_t *, cur_va);
                uint32_t n_base = node[0];
                uint32_t n_size = node[1] & 0x0FFFFFFFu;
                if (end_addr > n_base && base < n_base + n_size) {
                    rkos_spinlock_release(0x2001DDDCu, lock_state);
                    return 0x8000000Du;
                }
                cur_va = node[3];
            } while (cur_va != r_head);
        }
    }

    uint32_t *ent = RKOS_VMA_PTR(uint32_t *, 0x20001084u + 16u * slot);
    if (ent[3] == 0u) {
        uint32_t tag = ((slot - 5u) < 2u) ? (slot | 0x80000000u) : slot;
        ent[0] = tag;
        ent[1] = base;
        ent[2] = size;
        uint32_t h_va = rkos_alloc_tagged_handle(base, size);
        uint32_t *h = RKOS_VMA_PTR(uint32_t *, h_va);
        ent[3] = h_va;
        h[3] = h_va;
        h[4] = h_va;
        h[1] = (h[1] & 0x0FFFFFFFu) | 0x80000000u;
    } else {
        uint32_t h_va = rkos_alloc_tagged_handle(base, size);
        uint32_t *h = RKOS_VMA_PTR(uint32_t *, h_va);
        uint32_t n_base = h[0];
        uint32_t n_size = h[1] & 0x0FFFFFFFu;
        uint32_t head_va = ent[3];
        uint32_t cur_va = head_va;
        int inserted_at_end = 0;

        while (n_base >= *RKOS_VMA_PTR(const uint32_t *, cur_va)) {
            uint32_t prev_va = cur_va;
            cur_va = *RKOS_VMA_PTR(const uint32_t *, cur_va + 12u);
            if (cur_va == head_va) {
                h[3] = cur_va;
                *RKOS_VMA_PTR(uint32_t *, prev_va + 12u) = h_va;
                h[4] = prev_va;
                *RKOS_VMA_PTR(uint32_t *, cur_va + 16u) = h_va;
                inserted_at_end = 1;
                break;
            }
        }
        if (!inserted_at_end) {
            uint32_t prev_va = *RKOS_VMA_PTR(const uint32_t *, cur_va + 16u);
            h[3] = cur_va;
            h[4] = prev_va;
            *RKOS_VMA_PTR(uint32_t *, cur_va + 16u) = h_va;
            *RKOS_VMA_PTR(uint32_t *, prev_va + 12u) = h_va;
        }

        uint32_t old_base = ent[1];
        uint32_t max_end = old_base + ent[2];
        if (n_base >= old_base) {
            if (n_base + n_size > max_end) {
                max_end = n_base + n_size;
            }
            n_base = old_base;
        } else {
            ent[1] = n_base;
        }
        ent[2] = max_end - n_base;
    }

    rkos_spinlock_release(0x2001DDDCu, lock_state);
    return 0u;
}

/* @provenance RVA 0x11376 sub_11376 */
uint32_t rkos_mem_region_register_thunk(uint32_t slot, uint32_t base, uint32_t size)
{
    return rkos_mem_region_register(slot, base, size);
}

/* @shim RVA 0x11604 sub_11604 (pending_c: not shown equivalent to raw) */
uint32_t rkos_mem_region_apply_table(void)
{
    uint32_t table_va = *RKOS_VMA_PTR(const uint32_t *, 0x2001DE48u);
    if (table_va == 0u) {
        return 0u;
    }
    for (uint32_t off = 0u;; off += 12u) {
        const uint32_t *ent = RKOS_VMA_PTR(const uint32_t *, table_va + off);
        if (ent[1] == 0u && ent[2] == 0u) {
            break;
        }
        uint32_t res = rkos_mem_region_register(ent[0], ent[1], ent[2] - ent[1]);
        if (res != 0u) {
            return res;
        }
    }
    return 0u;
}

/* Canonical 2-byte `bx lr` nullsubs in `rkos` */
/* @provenance RVA 0x11374 nullsub_1 */
void rkos_nullsub_1(void) {}
/* @provenance RVA 0x42e0 nullsub_2 */
void rkos_nullsub_2(void) {}
/* @provenance RVA 0x42e2 nullsub_3 */
void rkos_nullsub_3(void) {}
/* @provenance RVA 0x42e4 nullsub_4 */
void rkos_nullsub_4(void) {}
/* @provenance RVA 0x4338 nullsub_5 */
void rkos_nullsub_5(void) {}
/* @provenance RVA 0x44c8 nullsub_6 */
void rkos_nullsub_6(void) {}
/* @provenance RVA 0x484c nullsub_7 */
void rkos_nullsub_7(void) {}
/* @provenance RVA 0x48b0 nullsub_8 */
void rkos_nullsub_8(void) {}
