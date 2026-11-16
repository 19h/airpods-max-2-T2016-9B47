/*
 * shared.h — Binary-exact RTKit & AOP firmware data structures, vtables,
 * header/trailer descriptors, and IPC message layouts verified from aopf.i64.
 */

#ifndef AOPF_SHARED_H
#define AOPF_SHARED_H

#include <stdint.h>
#include <stddef.h>

/* 1. RTKit 'uuid' v5 Image Header at RVA 0x000000C0..0x00000108 (0x48 bytes) */
typedef struct aopf_uuid_header {
    uint32_t magic;              /* +0x00: 0x64697575 ('uuid') */
    uint32_t version;            /* +0x04: 5 */
    uint32_t rtkit_ver_str_vma;  /* +0x08: 0x00073958 */
    uint32_t build_banner_vma;   /* +0x0c: 0x2080c290 */
    uint8_t  uuid_bytes[16];     /* +0x10: 61724c3e... */
    uint32_t reserved_20[2];     /* +0x20: 0, 0 */
    uint32_t metadata_tlv_vma;   /* +0x28: 0x000739bc */
    uint32_t metadata_tlv_size;  /* +0x2c: 0x00000132 */
    uint32_t text_end_vma;       /* +0x30: 0x00077b14 */
    uint32_t reserved_34[3];     /* +0x34: 0, 0, 0 */
    uint32_t alt_rtkit_ver_vma;  /* +0x40: 0x00073958 */
    uint32_t alt_banner_vma;     /* +0x44: 0x2080c290 */
} aopf_uuid_header_t;

/* 2. 'fwsg' v1 Segment Reload Trailer at EOF-0x60..EOF (0x91BE0..0x91C40) */
typedef struct aopf_fwsg_segment {
    uint64_t vmaddr;             /* +0x00 */
    uint32_t fileoff;            /* +0x08 */
    uint32_t filesize;           /* +0x0c */
    uint32_t vmsize;             /* +0x10 */
    uint32_t flags;              /* +0x14 */
    char     segname[8];         /* +0x18: "__TEXT\0\0" / "__DATA\0\0" */
} aopf_fwsg_segment_t;

typedef struct aopf_fwsg_trailer {
    uint32_t magic;              /* +0x00: 0x67737766 ('fwsg') */
    uint32_t version;            /* +0x04: 1 */
    uint32_t entries_offset;     /* +0x08: 0x00091be0 */
    uint32_t entry_count;        /* +0x0c: 2 */
    uint32_t reserved[4];        /* +0x10..+0x1f */
} aopf_fwsg_trailer_t;

/* 3. RTKit Metadata TLV Header (8-byte header + payload) */
typedef struct rtk_tlv_hdr {
    uint32_t tag;                /* +0x00 */
    uint32_t length;             /* +0x04 */
} rtk_tlv_hdr_t;

typedef struct rtk_tlv_span {
    const void *payload;
    uint32_t    length;
} rtk_tlv_span_t;

/* 4. AKF 128-bit Mailbox FIFO Message */
typedef struct rtk_mbox_msg128 {
    uint32_t words[4];
} rtk_mbox_msg128_t;

/* 5. Common RTKit C Driver Object Header (0x18 bytes on ARMv7E-M) */
typedef struct rtk_dev_hdr {
    const void *vt;              /* +0x00 */
    uint32_t    dev_flags;       /* +0x04 */
    uintptr_t   mmio_base;       /* +0x08 */
    uint32_t    mmio_hi;         /* +0x0c */
    uint32_t    reserved_10;     /* +0x10 */
    uint8_t     claimed_flags;   /* +0x14 */
    uint8_t     pad_15[3];       /* +0x15..+0x17 */
} rtk_dev_hdr_t;

/* 6. NVIC Controller Device & Vtable (@ 0x20819808, vt @ 0x00073AF0) */
typedef struct rtk_nvic_dev rtk_nvic_dev_t;

typedef struct rtk_nvic_vtable {
    uint32_t (*ioctl_fn)(rtk_nvic_dev_t *dev, uint32_t cmd, void *arg0, void *arg1);
    uint32_t (*unimpl_04)(void);
    uint32_t (*unimpl_08)(void);
    void     (*dispatch_irq)(rtk_nvic_dev_t *dev);
    uint32_t (*get_cpsr)(rtk_nvic_dev_t *dev);
    int      (*set_irq_enabled)(rtk_nvic_dev_t *dev, uint32_t irq, int enable);
    uint32_t (*trigger_irq)(rtk_nvic_dev_t *dev, uint32_t irq);
    uint32_t (*register_irq)(rtk_nvic_dev_t *dev, uint32_t irq, void (*fn)(void *), void *ctx);
    uint32_t (*set_irq_priority)(rtk_nvic_dev_t *dev, uint32_t irq, uint32_t prio);
    uint32_t (*nop_fn)(rtk_nvic_dev_t *dev);
    uint32_t (*unimpl_28)(void);
} rtk_nvic_vtable_t;

typedef struct rtk_irq_slot {
    void (*handler)(void *ctx);  /* +0x00 */
    void  *context;              /* +0x04 */
} rtk_irq_slot_t;

struct rtk_nvic_dev {
    const rtk_nvic_vtable_t *vt; /* +0x00 */
    uint32_t                 flags;
    uintptr_t                mmio_base;
    uint32_t                 num_irqs;
    rtk_irq_slot_t          *slots;
};

/* 7. SysTick Timer Device & Vtable (@ 0x2080CBA8, vt @ 0x00073B38) */
typedef struct rtk_systick_dev rtk_systick_dev_t;

typedef struct rtk_systick_vtable {
    uint32_t (*ioctl_fn)(rtk_systick_dev_t *dev, uint32_t cmd, void *arg0, void *arg1);
    uint32_t (*unimpl_04)(void);
    uint32_t (*unimpl_08)(void);
    void     (*set_callback)(rtk_systick_dev_t *dev, void (*cb)(void *), void *ctx);
    uint32_t (*arm_ticks)(rtk_systick_dev_t *dev, uint64_t ticks);
    uint32_t (*arm_us)(rtk_systick_dev_t *dev, uint64_t delay_us);
    uint32_t (*cancel)(rtk_systick_dev_t *dev, uint64_t *rem_ticks_out);
    uint64_t (*us_to_ticks)(rtk_systick_dev_t *dev, uint64_t delay_us);
    uint64_t (*ticks_to_us)(rtk_systick_dev_t *dev, uint64_t ticks);
} rtk_systick_vtable_t;

struct rtk_systick_dev {
    rtk_dev_hdr_t hdr;           /* +0x00..+0x17 */
    uint32_t      ext_clk_hz;    /* +0x18 */
    uint8_t       use_ext_clk;   /* +0x1c */
    uint8_t       pad_1d[3];     /* +0x1d..+0x1f */
    void        (*callback_fn)(void *ctx); /* +0x20 */
    void         *callback_ctx;  /* +0x24 */
    uint32_t      ten_khz_ticks; /* +0x28 */
    uint32_t      cpu_clk_hz;    /* +0x2c */
    uint32_t      rem_ticks_lo;  /* +0x30 */
    uint32_t      rem_ticks_hi;  /* +0x34 */
    uint8_t       periodic_mode; /* +0x38 */
    uint8_t       pad_39[3];     /* +0x39..+0x3b */
};

/* 8. Apple AOP 1 MHz Timebase Device & Vtable (@ 0x2080CBE8, vt @ 0x00073F70) */
typedef struct rtk_timebase_dev rtk_timebase_dev_t;

typedef struct rtk_timebase_vtable {
    uint32_t (*ioctl_fn)(rtk_timebase_dev_t *dev, uint32_t cmd, void *arg0, void *arg1);
    uint32_t (*unimpl_04)(void);
    uint32_t (*unimpl_08)(void);
    uint64_t (*get_ticks)(rtk_timebase_dev_t *dev);
    uint64_t (*get_ticks_wrap)(rtk_timebase_dev_t *dev);
    uint32_t (*sync_offset)(rtk_timebase_dev_t *dev, uint64_t target_ticks);
    uint64_t (*us_to_ticks)(rtk_timebase_dev_t *dev, uint64_t duration_us);
    uint64_t (*ticks_to_us)(rtk_timebase_dev_t *dev, uint64_t ticks);
} rtk_timebase_vtable_t;

struct rtk_timebase_dev {
    rtk_dev_hdr_t hdr;           /* +0x00..+0x17 */
    uint32_t      freq_hz;       /* +0x18: 1000000 */
    uint32_t      lo_reg_off;    /* +0x1c: 0x08 */
    uint32_t      hi_reg_off;    /* +0x20: 0x0c */
    uint32_t      reserved_24[3];/* +0x24..+0x2f */
    uint32_t      offset_lo;     /* +0x30 */
    uint32_t      offset_hi;     /* +0x34 */
    uint32_t      reserved_38[2];/* +0x38..+0x3f */
    uint32_t      sync_target_lo;/* +0x40 */
    uint32_t      sync_target_hi;/* +0x44 */
    uint32_t      sync_raw_lo;   /* +0x48 */
    uint32_t      sync_raw_hi;   /* +0x4c */
    uint32_t      fast_1mhz_flag;/* +0x50 */
    uint8_t       offset_neg;    /* +0x54 */
    uint8_t       slew_active;   /* +0x55 */
    uint8_t       pad_56[2];     /* +0x56..+0x57 */
};

/* 9. Apple AOP Timer Bank Device & Vtable (@ 0x2080C7F8, vt @ 0x00073DC0) */
typedef struct rtk_timer_chan_state {
    uint32_t fire_count;         /* +0x00 */
    uint32_t cb_modulo;          /* +0x04 */
    void   (*callback_fn)(void *ctx); /* +0x08 */
    void    *callback_ctx;       /* +0x0c */
} rtk_timer_chan_state_t;

typedef struct rtk_timer_bank_dev {
    rtk_dev_hdr_t           hdr;          /* +0x00..+0x17 */
    uint32_t                irq_num;      /* +0x18 */
    uint32_t                num_channels; /* +0x1c */
    rtk_timer_chan_state_t *channels;     /* +0x20 */
} rtk_timer_bank_dev_t;

/* 10. Fuse/OTP Device (@ 0x2080CB10, vt @ 0x00073BE4) */
typedef struct rtk_fuse_dev {
    rtk_dev_hdr_t hdr;           /* +0x00..+0x17 */
    uint32_t      reserved_18[2];/* +0x18..+0x1f */
    uint32_t      lock_word;     /* +0x20 */
} rtk_fuse_dev_t;

/* 11. PMGR Device (@ 0x2080D888, vt @ 0x00073BF0) */
typedef struct rtk_pmgr_entry {
    uint32_t dev_id;             /* +0x00 */
    uint32_t reg_offsets[3];     /* +0x04..+0x0f */
} rtk_pmgr_entry_t;

typedef struct rtk_pmgr_dev {
    rtk_dev_hdr_t           hdr;          /* +0x00..+0x17 */
    const rtk_pmgr_entry_t *entries;      /* +0x18 */
    uint32_t                reserved_1c[2];/* +0x1c..+0x23 */
    uint8_t                 locked_flag;  /* +0x24 */
    uint8_t                 pad_25[3];    /* +0x25..+0x27 */
} rtk_pmgr_dev_t;

/* 12. Watchdog Device & Vtable (@ 0x2080CCD8, vt @ 0x00073F9C) */
typedef struct rtk_wdt_dev rtk_wdt_dev_t;

typedef struct rtk_wdt_vtable {
    uint32_t (*ioctl_fn)(rtk_wdt_dev_t *dev, uint32_t cmd, void *arg0, void *arg1);
    uint32_t (*unimpl_04)(void);
    uint32_t (*unimpl_08)(void);
    uint32_t (*set_running)(rtk_wdt_dev_t *dev, int running);
    uint32_t (*kick)(rtk_wdt_dev_t *dev);
    uint32_t (*set_bark_us)(rtk_wdt_dev_t *dev, uint64_t bark_us);
    uint64_t (*get_bark_us)(rtk_wdt_dev_t *dev);
    uint32_t (*set_bite_en)(rtk_wdt_dev_t *dev, int enable);
    uint32_t (*set_bite_us)(rtk_wdt_dev_t *dev, uint64_t bite_us);
    uint64_t (*get_bite_us)(rtk_wdt_dev_t *dev);
    uint32_t (*set_callback)(rtk_wdt_dev_t *dev, void (*cb)(void *), void *ctx);
} rtk_wdt_vtable_t;

struct rtk_wdt_dev {
    rtk_dev_hdr_t hdr;               /* +0x00..+0x17 */
    uint32_t      bark_irq;          /* +0x18 */
    uint8_t       allow_bite_no_cb;  /* +0x1c */
    uint8_t       pad_1d[3];         /* +0x1d..+0x1f */
    uint32_t      bite_prescaler;    /* +0x20 */
    void        (*bark_cb)(void *ctx);/* +0x24 */
    void         *bark_ctx;          /* +0x28 */
};

/* 13. AKF Mailbox Device (@ 0x2080C5D0, vt @ 0x00073BBC) */
typedef struct rtk_mbox_dev {
    rtk_dev_hdr_t    hdr;             /* +0x00..+0x17 */
    uint32_t         reserved_18[6];  /* +0x18..+0x2f */
    rtk_nvic_dev_t  *irq_ctrl;        /* +0x30 */
    uint32_t         reserved_34;     /* +0x34 */
    uint32_t         rx_irq;          /* +0x38 */
    uint32_t         reserved_3c[3];  /* +0x3c..+0x47 */
    uint32_t         rx_defer_count;  /* +0x48 */
    uint32_t         reserved_4c[3];  /* +0x4c..+0x57 */
    uintptr_t        rx_int_set_mmio; /* +0x58 */
    uint32_t         reserved_5c[2];  /* +0x5c..+0x63 */
    uint8_t          rx_int_en_flags; /* +0x64 */
    uint8_t          pad_65[3];       /* +0x65..+0x67 */
    uint32_t         rx_int_bit_shift;/* +0x68 */
    uint32_t         reserved_6c[9];  /* +0x6c..+0x8f */
    uint32_t         rx_empty_arm_en; /* +0x90 */
    uint8_t          pad_94;          /* +0x94 */
    uint8_t          rx_drained_flag; /* +0x95 */
    uint8_t          pad_96[26];      /* +0x96..+0xaf */
    uint32_t         err_reg_off;     /* +0xb0 */
    uint32_t         rx_stat_off;     /* +0xb4 */
    uint32_t         rx_fifo_off;     /* +0xb8 */
} rtk_mbox_dev_t;

/* 14. RTKit Heap Block & Arena Context */
typedef struct rtk_heap_block {
    uint32_t               size_and_flags; /* +0x00: bit0=free, bit1=prev_free */
    struct rtk_heap_block *prev_free;      /* +0x04 */
    struct rtk_heap_block *next_free;      /* +0x08 */
} rtk_heap_block_t;

typedef struct rtk_heap_region {
    uintptr_t base;
    uint32_t  size;
} rtk_heap_region_t;

typedef struct rtk_heap_ctx {
    rtk_heap_region_t regions[8];    /* +0x00..+0x3f */
    uint32_t          region_count;  /* +0x40 */
    uint32_t          total_bytes;   /* +0x44 */
    rtk_heap_block_t *free_head;     /* +0x48 */
    uint8_t           initialized;   /* +0x4c */
    uint8_t           pad_4d[3];     /* +0x4d..+0x4f */
    uint32_t          stats[6];      /* +0x50..+0x67 */
} rtk_heap_ctx_t;

/* 15. MPU Shadow Region Descriptor (@ 0x2089BC9C, 8 entries * 16 bytes) */
typedef struct rtk_mpu_shadow_entry {
    uint32_t attr_flags;             /* +0x00 */
    uint32_t base_addr;              /* +0x04 */
    uint32_t size_bytes;             /* +0x08 */
    uint32_t rasr_val;               /* +0x0c */
} rtk_mpu_shadow_entry_t;

/* 16. VM Region Descriptor allocated by rtk_vm_entry_alloc (0x14 bytes on ARM) */
typedef struct rtk_vm_entry {
    uint32_t             base_addr;  /* +0x00 */
    uint32_t             size_perm;  /* +0x04: top nibble forced to 0x8 */
    uint32_t             reserved_08;/* +0x08 */
    uint32_t             flags_0c;   /* +0x0c */
    struct rtk_vm_entry *next;       /* +0x10 */
} rtk_vm_entry_t;

/* 17. AOP Audio TLV Log Buffer (0x0C bytes on ARM) */
typedef struct aop_aud_log_buf {
    uint8_t *data;                   /* +0x00 */
    uint32_t capacity;               /* +0x04 */
    uint32_t length;                 /* +0x08 */
} aop_aud_log_buf_t;

/* 18. AOP Key-Value Property Hash Table (0x24 bytes on ARM) */
typedef struct aop_prop_node {
    uint8_t  kind;                   /* +0x00: 0 = inline u32, 2 = indirect u32* */
    uint8_t  node_flags;             /* +0x01 */
    uint16_t reserved_02;            /* +0x02 */
    uint16_t prop_key;               /* +0x04 */
    uint16_t next_pool_idx;          /* +0x06 */
    uintptr_t val_or_ptr;            /* +0x08 */
    uint32_t aux_word;               /* +0x0c */
} aop_prop_node_t;

typedef struct aop_prop_arena {
    uintptr_t base_addr;             /* +0x00 */
    uint32_t  node_count;            /* +0x04 */
    uint32_t  node_stride;           /* +0x08 */
    uint16_t  index_mask;            /* +0x0c */
    uint16_t  pad_0e;                /* +0x0e */
} aop_prop_arena_t;

typedef struct aop_lock_vt {
    void     *reserved_00;
    void     *reserved_04;
    uint32_t (*lock_acquire)(void *lock_self);
    void     (*lock_release)(void *lock_self);
} aop_lock_vt_t;

typedef struct aop_lock_obj {
    const aop_lock_vt_t *vt;
} aop_lock_obj_t;

typedef struct aop_prop_table {
    uint32_t          table_flags;   /* +0x00 */
    uint32_t        (*hash_fn)(uint32_t key, uint32_t nbuckets); /* +0x04 */
    uint32_t          num_buckets;   /* +0x08 */
    aop_prop_node_t **buckets;       /* +0x0c */
    uint32_t          free_remain;   /* +0x10 */
    aop_prop_node_t  *alloc_pool;    /* +0x14 */
    aop_prop_arena_t *chain_arena;   /* +0x18 */
    aop_prop_node_t  *cached_node;   /* +0x1c */
    aop_lock_obj_t    inline_lock;   /* +0x20 */
} aop_prop_table_t;

#endif /* AOPF_SHARED_H */
