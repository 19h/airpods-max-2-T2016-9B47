#include "rkof/memmap.h"
#include "rkof/phy.h"
#include "rkof/rtkit.h"

extern int sub_18E930(uint32_t *word_out, uint16_t aligned_off);
extern void sub_1878C(void *log_pack, uint32_t media_id, uint32_t off, uint32_t exp_crc, uint32_t got_crc);
extern void sub_141B0(void *log_pack);
extern void sub_1A646(uintptr_t log_token, const void *log_pack);

/* @provenance RVA 0xfde94 sub_FDE94 */
int sub_FDE94(uint32_t power_mode)
{
    uintptr_t pwr_obj = *RKOF_PTR(const uintptr_t *, RKOF_DATA_PWR_VTABLE_PTR_ADDR);
    const uintptr_t *vtable = RKOF_PTR(const uintptr_t *, pwr_obj);
    uintptr_t fn_addr = vtable[0x0Cu / sizeof(uintptr_t)];
    if (fn_addr == 0u) {
        rtkit_assert_fail("RTKitOS.c", 0x18FCu, "rtk_power_set_domain");
    }
    typedef int (*pwr_set_fn)(uintptr_t obj, uint32_t domain_id, uint32_t mode);
    pwr_set_fn fn = (pwr_set_fn)fn_addr;
    return fn(RKOF_DATA_PWR_VTABLE_PTR_ADDR, 0x5Cu, power_mode);
}

/* @provenance RVA 0xfdec0 sub_FDEC0 */
uint32_t sub_FDEC0(uint32_t byte_offset)
{
    uint32_t low_word = 0u;
    uint16_t aligned_lo = (uint16_t)(byte_offset & 0xFFFCu);
    int rc = sub_18E930(&low_word, aligned_lo);
    if (rc != 0) {
        return (uint32_t)rc;
    }

    uint32_t unaligned = byte_offset & 3u;
    if (unaligned == 0u) {
        return low_word;
    }

    uint32_t high_word = 0u;
    uint16_t aligned_hi = (uint16_t)((byte_offset + 3u) & ~3u);
    (void)sub_18E930(&high_word, aligned_hi);

    uint32_t rshift = unaligned * 8u;
    return (low_word >> rshift) | (high_word << (32u - rshift));
}

/* @provenance RVA 0xfdf08 sub_FDF08 */
uint32_t sub_FDF08(
    uint32_t cur_off,
    uint32_t end_off,
    rkof_media_read32_fn reader,
    uint32_t *hdr_adj_out)
{
    if (cur_off >= end_off) {
        return 6u;
    }

    uint32_t alt_off = cur_off + 0xC0u;
    uint16_t magic_direct;
    uint16_t magic_alt = 0u;

    if (reader != NULL) {
        magic_direct = (uint16_t)(reader(cur_off) >> 16u);
        if (alt_off < end_off) {
            magic_alt = (uint16_t)(reader(alt_off) >> 16u);
        }
    } else {
        magic_direct = *RKOF_PTR(const uint16_t *, cur_off + 2u);
        if (alt_off < end_off) {
            magic_alt = (uint16_t)(*RKOF_PTR(const uint32_t *, alt_off) >> 16u);
        }
    }

    const uint16_t *magic_table = RKOF_PTR(const uint16_t *, RKOF_RODATA_NV_MAGICS_ADDR);
    uint32_t adj = 0u;
    for (uint32_t idx = 0u; idx < 6u; idx++) {
        uint16_t expected = magic_table[idx];
        if (magic_alt == expected) {
            adj = 0xC0u;
        }
        if (magic_direct == expected) {
            adj = 0u;
        }
        if (magic_direct == expected || magic_alt == expected) {
            *hdr_adj_out = adj;
            return idx;
        }
    }

    *hdr_adj_out = adj;
    return 6u;
}

/* @provenance RVA 0xfdf78 sub_FDF78 */
uint16_t sub_FDF78(
    uint16_t seed,
    uint32_t media_off,
    uint32_t byte_len,
    rkof_media_read32_fn reader)
{
    uint16_t crc = seed;

    if (reader != NULL) {
        while (byte_len >= 5u) {
            uint32_t word = reader(media_off);
            crc = sub_FDD74(crc, (const uint8_t *)&word, 4u);
            media_off += 4u;
            byte_len -= 4u;
        }
        if (byte_len != 0u) {
            uint32_t tail_word = reader(media_off);
            crc = sub_FDD74(crc, (const uint8_t *)&tail_word, byte_len);
        }
    } else {
        crc = sub_FDD74(crc, RKOF_PTR(const uint8_t *, media_off), byte_len);
    }

    return (crc == 0u) ? 0xFFFFu : crc;
}

/* @provenance RVA 0xfdfd0 sub_FDFD0 */
void sub_FDFD0(struct rkof_scanmedia_ctx *ctx)
{
    uint32_t media_id = ctx->media_id;
    uint32_t cur_off = ctx->start_off;
    uint32_t end_off = ctx->start_off + ctx->scan_len;
    rkof_media_read32_fn reader = ctx->reader;
    uint32_t records_seen = 0u;
    uint32_t hdr_adj = 0u;

    for (;;) {
        uint32_t kind = sub_FDF08(cur_off, end_off, reader, &hdr_adj);
        if (kind == 6u) {
            uint32_t aligned4 = (cur_off + 3u) & ~3u;
            kind = sub_FDF08(aligned4, end_off, reader, &hdr_adj);
            if (kind == 6u) {
                uint32_t aligned256 = (aligned4 + 0xFFu) & ~0xFFu;
                kind = sub_FDF08(aligned256, end_off, reader, &hdr_adj);
                if (kind == 6u) {
                    ctx->status_30 = 0u;
                    ctx->status_2c = 0u;
                    ctx->record_count = records_seen;
                    ctx->status_24 = 0u;
                    ctx->end_off_out = cur_off;
                    return;
                }
                cur_off = aligned256;
            } else {
                cur_off = aligned4;
            }
        }

        if (kind != 3u) {
            rtkit_assert_fail("scanmedia.c", 157u, "scan_media_entries");
        }

        const uint8_t *hdr_ptr;
        uint32_t hdr_off = hdr_adj + cur_off;
        if (reader != NULL) {
            uint32_t *heap_hdr = (uint32_t *)rt_malloc(0x18u);
            heap_hdr[0] = reader(hdr_off);
            heap_hdr[1] = reader(hdr_off + 4u);
            hdr_ptr = (const uint8_t *)heap_hdr;
        } else {
            hdr_ptr = RKOF_PTR(const uint8_t *, hdr_off);
        }

        uint16_t payload_len = *(const uint16_t *)(const void *)hdr_ptr;
        uint16_t stored_crc = *(const uint16_t *)(const void *)(hdr_ptr + 6u);
        uint32_t total_entry_sz = (uint32_t)payload_len + 8u;

        if (stored_crc != 0u) {
            uint16_t crc_head = sub_FDF78(0u, cur_off, hdr_adj + 6u, reader);
            uint16_t crc_full = sub_FDF78(
                crc_head,
                hdr_off + 8u,
                (uint32_t)payload_len - hdr_adj,
                reader
            );
            if (crc_full != stored_crc) {
                uint8_t log_pack[24] = { 0 };
                sub_1878C(log_pack, 3u, cur_off, stored_crc, crc_full);
                sub_1A646(0x200E6C8Du, log_pack);
                cur_off += total_entry_sz;
                continue;
            }
        } else if (media_id != 0u) {
            uint8_t log_pack[16] = { 0 };
            sub_141B0(log_pack);
            sub_1A646(0x200E6CB9u, log_pack);
        }

        if (ctx->callback != NULL) {
            ctx->callback(hdr_ptr, media_id, cur_off, total_entry_sz, reader);
        }

        cur_off += total_entry_sz;
        records_seen++;
    }
}
