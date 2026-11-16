/*
 * status.h -- radio revision, warm-boot handshake, telemetry, version
 * reporting and the diagnostic buffer export used by the MAC.
 */
#ifndef BTP_STATUS_H
#define BTP_STATUS_H

#include "btp.h"

/* Radio silicon revision, decoded from RADIO_ID. */
enum radio_revision {
    RADIO_REV_UNKNOWN   = 0,
    RADIO_REV_A0        = 2,
    RADIO_REV_A1        = 3,
    RADIO_REV_B0        = 4,
};

extern uint32_t radio_revision;

uint32_t radio_revision_detect(void);
uint32_t radio_revision_get(void);

/* Warm boot: the status page flag plus a 64-bit magic in PHY RAM. */
#define WARM_BOOT_MAGIC             0xE91D98A54DBCC725ull

/* The magic sits at a word-aligned (not double-word-aligned) address. */
typedef uint64_t __attribute__((aligned(4))) warm_boot_magic_t;
extern warm_boot_magic_t warm_boot_magic;

bool warm_boot_detected(void);
void warm_boot_set_state(uint32_t state);

/* Telemetry and version, read out through the mailbox. */
btp_status_t telemetry_read_and_reset(void *dst, uint32_t *copied, uint32_t capacity);
btp_status_t version_info_read(void *dst, uint32_t *copied, uint32_t capacity);
int          version_info_populate(struct phy_status_region *status);

/* Diagnostic buffers exported to the host (trace ring, syscfg). */
#define DIAG_BUFFER_COUNT           3
#define DIAG_BUFFER_TRACE           0
#define DIAG_BUFFER_SYSCFG          1
#define DIAG_CHUNK_MAX              0x96
#define DIAG_CHUNK_HEADER           0x10
#define DIAG_CHUNK_RESPONSE_SIZE    0xA6

struct diag_buffer {
    uint8_t  freezable;         /* can be frozen while the host reads it */
    uint8_t  reserved[3];
    uint32_t base;              /* address in PHY RAM */
    uint16_t length;
    uint16_t reserved_0a;
    uint32_t checksum;          /* XOR of the 32-bit words */
};

struct diag_state {
    uint32_t freeze_mask;       /* bit n: buffer n frozen */
    uint8_t  rssi_baseline[4];  /* RX RSSI calibration fields captured at init */
    struct diag_buffer buffers[DIAG_BUFFER_COUNT];
};
extern struct diag_state diag;

btp_status_t diag_buffer_read(uint8_t *dst, uint32_t *copied, uint32_t buffer_index,
                              uint32_t source_offset, uint32_t requested_length,
                              uint32_t response_capacity);
struct diag_buffer *diag_buffers_init(void);
int          diag_set_freeze(unsigned index, bool frozen);
uint32_t     xor_checksum_words(const void *words, uint32_t word_count);

/* Previous source spellings; descriptive names above are canonical. */
#define xor_checksum xor_checksum_words

#endif /* BTP_STATUS_H */
