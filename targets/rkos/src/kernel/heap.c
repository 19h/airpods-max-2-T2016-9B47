#include "rkos/rkos.h"

/*
 * Reconstructed hardened RTKitOS stage-1 kernel heap (`0x2C58..0x32C4`,
 * `0x4B58`, `0x4F1C`, `0x5134..0x53B4`, `0x6B58..0x768C`).
 *
 * Security invariants enforced identically to `../../rkos`:
 *   - Per-block header cookie: `__stack_chk_guard ^ blk_va ^ MAGIC`
 *     (`0x40DA110C` allocated, `0x1BADF4EE` free+scrubbed, `0xF4EE705E` free+raw)
 *   - Per-heap count + XOR-sum invariants over both allocated and free blocks
 *   - Free-block zero-fill scrubbing and use-after-free detection (`Freeblk nonzero:%p`)
 *   - Double-free detection (`Dblfree:%p`) and boundary footer checks (`!Heapblk:%p`)
 *   - Sorted ascending best-fit free list with high-end carving and coalescing
 */

#ifdef RKOS_HOST
#define RKOS_HEAP_EARLY
#else
#define RKOS_HEAP_EARLY __attribute__((section(".text.early")))
#endif

static inline uint32_t rkos_stack_guard_read(void)
{
    return *RKOS_VMA_PTR(const uint32_t *, RKOS_STACK_GUARD_VA);
}

static inline uint32_t rkos_kernel_heap_va(void)
{
    return *RKOS_VMA_PTR(const uint32_t *, RKOS_STATE_META_BASE);
}

static inline uint32_t *rkos_u32_at(uint32_t va)
{
    return RKOS_VMA_PTR(uint32_t *, va);
}

/* @provenance RVA 0x4b58 sub_4B58 */
uint32_t rkos_heap_init_struct(uint32_t heap_va)
{
    rkos_heap_t *heap = RKOS_VMA_PTR(rkos_heap_t *, heap_va);
    rkos_memset(heap, 0, sizeof(*heap));
    heap->check_enabled = 1u;
    return heap_va;
}

/* @shim RVA 0x31c0 sub_31C0 (pending_c: not shown equivalent to raw) */
RKOS_HEAP_EARLY int rkos_heap_range_in_regions(uint32_t heap_va, uint32_t start_va, uint32_t end_va)
{
    const rkos_heap_t *heap = RKOS_VMA_PTR(const rkos_heap_t *, heap_va);
    for (uint32_t i = 0u; i < heap->region_count; i++) {
        uint32_t rbase = heap->regions[i].base_va;
        uint32_t rend = rbase + heap->regions[i].size_bytes;
        if (start_va >= rbase && end_va <= rend) {
            return 1;
        }
    }
    return 0;
}

/* @provenance RVA 0x31b8 sub_31B8 */
RKOS_HEAP_EARLY int rkos_heap_span_valid(uint32_t heap_va, uint32_t start_va, uint32_t len)
{
    uint32_t end_va = start_va + len;
    if (end_va < start_va) {
        return 0;
    }
    return rkos_heap_range_in_regions(heap_va, start_va, end_va);
}

/* @provenance RVA 0x2f3c sub_2F3C */
RKOS_HEAP_EARLY int rkos_heap_blk_min_valid(uint32_t heap_va, uint32_t blk_va)
{
    return rkos_heap_span_valid(heap_va, blk_va, RKOS_HEAP_MIN_BLOCK);
}

/* @provenance RVA 0x2f44 sub_2F44 */
RKOS_HEAP_EARLY int rkos_heap_blk_alloc_valid(uint32_t heap_va, uint32_t blk_va)
{
    const rkos_heap_blk_t *blk = RKOS_VMA_PTR(const rkos_heap_blk_t *, blk_va);
    uint32_t guard = rkos_stack_guard_read();

    if ((blk->cookie ^ blk_va ^ guard) != RKOS_HEAP_COOKIE_ALLOC) {
        return 0;
    }
    if ((blk->size_and_flags & 1u) != 0u) {
        return 0;
    }
    return rkos_heap_span_valid(heap_va, blk_va, blk->size_and_flags & ~3u);
}

/* @shim RVA 0x2c58 sub_2C58 (pending_c: not shown equivalent to raw) */
RKOS_HEAP_EARLY uint32_t rkos_heap_mark_allocated(uint32_t heap_va, uint32_t blk_va,
                                                  uint32_t byte_size, int track_usage)
{
    rkos_heap_t *heap = RKOS_VMA_PTR(rkos_heap_t *, heap_va);
    rkos_heap_blk_t *blk = RKOS_VMA_PTR(rkos_heap_blk_t *, blk_va);
    uint32_t clean_sz = byte_size & ~3u;

    blk->size_and_flags = (byte_size & ~1u) | (blk->size_and_flags & 2u);
    heap->alloc_count++;
    heap->alloc_xor_cookie ^= blk_va;
    blk->cookie = rkos_stack_guard_read() ^ blk_va ^ RKOS_HEAP_COOKIE_ALLOC;

    uint32_t *next_hdr_flags = rkos_u32_at(blk_va + clean_sz + 4u);
    *next_hdr_flags &= ~2u;

    if (track_usage != 0) {
        uint32_t cur = heap->used_bytes + byte_size;
        heap->used_bytes = cur;
        if (heap->peak_used_bytes < cur) {
            heap->peak_used_bytes = cur;
        }
    }
    return blk_va;
}

/* @shim RVA 0x2f70 sub_2F70 (pending_c: not shown equivalent to raw) */
RKOS_HEAP_EARLY uint32_t rkos_heap_unmark_allocated(uint32_t heap_va, uint32_t blk_va, int track_usage)
{
    rkos_heap_t *heap = RKOS_VMA_PTR(rkos_heap_t *, heap_va);
    rkos_heap_blk_t *blk = RKOS_VMA_PTR(rkos_heap_blk_t *, blk_va);

    blk->cookie = 0u;
    heap->alloc_count--;
    heap->alloc_xor_cookie ^= blk_va;
    if (track_usage != 0) {
        heap->used_bytes -= (blk->size_and_flags & ~3u);
    }
    return heap_va;
}

/* @shim RVA 0x2f94 sub_2F94 (pending_c: not shown equivalent to raw) */
RKOS_HEAP_EARLY uint32_t rkos_heap_mark_free(uint32_t heap_va, uint32_t blk_va, uint32_t byte_size)
{
    rkos_heap_t *heap = RKOS_VMA_PTR(rkos_heap_t *, heap_va);
    rkos_heap_blk_t *blk = RKOS_VMA_PTR(rkos_heap_blk_t *, blk_va);
    uint32_t clean_sz = byte_size & ~3u;
    uint32_t scrub_len = clean_sz - 0x14u;

    blk->size_and_flags = (blk->size_and_flags & 2u) | clean_sz | 1u;
    *rkos_u32_at(blk_va + clean_sz + 4u) |= 2u;
    blk->meta0_or_prev = 0u;
    blk->meta1_or_next = 0u;
    *rkos_u32_at(blk_va + clean_sz - 4u) = clean_sz;

    if (!rkos_heap_span_valid(heap_va, blk_va + 0x10u, scrub_len)) {
        rtkit_panic("!Heapblk:%p", (const void *)(uintptr_t)blk_va);
    }

    uint32_t magic;
    if ((heap->check_enabled & 1u) != 0u) {
        rkos_memset(RKOS_VMA_RAW(blk_va + 0x10u, scrub_len), 0, scrub_len);
        magic = RKOS_HEAP_COOKIE_SCRUBBED;
    } else {
        magic = RKOS_HEAP_COOKIE_RAW;
    }

    heap->free_count++;
    heap->free_xor_cookie ^= blk_va;
    blk->cookie = magic ^ rkos_stack_guard_read() ^ blk_va;
    return blk_va;
}

/* @shim RVA 0x3154 sub_3154 (pending_c: not shown equivalent to raw) */
RKOS_HEAP_EARLY uint32_t rkos_heap_unmark_free(uint32_t heap_va, uint32_t blk_va)
{
    rkos_heap_t *heap = RKOS_VMA_PTR(rkos_heap_t *, heap_va);
    rkos_heap_blk_t *blk = RKOS_VMA_PTR(rkos_heap_blk_t *, blk_va);
    uint32_t clean_sz = blk->size_and_flags & ~3u;
    uint32_t body_len = clean_sz - 0x14u;

    if (!rkos_heap_span_valid(heap_va, blk_va + 0x10u, body_len)) {
        rtkit_panic("!Heapblk:%p", (const void *)(uintptr_t)blk_va);
    }
    if ((heap->check_enabled & 1u) != 0u &&
        !rkos_mem_is_const_byte(RKOS_VMA_RAW(blk_va + 0x10u, body_len), 0u, body_len)) {
        rtkit_panic("Freeblk nonzero:%p", (const void *)(uintptr_t)blk_va);
    }

    blk->cookie = 0u;
    heap->free_count--;
    heap->free_xor_cookie ^= blk_va;
    return heap->free_xor_cookie;
}

/* @shim RVA 0x3030 sub_3030 (pending_c: not shown equivalent to raw) */
RKOS_HEAP_EARLY int rkos_heap_freelist_unlink(uint32_t heap_va, uint32_t blk_va)
{
    rkos_heap_t *heap = RKOS_VMA_PTR(rkos_heap_t *, heap_va);
    rkos_heap_blk_t *blk = RKOS_VMA_PTR(rkos_heap_blk_t *, blk_va);
    uint32_t bsz = blk->size_and_flags & ~3u;

    if (*rkos_u32_at(blk_va + bsz - 4u) != bsz) {
        rtkit_panic("!Heapblk:%p", (const void *)(uintptr_t)blk_va);
    }

    uint32_t prev_va = blk->meta0_or_prev;
    uint32_t next_va = blk->meta1_or_next;

    if (prev_va != 0u) {
        rkos_heap_blk_t *prev = RKOS_VMA_PTR(rkos_heap_blk_t *, prev_va);
        uint32_t psz = prev->size_and_flags & ~3u;
        if (prev->meta1_or_next != blk_va || *rkos_u32_at(prev_va + psz - 4u) != psz) {
            rtkit_panic("!Heapblk:%p", (const void *)(uintptr_t)prev_va);
        }
        prev->meta1_or_next = next_va;
    } else {
        if (heap->free_head_va != blk_va) {
            rtkit_panic("!Heapblk:%p", (const void *)(uintptr_t)blk_va);
        }
        heap->free_head_va = next_va;
    }

    if (next_va != 0u) {
        rkos_heap_blk_t *next = RKOS_VMA_PTR(rkos_heap_blk_t *, next_va);
        uint32_t nsz = next->size_and_flags & ~3u;
        if (next->meta0_or_prev != blk_va || *rkos_u32_at(next_va + nsz - 4u) != nsz) {
            rtkit_panic("!Heapblk:%p", (const void *)(uintptr_t)next_va);
        }
        next->meta0_or_prev = prev_va;
    }

    blk->meta0_or_prev = 0u;
    blk->meta1_or_next = 0u;
    return 0;
}

/* @shim RVA 0x30dc sub_30DC (pending_c: not shown equivalent to raw) */
RKOS_HEAP_EARLY uint32_t rkos_heap_freelist_insert(uint32_t heap_va, uint32_t blk_va)
{
    rkos_heap_t *heap = RKOS_VMA_PTR(rkos_heap_t *, heap_va);
    rkos_heap_blk_t *blk = RKOS_VMA_PTR(rkos_heap_blk_t *, blk_va);
    uint32_t bsz = blk->size_and_flags & ~3u;

    uint32_t prev_va = 0u;
    uint32_t cur_va = heap->free_head_va;
    uint32_t walked = 1u;

    while (cur_va != 0u) {
        rkos_heap_blk_t *cur = RKOS_VMA_PTR(rkos_heap_blk_t *, cur_va);
        uint32_t csz = cur->size_and_flags & ~3u;
        if (csz >= bsz) {
            if (*rkos_u32_at(cur_va + csz - 4u) != csz) {
                rtkit_panic("!Heapblk:%p", (const void *)(uintptr_t)cur_va);
            }
            cur->meta0_or_prev = blk_va;
            blk->meta1_or_next = cur_va;
            break;
        }
        if (walked >= heap->free_count) {
            rtkit_panic("Inconsist heap");
        }
        walked++;
        prev_va = cur_va;
        cur_va = cur->meta1_or_next;
    }

    if (prev_va != 0u) {
        rkos_heap_blk_t *prev = RKOS_VMA_PTR(rkos_heap_blk_t *, prev_va);
        uint32_t psz = prev->size_and_flags & ~3u;
        if (*rkos_u32_at(prev_va + psz - 4u) != psz) {
            rtkit_panic("!Heapblk:%p", (const void *)(uintptr_t)prev_va);
        }
        prev->meta1_or_next = blk_va;
        blk->meta0_or_prev = prev_va;
    } else {
        heap->free_head_va = blk_va;
        blk->meta0_or_prev = 0u;
    }
    return heap_va;
}

/* @shim RVA 0x30a0 sub_30A0 (pending_c: not shown equivalent to raw) */
RKOS_HEAP_EARLY uint32_t rkos_heap_coalesce_pair(uint32_t heap_va, uint32_t left_va, uint32_t right_va)
{
    rkos_heap_blk_t *left = RKOS_VMA_PTR(rkos_heap_blk_t *, left_va);
    rkos_heap_blk_t *right = RKOS_VMA_PTR(rkos_heap_blk_t *, right_va);
    uint32_t combined = (left->size_and_flags & ~3u) + (right->size_and_flags & ~3u);

    rkos_heap_unmark_free(heap_va, right_va);
    rkos_heap_unmark_free(heap_va, left_va);
    return rkos_heap_mark_free(heap_va, left_va, combined);
}

/* @shim RVA 0x2d84 heap_alloc_or_coalesce (pending_c: not shown equivalent to raw) */
RKOS_HEAP_EARLY uint32_t heap_alloc_or_coalesce(uint32_t heap_va)
{
    rkos_heap_t *heap = RKOS_VMA_PTR(rkos_heap_t *, heap_va);
    if ((heap->check_enabled & 1u) == 0u) {
        return 0u;
    }

    uint32_t guard = rkos_stack_guard_read();
    uint32_t seen_free = 0u;
    uint32_t seen_free_xor = 0u;
    uint32_t seen_alloc = 0u;
    uint32_t seen_alloc_xor = 0u;

    for (uint32_t r = 0u; r < heap->region_count; r++) {
        uint32_t rbase = heap->regions[r].base_va;
        uint32_t rsize = heap->regions[r].size_bytes;
        uint32_t cur_va = ((rbase + 0x17u) & ~7u) - 0x10u;
        uint32_t end_va = ((rbase + rsize - 8u) & ~7u) - 0x10u;
        int prev_was_free = 0;

        while (cur_va < end_va) {
            rkos_heap_blk_t *blk = RKOS_VMA_PTR(rkos_heap_blk_t *, cur_va);
            uint32_t decoded = guard ^ blk->cookie ^ cur_va;
            uint32_t sf = blk->size_and_flags;
            uint32_t bsz = sf & ~3u;

            if (decoded == RKOS_HEAP_COOKIE_ALLOC) {
                if ((sf & 1u) != 0u || (((sf & 2u) != 0u) != prev_was_free)) {
                    rtkit_panic("!Heapblk:%p", (const void *)(uintptr_t)cur_va);
                }
                seen_alloc++;
                if (seen_alloc > heap->alloc_count) {
                    rtkit_panic("Inconsist heap");
                }
                seen_alloc_xor ^= cur_va;
                prev_was_free = 0;
            } else if (decoded == RKOS_HEAP_COOKIE_SCRUBBED || decoded == RKOS_HEAP_COOKIE_RAW) {
                if ((sf & 1u) == 0u || prev_was_free || (sf & 2u) != 0u) {
                    rtkit_panic("!Heapblk:%p", (const void *)(uintptr_t)cur_va);
                }
                seen_free++;
                if (seen_free > heap->free_count) {
                    rtkit_panic("Inconsist heap");
                }
                seen_free_xor ^= cur_va;
                if (decoded == RKOS_HEAP_COOKIE_RAW) {
                    uint32_t scrub_len = bsz - 0x14u;
                    rkos_memset(RKOS_VMA_RAW(cur_va + 0x10u, scrub_len), 0, scrub_len);
                    blk->cookie = guard ^ cur_va ^ RKOS_HEAP_COOKIE_SCRUBBED;
                }
                prev_was_free = 1;
            } else {
                rtkit_panic("!Heapblk:%p", (const void *)(uintptr_t)cur_va);
            }

            if (bsz < RKOS_HEAP_MIN_BLOCK || cur_va + bsz > end_va) {
                rtkit_panic("!Heapblk:%p", (const void *)(uintptr_t)cur_va);
            }
            cur_va += bsz;
        }
    }

    if (seen_free != heap->free_count || seen_free_xor != heap->free_xor_cookie ||
        seen_alloc != heap->alloc_count || seen_alloc_xor != heap->alloc_xor_cookie) {
        rtkit_panic("Inconsist heap");
    }

    uint32_t list_count = 0u;
    uint32_t list_xor = 0u;
    uint32_t prev_va = 0u;
    uint32_t node_va = heap->free_head_va;

    while (node_va != 0u) {
        list_count++;
        if (list_count > seen_free) {
            rtkit_panic("Inconsist heap");
        }
        rkos_heap_blk_t *node = RKOS_VMA_PTR(rkos_heap_blk_t *, node_va);
        if (!rkos_heap_blk_min_valid(heap_va, node_va) || node->meta0_or_prev != prev_va) {
            rtkit_panic("!Heapblk:%p", (const void *)(uintptr_t)node_va);
        }
        list_xor ^= node_va;
        prev_va = node_va;
        node_va = node->meta1_or_next;
    }

    if (list_count != seen_free || list_xor != seen_free_xor) {
        rtkit_panic("Inconsist heap");
    }
    return heap_va + 0x48u;
}

/* @shim RVA 0x31e8 heap_alloc_impl (pending_c: not shown equivalent to raw) */
RKOS_HEAP_EARLY uint32_t heap_alloc_impl(uint32_t heap_va, uint32_t size,
                                         uint32_t arg2, uint32_t arg3)
{
    (void)arg2;
    (void)arg3;
    heap_alloc_or_coalesce(heap_va);
    if (size > 0xFFFFFFE8u) {
        return 0u;
    }

    uint32_t req = (size + 0x17u) & ~7u;
    if (req < RKOS_HEAP_MIN_BLOCK) {
        req = RKOS_HEAP_MIN_BLOCK;
    }

    rkos_heap_t *heap = RKOS_VMA_PTR(rkos_heap_t *, heap_va);
    uint32_t cur_va = heap->free_head_va;
    uint32_t cur_sz = 0u;

    while (cur_va != 0u) {
        rkos_heap_blk_t *cur = RKOS_VMA_PTR(rkos_heap_blk_t *, cur_va);
        cur_sz = cur->size_and_flags & ~3u;
        if (cur_sz >= req) {
            break;
        }
        cur_va = cur->meta1_or_next;
    }

    if (cur_va == 0u) {
        heap_alloc_or_coalesce(heap_va);
        return 0u;
    }

    rkos_heap_freelist_unlink(heap_va, cur_va);
    uint32_t chosen_va = cur_va;

    if (cur_sz >= req + RKOS_HEAP_MIN_BLOCK) {
        uint32_t rem_sz = cur_sz - req;
        rkos_heap_unmark_free(heap_va, cur_va);
        chosen_va = cur_va + rem_sz;
        rkos_heap_blk_t *carved = RKOS_VMA_PTR(rkos_heap_blk_t *, chosen_va);
        carved->size_and_flags = 0u;
        rkos_heap_mark_allocated(heap_va, chosen_va, req, 1);
        rkos_heap_mark_free(heap_va, cur_va, rem_sz);
        rkos_heap_freelist_insert(heap_va, cur_va);
    } else {
        rkos_heap_unmark_free(heap_va, cur_va);
        rkos_heap_mark_allocated(heap_va, cur_va, cur_sz, 1);
    }

    rkos_heap_blk_t *chosen = RKOS_VMA_PTR(rkos_heap_blk_t *, chosen_va);
    uint32_t cur_task = *rkos_u32_at(RKOS_CURRENT_TASK_VA);
    if (cur_task == 0u || cur_task == 4u) {
        chosen->meta0_or_prev = 0xFFFFFFFFu;
    } else {
        chosen->meta0_or_prev = *rkos_u32_at(cur_task + 8u);
    }
    chosen->meta1_or_next = 0u;

    heap_alloc_or_coalesce(heap_va);
    return chosen_va + RKOS_HEAP_HDR_SIZE;
}

/* @shim RVA 0x2cac heap_free_impl (pending_c: not shown equivalent to raw) */
RKOS_HEAP_EARLY uint32_t heap_free_impl(uint32_t heap_va, uint32_t ptr_va, int track_usage)
{
    heap_alloc_or_coalesce(heap_va);
    uint32_t blk_va = ptr_va - RKOS_HEAP_HDR_SIZE;

    if (!rkos_heap_blk_min_valid(heap_va, blk_va)) {
        rtkit_panic("!Heapblk:%p", (const void *)(uintptr_t)blk_va);
    }

    rkos_heap_blk_t *blk = RKOS_VMA_PTR(rkos_heap_blk_t *, blk_va);
    uint32_t decoded = rkos_stack_guard_read() ^ blk_va ^ blk->cookie;
    if (decoded == RKOS_HEAP_COOKIE_SCRUBBED || decoded == RKOS_HEAP_COOKIE_RAW) {
        rtkit_panic("Dblfree:%p", (const void *)(uintptr_t)blk_va);
    }
    if (!rkos_heap_blk_alloc_valid(heap_va, blk_va)) {
        rtkit_panic("!Heapblk:%p", (const void *)(uintptr_t)blk_va);
    }

    uint32_t bsz = blk->size_and_flags & ~3u;
    rkos_heap_unmark_allocated(heap_va, blk_va, track_usage);
    rkos_heap_mark_free(heap_va, blk_va, bsz);

    uint32_t next_va = blk_va + (blk->size_and_flags & ~3u);
    if ((*rkos_u32_at(next_va + 4u) & 1u) != 0u) {
        rkos_heap_freelist_unlink(heap_va, next_va);
        rkos_heap_coalesce_pair(heap_va, blk_va, next_va);
    }

    uint32_t merged_va = blk_va;
    if ((blk->size_and_flags & 2u) != 0u) {
        uint32_t prev_sz = *rkos_u32_at(blk_va - 4u);
        uint32_t prev_va = blk_va - prev_sz;
        rkos_heap_freelist_unlink(heap_va, prev_va);
        rkos_heap_coalesce_pair(heap_va, prev_va, blk_va);
        merged_va = prev_va;
    }

    rkos_heap_freelist_insert(heap_va, merged_va);
    return heap_alloc_or_coalesce(heap_va);
}

/* @shim RVA 0x32c4 heap_free_blk (pending_c: not shown equivalent to raw) */
RKOS_HEAP_EARLY uint32_t heap_free_blk(uint32_t heap_va, uint32_t ptr_va)
{
    return heap_free_impl(heap_va, ptr_va, 1);
}

/* @shim RVA 0x4f1c sub_4F1C (pending_c: not shown equivalent to raw) */
uint32_t rkos_kernel_heap_add_region(uint32_t base_va, uint32_t size_bytes)
{
    uint32_t irq_state = crit_enter();
    if (base_va == 0u || size_bytes < 0x400u) {
        crit_exit(irq_state);
        return 0x80000001u;
    }

    uint32_t heap_va = rkos_kernel_heap_va();
    rkos_heap_t *heap = RKOS_VMA_PTR(rkos_heap_t *, heap_va);
    uint32_t rcount = heap->region_count;

    for (uint32_t i = 0u; i < rcount; i++) {
        uint32_t rbase = heap->regions[i].base_va;
        uint32_t rsize = heap->regions[i].size_bytes;
        if (rbase + rsize > base_va && base_va + size_bytes > rbase) {
            crit_exit(irq_state);
            return 0x8000000Du;
        }
    }

    if (rcount >= 8u) {
        crit_exit(irq_state);
        return 0x80000005u;
    }

    uint32_t first_blk_va = ((base_va + 0x17u) & ~7u) - 0x10u;
    uint32_t sentinel_va  = ((base_va + size_bytes - 8u) & ~7u) - 0x10u;
    uint32_t usable_span  = sentinel_va - first_blk_va;

    heap->regions[rcount].base_va = base_va;
    heap->regions[rcount].size_bytes = size_bytes;
    heap->region_count = rcount + 1u;

    rkos_memset(RKOS_VMA_RAW(sentinel_va, RKOS_HEAP_MIN_BLOCK), 0, RKOS_HEAP_MIN_BLOCK);
    rkos_heap_blk_t *first_blk = RKOS_VMA_PTR(rkos_heap_blk_t *, first_blk_va);
    first_blk->size_and_flags = 0u;
    rkos_heap_mark_allocated(heap_va, first_blk_va, usable_span, 0);
    first_blk->size_and_flags &= ~2u;
    heap_free_impl(heap_va, first_blk_va + RKOS_HEAP_HDR_SIZE, 0);
    heap->total_bytes += size_bytes;

    crit_exit(irq_state);
    return 0u;
}

/* @shim RVA 0x5134 sub_5134 (pending_c: not shown equivalent to raw) */
void rkos_assert_status_ok(uint32_t status)
{
    if ((status & 0x90000000u) != 0u) {
        rtkit_panic("!mempool init");
    }
}

/* @provenance RVA 0x515c rtkit_malloc */
uint32_t rtkit_malloc(uint32_t size)
{
    uint32_t irq_state = crit_enter();
    uint32_t ptr_va = heap_alloc_impl(rkos_kernel_heap_va(), size, 0u, 0u);
    crit_exit(irq_state);
    return ptr_va;
}

/* @shim RVA 0x5184 sub_5184 (pending_c: not shown equivalent to raw) */
uint32_t rkos_realloc(uint32_t ptr_va, uint32_t new_size)
{
    if (ptr_va == 0u) {
        return rtkit_malloc(new_size);
    }
    if (new_size == 0u) {
        rtkit_free(ptr_va);
        return 0u;
    }

    uint32_t raw_ptr_va = ptr_va;
    if (*rkos_u32_at(ptr_va - 4u) == RKOS_HEAP_ALIGN_MAGIC) {
        raw_ptr_va = *rkos_u32_at(ptr_va - 8u);
    }

    uint32_t blk_va = raw_ptr_va - RKOS_HEAP_HDR_SIZE;
    rkos_heap_blk_t *blk = RKOS_VMA_PTR(rkos_heap_blk_t *, blk_va);
    uint32_t old_total = blk->size_and_flags & ~3u;
    uint32_t old_payload = old_total - (ptr_va - blk_va);

    uint32_t new_va = rtkit_malloc(new_size);
    if (new_va == 0u) {
        return 0u;
    }
    uint32_t copy_len = (old_payload < new_size) ? old_payload : new_size;
    rkos_memcpy(RKOS_VMA_RAW(new_va, copy_len), RKOS_VMA_RAW(ptr_va, copy_len), copy_len);
    rtkit_free(ptr_va);
    return new_va;
}

/* @provenance RVA 0x5304 sub_5304 */
uint32_t rkos_calloc(uint32_t count, uint32_t size)
{
    uint64_t prod = (uint64_t)count * (uint64_t)size;
    uint32_t irq_state = crit_enter();
    uint32_t ptr_va = 0u;

    if ((prod >> 32) == 0ULL) {
        uint32_t total = (uint32_t)prod;
        ptr_va = heap_alloc_impl(rkos_kernel_heap_va(), total, 0u, 0u);
        if (ptr_va != 0u) {
            rkos_memset(RKOS_VMA_RAW(ptr_va, total), 0, total);
        }
    }
    crit_exit(irq_state);
    return ptr_va;
}

/* @provenance RVA 0x5344 sub_5344 */
uint32_t rkos_memalign(uint32_t align, uint32_t size)
{
    if (align == 0u || (align & (align - 1u)) != 0u) {
        return 0u;
    }

    uint32_t irq_state = crit_enter();
    uint32_t extra = 0u;
    if (align >= 9u) {
        if (size > 0xFFFFFFF8u - align) {
            crit_exit(irq_state);
            return 0u;
        }
        extra = align + 7u;
    }

    uint32_t raw_va = heap_alloc_impl(rkos_kernel_heap_va(), size + extra, 0u, 0u);
    uint32_t aligned_va = raw_va;
    if (extra != 0u && raw_va != 0u) {
        aligned_va = (raw_va + extra) & ~(align - 1u);
        *rkos_u32_at(aligned_va - 8u) = raw_va;
        *rkos_u32_at(aligned_va - 4u) = RKOS_HEAP_ALIGN_MAGIC;
    }
    crit_exit(irq_state);
    return aligned_va;
}

/* @provenance RVA 0x53b4 rtkit_free */
void rtkit_free(uint32_t ptr_va)
{
    if (ptr_va == 0u) {
        return;
    }
    uint32_t raw_va = ptr_va;
    if (*rkos_u32_at(ptr_va - 4u) == RKOS_HEAP_ALIGN_MAGIC) {
        raw_va = *rkos_u32_at(ptr_va - 8u);
    }
    uint32_t irq_state = crit_enter();
    (void)heap_free_blk(rkos_kernel_heap_va(), raw_va);
    crit_exit(irq_state);
}

/* @provenance RVA 0x7684 j_rtkit_malloc */
uint32_t j_rtkit_malloc(uint32_t size)
{
    return rtkit_malloc(size);
}

/* @provenance RVA 0x766c cxx_operator_new */
uint32_t cxx_operator_new(uint32_t size)
{
    uint32_t ptr_va = j_rtkit_malloc(size);
    if (ptr_va == 0u) {
        rtkit_panic("CXXnew:%zu", (size_t)size);
    }
    return ptr_va;
}

/* @provenance RVA 0x768c cxx_operator_new_array */
uint32_t cxx_operator_new_array(uint32_t size)
{
    uint32_t ptr_va = j_rtkit_malloc(size);
    if (ptr_va == 0u) {
        rtkit_panic("CXXnew[]:%zu", (size_t)size);
    }
    return ptr_va;
}

/* @provenance RVA 0x6b58 j_rtkit_free */
void j_rtkit_free(uint32_t ptr_va) { rtkit_free(ptr_va); }
/* @provenance RVA 0x6ce4 j_rtkit_free_0 */
void j_rtkit_free_0(uint32_t ptr_va) { rtkit_free(ptr_va); }
/* @provenance RVA 0x6f18 j_rtkit_free_1 */
void j_rtkit_free_1(uint32_t ptr_va) { rtkit_free(ptr_va); }
/* @provenance RVA 0x6f78 j_rtkit_free_2 */
void j_rtkit_free_2(uint32_t ptr_va) { rtkit_free(ptr_va); }
/* @provenance RVA 0x7050 j_rtkit_free_3 */
void j_rtkit_free_3(uint32_t ptr_va) { rtkit_free(ptr_va); }
/* @provenance RVA 0x7220 j_rtkit_free_4 */
void j_rtkit_free_4(uint32_t ptr_va) { rtkit_free(ptr_va); }
/* @provenance RVA 0x72e4 j_rtkit_free_5 */
void j_rtkit_free_5(uint32_t ptr_va) { rtkit_free(ptr_va); }
/* @provenance RVA 0x73c4 j_rtkit_free_6 */
void j_rtkit_free_6(uint32_t ptr_va) { rtkit_free(ptr_va); }
/* @provenance RVA 0x73f0 j_rtkit_free_7 */
void j_rtkit_free_7(uint32_t ptr_va) { rtkit_free(ptr_va); }
/* @provenance RVA 0x7460 j_rtkit_free_8 */
void j_rtkit_free_8(uint32_t ptr_va) { rtkit_free(ptr_va); }
/* @provenance RVA 0x74f8 j_rtkit_free_9 */
void j_rtkit_free_9(uint32_t ptr_va) { rtkit_free(ptr_va); }
/* @provenance RVA 0x7688 j_rtkit_free_10 */
void j_rtkit_free_10(uint32_t ptr_va) { rtkit_free(ptr_va); }
