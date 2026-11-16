/*
 * shared.h -- memory shared between the PHY and the host MAC, and the RTKit
 * diagnostic records the host reads after a crash.
 *
 * All structures are byte-exact with the layout in the recovered image and are
 * packed: the mailbox fields are not naturally aligned and the firmware clears
 * CCR.UNALIGN_TRP so it can address them with 32-bit accesses.
 */
#ifndef BTP_SHARED_H
#define BTP_SHARED_H

#include <stdint.h>
#include <stddef.h>
#include "memmap.h"

#pragma pack(push, 1)

/* ---- NVV calibration container ------------------------------------------- */

#define NVV_BLOB_MAGIC              0x4E56u          /* "NV" */

struct nvv_blob_header {
    uint16_t payload_length;
    uint16_t magic;
    uint32_t checksum;
};

/* ---- MAC <-> PHY mailbox (0x4D280000) ------------------------------------ */

#define MAC_RESPONSE_PAYLOAD_SIZE   0xB4
#define MAC_FRS_RESULT_CHANNELS     0x80
#define MAC_FRS_GAIN_CHANNELS       0x7E

/* Request from the MAC.  Only the fields the PHY reads are named. */
struct mac_request {
    uint16_t reserved_000;
    uint16_t command;                   /* enum mac_command */
    uint8_t  reserved_004;
    uint8_t  length;                    /* bytes following the command header */
    uint16_t reserved_006;
    union {
        struct {                        /* MAC_CMD_SUBCOMMAND */
            uint16_t id;                /* enum mac_subcommand */
            uint16_t length;            /* bytes of params */
            union {
                uint8_t  u8[0xB8];
                uint32_t u32[0x2E];
                struct {                /* generic {index, enable, value} shape */
                    uint8_t  index;
                    uint8_t  enable;
                    uint16_t value;
                    uint8_t  extra;
                } ovr;
                struct {                /* MAC_SUB_SYSCFG */
                    uint8_t  op;        /* 1 = write chunk, 2 = apply */
                    uint8_t  length;
                    uint16_t offset;
                    uint8_t  data[0xB0];
                } syscfg;
                struct {                /* MAC_SUB_TXPWR_TEST_MODE */
                    uint8_t  mode;
                    uint8_t  stage;
                    uint8_t  post_stage;
                    uint8_t  delay;
                    uint8_t  seq_code;
                    uint8_t  gain_code;
                    union {
                        int8_t override_value; /* mode 4 target / mode 5 index */
                        int8_t reserved;       /* previous source spelling */
                    };
                } txpwr_test;
                struct {                /* MAC_SUB_CHANNEL_LIMIT */
                    uint8_t  op;        /* enum txpwr_limit_op */
                    uint8_t  band;
                    uint8_t  channel;   /* edge channel index */
                    uint8_t  pkt_type;
                    int8_t   power;     /* cap applied beyond the edge */
                } limit;
                struct {                /* MAC_SUB_CW_TONE */
                    uint8_t  enable;
                    uint8_t  band;
                    uint8_t  channel;
                    uint8_t  mode;
                } tone;
                struct {                /* MAC_SUB_DIAG_BUFFER */
                    uint8_t  op;        /* 0 = unfreeze, 1 = read */
                    uint8_t  index;
                    uint16_t offset;
                    uint8_t  length;
                } diag;
                struct {                /* MAC_SUB_PRBS */
                    uint8_t  mode;
                    uint8_t  band;
                    uint8_t  channel;
                } prbs;
            } p;
        } sub;
        struct {                        /* MAC_CMD_TXPWR_BAND_TABLE */
            uint8_t  table[0x24];       /* struct txpwr_entry[9] */
            uint8_t  band;
        } band_table;
        uint8_t raw[0x1DC];
    } u;                                /* 0x008 .. 0x1E4 */
    struct {                            /* IPC_FROM_MAC_FRS_REQUEST */
        uint8_t  band;                  /* 0x1E4 */
        uint8_t  gain_index;            /* 0x1E5 */
        uint8_t  channel_bitmap[16];    /* 0x1E6 */
        uint8_t  half_span;             /* 0x1F6 */
        uint8_t  passes;                /* 0x1F7, low 3 bits */
        uint8_t  notify;                /* 0x1F8, bit 0 */
        uint32_t coex_enable;           /* 0x1F9 (IPC_FROM_MAC_COEX_REQUEST) */
    } frs;
};

/* Full-table TX power update rides at a fixed offset of the request area. */
#define MAC_REQUEST_TXPWR_TABLE_OFFSET  0x0C4
#define MAC_REQUEST_FRS_OFFSET          0x1E4

struct mac_response {
    uint16_t flags;                     /* 0x400 */
    uint16_t command;                   /* 0x402 */
    uint8_t  status;                    /* 0x404 */
    uint8_t  length;                    /* 0x405 */
    uint16_t reserved_406;
    union {                             /* 0x408 */
        uint8_t  u8[MAC_RESPONSE_PAYLOAD_SIZE];
        uint16_t u16[MAC_RESPONSE_PAYLOAD_SIZE / 2];
        uint32_t u32[MAC_RESPONSE_PAYLOAD_SIZE / 4];
    } payload;
};

#define MAC_RESPONSE_READY          0x0080u

struct mac_mailbox {
    struct mac_request  req;            /* 0x000 */
    uint8_t             reserved_1fd[0x203];
    struct mac_response resp;           /* 0x400 */
    uint8_t             reserved_4bc[4];
    int16_t             frs_rssi[MAC_FRS_RESULT_CHANNELS];     /* 0x4C0 */
    uint8_t             frs_status;     /* 0x5C0 */
    uint8_t             crash[20];      /* 0x5C1, struct crash_descriptor, unaligned */
    uint8_t             reserved_5d5[0x29];
    uint8_t             frs_gain[MAC_FRS_GAIN_CHANNELS];       /* 0x5FE */
};

/* ---- status / telemetry page (0x4D285000) -------------------------------- */

struct reg_restore_entry {
    uint32_t address;
    uint32_t value;
};

struct packet_stats {
    uint32_t tx_count;
    uint32_t rx_count;
    uint32_t rssi_histogram[4];
    int32_t  cfo_mean;
    int32_t  cfo_max;
    int32_t  cfo_min;
};

struct telemetry {
    uint32_t max_tx_isr_latency;
    uint32_t max_rx_isr_latency;
    struct packet_stats stats;
};

/* 9A348 exports 185 here; 8E251/8E258 export 183. In the raw images this
 * short string moves from ROM 0x821F to 0x8222 with the +3-byte string shift. */
#define VERSION_STRING              "185{T:w3}{C:B0}"
#define VERSION_STEPPING            "B0"

struct version_info {
    char     version[32];
    uint32_t build_id;
    uint32_t format;
    uint32_t nvv_presence0;
    uint32_t nvv_presence1;
    uint16_t protocol;                  /* always 0x000D */
    char     module[5];
    uint8_t  radio_revision;
    char     stepping[3];
    uint8_t  reserved[68];
};

struct phy_status_region {
    uint32_t warm_boot_flag;            /* 0x0000, non-zero after a completed boot */
    uint32_t warm_restore_count;        /* 0x0004 */
    struct reg_restore_entry warm_restore[0x400];   /* 0x0008 */
    struct telemetry telemetry;         /* 0x2008 */
    struct version_info version;        /* 0x2034 */
};

/* ---- RTKit trace ring (0x4D289000) --------------------------------------- */

#define TRACE_RING_ENTRIES          18

struct trace_record {
    uint32_t type;
    uint32_t subtype;
    uint32_t start_ts;
    uint32_t end_ts;
    uint32_t arg0;
    uint32_t arg1;
    uint32_t site;
};

struct trace_ring {
    uint32_t next;
    struct trace_record records[TRACE_RING_ENTRIES];
    uint32_t reserved;
};

/* ---- RTKit crash record (0x4D289200) ------------------------------------- */

struct crash_record {
    char     message[32];
    uint8_t  image_identity[32];        /* ROM 0x00C0..0x00DF */
    uint32_t error_code;
    uint32_t detail0;
    uint32_t detail1;
};

/* Copied into mac_mailbox.crash before the crash doorbell is rung. */
struct crash_descriptor {
    uint32_t error_code;
    uint32_t trace_ring;
    uint32_t trace_size;
    uint32_t detail0;
    uint32_t detail1;
};

#pragma pack(pop)

/* Pointer cells for the shared objects.  They are ordinary .data variables
 * (the firmware loads them at run time rather than folding the addresses). */
extern struct mac_mailbox        *mac_mbox;
extern struct trace_ring         *volatile trace_ring;
extern struct crash_record       *crash_record;
extern struct phy_status_region  *phy_status;

#define PHY_STATUS      BTP_REF(struct phy_status_region, PHY_STATUS_BASE)
#define NVV_MDMI_BLOB   BTP_ARR(uint8_t, NVV_MDMI_SIZE, NVV_MDMI_BASE)

#endif /* BTP_SHARED_H */
