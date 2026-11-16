#include "rkof/libc.h"
#include "rkof/memmap.h"
#include "rkof/regs.h"
#include "rkof/rtkit.h"
#include "rkof/wnvv.h"

extern void sub_18EC0(void *log_pack, uint32_t tag_id, uint32_t len);
extern void sub_141B8(void *log_pack, uint32_t tag_id);
extern void sub_1A646(uintptr_t log_token, const void *log_pack);

/* @provenance RVA 0xfe4d4 sub_FE4D4 */
struct rkof_wnvv_node *sub_FE4D4(uint32_t tag_id, struct rkof_wnvv_node ***prev_next_out)
{
    struct rkof_wnvv_node **link = RKOF_PTR(
        struct rkof_wnvv_node **,
        STATE_BT_PHY_OTP_BASE + 0x30u
    );
    struct rkof_wnvv_node *cur = *link;

    while (cur != NULL && (uint32_t)cur->tag_id < tag_id) {
        link = &cur->next;
        cur = *link;
    }

    if (prev_next_out != NULL) {
        *prev_next_out = link;
        cur = *link;
    }

    if (cur != NULL && (uint32_t)cur->tag_id == tag_id) {
        return cur;
    }
    return NULL;
}

/* @provenance RVA 0xfe504 sub_FE504 */
const uint8_t *sub_FE504(uint32_t tag_id, uint16_t *len_out)
{
    if (tag_id > 0x3FFEu) {
        return NULL;
    }

    struct rkof_wnvv_node *node = sub_FE4D4(tag_id, NULL);
    if (node == NULL) {
        return NULL;
    }

    *len_out = node->payload_len;
    return node->payload_ptr;
}

/* @provenance RVA 0xfe2b0 sub_FE2B0 */
void sub_FE2B0(
    const void *hdr_ptr,
    uint32_t media_id,
    uint32_t entry_off,
    uint32_t total_size,
    rkof_media_read32_fn reader)
{
    (void)hdr_ptr;
    int32_t rem = (int32_t)total_size - 8;
    uint32_t cur_off = entry_off + 8u;

    while (rem != 0) {
        uint32_t raw_hdr = (reader != NULL)
            ? reader(cur_off)
            : *RKOF_PTR(const uint32_t *, cur_off);

        uint32_t tag_id;
        uint32_t payload_len;
        uint32_t total_rec_len;
        uint32_t data_off;

        if ((raw_hdr & 0x00008000u) == 0u) {
            tag_id = (raw_hdr & 0xFFFFu) >> 8u;
            payload_len = 1u;
            total_rec_len = 2u;
            data_off = cur_off;
        } else if ((raw_hdr & 0x0000C000u) == 0x00008000u) {
            uint32_t enc_len = (raw_hdr >> 11u) & 7u;
            tag_id = raw_hdr & 0x07FFu;
            payload_len = enc_len + 1u;
            total_rec_len = enc_len + 3u;
            data_off = cur_off + 2u;
        } else {
            payload_len = raw_hdr >> 16u;
            tag_id = raw_hdr & 0x3FFFu;
            total_rec_len = payload_len + 4u;
            data_off = cur_off + 4u;
        }

        if (rem < (int32_t)total_rec_len) {
            return;
        }

        struct rkof_wnvv_node **insert_link = NULL;
        struct rkof_wnvv_node *existing = sub_FE4D4(tag_id, &insert_link);
        struct rkof_wnvv_node *append_old = NULL;

        if ((tag_id & 0x0F00u) == 0x0C00u) {
            if (media_id != 1u) {
                uint8_t log_pack[16] = { 0 };
                sub_141B8(log_pack, tag_id);
                sub_1A646(0x200E6D62u, log_pack);
                rtkit_assert_fail("wnvv.c", 165u, "wnvv_parse_container");
            }
            if ((payload_len & 3u) != 0u) {
                uint8_t log_pack[16] = { 0 };
                sub_18EC0(log_pack, tag_id, payload_len);
                sub_1A646(0x200E6D3Cu, log_pack);
                rtkit_assert_fail("wnvv.c", 173u, "wnvv_parse_container");
            }
            append_old = existing;
        }

        uint32_t reused_in_place = 0u;
        struct rkof_wnvv_node *target_node = existing;

        if (existing != NULL) {
            if (existing->payload_len >= payload_len && append_old == NULL) {
                reused_in_place = 1u;
            } else {
                if (append_old != NULL) {
                    payload_len += append_old->payload_len;
                }
                *insert_link = existing->next;
                target_node = NULL;
            }
        }

        if (target_node == NULL) {
            size_t node_hdr_sz = sizeof(struct rkof_wnvv_node);
            size_t alloc_sz = (reader != NULL)
                ? (node_hdr_sz + (size_t)(uint16_t)payload_len)
                : node_hdr_sz;
            target_node = (struct rkof_wnvv_node *)rt_malloc(alloc_sz);
            if (target_node == NULL) {
                rtkit_assert_fail("wnvv.c", 198u, "wnvv_parse_container");
            }
        }

        target_node->payload_len = (uint16_t)payload_len;
        target_node->tag_id = (uint16_t)tag_id;

        if (reader != NULL) {
            uint8_t *inline_buf = (uint8_t *)target_node + sizeof(struct rkof_wnvv_node);
            uint16_t total_copy = (uint16_t)payload_len;
            target_node->payload_ptr = inline_buf;

            uint8_t *dst_cursor = inline_buf;
            uint32_t rem_copy = total_copy;
            if (append_old != NULL) {
                uint16_t old_len = append_old->payload_len;
                if (old_len > total_copy) {
                    rtkit_assert_fail("wnvv.c", 213u, "wnvv_parse_container");
                }
                memcpy_thunk(
                    inline_buf,
                    (const uint8_t *)append_old + sizeof(struct rkof_wnvv_node),
                    old_len
                );
                rem_copy = (uint32_t)total_copy - (uint32_t)old_len;
                dst_cursor = inline_buf + (old_len & ~3u);
            }

            uint8_t *word_limit = inline_buf + (total_copy & ~3u);
            uint32_t src_cursor = data_off;
            while (rem_copy >= 4u) {
                if (dst_cursor > word_limit) {
                    rtkit_assert_fail("wnvv.c", 220u, "wnvv_parse_container");
                }
                uint32_t w = reader(src_cursor);
                *(uint32_t *)(void *)dst_cursor = w;
                dst_cursor += 4u;
                rem_copy -= 4u;
                src_cursor += 4u;
            }
            if (rem_copy != 0u) {
                uint32_t tail = reader(src_cursor);
                const uint8_t *tail_bytes = (const uint8_t *)&tail;
                for (uint32_t idx = 0u; idx < rem_copy; idx++) {
                    dst_cursor[idx] = tail_bytes[idx];
                }
            }
        } else {
            target_node->payload_ptr = RKOF_PTR(const uint8_t *, data_off);
        }

        if (reused_in_place == 0u) {
            target_node->next = *insert_link;
            *insert_link = target_node;
        }

        cur_off += total_rec_len;
        rem -= (int32_t)total_rec_len;

        if (tag_id == 0x9Fu) {
            uint32_t cal_word = *(const uint32_t *)(const void *)target_node->payload_ptr;
            mmio32_rmw_mask(RKOF_RF_PMGR_CAL_TRIM_ADDR, 0x0000007Cu, cal_word & 0x0000007Cu);
        } else if (tag_id == 0x96u) {
            sub_18A134(50ULL);
            uint32_t cal_word = *(const uint32_t *)(const void *)target_node->payload_ptr;
            mmio32_rmw_mask(RKOF_RF_PMGR_CAL_TRIM_ADDR, 0x3FC00000u, cal_word & 0x3FC00000u);
        }
    }
}

/* @provenance RVA 0xfe564 sub_FE564 */
uint8_t sub_FE564(uint32_t crc_init, const uint8_t *buf, uint32_t len)
{
    const uint8_t *crc_table = RKOF_PTR(const uint8_t *, RKOF_RODATA_CRC8_TABLE_ADDR);
    uint8_t crc = (uint8_t)crc_init;
    for (uint32_t idx = 0u; idx < len; idx++) {
        crc = crc_table[crc ^ buf[idx]];
    }
    return crc;
}

/* @provenance RVA 0xfe580 sub_FE580 */
uint32_t sub_FE580(const uint32_t *state_words)
{
    uint32_t diff_a = state_words[0x80u / 4u] ^ state_words[0x84u / 4u];
    uint32_t diff_b = state_words[0x58u / 4u] ^ state_words[0x5Cu / 4u];
    uint32_t diff_c = state_words[0x6Cu / 4u] ^ state_words[0x70u / 4u];
    return ((diff_a | diff_b | diff_c) != 0u) ? 1u : 0u;
}

/* @provenance RVA 0xfe59e sub_FE59E */
uint16_t sub_FE59E(const struct rkof_byte_ring *ring)
{
    /*
     * Preserves exact Thumb-2 condition-flag behavior at 0xFE5AE..0xFE5DC:
     * when ring[(rd + 1) % size] == 0xBC, bytes at +2 (high) and +3 (low)
     * form big-endian u16; otherwise bytes at +3 (high) and +2 (low) form
     * little-endian u16.
     */
    const uint8_t *buf = ring->buf;
    uint32_t sz = ring->size;
    uint32_t rd = ring->rd;

    uint8_t tag = buf[(rd + 1u) % sz];
    uint32_t hi_off = (tag == 0xBCu) ? 2u : 3u;
    uint32_t lo_off = (tag == 0xBCu) ? 3u : 2u;

    uint8_t hi_byte = buf[(rd + hi_off) % sz];
    uint8_t lo_byte = buf[(rd + lo_off) % sz];
    return (uint16_t)(((uint16_t)hi_byte << 8u) | (uint16_t)lo_byte);
}

/* @provenance RVA 0xfe7ce sub_FE7CE */
uint32_t sub_FE7CE(const struct rkof_byte_ring *ring)
{
    uint32_t wr = ring->wr;
    uint32_t rd = ring->rd;
    if (wr < rd) {
        return rd + (~wr);
    }
    return ring->size + rd + (~wr);
}
