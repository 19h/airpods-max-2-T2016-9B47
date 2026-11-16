/*
 * cxxabi.c — RTKit segregated free-list heap primitives and Embedded C++
 * Itanium ABI runtime (__cxa_guard_*, __cxa_pure_virtual, operator new/delete)
 * reconstructed from aopf.i64.
 */

#include <stddef.h>
#include <stdint.h>
#include "aopf/cpu.h"
#include "aopf/libc.h"
#include "aopf/memmap.h"
#include "aopf/regs.h"
#include "aopf/rtkit.h"

static inline uint32_t heap_blk_size(const rtk_heap_block_t *blk)
{
    return blk->size_and_flags & ~3u;
}

static inline uint32_t *heap_blk_footer(rtk_heap_block_t *blk, uint32_t sz)
{
    return (uint32_t *)(void *)((uint8_t *)blk + sz - 4u);
}

static inline rtk_heap_block_t *heap_next_phys(rtk_heap_block_t *blk, uint32_t sz)
{
    return (rtk_heap_block_t *)(void *)((uint8_t *)blk + sz);
}

/* @provenance RVA 0x5ed54 sub_5ED54 */
void rtk_heap_init_free_hdr(rtk_heap_block_t *blk, uint32_t byte_size)
{
    uint32_t prev_flag = blk->size_and_flags & 2u;
    blk->size_and_flags = prev_flag | byte_size | 1u;
    uint32_t clean_sz = byte_size & ~3u;
    rtk_heap_block_t *phys_next = heap_next_phys(blk, clean_sz);
    phys_next->size_and_flags |= 2u;
    blk->next_free = NULL;
    blk->prev_free = NULL;
    *heap_blk_footer(blk, heap_blk_size(blk)) = byte_size;
}

/* @provenance RVA 0x5ed84 rtk_heap_unlink_block */
void rtk_heap_unlink_block(rtk_heap_ctx_t *heap, rtk_heap_block_t *blk)
{
    uint32_t sz = heap_blk_size(blk);
    if (*heap_blk_footer(blk, sz) != sz) {
        rtk_panic("!Heapblk:%p", (void *)blk);
    }

    rtk_heap_block_t *prev = blk->prev_free;
    rtk_heap_block_t *next = blk->next_free;
    if (prev != NULL) {
        if (prev->next_free != blk) {
            rtk_panic("!Heapblk:%p", (void *)blk);
        }
        uint32_t prev_sz = heap_blk_size(prev);
        if (*heap_blk_footer(prev, prev_sz) != prev_sz) {
            rtk_panic("!Heapblk:%p", (void *)prev);
        }
        prev->next_free = next;
    } else {
        if (heap->free_head != blk) {
            rtk_panic("!Heapblk:%p", (void *)blk);
        }
        heap->free_head = next;
    }

    if (next != NULL) {
        if (next->prev_free != blk) {
            rtk_panic("!Heapblk:%p", (void *)blk);
        }
        uint32_t next_sz = heap_blk_size(next);
        if (*heap_blk_footer(next, next_sz) != next_sz) {
            rtk_panic("!Heapblk:%p", (void *)next);
        }
        next->prev_free = prev;
    }
    blk->prev_free = NULL;
    blk->next_free = NULL;
}

/* @provenance RVA 0x5ee04 rtk_heap_insert_block */
void rtk_heap_insert_block(rtk_heap_ctx_t *heap, rtk_heap_block_t *blk)
{
    uint32_t ins_sz = heap_blk_size(blk);
    rtk_heap_block_t *prev = NULL;
    rtk_heap_block_t *cur = heap->free_head;

    while (cur != NULL) {
        uint32_t cur_sz = heap_blk_size(cur);
        if (cur_sz >= ins_sz) {
            if (*heap_blk_footer(cur, cur_sz) != cur_sz) {
                rtk_panic("!Heapblk:%p", (void *)cur);
            }
            cur->prev_free = blk;
            blk->next_free = cur;
            break;
        }
        prev = cur;
        cur = cur->next_free;
    }
    if (cur == NULL) {
        blk->next_free = NULL;
    }
    if (prev != NULL) {
        uint32_t prev_sz = heap_blk_size(prev);
        if (*heap_blk_footer(prev, prev_sz) != prev_sz) {
            rtk_panic("!Heapblk:%p", (void *)prev);
        }
        prev->next_free = blk;
        blk->prev_free = prev;
    } else {
        blk->prev_free = NULL;
        heap->free_head = blk;
    }
}

static rtk_heap_block_t *rtk_heap_mark_in_use(rtk_heap_ctx_t *heap,
                                              rtk_heap_block_t *blk,
                                              uint32_t alloc_sz)
{
    blk->size_and_flags = (alloc_sz & ~1u) | (blk->size_and_flags & 2u);
    rtk_heap_block_t *phys_next = heap_next_phys(blk, alloc_sz & ~3u);
    phys_next->size_and_flags &= ~2u;

    uint32_t new_used = heap->stats[4] + alloc_sz;
    heap->stats[4] = new_used;
    if (heap->stats[5] < new_used) {
        heap->stats[5] = new_used;
    }
    return blk;
}

/* @provenance RVA 0x5ee8e rtk_heap_alloc */
void *rtk_heap_alloc(rtk_heap_ctx_t *heap, size_t req_size)
{
    if (heap == NULL || req_size > (size_t)(~0u - 12u)) {
        return NULL;
    }
    uint32_t needed = (uint32_t)((req_size + 11u) & ~7u);
    if (needed < 16u) {
        needed = 16u;
    }

    rtk_heap_block_t *cur = heap->free_head;
    while (cur != NULL && heap_blk_size(cur) < needed) {
        cur = cur->next_free;
    }
    if (cur == NULL) {
        return NULL;
    }

    uint32_t full_sz = heap_blk_size(cur);
    rtk_heap_unlink_block(heap, cur);

    rtk_heap_block_t *chosen;
    if (full_sz >= needed + 16u) {
        uint32_t rem_sz = full_sz - needed;
        rtk_heap_block_t *tail_blk = heap_next_phys(cur, rem_sz);
        chosen = rtk_heap_mark_in_use(heap, tail_blk, full_sz - rem_sz);
        rtk_heap_init_free_hdr(cur, rem_sz);
        rtk_heap_insert_block(heap, cur);
    } else {
        chosen = rtk_heap_mark_in_use(heap, cur, full_sz);
    }

    return (void *)((uint8_t *)chosen + 4u);
}

/* @shim RVA 0x5ef16 sub_5EF16 */
void rtk_heap_free_block(rtk_heap_ctx_t *heap, void *ptr)
{
    if (heap == NULL || ptr == NULL) {
        return;
    }
    rtk_heap_block_t *blk = (rtk_heap_block_t *)(void *)((uint8_t *)ptr - 4u);
    uint32_t sz = heap_blk_size(blk);
    if (heap->stats[4] >= sz) {
        heap->stats[4] -= sz;
    }
    rtk_heap_init_free_hdr(blk, sz);
    rtk_heap_insert_block(heap, blk);
}

static uint32_t rtk_heap_lock_acquire(void)
{
    volatile uint32_t *lock_ptr = AOPF_VMA_PTR(volatile uint32_t *, AOPF_HEAP_FASTLOCK_VMA);
    return rtk_fastlock_lock(lock_ptr);
}

/* @shim RVA 0x4f18c sub_4F18C */
void rtk_heap_lock_release(uint32_t prev_status)
{
    volatile uint32_t *lock_ptr = AOPF_VMA_PTR(volatile uint32_t *, AOPF_HEAP_FASTLOCK_VMA);
    rtk_fastlock_unlock_if_zero(lock_ptr, prev_status);
}

static rtk_heap_ctx_t *rtk_get_global_heap(void)
{
    rtk_heap_ctx_t **slot = AOPF_VMA_PTR(rtk_heap_ctx_t **, AOPF_HEAP_CTX_PTR_VMA);
    return *slot;
}

/* @shim RVA 0x4f198 rtk_malloc */
void *rtk_malloc(size_t req_size)
{
    uint32_t lock_tok = rtk_heap_lock_acquire();
    void *res = rtk_heap_alloc(rtk_get_global_heap(), req_size);
    rtk_heap_lock_release(lock_tok);
    return res;
}

/* @shim RVA 0x4f2f0 sub_4F2F0 */
void *rtk_calloc(size_t count, size_t elem_size)
{
    uint64_t prod = (uint64_t)count * (uint64_t)elem_size;
    uint32_t lock_tok = rtk_heap_lock_acquire();
    void *res = NULL;
    if ((prod >> 32) == 0ull) {
        res = rtk_heap_alloc(rtk_get_global_heap(), (size_t)prod);
        if (res != NULL) {
            rtk_memset(res, 0, (size_t)prod);
        }
    }
    rtk_heap_lock_release(lock_tok);
    return res;
}

/* @shim RVA 0x4f1c0 rtk_realloc */
void *rtk_realloc(void *old_ptr, size_t new_size)
{
    uint32_t lock_tok = rtk_heap_lock_acquire();
    rtk_heap_ctx_t *heap = rtk_get_global_heap();
    void *new_ptr = rtk_heap_alloc(heap, new_size);
    if (old_ptr != NULL && new_ptr != NULL) {
        const rtk_heap_block_t *old_blk =
            (const rtk_heap_block_t *)(const void *)((const uint8_t *)old_ptr - 4u);
        size_t old_payload = (size_t)(heap_blk_size(old_blk) - 4u);
        size_t copy_len = (new_size < old_payload) ? new_size : old_payload;
        rtk_memcpy_wrap(new_ptr, old_ptr, copy_len);
        rtk_heap_free_block(heap, old_ptr);
    }
    rtk_heap_lock_release(lock_tok);
    return new_ptr;
}

/* @shim RVA 0x4f330 sub_4F330 */
void *rtk_memalign(size_t alignment, size_t req_size)
{
    if (alignment == 0u || (alignment & (alignment - 1u)) != 0u) {
        return NULL;
    }
    uint32_t lock_tok = rtk_heap_lock_acquire();
    void *aligned_ptr = NULL;
    size_t extra = 0u;
    int overflow = 0;

    if (alignment >= 9u) {
        size_t max_sz = (~(size_t)7u) - alignment;
        if (req_size > max_sz) {
            overflow = 1;
        } else {
            extra = alignment + 7u;
        }
    }

    if (!overflow) {
        void *raw_ptr = rtk_heap_alloc(rtk_get_global_heap(), req_size + extra);
        if (extra == 0u || raw_ptr == NULL) {
            aligned_ptr = raw_ptr;
        } else {
            uintptr_t raw_addr = (uintptr_t)raw_ptr + extra;
            uintptr_t mask = ~((uintptr_t)alignment - 1u);
            uintptr_t aligned_addr = raw_addr & mask;
            /* Note: on 32-bit ARM, [-8] is raw_ptr and [-4] is magic 0xAB5C155A */
#if __SIZEOF_POINTER__ == 4
            ((uint32_t *)aligned_addr)[-2] = (uint32_t)(uintptr_t)raw_ptr;
            ((uint32_t *)aligned_addr)[-1] = RTK_MEMALIGN_MAGIC;
#else
            void **orig_slot = (void **)(aligned_addr - sizeof(void *) - sizeof(uint32_t));
            *orig_slot = raw_ptr;
            ((uint32_t *)aligned_addr)[-1] = RTK_MEMALIGN_MAGIC;
#endif
            aligned_ptr = (void *)aligned_addr;
        }
    }

    rtk_heap_lock_release(lock_tok);
    return aligned_ptr;
}

/* @shim RVA 0x4f3a0 rtk_free */
void rtk_free(void *ptr)
{
    if (ptr == NULL) {
        return;
    }
    void *real_ptr = ptr;
    if (((const uint32_t *)ptr)[-1] == RTK_MEMALIGN_MAGIC) {
#if __SIZEOF_POINTER__ == 4
        real_ptr = (void *)(uintptr_t)((const uint32_t *)ptr)[-2];
#else
        uintptr_t slot_addr = (uintptr_t)ptr - sizeof(void *) - sizeof(uint32_t);
        real_ptr = *(void *const *)slot_addr;
#endif
    }
    uint32_t lock_tok = rtk_heap_lock_acquire();
    rtk_heap_free_block(rtk_get_global_heap(), real_ptr);
    rtk_heap_lock_release(lock_tok);
}

/* @shim RVA 0x5de88 rtk_cxa_guard_unlock */
void rtk_cxa_guard_unlock(void)
{
    uint32_t *depth_ptr = AOPF_VMA_PTR(uint32_t *, AOPF_CXA_GUARD_DEPTH_VMA);
    uint32_t rem = (*depth_ptr) - 1u;
    *depth_ptr = rem;
    if (rem == 0u) {
        volatile uint32_t *guard_lock = AOPF_VMA_PTR(volatile uint32_t *, AOPF_CXA_GUARD_LOCK_VMA);
        (void)rtk_fastlock_unlock(guard_lock);
    }
}

/* @shim RVA 0x5de48 __cxa_guard_acquire */
int rtk_cxa_guard_acquire(uint8_t *guard_obj)
{
    if (guard_obj[0] != 0u) {
        return 0;
    }
    volatile uint32_t *guard_lock = AOPF_VMA_PTR(volatile uint32_t *, AOPF_CXA_GUARD_LOCK_VMA);
    uint32_t *depth_ptr = AOPF_VMA_PTR(uint32_t *, AOPF_CXA_GUARD_LOCK_VMA + 4u);

    (void)rtk_fastlock_lock(guard_lock);
    *depth_ptr = (*depth_ptr) + 1u;

    if (guard_obj[0] != 0u) {
        rtk_cxa_guard_unlock();
        return 0;
    }
    if (guard_obj[1] != 0u) {
        rtk_panic("CXXGrdAcq");
    }
    guard_obj[1] = 1u;
    return 1;
}

/* @shim RVA 0x5dea4 sub_5DEA4 */
void rtk_cxa_guard_release(uint8_t *guard_obj)
{
    guard_obj[0] = 1u;
    rtk_cxa_guard_unlock();
}

/* @shim RVA 0x5deac __cxa_pure_virtual */
void rtk_cxa_pure_virtual(void)
{
    rtk_panic("CXXPureVirt");
}

/* @shim RVA 0x5ded4 j_rtk_malloc */
void *rtk_cxx_malloc_thunk(size_t req_size)
{
    return rtk_malloc(req_size);
}

/* @shim RVA 0x5dedc j_rtk_free */
void rtk_cxx_free_thunk(void *ptr)
{
    rtk_free(ptr);
}

/* @shim RVA 0x5debc operator_new */
void *rtk_cxx_operator_new(size_t req_size)
{
    void *mem = rtk_cxx_malloc_thunk(req_size);
    if (mem == NULL) {
        rtk_panic("CXXnew:%zu", req_size);
    }
    return mem;
}

/* @shim RVA 0x5dee8 operator_new_array */
void *rtk_cxx_operator_new_array(size_t req_size)
{
    void *mem = rtk_cxx_malloc_thunk(req_size);
    if (mem == NULL) {
        rtk_panic("CXXnew[]:%zu", req_size);
    }
    return mem;
}

/* @shim RVA 0x5dee0 operator_delete */
void rtk_cxx_operator_delete(void *ptr)
{
    rtk_free(ptr);
}

/* @shim RVA 0x5df04 j_rtk_free_1 */
void rtk_cxx_operator_delete_array(void *ptr)
{
    rtk_free(ptr);
}
