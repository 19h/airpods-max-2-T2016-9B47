/*
 * status.c -- things the MAC asks the PHY about itself: radio revision,
 * warm-boot state, telemetry, version and the diagnostic buffer export.
 */
#include "btp/btp.h"
#include "btp/status.h"
#include "btp/rtkit.h"
#include "btp/trace.h"
#include "btp/nvv.h"
#include "btp/rx.h"
#include "btp/tx.h"
#include "btp/syscfg.h"

uint32_t          radio_revision  BTP_STATE(radio_rev);
warm_boot_magic_t warm_boot_magic BTP_STATE(warm_boot);
struct diag_state diag            BTP_STATE(diag);

/* .data */
struct phy_status_region *phy_status BTP_DATA(phy_status) = (struct phy_status_region *)PHY_STATUS_BASE;
struct crash_record      *crash_record BTP_DATA(crash_record) = (struct crash_record *)CRASH_RECORD_BASE;
uint32_t btp_build_id BTP_DATA(btp_build_id) = 0xD3E016BDu;
uint32_t btp_version_format BTP_DATA(btp_version_format) = 1;

/* ---- radio revision ------------------------------------------------------------ */

/* @provenance RVA 0x4B78 btp_detect_radio_revision */
uint32_t radio_revision_detect(void)
{
    uint32_t major = FIELD_GET(RADIO_ID_MAJOR, RADIO_ID);
    uint32_t minor = FIELD_GET(RADIO_ID_MINOR, RADIO_ID);
    uint32_t revision;

    if (major == 'A' && minor == 0)
        revision = RADIO_REV_A0;
    else if (major == 'A' && minor == 1)
        revision = RADIO_REV_A1;
    else if (major == 'B' && minor == 0)
        revision = RADIO_REV_B0;
    else
        revision = RADIO_REV_UNKNOWN;

    radio_revision = revision;
    return revision;
}

/* @provenance RVA 0x4BB8 btp_get_radio_revision */
uint32_t radio_revision_get(void)
{
    return radio_revision;
}

/* ---- warm boot ------------------------------------------------------------------ */

/* @provenance RVA 0x4D58 btp_is_warm_boot */
bool warm_boot_detected(void)
{
    return warm_boot_magic == WARM_BOOT_MAGIC && PHY_STATUS.warm_boot_flag != 0;
}

/* @provenance RVA 0x4D94 btp_set_warm_boot_state */
void warm_boot_set_state(uint32_t state)
{
    if (state != 0)
        warm_boot_magic = WARM_BOOT_MAGIC;
    PHY_STATUS.warm_boot_flag = state;
}

/* ---- telemetry ------------------------------------------------------------------- */

/* Keep SDIV's signed-overflow and configured divide-by-zero behavior on the
 * target. A C INT32_MIN / -1 expression would have undefined behavior. */
static int32_t telemetry_divide(int32_t numerator, uint32_t denominator_bits)
{
#ifndef BTP_HOST
    int32_t result;

    __asm__ volatile("sdiv %0, %1, %2" : "=r"(result)
                     : "r"(numerator), "r"(denominator_bits));
    return result;
#else
    int32_t denominator = denominator_bits <= INT32_MAX
        ? (int32_t)denominator_bits : -1 - (int32_t)(UINT32_MAX - denominator_bits);

    /* Host adapter uses the nontrapping result for a zero divisor. */
    if (denominator == 0)
        return 0;
    if (numerator == INT32_MIN && denominator == -1)
        return INT32_MIN;
    return numerator / denominator;
#endif
}

/* @provenance RVA 0x4BC4 btp_read_and_reset_telemetry */
btp_status_t telemetry_read_and_reset(void *dst, uint32_t *copied, uint32_t capacity)
{
    volatile struct telemetry *tele = &PHY_STATUS.telemetry;

    if (dst == NULL || copied == NULL || capacity <= sizeof *tele - 1) {
        trace_log_error(TRACE_ERR_CLASS_COMMAND, 0, TRACE_ERR_TELEMETRY_ARGS);
        return BTP_ERR_INVALID;
    }

    volatile struct tx_state *tx_state = &tx;
    volatile struct rx_state *rx_state = &rx;

    tele->stats.tx_count = tx_state->packet_count;
    tele->stats.rx_count = rx_state->packet_count;
    tele->stats.rssi_histogram[0] = rx_state->rssi_histogram[0];
    tele->stats.rssi_histogram[1] = rx_state->rssi_histogram[1];
    tele->stats.rssi_histogram[2] = rx_state->rssi_histogram[2];
    tele->stats.rssi_histogram[3] = rx_state->rssi_histogram[3];
    uint32_t count = rx_state->packet_count;
    int32_t cfo = rx_state->cfo_sum;

    if (count != 0)
        cfo = telemetry_divide(cfo, rx_state->packet_count);
    tele->stats.cfo_mean = cfo;
    tele->stats.cfo_max = rx_state->cfo_max;
    tele->stats.cfo_min = rx_state->cfo_min;

    tx_state->packet_count = 0;
    rx_state->packet_count = 0;
    rx_state->rssi_histogram[0] = 0;
    rx_state->rssi_histogram[1] = 0;
    rx_state->rssi_histogram[2] = 0;
    rx_state->rssi_histogram[3] = 0;
    rx_state->cfo_sum = 0;
    rx_state->cfo_max = RX_CFO_MAX_INIT;
    rx_state->cfo_min = RX_CFO_MIN_INIT;

    memcpy(dst, (const void *)tele, sizeof *tele);
    *copied = sizeof *tele;
    return BTP_OK;
}

/* ---- version ------------------------------------------------------------------------ */

/* @provenance RVA 0x4C9C btp_read_version_info */
btp_status_t version_info_read(void *dst, uint32_t *copied, uint32_t capacity)
{
    if (dst == NULL || copied == NULL || capacity <= sizeof(struct version_info) - 1) {
        trace_log_error(TRACE_ERR_CLASS_COMMAND, 0, TRACE_ERR_VERSION_ARGS);
        return BTP_ERR_INVALID;
    }

    version_info_populate(&PHY_STATUS);
    memcpy(dst, &PHY_STATUS.version, sizeof(struct version_info));
    *copied = sizeof(struct version_info);
    return BTP_OK;
}

/* @provenance RVA 0x4CE4 btp_populate_version_info */
int version_info_populate(struct phy_status_region *status)
{
    struct version_info *info = &status->version;

    strlcat(info->version, VERSION_STRING, sizeof info->version);
    info->format = btp_version_format;
    info->build_id = btp_build_id;
    info->protocol = 0x000D;
    info->nvv_presence1 = nvv_presence.p1;
    info->nvv_presence0 = nvv_presence.p0;
    strlcpy(info->module, nvv.mod_info, sizeof info->module);
    strlcpy(info->stepping, VERSION_STEPPING, sizeof info->stepping);
    info->radio_revision = (uint8_t)radio_revision;
    return 0;
}

/* ---- diagnostic buffers ---------------------------------------------------------- */

#pragma pack(push, 1)
/* Header of a diag_buffer_read() response, followed by the data at +0x10. */
struct diag_chunk_header {
    uint8_t  buffer_index;
    uint16_t source_offset;
    uint16_t total_buffer_length;
    uint8_t  chunk_length;
    uint32_t checksum;
    uint8_t  reserved[6];
};
#pragma pack(pop)

/* @provenance RVA 0x4DBC btp_read_diagnostic_buffer_chunk */
btp_status_t diag_buffer_read(uint8_t *dst, uint32_t *copied, uint32_t buffer_index,
                              uint32_t source_offset, uint32_t requested_length,
                              uint32_t response_capacity)
{
    volatile struct diag_chunk_header *response_header = (volatile void *)dst;
    const volatile struct diag_buffer *descriptor;
    uint32_t checksum;
    uint16_t total_buffer_length;
    uint8_t chunk_length;

    if (dst == NULL || copied == NULL || response_capacity < DIAG_CHUNK_HEADER
        || buffer_index >= DIAG_BUFFER_COUNT) {
        trace_log_error(TRACE_ERR_CLASS_COMMAND, 0, TRACE_ERR_DIAG_ARGS);
        return BTP_ERR_INVALID;
    }

    descriptor = &diag.buffers[buffer_index];
    /* The output can overlap this descriptor. The image samples these two
     * fields before the first header store, and samples base after the last.
     * Preserve both the access order and the individual packed store widths. */
    checksum = descriptor->checksum;
    total_buffer_length = descriptor->length;
    response_header->source_offset = (uint16_t)source_offset;
    response_header->buffer_index = (uint8_t)buffer_index;
    response_header->checksum = checksum;
    response_header->total_buffer_length = total_buffer_length;

    chunk_length = (uint8_t)requested_length;
    if (requested_length >= DIAG_CHUNK_MAX)
        chunk_length = DIAG_CHUNK_MAX;
    if (total_buffer_length < requested_length)
        chunk_length = (uint8_t)total_buffer_length;
    response_header->chunk_length = chunk_length;

    memcpy(dst + DIAG_CHUNK_HEADER, (const uint8_t *)BTP_PTR(descriptor->base) + source_offset, chunk_length);
    *copied = DIAG_CHUNK_RESPONSE_SIZE;
    return BTP_OK;
}

/* @provenance RVA 0x4E2C btp_init_diagnostic_buffer_table */
struct diag_buffer *diag_buffers_init(void)
{
    struct diag_buffer *trace = &diag.buffers[DIAG_BUFFER_TRACE];
    struct diag_buffer *cfg = &diag.buffers[DIAG_BUFFER_SYSCFG];
    uint16_t payload_length;

    cfg->base = (uint32_t)(uintptr_t)&syscfg.block;
    payload_length = syscfg.block.header.payload_length;
    trace->length = sizeof(struct trace_ring);
    cfg->freezable = 0;
    trace->checksum = 0;
    cfg->checksum = 0;
    trace->base = TRACE_RING_BASE;
    trace->freezable = 1;
    cfg->length = payload_length + sizeof(struct syscfg_header);
    return diag.buffers;
}

/* @provenance RVA 0x4E5C btp_set_trace_freeze_bit */
int diag_set_freeze(unsigned index, bool frozen)
{
    uint32_t bit = 1u << index;
    bool enabled = irq_save();

    if (frozen)
        diag.freeze_mask |= bit;
    else
        diag.freeze_mask &= ~bit;
    irq_restore(enabled);
    return 0;
}

/* ROM 0x4E88 loads one 32-bit word per iteration; callers divide byte
 * lengths by four. No trailing-byte processing is performed. */
/* @provenance RVA 0x4E88 btp_compute_xor_checksum */
uint32_t xor_checksum_words(const void *words, uint32_t word_count)
{
    const uint32_t *w = words;
    uint32_t checksum = 0;

    for (uint32_t i = 0; i < word_count; ++i)
        checksum ^= w[i];
    return checksum;
}
