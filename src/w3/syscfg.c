/*
 * syscfg.c -- SysCfg, the per-device factory configuration block.
 *
 * The MAC uploads the block in chunks (MAC_SUB_SYSCFG op 1) and then asks for
 * it to be applied (op 2).  The payload is a sequence of TLVs; the two the PHY
 * understands supply a TX maximum-power table and an RSSI calibration table.
 */
#include "btp/btp.h"
#include "btp/syscfg.h"
#include "btp/trace.h"
#include "btp/status.h"
#include "btp/nvv.h"
#include "btp/txpwr.h"

struct syscfg_state syscfg BTP_STATE(syscfg);

_Static_assert(sizeof(struct syscfg_block) == 1012, "syscfg block size");

/* @provenance RVA 0x412C syscfg_init */
int syscfg_init(void)
{
    syscfg.rssi_cal = NULL;
    return dbg_printf("\nsyscfg_g @ %08x\n", &syscfg.block);
}

/* @provenance RVA 0x4148 syscfg_write_chunk_checked */
btp_status_t syscfg_write(uint32_t length, uint32_t offset, const void *data)
{
    if (length > SYSCFG_CHUNK_MAX)
        return BTP_ERR_INVALID;
    return syscfg_store_chunk(length, offset, data);
}

/* @provenance RVA 0x4150 syscfg_write_chunk */
btp_status_t syscfg_store_chunk(uint32_t length, uint32_t offset, const void *data)
{
    if (offset + length > SYSCFG_WRITE_LIMIT || (offset & 3) != 0)
        return BTP_ERR_INVALID;
    memcpy((uint8_t *)&syscfg.block + offset, data, length);
    return BTP_OK;
}

/* @provenance RVA 0x417C syscfg_verify_checksum */
btp_status_t syscfg_verify_checksum(void)
{
    const struct syscfg_header *hdr = &syscfg.block.header;
    uint32_t computed = xor_checksum_words(syscfg.block.payload, hdr->payload_length >> 2);

    if (computed == hdr->checksum)
        return BTP_OK;
    dbg_printf("\n checksum ERROR got %08x expect  %08x\n", hdr->checksum, computed);
    return BTP_ERR_INVALID;
}

/* @provenance RVA 0x41AC syscfg_find_tlv_tag */
void *syscfg_find_tag(unsigned tag)
{
    const uint8_t *payload = syscfg.block.payload;
    uint32_t end = syscfg.block.header.payload_length + sizeof(struct syscfg_header);
    uint32_t offset = 0;

    while (offset < end) {
        const struct syscfg_tlv_header *tlv = (const void *)(payload + offset);

        if (tlv->tag == tag)
            return (void *)(payload + offset + sizeof *tlv);
        offset += (uint16_t)((tlv->length & ~3u) + sizeof *tlv);
    }
    return NULL;
}

/* @provenance RVA 0x41E4 syscfg_apply */
btp_status_t syscfg_apply(void)
{
    const struct syscfg_header *hdr = &syscfg.block.header;
    const uint8_t *tx_table;
    btp_status_t rc;

    dbg_printf("\nappkying syscfg ver %d, dev %04x len=%d checksum=%08x\n",
               hdr->version, hdr->device, hdr->payload_length, hdr->checksum);

    if (hdr->payload_length > SYSCFG_PAYLOAD_MAX)
        return BTP_ERR_INVALID;
    rc = syscfg_verify_checksum();
    if (rc == BTP_ERR_INVALID)
        return BTP_ERR_INVALID;

    tx_table = syscfg_find_tag(SYSCFG_TLV_TX_MAXPWR_CAL);
    if (tx_table != NULL) {
        /* The SysCfg table replaces the NVV one and resets the offsets. */
        nvv.tx_maxpwr_cal = tx_table;
        txpwr_init();
        txpwr.cal_offset = 0;
        txpwr.global_offset = (uint8_t)hdr->flags;
        nvv_presence.p0 |= NVV_P0_SYSCFG_TXPWR;
    }

    syscfg.rssi_cal = syscfg_find_tag(SYSCFG_TLV_RSSI_CAL);
    dbg_printf("\n syscfg_rssicalp=%08x\n", syscfg.rssi_cal);
    if (syscfg.rssi_cal != NULL)
        nvv_presence.p0 |= NVV_P0_SYSCFG_RSSI_CAL;

    return rc;
}
