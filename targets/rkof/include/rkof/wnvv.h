#ifndef RKOF_WNVV_H
#define RKOF_WNVV_H

#include <stdint.h>
#include "rkof/phy.h"

/*
 * Wireless NVRAM / OTP Tag Linked-List & Ring Helpers (src/phy/wnvv.c)
 */

struct rkof_wnvv_node {
    struct rkof_wnvv_node *next;    /* +0x00 */
    uint16_t tag_id;                /* +0x04 */
    uint16_t payload_len;           /* +0x06 */
    const uint8_t *payload_ptr;     /* +0x08 */
};

struct rkof_byte_ring {
    uint32_t flags;                 /* +0x00 */
    uint32_t size;                  /* +0x04 */
    uint32_t wr;                    /* +0x08 */
    uint32_t rd;                    /* +0x0C */
    uint8_t *buf;                   /* +0x10 */
};

void sub_FE2B0(
    const void *hdr_ptr,
    uint32_t media_id,
    uint32_t entry_off,
    uint32_t total_size,
    rkof_media_read32_fn reader
);
struct rkof_wnvv_node *sub_FE4D4(uint32_t tag_id, struct rkof_wnvv_node ***prev_next_out);
const uint8_t *sub_FE504(uint32_t tag_id, uint16_t *len_out);
uint8_t sub_FE564(uint32_t crc_init, const uint8_t *buf, uint32_t len);
uint32_t sub_FE580(const uint32_t *state_words);
uint16_t sub_FE59E(const struct rkof_byte_ring *ring);
uint32_t sub_FE7CE(const struct rkof_byte_ring *ring);

#endif /* RKOF_WNVV_H */
