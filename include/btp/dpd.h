/*
 * dpd.h -- digital pre-distortion capture and actuation experiments.
 */
#ifndef BTP_DPD_H
#define BTP_DPD_H

#include "btp.h"

#define DPD_CAPTURE_WORDS           752
#define DPD_ACTUATION_WORDS         240
#define DPD_TEST_WORDS              480     /* 0x780 bytes */
#define DPD_TABLE_ACTUATION_BYTES   0x3C0
#define DPD_TABLE_TEST_BYTES        0x780
#define DPD_TABLE_FULL_BYTES        0xB40

enum dpd_command {
    DPD_CMD_SET_MODE    = 0,
    DPD_CMD_ACTUATE     = 1,
    DPD_CMD_TEST        = 2,
    DPD_CMD_GET_RESULT  = 3,
};

struct dpd_state {
    const void *test_table;         /* 0x780-byte NVV table */
    uint32_t    actuation_enable;
    uint32_t    actuation_gtb;      /* GTB count at which to report */
    const void *actuation_table;    /* 0x3C0-byte NVV table */
    uint32_t    capture_gtb;        /* GTB count at which to capture; 0 = off */
    uint32_t    test_enable;
    uint32_t    test_gtb;
    uint32_t    result_armed;       /* set on entry to every DPD command, including invalid opcodes */
    uint32_t    result_ptr;
    uint32_t    result_len;
    uint32_t    capture[DPD_CAPTURE_WORDS];
    uint32_t    mode;
};
extern struct dpd_state dpd;

void     dpd_set_tables(const void *table, uint32_t length);
void     dpd_on_tx_setup(void);
void     dpd_capture_done_isr(void);
uint32_t dpd_handle_command(const uint32_t *cmd, uint32_t *out, int *out_len);

#endif /* BTP_DPD_H */
