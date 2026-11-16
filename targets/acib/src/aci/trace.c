#include "acib/aci.h"
#include "acib/bus.h"
#include "acib/rtkit.h"

/*
 * Trace Manager buffer control block utilities and binary diagnostic
 * packet header encoders reconstructed from acib ROM 0x0e44a0..0x0e4876.
 */

/* @provenance RVA 0x0e44a0 tm_buffer_util_cb_init */
struct acib_tm_buf_cb *tm_buffer_util_cb_init(uint32_t pool_id)
{
    if (pool_id != 0u) {
        rtk_abort_panic("tm_buffer_util_cb_init: fail to allocate buffer control block",
                        0u, 0u, 0u);
        return NULL;
    }
    g_tm_default_cb.used = 0u;
    g_tm_default_cb.capacity = 0u;
    g_tm_default_cb.base = NULL;
    return &g_tm_default_cb;
}

/* @provenance RVA 0x0e44c0 sub_E44C0 */
uint16_t tm_buffer_util_get_free(const struct acib_tm_buf_cb *cb)
{
    return (uint16_t)(cb->capacity - cb->used);
}

/* @provenance RVA 0x0e44cc tm_buffer_util_decrement_remaining */
void tm_buffer_util_decrement_remaining(struct acib_tm_buf_cb *cb, uint32_t consume_bytes)
{
    uint16_t free_bytes = (uint16_t)(cb->capacity - cb->used);
    if ((uint32_t)free_bytes >= consume_bytes) {
        cb->used = (uint16_t)(cb->used + (uint16_t)consume_bytes);
        return;
    }
    rtk_abort_panic(
        "tm_buffer_util_decrement_remaining_byte_size: attempt to increase used bytes more than available buffer length",
        0u, 0u, 0u);
}

/* @provenance RVA 0x0e44ec sub_E44EC */
uint16_t tm_buffer_util_get_used(const struct acib_tm_buf_cb *cb)
{
    return cb->used;
}

/* @provenance RVA 0x0e44f0 sub_E44F0 */
void tm_buffer_util_reset(struct acib_tm_buf_cb *cb)
{
    cb->used = 0u;
    cb->capacity = 0u;
    cb->base = NULL;
}

/* @provenance RVA 0x0e44f8 sub_E44F8 */
void tm_buffer_util_bind(struct acib_tm_buf_cb *cb, uint8_t *storage, uint16_t capacity)
{
    cb->capacity = capacity;
    cb->base = storage;
    cb->used = 0u;
}

/* @provenance RVA 0x0e4504 sub_E4504 */
uint32_t tm_buffer_util_has_storage(const struct acib_tm_buf_cb *cb)
{
    return (cb->base != NULL) ? 1u : 0u;
}

/* @provenance RVA 0x0e450e sub_E450E */
uint8_t *tm_buffer_util_write_ptr(const struct acib_tm_buf_cb *cb)
{
    if (cb->base == NULL) {
        return NULL;
    }
    return cb->base + cb->used;
}

/* @provenance RVA 0x0e451c sub_E451C */
uint8_t *tm_buffer_util_base_ptr(const struct acib_tm_buf_cb *cb)
{
    return cb->base;
}

/* @provenance RVA 0x0e47d0 sub_E47D0 */
uint32_t tm_pkt_write_hdr4(uint8_t *dst, const uint8_t *hdr_in)
{
    dst[0] = 0xBAu;
    uint32_t ts = acib_read_unaligned_le32(hdr_in);
    uint16_t raw_len = acib_read_unaligned_le16(hdr_in + 4u);
    acib_write_unaligned_le32(dst + 1u, ts);
    dst[7] = 0x20u;

    uint32_t pay_len = (uint32_t)raw_len - 7u;
    uint32_t hi_bits = pay_len & ~0x3Fu;
    uint16_t encoded = (uint16_t)(((pay_len & 0xFFu) | (hi_bits << 1)) | 0x80u);
    acib_write_unaligned_le16(dst + 5u, encoded);
    return 8u;
}

/* @provenance RVA 0x0e47f8 sub_E47F8 */
uint32_t tm_pkt_write_hdr8(uint8_t *dst, const uint8_t *ts_info, uint32_t type_bits)
{
    dst[8] = (uint8_t)(type_bits & 3u);
    acib_write_unaligned_le32(dst + 12u, acib_read_unaligned_le32(ts_info));
    acib_write_unaligned_le32(dst + 16u, acib_read_unaligned_le32(ts_info + 4u));

    uint8_t b8 = ts_info[8];
    acib_write_unaligned_le16(dst + 10u, 0u);
    dst[9] = (uint8_t)((uint8_t)(b8 << 4) | (b8 >> 4));
    return 20u;
}

/* @provenance RVA 0x0e4828 sub_E4828 */
uint32_t tm_pkt_write_ts_event(uint8_t *dst, const uint8_t *ev_desc)
{
    uint8_t kind = ev_desc[0];
    switch (kind) {
    case 1u:
        return tm_pkt_write_hdr8(dst, ev_desc + 4u, ev_desc[1]);
    case 2u:
        acib_write_unaligned_le16(dst, 2u);
        acib_write_unaligned_le16(dst + 2u, acib_read_unaligned_le16(ev_desc + 4u));
        return 4u;
    case 3u:
        return 0u;
    case 4u:
        dst[0] = 4u;
        dst[1] = ev_desc[4];
        acib_write_unaligned_le16(dst + 2u, acib_read_unaligned_le16(ev_desc + 6u));
        return 4u;
    case 5u:
        return tm_pkt_write_hdr4(dst, ev_desc + 4u);
    default:
        return 0u;
    }
}
