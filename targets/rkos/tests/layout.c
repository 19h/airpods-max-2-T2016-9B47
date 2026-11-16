#include <stddef.h>
#include "rkos/rkos.h"

/*
 * Compile-time binary ABI & RAM_BSS layout proofs for `rkos`.
 * Verifies every wire-format descriptor, kernel object header, and pinned
 * `.state.*` block size/offset against `rkos` (`fwsg`) and `rkos.i64`.
 */

/* 1. Wire-format & header structures */
_Static_assert(sizeof(rkos_fwsg_seg_t) == 0x20u, "fwsg segment descriptor size");
_Static_assert(sizeof(rkos_fwsg_footer_t) == 0x20u, "fwsg footer size");
_Static_assert(sizeof(rkos_uuid_hdr_t) == 0x48u, "RTKit uuid header size");
_Static_assert(sizeof(rkos_tlv_hdr_t) == 0x08u, "RTKit TLV header size");
_Static_assert(sizeof(rkos_der_input_t) == 0x08u, "ASN.1 DER input slice size");
_Static_assert(sizeof(rkos_der_tlv_t) == 0x10u, "ASN.1 DER decoded TLV size");
_Static_assert(sizeof(rkos_ftab_hdr_t) == 0x30u, "rkosftab header size");
_Static_assert(offsetof(rkos_ftab_hdr_t, magic) == 0x20u, "rkosftab magic offset");
_Static_assert(offsetof(rkos_ftab_hdr_t, entry_count) == 0x28u, "rkosftab entry_count offset");
_Static_assert(sizeof(rkos_ftab_entry_t) == 0x10u, "rkosftab entry size");
_Static_assert(sizeof(rkos_bmid_hdr_t) == 0x10u, "BMID header size");
_Static_assert(sizeof(rkos_bmid_slot_t) == 0x30u, "BMID slot size");
_Static_assert(offsetof(rkos_bmid_slot_t, range_start) == 0x10u, "BMID range_start offset");
_Static_assert(sizeof(rkos_remap_entry_t) == 0x0Cu, "Stage-2 remap entry size");
_Static_assert(sizeof(rkos_clhe_hdr_t) == 0x20u, "Crashlog CLHE header size");
_Static_assert(sizeof(rkos_clhe_sect_t) == 0x10u, "Crashlog section header size");

/* 2. Kernel heap, scheduler, mutex & corecrypto structures */
_Static_assert(sizeof(rkos_heap_blk_t) == 0x10u, "Heap block header size");
_Static_assert(sizeof(rkos_heap_region_t) == 0x08u, "Heap region descriptor size");
_Static_assert(sizeof(rkos_heap_t) == 0x68u, "Kernel heap state size");
_Static_assert(offsetof(rkos_heap_t, region_count) == 0x40u, "Heap region_count offset");
_Static_assert(offsetof(rkos_heap_t, total_bytes) == 0x44u, "Heap total_bytes offset");
_Static_assert(offsetof(rkos_heap_t, free_head_va) == 0x48u, "Heap free_head_va offset");
_Static_assert(offsetof(rkos_heap_t, check_enabled) == 0x4Cu, "Heap check_enabled offset");
_Static_assert(offsetof(rkos_heap_t, free_count) == 0x50u, "Heap free_count offset");
_Static_assert(offsetof(rkos_heap_t, alloc_count) == 0x58u, "Heap alloc_count offset");
_Static_assert(offsetof(rkos_heap_t, used_bytes) == 0x60u, "Heap used_bytes offset");
_Static_assert(sizeof(rkos_thread_t) == 0x34u, "Thread prefix size");
_Static_assert(sizeof(rkos_mutex_t) == 0x10u, "Mutex size");
_Static_assert(sizeof(rkos_timer_queue_t) == 0x10u, "Timer queue size");
_Static_assert(sizeof(rkos_mem_region_t) == 0x10u, "Memory region entry size");
_Static_assert(sizeof(rkos_ccdigest_info_t) == 0x20u, "ccdigest_info core size");

/* 3. Contiguous RAM_BSS state blocks (`0x20000BA4..0x2001F068`, `0x1E4C4` bytes) */
_Static_assert(sizeof(rkos_state_warm_t)  == 0x0001Cu, ".state.warm size");
_Static_assert(sizeof(rkos_state_qspi_t)  == 0x00180u, ".state.qspi size");
_Static_assert(sizeof(rkos_state_irq_t)   == 0x00338u, ".state.irq size");
_Static_assert(sizeof(rkos_state_meta_t)  == 0x000C8u, ".state.meta size");
_Static_assert(sizeof(rkos_state_arena_t) == 0x1C000u, ".state.arena size");
_Static_assert(sizeof(rkos_state_psp_t)   == 0x00800u, ".state.psp size");
_Static_assert(sizeof(rkos_state_plat_t)  == 0x00390u, ".state.plat size");
_Static_assert(sizeof(rkos_state_sched_t) == 0x00130u, ".state.sched size");
_Static_assert(sizeof(rkos_state_fault_t) == 0x00260u, ".state.fault size");
_Static_assert(sizeof(rkos_state_msp_t)   == 0x01008u, ".state.msp size");

_Static_assert(
    sizeof(rkos_state_warm_t) +
    sizeof(rkos_state_qspi_t) +
    sizeof(rkos_state_irq_t) +
    sizeof(rkos_state_meta_t) +
    sizeof(rkos_state_arena_t) +
    sizeof(rkos_state_psp_t) +
    sizeof(rkos_state_plat_t) +
    sizeof(rkos_state_sched_t) +
    sizeof(rkos_state_fault_t) +
    sizeof(rkos_state_msp_t) == RKOS_RAM_BSS_SIZE,
    "Sum of all 10 .state.* blocks must equal exact RAM_BSS size 0x1E4C4"
);

_Static_assert(RKOS_STATE_PLAT_BASE + offsetof(rkos_state_plat_t, aux_heap) == 0x2001DAA0u,
               "g_aux_heap VMA 0x2001DAA0");
_Static_assert(RKOS_STATE_PLAT_BASE + offsetof(rkos_state_plat_t, kernel_heap) == 0x2001DC38u,
               "g_kernel_heap VMA 0x2001DC38");
_Static_assert(RKOS_STATE_SCHED_BASE + offsetof(rkos_state_sched_t, ready_queues) == 0x2001DCD8u,
               "ready_queues VMA 0x2001DCD8");
_Static_assert(RKOS_STATE_FAULT_BASE + offsetof(rkos_state_fault_t, mpu_regions) == 0x2001DED0u,
               "mpu_regions VMA 0x2001DED0");

int rkos_layout_check_anchor(void);
int rkos_layout_check_anchor(void)
{
    return (int)RKOS_RAM_BSS_SIZE;
}
