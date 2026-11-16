#ifndef RKOF_SHARED_H
#define RKOF_SHARED_H

#include <stdint.h>

/*
 * Wire formats & cross-core shared SRAM structures between:
 *   - rkof (Main RTKit supervisor / W3 calibration & coredump host)
 *   - acib (Bluetooth Controller / W3 MAC)
 *   - phyb (W3 Bluetooth / Wireless PHY coprocessor)
 *   - rkos (Stage-1 RTKit flash loader)
 */

struct __attribute__((packed)) rkof_uuid_header {
    char     magic[4];              /* +0x00: "uuid" */
    uint32_t version;               /* +0x04: 5 */
    uint32_t rtkit_build_vma;       /* +0x08: VMA of RTKit build tag */
    uint32_t product_build_vma;     /* +0x0C: VMA of product build tag */
    uint8_t  uuid[16];              /* +0x10: 16-byte image UUID */
    uint32_t reserved_20;           /* +0x20 */
    uint32_t reserved_24;           /* +0x24 */
    uint32_t meta_tlv_vma;          /* +0x28: VMA of FourCC TLV stream */
    uint32_t meta_tlv_size;         /* +0x2C: byte size of TLV stream */
    uint32_t text_end_vma;          /* +0x30: aligned __TEXT end VMA */
    uint32_t reserved_34[3];        /* +0x34..+0x3F */
    uint32_t rtkit_build_vma_dup;   /* +0x40 */
    uint32_t product_build_vma_dup; /* +0x44 */
};

struct __attribute__((packed)) rkof_tlv_record {
    uint32_t tag_le;                /* +0x00: reversed ASCII FourCC */
    uint32_t length;                /* +0x04: payload byte length */
};

struct __attribute__((packed)) rkof_fwsg_segment {
    uint64_t vmaddr;
    uint32_t fileoff;
    uint32_t filesize;
    uint32_t vmsize;
    uint32_t flags;
    char     segname[8];
};

struct __attribute__((packed)) rkof_fwsg_trailer {
    char     magic[4];              /* "fwsg" */
    uint32_t version;               /* 1 */
    uint32_t table_off;             /* EOF - 0x60 */
    uint32_t seg_count;             /* 2 */
    uint32_t reserved[4];
};

/* W3 MAC <-> PHY Shared Mailbox & Lock Region at 0x4D280000 */
struct __attribute__((packed)) phy_mac_mailbox {
    uint8_t  cmd_payload[0x0C0];        /* +0x000: MAC -> PHY command slot */
    uint16_t dekker_flag_mac;           /* +0x0C0: Dekker spinlock MAC flag */
    uint16_t dekker_flag_phy;           /* +0x0C2: Dekker spinlock PHY flag */
    uint8_t  txpwr_table[0x120];        /* +0x0C4..+0x1E3: Dekker-guarded 8-band TX power table */
    uint8_t  frs_scan_window[0x21C];    /* +0x1E4..+0x3FF: FRS channel scan */
    uint8_t  resp_payload[0x1C1];       /* +0x400..+0x5C0: PHY -> MAC response */
    uint8_t  crash_inline_copy[0x14];   /* +0x5C1..+0x5D4: 20B MAC panic copy */
};

/* W3 PHY Shared Trace Ring at 0x4D289000 (captured by sub_FDC38) */
struct __attribute__((packed)) phy_trace_ring {
    uint8_t  entries[0x1FC];            /* +0x000..+0x1FB */
    uint32_t write_idx;                 /* +0x1FC */
};

/* W3 PHY Shared Crash Record at 0x4D289200 (captured by sub_FDC38 & sub_FDCB4) */
struct __attribute__((packed)) phy_crash_record {
    uint8_t  header_and_regs[0x40];     /* +0x00..+0x3F */
    uint32_t error_code;                /* +0x40 (0x4D289240) */
    uint32_t detail1_ptr;               /* +0x44 (0x4D289244) */
    uint32_t detail1_len;               /* +0x48 (0x4D289248) */
    uint8_t  tail_context[0x4C];        /* +0x4C..+0x97 */
};

/* W3 PHY Calibration Container Header at 0x4D28D000 ('NV' = 0x4E56) */
struct __attribute__((packed)) phy_nvv_container_hdr {
    uint16_t payload_len;               /* +0x00 */
    uint16_t magic;                     /* +0x02: 0x4E56 ('NV') */
    uint16_t flags;                     /* +0x04 */
    uint16_t crc16;                     /* +0x06 */
};

/* W3 PHY Warm-Restore Register Entry at 0x4D291008[i] */
struct __attribute__((packed)) phy_warm_restore_pair {
    uint32_t reg_addr;                  /* +0x00 */
    uint32_t reg_val;                   /* +0x04 */
};

/* W3 PHY Shared Status & Warm-Restore Region at 0x4D291000 (0x20B3 bytes) */
struct __attribute__((packed)) phy_status_region {
    uint32_t slpr_state;                                /* +0x0000 (0x4D291000) */
    uint32_t slpr_reg_num;                              /* +0x0004 (0x4D291004) */
    struct phy_warm_restore_pair warm_regs[0x400];      /* +0x0008..+0x2007 */
    uint8_t  status_tail[0xAB];                         /* +0x2008..+0x20B2 */
};

/* Carveout Grant Descriptor at 0x20024220 */
struct rkof_carveout_info {
    uint32_t reserved[4];               /* +0x00..+0x0F */
    uint32_t region_a_base;             /* +0x10 */
    uint32_t region_a_size;             /* +0x14 */
    uint32_t shared_vma;                /* +0x18 */
    uint32_t shared_meta;               /* +0x1C */
};

/* Coprocessor Shared-Memory Grant Block (e.g. 0x200009B0, 0x2000C988+0x48) */
struct rkof_coproc_grant {
    uint32_t grant_vma;                 /* +0x00 */
    uint32_t grant_flags;               /* +0x04 */
    uint32_t reserved_08;               /* +0x08 */
    uint32_t grant_meta;                /* +0x0C */
};

#endif /* RKOF_SHARED_H */
