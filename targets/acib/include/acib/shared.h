#ifndef ACIB_SHARED_H
#define ACIB_SHARED_H

#include "acib/acib.h"
#include "acib/memmap.h"

/*
 * Exact MAC (acib) <-> W3 PHY (phyb) Shared Mailbox Contract at 0x4D280000.
 *
 * Cross-verified between acib (sub_7C820, sub_7CB20, sub_7CD5C, sub_7D35C,
 * sub_7D9D4, sub_7DB44, sub_7DB60, sub_84EB8) and phyb's MAC_MAILBOX_BASE.
 *
 * Request window  : g_phy_mbox_req  (0x1002268C) -> 0x4D280000 (size 0x200)
 * Response window : g_phy_mbox_resp (0x10022690) -> 0x4D280400 on B0 phyb
 *                   (falls back to 0x4D2801E6 if legacy PHY mutates +0x1EE)
 */

enum acib_phy_cmd_opcode {
    ACIB_PHY_CMD_HANDSHAKE_INIT = 0,
    ACIB_PHY_CMD_RF_CONFIG      = 1,
    ACIB_PHY_CMD_SET_OP2        = 2,
    ACIB_PHY_CMD_QUERY_READY    = 3,
    ACIB_PHY_CMD_SLEEP_ASYNC    = 4,
};

/*
 * Request window @ 0x4D280000 (0x200 bytes)
 */
struct ACIB_PACKED acib_phy_mbox_req {
    uint16_t flags0;                /* +0x000: cmd flags / bit14 cleared on ack */
    uint16_t opcode;                /* +0x002: enum acib_phy_cmd_opcode */
    uint32_t reserved_004;          /* +0x004 */
    uint16_t args_u16[4];           /* +0x008..+0x00F: handshake {3, 0x5B, 1, 0} */
    uint8_t  cmd_payload[0xB0];     /* +0x010..+0x0BF */
    uint16_t mac_want_lock;         /* +0x0C0: Dekker spinlock MAC intent flag */
    uint16_t phy_want_lock;         /* +0x0C2: Dekker spinlock PHY intent flag */
    uint8_t  shared_link_table[0x120]; /* +0x0C4..+0x1E3: 288-byte MAC->PHY sync table */
    uint8_t  frs_mode;              /* +0x1E4: Fast RSSI Scan mode (0..7, != 3) */
    uint8_t  frs_param;             /* +0x1E5: scan slot/dwell parameter */
    uint8_t  frs_chan_bitmap[0x10]; /* +0x1E6..+0x1F5: 128-bit channel mask (also +0x1EE legacy probe) */
    uint8_t  frs_dwell_ticks;       /* +0x1F6: per-channel dwell count */
    uint8_t  frs_band_flags;        /* +0x1F7: bits[2:0] band select */
    uint8_t  frs_ctrl_flags;        /* +0x1F8: bit[0] single-shot / trigger flag */
    uint32_t coex_grant_word;       /* +0x1F9..+0x1FC: unaligned 32-bit coex state (db bit 2) */
    uint8_t  pad_1fd[3];            /* +0x1FD..+0x1FF */
};

/*
 * Response window @ 0x4D280400 (0x280 bytes)
 */
struct ACIB_PACKED acib_phy_mbox_resp {
    uint8_t  hdr[8];                /* +0x000..+0x007 */
    uint16_t fw_version[4];         /* +0x008..+0x00F: PHY version tuple / op3 status @ +0x008 */
    uint8_t  cmd_resp_body[0xB0];   /* +0x010..+0x0BF */
    int16_t  frs_rssi_q[0x7E];      /* +0x0C0..+0x1BB: 126 signed 16-bit channel RSSI samples */
    uint8_t  reserved_1bc[4];       /* +0x1BC..+0x1BF */
    uint8_t  frs_status;            /* +0x1C0: 0xFF while busy, 0x00 ok, nonzero error */
    uint32_t crash_words[5];        /* +0x1C1..+0x1D4: unaligned 5-word PHY panic frame */
    uint8_t  cal_telemetry[0x29];   /* +0x1D5..+0x1FD: 41-byte PHY calibration status block */
    uint8_t  frs_aux_metrics[0x7E]; /* +0x1FE..+0x27B: 126-byte per-channel auxiliary metric */
    uint8_t  pad_27c[4];            /* +0x27C..+0x27F */
};

#endif /* ACIB_SHARED_H */
