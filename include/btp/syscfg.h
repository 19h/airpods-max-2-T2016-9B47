/*
 * syscfg.h -- SysCfg: the factory-programmed TLV block uploaded by the MAC.
 */
#ifndef BTP_SYSCFG_H
#define BTP_SYSCFG_H

#include "btp.h"

#define SYSCFG_PAYLOAD_MAX          1000
#define SYSCFG_CHUNK_MAX            0xB0
#define SYSCFG_WRITE_LIMIT          0x3F4

enum syscfg_tlv {
    SYSCFG_TLV_TX_MAXPWR_CAL    = 1,    /* same layout as NVV_TAG_TX_MAXPWR_CAL_V2 */
    SYSCFG_TLV_RSSI_CAL         = 2,    /* int8[band][3] */
};

#pragma pack(push, 1)
struct syscfg_header {
    uint16_t version;
    uint16_t device;
    uint16_t payload_length;
    uint16_t flags;             /* low byte: TX power global offset */
    uint32_t checksum;          /* XOR of the payload words */
};

struct syscfg_tlv_header {
    uint16_t tag;
    uint16_t length;            /* rounded up to a multiple of 4 */
};

struct syscfg_block {
    struct syscfg_header header;
    uint8_t payload[SYSCFG_PAYLOAD_MAX];
};
#pragma pack(pop)

struct syscfg_state {
    struct syscfg_block block;
    const int8_t *rssi_cal;     /* SYSCFG_TLV_RSSI_CAL payload, or NULL */
};
extern struct syscfg_state syscfg;

int          syscfg_init(void);
btp_status_t syscfg_write(uint32_t length, uint32_t offset, const void *data);
btp_status_t syscfg_store_chunk(uint32_t length, uint32_t offset, const void *data);
btp_status_t syscfg_verify_checksum(void);
void        *syscfg_find_tag(unsigned tag);
btp_status_t syscfg_apply(void);

#endif /* BTP_SYSCFG_H */
