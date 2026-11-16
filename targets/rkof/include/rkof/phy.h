#ifndef RKOF_PHY_H
#define RKOF_PHY_H

#include <stddef.h>
#include <stdint.h>

/*
 * W3 PHY Calibration, Modem Warm-Restore, Coredump/Crash Capture, & Shell
 * (src/phy/modem_inits.c, src/phy/scanmedia.c, src/phy/bt_phy.c)
 */

struct rkof_nvv_mdmi_spec {
    uint16_t tag_id;
    uint16_t expected_len;
};

typedef uint32_t (*rkof_media_read32_fn)(uint32_t offset);
typedef void (*rkof_scan_entry_cb)(
    const void *hdr_ptr,
    uint32_t media_id,
    uint32_t entry_off,
    uint32_t total_size,
    rkof_media_read32_fn reader
);

struct rkof_scanmedia_ctx {
    uint32_t media_id;              /* +0x00 */
    uint32_t start_off;             /* +0x04 */
    uint32_t scan_len;              /* +0x08 */
    rkof_media_read32_fn reader;    /* +0x0C */
    uint32_t end_off_out;           /* +0x10 */
    uint32_t reserved_14;           /* +0x14 */
    rkof_scan_entry_cb callback;    /* +0x18 */
    uint32_t reserved_1c[2];        /* +0x1C..+0x23 */
    uint32_t status_24;             /* +0x24 */
    uint32_t record_count;          /* +0x28 */
    uint32_t status_2c;             /* +0x2C */
    uint32_t status_30;             /* +0x30 */
};

/* src/phy/modem_inits.c */
uint16_t sub_FDD74(uint16_t seed, const uint8_t *data, uint32_t len);
void sub_FDD90(int assert_reset);
void sub_FDDDC(const uint32_t *pairs, uint32_t byte_len);
void sub_FDDF8(const uint32_t *triples, uint32_t byte_len);
void sub_FDE48(uint32_t reg_addr, uint32_t reg_val);
void sub_FDE74(void);
void bt_phy_mmio_init_seq(void);
void bt_phy_mmio_pre_otp(void);

/* src/phy/scanmedia.c */
int sub_FDE94(uint32_t power_mode);
uint32_t sub_FDEC0(uint32_t byte_offset);
uint32_t sub_FDF08(
    uint32_t cur_off,
    uint32_t end_off,
    rkof_media_read32_fn reader,
    uint32_t *hdr_adj_out
);
uint16_t sub_FDF78(
    uint16_t seed,
    uint32_t media_off,
    uint32_t byte_len,
    rkof_media_read32_fn reader
);
void sub_FDFD0(struct rkof_scanmedia_ctx *ctx);

/* src/phy/bt_phy.c */
int bt_phy_otp_init(void);
void sub_FDC04(uint8_t *log_pack);
uint32_t sub_FDC38(void *unused_ctx, uint8_t *dst_buf, uint32_t max_trace_bytes);
uint32_t sub_FDCB4(void *unused_ctx, uint32_t *dst_words, uint32_t max_bytes);
int sub_FDD1C(uint32_t *power_state_out);
int sub_FDD48(uint32_t power_state);
int phy_shell_cmd(void *shell_ctx, const char *const *argv);

#endif /* RKOF_PHY_H */
