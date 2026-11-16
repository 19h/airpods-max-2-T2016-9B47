/*
 * threading.c — RTKit fastlocks, spinlocks, stack watermark scanner, and
 * priority ready-queue bitmap helpers reconstructed from aopf.i64.
 */

#include <stddef.h>
#include <stdint.h>
#include "aopf/cpu.h"
#include "aopf/memmap.h"
#include "aopf/regs.h"
#include "aopf/rtkit.h"

#define AOPF_CUR_THREAD_VMA          0x20819844u
#define RTK_ERR_LOCK_CONTENDED       0x80000006u
#define RTK_LOCK_RECURSIVE_OK        0x00020002u

/* @provenance RVA 0x4ebc0 sub_4EBC0 */
uint32_t rtk_spinlock_acquire(volatile uint32_t *lock_word)
{
    uint32_t prev_basepri = rtk_enter_critical();
    if (*lock_word != 0u) {
        return RTK_LOCK_RECURSIVE_OK;
    }
    *lock_word = prev_basepri + 1u;
    return RTK_OK;
}

void rtk_spinlock_release(volatile uint32_t *lock_word)
{
    uint32_t saved_plus_one = *lock_word;
    *lock_word = 0u;
    if (saved_plus_one != 0u) {
        rtk_exit_critical(saved_plus_one - 1u);
    }
}

/* @provenance RVA 0x50424 sub_50424 */
uint32_t rtk_fastlock_trylock(volatile uint32_t *lock_word)
{
    uint32_t cur_thr = *AOPF_VMA_PTR(const uint32_t *, AOPF_CUR_THREAD_VMA);
    uint32_t low_bits = (*lock_word) & 3u;
    uint32_t desired = low_bits | cur_thr;

    uint32_t expected = low_bits;
    if (__atomic_compare_exchange_n(lock_word, &expected, desired, 0,
                                    __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
        return RTK_OK;
    }

    uint32_t cur_owner = (*lock_word) & ~3u;
    if (cur_owner == cur_thr) {
        return RTK_LOCK_RECURSIVE_OK;
    }
    return RTK_ERR_LOCK_CONTENDED;
}

/* @shim RVA 0x50404 rtk_fastlock_lock */
uint32_t rtk_fastlock_lock(volatile uint32_t *lock_word)
{
    return rtk_fastlock_trylock(lock_word);
}

/* @shim RVA 0x5032c rtk_fastlock_unlock */
uint32_t rtk_fastlock_unlock(volatile uint32_t *lock_word)
{
    uint32_t cur_thr = *AOPF_VMA_PTR(const uint32_t *, AOPF_CUR_THREAD_VMA);
    uint32_t keep_bit = (*lock_word) & 2u;
    uint32_t expected = cur_thr | keep_bit;
    if (__atomic_compare_exchange_n(lock_word, &expected, keep_bit, 0,
                                    __ATOMIC_RELEASE, __ATOMIC_RELAXED)) {
        return RTK_OK;
    }
    return RTK_ERR_NULL_KEY;
}

/* @shim RVA 0x50324 sub_50324 */
void rtk_fastlock_unlock_if_zero(volatile uint32_t *lock_word, uint32_t prev_status)
{
    if (prev_status == 0u) {
        (void)rtk_fastlock_unlock(lock_word);
    }
}

/* @provenance RVA 0x502fc sub_502FC */
size_t rtk_stack_unused_bytes(const uint32_t *stack_bottom, size_t stack_bytes)
{
    const uint32_t *cur = stack_bottom;
    size_t rem = stack_bytes;

    while (rem >= 8u) {
        uint32_t lo_diff = cur[0] ^ RTK_STACK_PAINT_LO;
        uint32_t hi_diff = cur[1] ^ RTK_STACK_PAINT_HI;
        if ((lo_diff | hi_diff) != 0u) {
            return rem;
        }
        cur += 2u;
        rem -= 8u;
    }
    return 0u;
}

uint32_t rtk_readyq_highest_prio(uint32_t ready_bitmap)
{
    if (ready_bitmap == 0u) {
        return 32u;
    }
    return 31u - aopf_clz32(ready_bitmap);
}
