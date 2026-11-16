/*
 * frs.h -- fast RSSI scan.
 *
 * A scan steps the receiver across a list of channels and records, for each
 * channel, the RSSI reported by the 17-bin correlator together with the AGC
 * gain that was in effect.  Up to three passes with different channel offsets
 * are merged; the results land in the MAC mailbox.
 */
#ifndef BTP_FRS_H
#define BTP_FRS_H

#include "btp.h"

#define FRS_BINS                    17
#define FRS_HALF_SPAN_MAX           8
#define FRS_PASS_MAX                3
#define FRS_CHANNEL_MAX             126
#define FRS_GAIN_CHANNEL_SLOTS      0x98
#define FRS_RSSI_CHANNEL_SLOTS      0x98
#define FRS_SAMPLE_GUARD            8      /* slots before channel 0 in the sample rows */

/* RSSI sample encodings: negative values are quarter-dB readings, small
 * non-negative values are status codes, 0x7FFF is "not scanned". */
enum frs_sample_status {
    FRS_STATUS_INVALID      = 0,
    FRS_STATUS_CLIPPED      = 1,
    FRS_STATUS_PLL_UNLOCKED = 2,
    FRS_STATUS_AGC_FAILED   = 3,
};
#define FRS_SAMPLE_UNSCANNED        0x7FFF
#define FRS_SAMPLE_BAD_STATUS       0x7FFB
#define FRS_SAMPLE_FLOOR            (-0x1B8)
#define FRS_RSSI_SCALE              0.25f

/* Gain-index override argument that means "leave the AGC in automatic mode". */
#define FRS_GAIN_AUTO               0x7F
#define FRS_GAIN_INDEX_MAX          0xC

struct frs_state {
    uint8_t  band;
    uint8_t  reserved_01;
    uint16_t fixed_gain_word;
    uint8_t  fixed_gain_index;
    uint8_t  reserved_05[0xFB];
    uint8_t  gain_rows[FRS_PASS_MAX][FRS_GAIN_CHANNEL_SLOTS];     /* +0x100 */
    uint8_t *channel_list;
    uint32_t channel_count;
    uint8_t *result_gain;
    int16_t *result_rssi;
    uint8_t  pass_count;
    uint8_t  reserved_2d9;
    int16_t  rssi_rows[FRS_PASS_MAX][FRS_RSSI_CHANNEL_SLOTS];     /* +0x2DA */
    uint8_t  reserved_66a[2];
    uint32_t saved_regs[22];                                       /* +0x66C */
    uint8_t  half_span;
    uint8_t  single_channel;        /* one channel, one pass: tune via SEQ_CHAN */
    uint8_t  center_bin_enable;
    uint8_t  reserved_6c7;
};
extern struct frs_state frs;

int      frs_scan_pass(int16_t *rssi_row, const uint8_t *channels, int count, int offset);
uint32_t frs_unpack_channel_bitmap(const uint8_t *bitmap, int bytes, uint8_t *channels,
                                   int *count);
uint32_t frs_run_scan(const uint8_t *channels, int count);
uint32_t frs_process_results(int16_t *rssi_out, uint8_t *gain_out);
uint32_t frs_dump_readings(unsigned band);
bool     frs_configure(uint8_t band, uint32_t half_span, uint32_t passes, bool allow_single);
uint32_t frs_enter(uint32_t gain_index, int16_t *rssi_out, uint8_t *gain_out);
uint32_t frs_exit(void);
void     frs_correlator_init(void);
char    *frs_format_rssi(int value, float scale);

#endif /* BTP_FRS_H */
