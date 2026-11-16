#ifndef RKOS_SHARED_H
#define RKOS_SHARED_H

#include <stddef.h>
#include <stdint.h>
#include "rkos/memmap.h"

/*
 * Exact binary ABI, wire-format records, kernel structures, and RAM_BSS state
 * blocks for `rkos`. Checked at compile time by `tests/layout.c`.
 */

/* 1. `fwsg` wire-format segment table (0x20BC0..0x20C00) & footer (0x20C00..0x20C20) */
#define RKOS_FWSG_MAGIC_LE      0x67737766u /* "fwsg" */

typedef struct {
    uint32_t vma_lo;
    uint32_t vma_hi;
    uint32_t file_offset;
    uint32_t file_size;
    uint32_t mem_size;
    uint32_t flags;
    char     name[8];
} rkos_fwsg_seg_t; /* 0x20 bytes */

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t seg_table_offset;
    uint32_t seg_count;
    uint32_t reserved[4];
} rkos_fwsg_footer_t; /* 0x20 bytes */

/* 2. RTKit 'uuid' image header at RVA 0x000000C0..0x00000108 (0x48 bytes).
 * Only tag/version/uuid are confirmed; the other words are named by observed
 * value (raw bytes at file 0xC0, see verification/inventory.json). */
typedef struct {
    uint32_t tag_le;          /* +0x00: 'uuid' (0x64697575 LE) */
    uint32_t version;         /* +0x04: 5 */
    uint32_t ptr_a;           /* +0x08: 0x00016090 (ROM_RODATA) */
    uint32_t ptr_b;           /* +0x0C: 0x20000818 (RAM_DATA) */
    uint8_t  uuid[16];        /* +0x10: 32166990-6147-3171-a396-018473417533 */
    uint32_t zero0[2];        /* +0x20 */
    uint32_t word_0x28;       /* +0x28: 0x00016400 */
    uint32_t word_0x2c;       /* +0x2C: 0x0000017A */
    uint32_t fwsg_table_off;  /* +0x30: 0x00020BC0 (fwsg segment table file offset) */
    uint32_t zero1[3];        /* +0x34 */
    uint32_t ptr_a_copy;      /* +0x40: 0x00016090 */
    uint32_t ptr_b_copy;      /* +0x44: 0x20000818 */
} rkos_uuid_hdr_t; /* 0x48 bytes */

/* 3. RTKit metadata TLV record header (0x0001714C..0x00017580) */
typedef struct {
    uint32_t tag_le;
    uint32_t length;
} rkos_tlv_hdr_t; /* 0x08 bytes */

/* 4. ASN.1 DER parser structs (`sub_57C` at 0x57C, `sub_582` at 0x582) */
typedef struct {
    uint32_t ptr_va;          /* +0x00: input byte pointer */
    uint32_t length;          /* +0x04: input byte length */
} rkos_der_input_t; /* 0x08 bytes */

typedef struct {
    uint32_t tag_number_lo;   /* +0x00: decoded tag number low 32 bits */
    uint32_t tag_hi_and_cls;  /* +0x04: high tag bits | ((id_byte & 0xE0) << 24) */
    uint32_t content_va;      /* +0x08: content start VA */
    uint32_t content_len;     /* +0x0C: clamped/validated content length */
} rkos_der_tlv_t; /* 0x10 bytes */

/* 5. FTAB ('rkosftab') header & file entry structures */
typedef struct {
    uint32_t generation;      /* +0x00 */
    uint32_t valid_magic;     /* +0x04 */
    uint32_t reserved0[2];    /* +0x08 */
    uint32_t manifest_off;    /* +0x10 */
    uint32_t manifest_len;    /* +0x14 */
    uint32_t reserved1[2];    /* +0x18 */
    uint8_t  magic[8];        /* +0x20: "rkosftab" */
    uint32_t entry_count;     /* +0x28 */
    uint32_t pad2c;           /* +0x2C */
} rkos_ftab_hdr_t; /* 0x30 bytes */

typedef struct {
    char     tag[4];          /* +0x00: e.g. "rkof", "bver" */
    uint32_t data_offset;     /* +0x04 */
    uint32_t data_length;     /* +0x08 */
    uint32_t flags;           /* +0x0C */
} rkos_ftab_entry_t; /* 0x10 bytes */

/* 6. Boot Manifest ID ('BMID') table (validated at RVA 0x4DC0, populated at 0x4DE8) */
#define RKOS_BMID_MAGIC         0x44494D42u /* 'BMID' */
#define RKOS_BMID_VERSION       0x20251121u

typedef struct {
    uint32_t magic;           /* +0x00: 0x44494D42 ('BMID') */
    uint32_t magic_hi;        /* +0x04: 0x00000000 */
    uint32_t version;         /* +0x08: 0x20251121 */
    uint32_t desc_offset;     /* +0x0C: offset to table descriptor */
} rkos_bmid_hdr_t; /* 0x10 bytes */

typedef struct {
    uint32_t tag_info[4];     /* +0x00..+0x0F */
    uint32_t range_start;     /* +0x10 */
    uint32_t range_end_incl;  /* +0x14 */
    uint32_t digest_words[6]; /* +0x18..+0x2F */
} rkos_bmid_slot_t; /* 0x30 bytes */

/* 7. Stage-2 Address Remap Descriptor (12-byte entries at 0x200004C8, built by sub_4B84) */
typedef struct {
    uint32_t kind;            /* +0x00 */
    uint32_t mapped_va;       /* +0x04 */
    uint32_t size;            /* +0x08 */
} rkos_remap_entry_t; /* 0x0C bytes */

/* 8. RTKit Crashlog ('CLHE' = 0x434C4845, v3.3) structures */
#define RKOS_CLHE_MAGIC         0x434C4845u
#define RKOS_CLHE_VER_3_3       0x00000303u

typedef struct {
    uint32_t signature;       /* +0x00: 0x434C4845 ('CLHE') */
    uint32_t version;         /* +0x04: 0x00000303 */
    uint32_t total_length;    /* +0x08 */
    uint32_t flags_reason;    /* +0x0C */
    uint32_t fault_pc;        /* +0x10 */
    uint32_t reserved14;      /* +0x14 */
    uint32_t nested_reason;   /* +0x18 */
    uint32_t platform_tag;    /* +0x1C */
} rkos_clhe_hdr_t; /* 0x20 bytes */

typedef struct {
    uint32_t section_sig;     /* +0x00: FourCC e.g. 'strC', 'verC', 'timC', 'MgrC', 'tscC' */
    uint32_t reserved04;      /* +0x04 */
    uint32_t version;         /* +0x08 */
    uint32_t section_size;    /* +0x0C: includes 0x10-byte header */
} rkos_clhe_sect_t; /* 0x10 bytes */

/* 9. Hardened Kernel Heap (`0x2C58..0x32C4`, `0x4B58`, `0x4F1C`, `0x50B4..0x53B4`) */
#define RKOS_HEAP_COOKIE_ALLOC    0x40DA110Cu
#define RKOS_HEAP_COOKIE_SCRUBBED 0x1BADF4EEu
#define RKOS_HEAP_COOKIE_RAW      0xF4EE705Eu
#define RKOS_HEAP_ALIGN_MAGIC     0xAB5C155Au
#define RKOS_HEAP_MIN_BLOCK       0x18u
#define RKOS_HEAP_HDR_SIZE        0x10u

typedef struct {
    uint32_t cookie;          /* +0x00: __stack_chk_guard ^ blk_va ^COOKIE_MAGIC */
    uint32_t size_and_flags;  /* +0x04: (byte_size & ~3u) | (prev_free?2:0) | (is_free?1:0) */
    uint32_t meta0_or_prev;   /* +0x08: owner_id (alloc) or free_prev_va (free) */
    uint32_t meta1_or_next;   /* +0x0C: caller_tag (alloc) or free_next_va (free) */
} rkos_heap_blk_t; /* 0x10 bytes header (user pointer at +0x10; free footer at +byte_size-4) */

typedef struct {
    uint32_t base_va;         /* +0x00 */
    uint32_t size_bytes;      /* +0x04 */
} rkos_heap_region_t; /* 0x08 bytes */

typedef struct {
    rkos_heap_region_t regions[8];       /* +0x00..+0x3F (64 bytes) */
    uint32_t           region_count;     /* +0x40 */
    uint32_t           total_bytes;      /* +0x44 */
    uint32_t           free_head_va;     /* +0x48 */
    uint8_t            check_enabled;    /* +0x4C: bit 0 enables scrub & full invariant check */
    uint8_t            pad_4d[3];        /* +0x4D..+0x4F */
    uint32_t           free_count;       /* +0x50 */
    uint32_t           free_xor_cookie;  /* +0x54: XOR of all free block VAs */
    uint32_t           alloc_count;      /* +0x58 */
    uint32_t           alloc_xor_cookie; /* +0x5C: XOR of all allocated block VAs */
    uint32_t           used_bytes;       /* +0x60 */
    uint32_t           peak_used_bytes;  /* +0x64 */
} rkos_heap_t; /* 0x68 bytes */

/* 10. RTKit Scheduler Thread, Mutex, Timer Queue, and Memory Region Descriptor */
typedef struct {
    uint32_t saved_sp;        /* +0x00 */
    uint32_t stack_base;      /* +0x04 */
    uint32_t stack_size;      /* +0x08 */
    uint32_t entry_fn;        /* +0x0C */
    uint32_t entry_arg;       /* +0x10 */
    uint8_t  priority;        /* +0x14 (0..63) */
    uint8_t  base_priority;   /* +0x15 */
    uint8_t  state;           /* +0x16 */
    uint8_t  flags;           /* +0x17 */
    uint32_t sched_flags;     /* +0x18 */
    uint32_t rq_next_va;      /* +0x1C */
    uint32_t rq_prev_va;      /* +0x20 */
    uint32_t wait_next_va;    /* +0x24 */
    uint32_t wait_prev_va;    /* +0x28 */
    uint32_t owned_mutex_va;  /* +0x2C */
    uint32_t name_va;         /* +0x30 */
} rkos_thread_t; /* 0x34 bytes core prefix */

typedef struct {
    volatile uint32_t owner_and_waiters; /* +0x00: thread_va | (has_waiters ? 1 : 0) */
    uint32_t wait_head_va;               /* +0x04 */
    uint32_t wait_tail_va;               /* +0x08 */
    uint32_t flags;                      /* +0x0C */
} rkos_mutex_t; /* 0x10 bytes */

typedef struct {
    uint32_t head_va;         /* +0x00 */
    uint32_t armed;           /* +0x04 */
    uint64_t next_deadline;   /* +0x08 */
} rkos_timer_queue_t; /* 0x10 bytes */

typedef struct {
    uint32_t base;            /* +0x00 */
    uint32_t size;            /* +0x04 */
    uint32_t attr;            /* +0x08 */
    uint32_t flags;           /* +0x0C */
} rkos_mem_region_t; /* 0x10 bytes */

/* 11. Corecrypto Digest Descriptor (`ccdigest_info`, used at 0x11910..0x126F6) */
typedef struct {
    uint32_t output_size;     /* +0x00: e.g. 32 (SHA-256), 48 (SHA-384), 64 (SHA-512) */
    uint32_t state_size;      /* +0x04: e.g. 32 (SHA-256), 64 (SHA-384/512) */
    uint32_t block_size;      /* +0x08: e.g. 64 (SHA-256), 128 (SHA-384/512) */
    uint32_t oid_size;        /* +0x0C */
    uint32_t oid_ptr;         /* +0x10 */
    uint32_t initial_state;   /* +0x14: pointer to IV */
    uint32_t compress_fn;     /* +0x18: block compression function */
    uint32_t final_fn;        /* +0x1C: finalization function */
} rkos_ccdigest_info_t; /* 0x20 bytes */

/*
 * Pinned RAM_BSS state block structs (`0x20000BA4..0x2001F068`, total `0x1E4C4` bytes).
 * Placed in contiguous `.state.*` sections by `link/rkos.ld`.
 */
typedef struct {
    uint8_t bytes[0x1C];
} rkos_state_warm_t; /* 0x20000BA4..0x20000BC0 */

typedef struct {
    uint8_t qspi_ctrl[0x78];   /* 0x20000BC0..0x20000C38 */
    uint8_t qspi_flash[0x108]; /* 0x20000C38..0x20000D40 */
} rkos_state_qspi_t; /* 0x20000BC0..0x20000D40 (0x180 bytes) */

typedef struct {
    uint32_t nmi_saved_sp;     /* 0x20000D40 */
    uint32_t reserved_d44[3];  /* 0x20000D44..0x20000D50 */
    uint32_t irq_handlers[72]; /* 0x20000D50..0x20000E70 */
    uint32_t irq_rsvd_e70[2];  /* 0x20000E70..0x20000E78 */
    uint32_t irq_contexts[72]; /* 0x20000E78..0x20000F98 */
    uint8_t  fb_aperture[0x28];/* 0x20000F98..0x20000FC0 (0xFA0..0xFBC stage2 window) */
    uint8_t  spidma_drv[0xB8]; /* 0x20000FC0..0x20001078 (initialized by sub_12C30) */
} rkos_state_irq_t; /* 0x20000D40..0x20001078 (0x338 bytes) */

typedef struct {
    uint32_t          kernel_heap_va;  /* 0x20001078 -> 0x2001DC38 */
    uint32_t          aux_heap_va;     /* 0x2000107C -> 0x2001DAA0 */
    uint32_t          region_lock[2];  /* 0x20001080..0x20001088 */
    rkos_mem_region_t regions[9];      /* 0x20001088..0x20001118 (9 x 16B) */
    uint8_t           pad_1118[0x28];  /* 0x20001118..0x20001140 */
} rkos_state_meta_t; /* 0x20001078..0x20001140 (0xC8 bytes) */

typedef struct {
    uint8_t arena[0x1C000];
} rkos_state_arena_t; /* 0x20001140..0x2001D140 (0x1C000 bytes) */

typedef struct {
    uint8_t stack[0x800];
} rkos_state_psp_t; /* 0x2001D140..0x2001D940 (0x800 bytes) */

typedef struct {
    uint8_t     drv_early[0x160];   /* 0x2001D940..0x2001DAA0 */
    rkos_heap_t aux_heap;           /* 0x2001DAA0..0x2001DB08 (0x68 bytes) */
    uint8_t     rtbuddy_mbox[0x130];/* 0x2001DB08..0x2001DC38 (sub_12C30) */
    rkos_heap_t kernel_heap;        /* 0x2001DC38..0x2001DCA0 (0x68 bytes) */
    uint8_t     clog_inited;        /* 0x2001DCA0 */
    uint8_t     clog_pad[0x2F];     /* 0x2001DCA1..0x2001DCD0 */
} rkos_state_plat_t; /* 0x2001D940..0x2001DCD0 (0x390 bytes) */

typedef struct {
    uint64_t           ready_mask;       /* 0x2001DCD0..0x2001DCD8 */
    uint32_t           ready_queues[64]; /* 0x2001DCD8..0x2001DDD8 */
    rkos_timer_queue_t timer_queue;      /* 0x2001DDD8..0x2001DDE8 */
    uint32_t           resched_pending;  /* 0x2001DDE8 */
    uint32_t           sched_hooks[4];   /* 0x2001DDEC..0x2001DDFC */
    uint32_t           preempt_flag;     /* 0x2001DDFC */
} rkos_state_sched_t; /* 0x2001DCD0..0x2001DE00 (0x130 bytes) */

typedef struct {
    uint32_t fault_depth;          /* 0x2001DE00 */
    uint32_t sys_ptrs[0x15];       /* 0x2001DE04..0x2001DE58 */
    uint32_t fault_reason;         /* 0x2001DE58 */
    uint32_t fault_regs_va;        /* 0x2001DE5C */
    uint32_t fault_frame_size;     /* 0x2001DE60 */
    uint8_t  stage2_handoff[0x6C]; /* 0x2001DE64..0x2001DED0 */
    uint32_t mpu_regions;          /* 0x2001DED0 */
    uint32_t mpu_min_align;        /* 0x2001DED4 */
    uint32_t mpu_valid_mask;       /* 0x2001DED8 */
    uint32_t mpu_rsvd_dedc;        /* 0x2001DEDC */
    uint32_t mpu_shadow[96];       /* 0x2001DEE0..0x2001E060 (16 x 24B) */
} rkos_state_fault_t; /* 0x2001DE00..0x2001E060 (0x260 bytes) */

typedef struct {
    uint8_t  stack[0x1000];        /* 0x2001E060..0x2001F060 */
    uint32_t basepri_default;      /* 0x2001F060 */
    uint32_t irq_prio_shift;       /* 0x2001F064 */
} rkos_state_msp_t; /* 0x2001E060..0x2001F068 (0x1008 bytes) */

extern rkos_state_warm_t  g_state_warm;
extern rkos_state_qspi_t  g_state_qspi;
extern rkos_state_irq_t   g_state_irq;
extern rkos_state_meta_t  g_state_meta;
extern rkos_state_arena_t g_state_arena;
extern rkos_state_psp_t   g_state_psp;
extern rkos_state_plat_t  g_state_plat;
extern rkos_state_sched_t g_state_sched;
extern rkos_state_fault_t g_state_fault;
extern rkos_state_msp_t   g_state_msp;

extern const uint8_t g_rkos_rtkit_hdr[RKOS_RTKIT_HDR_SIZE];
extern const uint8_t g_rkos_rodata[RKOS_ROM_RODATA_SIZE];
extern uint8_t       g_rkos_data[RKOS_RAM_DATA_SIZE];

#endif /* RKOS_SHARED_H */
