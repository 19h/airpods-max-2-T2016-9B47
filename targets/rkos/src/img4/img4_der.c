#include "rkos/rkos.h"

/*
 * Reconstructed Apple Image4 (`IM4M` / `IM4C` / `MANB` / `MANP`) and ASN.1 DER
 * validation & manifest property lookup routines (`0x057C..0x1124`).
 */

#ifdef RKOS_HOST
#define RKOS_IMG4_EARLY
void rkos_host_on_halt(uint32_t code);
static void rkos_img4_trap(void)
{
    rkos_host_on_halt(0xFEu);
}
#else
#define RKOS_IMG4_EARLY __attribute__((section(".text.early")))
static void rkos_img4_trap(void)
{
    __builtin_trap();
}
#endif

/* @provenance RVA 0x582 sub_582 */
RKOS_IMG4_EARLY int rkos_der_decode_ex(const rkos_der_input_t *in_slice,
                                       rkos_der_tlv_t *out_tlv,
                                       uint32_t *raw_len_out)
{
    uint32_t in_len = in_slice->length;
    if (in_len < 2u) {
        return 3;
    }

    uint32_t base_va = in_slice->ptr_va;
    if (base_va == 0xFFFFFFFFu) {
        rkos_img4_trap();
    }

    const uint8_t *buf = (const uint8_t *)RKOS_VMA_RAW(base_va, in_len);
    uint8_t id_byte = buf[0];
    uint32_t tag_lo = (uint32_t)(id_byte & 0x1Fu);
    uint32_t tag_hi = 0u;
    uint32_t pos = 1u;
    uint32_t rem = in_len - 1u;

    if (tag_lo == 0x1Fu) {
        uint8_t first_sub = buf[pos];
        if (first_sub == 0x80u || first_sub < 0x1Fu || rem < 2u) {
            return 3;
        }
        tag_lo = 0u;
        uint32_t hi_acc = 0u;
        for (;;) {
            if ((base_va + pos) == 0xFFFFFFFFu || pos >= in_len) {
                rkos_img4_trap();
            }
            uint8_t sub_byte = buf[pos++];
            rem--;
            uint32_t next_hi = (hi_acc << 7) | (tag_lo >> 25);
            tag_lo = (tag_lo << 7) | (uint32_t)(sub_byte & 0x7Fu);
            if ((sub_byte & 0x80u) == 0u) {
                tag_hi = next_hi;
                break;
            }
            hi_acc = next_hi;
            if (rem < 2u || (hi_acc >> 25) != 0u) {
                return 3;
            }
        }
        if ((tag_hi >> 22) != 0u) {
            return 3;
        }
    }

    out_tlv->tag_number_lo = tag_lo;
    out_tlv->tag_hi_and_cls = tag_hi | (((uint32_t)(id_byte & 0xE0u)) << 24);

    if ((base_va + pos) == 0xFFFFFFFFu || pos >= in_len) {
        rkos_img4_trap();
    }

    uint8_t len_byte = buf[pos++];
    rem--;

    if ((len_byte & 0x80u) == 0u) {
        uint32_t val_len = (uint32_t)len_byte;
        if (raw_len_out == NULL && rem < val_len) {
            return 3;
        }
        uint32_t clamped = (rem < val_len) ? rem : val_len;
        out_tlv->content_va = base_va + pos;
        out_tlv->content_len = clamped;
        if (raw_len_out != NULL) {
            *raw_len_out = val_len;
        }
        return 0;
    }

    uint32_t len_octets = (uint32_t)(len_byte & 0x7Fu);
    if (len_octets == 0u || len_octets > 4u || (len_octets - 1u) >= rem) {
        return 3;
    }
    if (pos >= in_len) {
        rkos_img4_trap();
    }
    if (buf[pos] == 0u) {
        return 3;
    }

    uint32_t avail_after_len = rem - len_octets;
    uint32_t content_va = base_va + pos + len_octets;
    uint32_t long_len = 0u;
    for (uint32_t idx = 0u; idx < len_octets; idx++) {
        long_len = (long_len << 8) | (uint32_t)buf[pos++];
    }

    if (long_len < 0x80u || (raw_len_out == NULL && long_len > avail_after_len)) {
        return 3;
    }

    uint32_t clamped = (long_len < avail_after_len) ? long_len : avail_after_len;
    out_tlv->content_va = content_va;
    out_tlv->content_len = clamped;
    if (raw_len_out != NULL) {
        *raw_len_out = long_len;
    }
    return 0;
}

/* @provenance RVA 0x57c sub_57C */
RKOS_IMG4_EARLY int rkos_der_decode(const rkos_der_input_t *in_slice, rkos_der_tlv_t *out_tlv)
{
    return rkos_der_decode_ex(in_slice, out_tlv, NULL);
}

/* @provenance RVA 0x712 sub_712 */
RKOS_IMG4_EARLY int rkos_der_decode_raw(const rkos_der_input_t *in_slice,
                                        rkos_der_tlv_t *out_tlv,
                                        int allow_trunc)
{
    uint32_t in_len = in_slice->length;
    if (in_len < 2u) {
        return 3;
    }

    uint32_t base_va = in_slice->ptr_va;
    if (base_va == 0xFFFFFFFFu) {
        rkos_img4_trap();
    }

    const uint8_t *buf = (const uint8_t *)RKOS_VMA_RAW(base_va, in_len);
    uint8_t id_byte = buf[0];
    uint32_t tag_lo = (uint32_t)(id_byte & 0x1Fu);
    uint32_t tag_hi = 0u;
    uint32_t pos = 1u;
    uint32_t rem = in_len - 1u;

    if (tag_lo == 0x1Fu) {
        uint8_t first_sub = buf[pos];
        if (first_sub == 0x80u || first_sub < 0x1Fu || rem < 2u) {
            return 3;
        }
        tag_lo = 0u;
        uint32_t hi_acc = 0u;
        for (;;) {
            if ((base_va + pos) == 0xFFFFFFFFu || pos >= in_len) {
                rkos_img4_trap();
            }
            uint8_t sub_byte = buf[pos++];
            rem--;
            uint32_t next_hi = (hi_acc << 7) | (tag_lo >> 25);
            tag_lo = (tag_lo << 7) | (uint32_t)(sub_byte & 0x7Fu);
            if ((sub_byte & 0x80u) == 0u) {
                tag_hi = next_hi;
                break;
            }
            hi_acc = next_hi;
            if (rem < 2u || (hi_acc >> 25) != 0u) {
                return 3;
            }
        }
        if ((tag_hi >> 22) != 0u) {
            return 3;
        }
    }

    out_tlv->tag_number_lo = tag_lo;
    out_tlv->tag_hi_and_cls = tag_hi | (((uint32_t)(id_byte & 0xE0u)) << 24);

    if ((base_va + pos) == 0xFFFFFFFFu || pos >= in_len) {
        rkos_img4_trap();
    }

    uint8_t len_byte = buf[pos++];
    rem--;

    uint32_t content_va = base_va + pos;
    uint32_t val_len = (uint32_t)len_byte;

    if ((len_byte & 0x80u) == 0u) {
        if (allow_trunc == 0 && rem < val_len) {
            return 3;
        }
    } else {
        uint32_t len_octets = (uint32_t)(len_byte & 0x7Fu);
        if (len_octets == 0u || len_octets > 4u || (len_octets - 1u) >= rem) {
            return 3;
        }
        if (pos >= in_len) {
            rkos_img4_trap();
        }
        if (buf[pos] == 0u) {
            return 3;
        }
        uint32_t avail_after_len = rem - len_octets;
        content_va = base_va + pos + len_octets;
        val_len = 0u;
        for (uint32_t idx = 0u; idx < len_octets; idx++) {
            val_len = (val_len << 8) | (uint32_t)buf[pos++];
        }
        if (val_len < 0x80u || (allow_trunc == 0 && val_len > avail_after_len)) {
            return 3;
        }
    }

    if (val_len > ((~content_va) & 0x7FFFFFFFu)) {
        return 7;
    }
    if (content_va > content_va + val_len) {
        rkos_img4_trap();
    }
    out_tlv->content_va = content_va;
    out_tlv->content_len = val_len;
    return 0;
}

/* @provenance RVA 0x8c8 sub_8C8 */
RKOS_IMG4_EARLY int rkos_der_parse_u64(const rkos_der_input_t *in_slice, uint64_t *out_val)
{
    uint32_t len = in_slice->length;
    if (len == 0u) {
        return 3;
    }

    const uint8_t *buf = (const uint8_t *)RKOS_VMA_RAW(in_slice->ptr_va, len);
    if ((buf[0] & 0x80u) != 0u) {
        return 3;
    }

    if (buf[0] != 0u) {
        if (len > 8u) {
            return 7;
        }
    } else if (len != 1u) {
        if ((buf[1] & 0x80u) == 0u) {
            return 3;
        }
        if (len > 9u) {
            return 7;
        }
    }

    uint64_t acc = 0ULL;
    for (uint32_t idx = 0u; idx < len; idx++) {
        acc = (acc << 8) | (uint64_t)buf[idx];
    }
    *out_val = acc;
    return 0;
}

/* @provenance RVA 0x876 sub_876 */
RKOS_IMG4_EARLY int rkos_der_parse_u32(const rkos_der_input_t *in_slice, uint32_t *out_val)
{
    uint64_t val64 = ~0ULL;
    int rc = rkos_der_parse_u64(in_slice, &val64);
    if (rc != 0) {
        return rc;
    }
    if ((val64 >> 32) != 0ULL) {
        return 7;
    }
    *out_val = (uint32_t)val64;
    return 0;
}

/* @provenance RVA 0x938 sub_938 */
RKOS_IMG4_EARLY int rkos_der_slice_to_bounds(const rkos_der_input_t *in_slice, uint32_t out_bounds[2])
{
    uint32_t start_va = in_slice->ptr_va;
    uint32_t len = in_slice->length;
    uint32_t end_va = start_va + len;
    if (end_va < start_va) {
        rkos_img4_trap();
    }
    out_bounds[0] = start_va;
    out_bounds[1] = end_va;
    return 0;
}

/* @provenance RVA 0x950 sub_950 */
RKOS_IMG4_EARLY int rkos_der_next_tlv(uint32_t cursor_bounds[2], rkos_der_tlv_t *out_tlv)
{
    uint32_t cur_va = cursor_bounds[0];
    uint32_t end_va = cursor_bounds[1];
    if (cur_va >= end_va) {
        return 1;
    }

    rkos_der_input_t slice = { cur_va, end_va - cur_va };
    int rc = rkos_der_decode_ex(&slice, out_tlv, NULL);
    if (rc == 0) {
        uint32_t next_va = out_tlv->content_va + out_tlv->content_len;
        if (next_va < out_tlv->content_va || next_va > end_va || cur_va > next_va) {
            rkos_img4_trap();
        }
        cursor_bounds[0] = next_va;
    }
    return rc;
}

/* @provenance RVA 0xa46 sub_A46 */
RKOS_IMG4_EARLY int rkos_der_parse_sequence_body(const rkos_der_input_t *seq_body,
                                                 uint32_t spec_count,
                                                 const rkos_der_spec_t *specs,
                                                 void *out_buf,
                                                 uint32_t out_size,
                                                 uint32_t zero_size)
{
    if (zero_size != 0u) {
        (void)rkos_memset(out_buf, 0, zero_size);
    }

    uint32_t start_va = seq_body->ptr_va;
    uint32_t body_len = seq_body->length;
    uint32_t end_va = start_va + body_len;
    if (end_va < start_va) {
        rkos_img4_trap();
    }

    uint32_t bounds[2] = { start_va, end_va };
    if (spec_count == 0u) {
        return (start_va != end_va) ? 3 : 0;
    }

    uint8_t *dst_bytes = (uint8_t *)out_buf;
    uint32_t idx = 0u;

    for (;;) {
        uint32_t elem_start_va = bounds[0];
        uint32_t elem_end_va = bounds[1];
        rkos_der_tlv_t tlv = { 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu };
        int rc = rkos_der_next_tlv(bounds, &tlv);
        if (rc != 0) {
            if (rc == 1) {
                while (idx < spec_count) {
                    if ((specs[idx].flags & 1u) == 0u) {
                        return 5;
                    }
                    idx = (idx + 1u) & 0xFFFFu;
                }
                return 0;
            }
            return rc;
        }

        if (idx >= spec_count) {
            return 2;
        }

        uint64_t actual_tag = ((uint64_t)tlv.tag_hi_and_cls << 32) | (uint64_t)tlv.tag_number_lo;
        for (;;) {
            uint16_t flg = specs[idx].flags;
            uint64_t exp_tag = ((uint64_t)specs[idx].tag_hi << 32) | (uint64_t)specs[idx].tag_lo;
            if ((flg & 2u) != 0u || exp_tag == actual_tag) {
                break;
            }
            if ((flg & 1u) != 0u) {
                idx = (idx + 1u) & 0xFFFFu;
                if (idx < spec_count) {
                    continue;
                }
            }
            return 2;
        }

        uint16_t matched_flags = specs[idx].flags;
        if ((matched_flags & 4u) == 0u) {
            uint32_t field_off = specs[idx].out_offset;
            if (field_off > 0xFFFFFFF7u || field_off + 8u > out_size) {
                return 7;
            }
            rkos_der_input_t *slot = (rkos_der_input_t *)(void *)(dst_bytes + field_off);
            if ((matched_flags & 8u) == 0u) {
                slot->ptr_va = tlv.content_va;
                slot->length = tlv.content_len;
            } else if (elem_start_va < tlv.content_va) {
                uint32_t hdr_len = tlv.content_va - elem_start_va;
                uint32_t full_len = hdr_len + tlv.content_len;
                if (full_len < hdr_len || elem_end_va < elem_start_va ||
                    full_len > (elem_end_va - elem_start_va)) {
                    rkos_img4_trap();
                }
                slot->ptr_va = elem_start_va;
                slot->length = full_len;
            } else {
                slot->ptr_va = elem_start_va;
                slot->length = tlv.content_len;
                return 3;
            }
        }

        idx = (idx + 1u) & 0xFFFFu;
        if (idx == spec_count) {
            uint32_t consumed_end = tlv.content_va + tlv.content_len;
            return (consumed_end != end_va) ? 3 : 0;
        }
        if (idx >= spec_count) {
            return (bounds[0] != bounds[1]) ? 3 : 0;
        }
    }
}

/* @provenance RVA 0x9c0 sub_9C0 */
RKOS_IMG4_EARLY int rkos_der_parse_sequence(const rkos_der_input_t *in_slice,
                                            uint32_t spec_count,
                                            const rkos_der_spec_t *specs,
                                            void *out_buf,
                                            uint32_t out_size,
                                            uint32_t zero_size)
{
    rkos_der_tlv_t tlv = { 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu };
    int rc = rkos_der_decode_ex(in_slice, &tlv, NULL);
    if (rc != 0) {
        return rc;
    }
    if (tlv.tag_number_lo != 0x10u || tlv.tag_hi_and_cls != 0x20000000u) {
        return 2;
    }
    rkos_der_input_t inner = { tlv.content_va, tlv.content_len };
    return rkos_der_parse_sequence_body(&inner, spec_count, specs, out_buf, out_size, zero_size);
}

/* @provenance RVA 0xc08 sub_C08 */
RKOS_IMG4_EARLY int rkos_der_find_tag_in_bounds(uint32_t cursor_bounds[2],
                                                uint32_t unused_arg,
                                                uint32_t tag_lo,
                                                uint32_t tag_hi,
                                                rkos_der_input_t *out_slice)
{
    (void)unused_arg;
    rkos_der_tlv_t tlv = { 0u, 0u, 0u, 0u };
    for (;;) {
        int rc = rkos_der_next_tlv(cursor_bounds, &tlv);
        if (rc != 0) {
            return rc;
        }
        if (tlv.tag_number_lo == tag_lo && tlv.tag_hi_and_cls == tag_hi) {
            out_slice->ptr_va = tlv.content_va;
            out_slice->length = tlv.content_len;
            return 0;
        }
    }
}

/* @provenance RVA 0xc50 sub_C50 */
RKOS_IMG4_EARLY int rkos_img4_cmp_fourcc(const rkos_der_input_t *in_slice, uint32_t expected_fourcc)
{
    if (in_slice->length < 4u) {
        return -1;
    }
    if (in_slice->length != 4u) {
        return 1;
    }
    uint32_t actual = 0u;
    if (rkos_der_parse_u32(in_slice, &actual) != 0) {
        return -2;
    }
    if (actual < expected_fourcc) {
        return -1;
    }
    return (actual > expected_fourcc) ? 1 : 0;
}

/* @provenance RVA 0xcac sub_CAC */
RKOS_IMG4_EARLY int rkos_img4_decode_typed_seq(const rkos_der_input_t *in_slice,
                                               uint32_t spec_count,
                                               const rkos_der_spec_t *specs,
                                               void *out_body,
                                               uint32_t expected_fourcc)
{
    if (in_slice == NULL || out_body == NULL) {
        return 6;
    }
    if (in_slice->ptr_va == 0u || in_slice->length == 0u) {
        return 0;
    }

    int rc = rkos_der_parse_sequence(in_slice, spec_count, specs, out_body, 0x90u, 0u);
    if (rc != 0) {
        return rc;
    }

    const rkos_der_input_t *hdr_fields = (const rkos_der_input_t *)out_body;
    if (rkos_img4_cmp_fourcc(&hdr_fields[0], expected_fourcc) != 0) {
        return 2;
    }

    uint32_t ver = 0u;
    rc = rkos_der_parse_u32(&hdr_fields[1], &ver);
    if (rc != 0) {
        return rc;
    }
    if (ver > 2u) {
        return 2;
    }
    return 0;
}

/* @provenance RVA 0xc8c sub_C8C */
RKOS_IMG4_EARLY int rkos_img4_decode_im4m_signed(const rkos_der_input_t *in_slice, void *out_body)
{
    const rkos_der_spec_t *specs = RKOS_VMA_PTR(const rkos_der_spec_t *, 0x00013360u);
    return rkos_img4_decode_typed_seq(in_slice, 5u, specs, out_body, 0x494D344Du);
}

/* @provenance RVA 0xd00 sub_D00 */
RKOS_IMG4_EARLY int rkos_img4_decode_im4m_unsigned(const rkos_der_input_t *in_slice, void *out_body)
{
    const rkos_der_spec_t *specs = RKOS_VMA_PTR(const rkos_der_spec_t *, 0x00013438u);
    return rkos_img4_decode_typed_seq(in_slice, 3u, specs, out_body, 0x494D344Du);
}

/* @provenance RVA 0xd20 sub_D20 */
RKOS_IMG4_EARLY int rkos_img4_decode_im4c_signed(const rkos_der_input_t *in_slice, void *out_body)
{
    const rkos_der_spec_t *specs = RKOS_VMA_PTR(const rkos_der_spec_t *, 0x000133D8u);
    return rkos_img4_decode_typed_seq(in_slice, 4u, specs, out_body, 0x494D3443u);
}

/* @provenance RVA 0xd40 sub_D40 */
RKOS_IMG4_EARLY int rkos_img4_decode_im4c_unsigned(const rkos_der_input_t *in_slice, void *out_body)
{
    const rkos_der_spec_t *specs = RKOS_VMA_PTR(const rkos_der_spec_t *, 0x00013438u);
    return rkos_img4_decode_typed_seq(in_slice, 3u, specs, out_body, 0x494D3443u);
}

/* @provenance RVA 0xd60 sub_D60 */
RKOS_IMG4_EARLY int rkos_img4_parse_prop_entry(const rkos_der_input_t *in_slice,
                                               uint32_t unused_arg,
                                               uint32_t prop_fourcc,
                                               uint32_t prop_cls_hi,
                                               uint32_t val_tag_lo,
                                               uint32_t val_tag_hi,
                                               uint32_t *out_item)
{
    (void)unused_arg;
    rkos_der_spec_t local_specs[2];
    (void)rkos_memset(local_specs, 0, sizeof(local_specs));
    local_specs[0].out_offset = 0u;
    local_specs[0].tag_lo = 0x16u;
    local_specs[0].tag_hi = 0u;
    local_specs[1].out_offset = 0x10u;
    local_specs[1].tag_lo = val_tag_lo;
    local_specs[1].tag_hi = val_tag_hi;

    int rc = rkos_der_parse_sequence(in_slice, 2u, local_specs, out_item, 0x20u, 0u);
    if (rc != 0) {
        return rc;
    }

    uint32_t parsed_tag = 0u;
    rc = rkos_der_parse_u32((const rkos_der_input_t *)(const void *)out_item, &parsed_tag);
    if (rc != 0) {
        return rc;
    }
    if (((prop_cls_hi ^ 0xE0000000u) | (parsed_tag ^ prop_fourcc)) != 0u) {
        return 2;
    }

    out_item[2] = prop_fourcc;
    out_item[3] = prop_cls_hi | 0xE0000000u;
    out_item[6] = val_tag_lo;
    out_item[7] = val_tag_hi;
    return 0;
}

/* @provenance RVA 0xde4 sub_DE4 */
RKOS_IMG4_EARLY int rkos_img4_find_prop_in_bounds(uint32_t cursor_bounds[2],
                                                  uint32_t unused_arg,
                                                  uint32_t prop_fourcc,
                                                  uint32_t prop_cls_hi,
                                                  uint32_t val_tag_lo,
                                                  uint32_t val_tag_hi,
                                                  uint32_t *out_item)
{
    (void)unused_arg;
    rkos_der_input_t found = { 0u, 0u };
    int rc = rkos_der_find_tag_in_bounds(cursor_bounds, 0u, prop_fourcc, prop_cls_hi, &found);
    if (rc != 0) {
        return rc;
    }
    return rkos_img4_parse_prop_entry(&found, 0u, prop_fourcc, prop_cls_hi,
                                      val_tag_lo, val_tag_hi, out_item);
}

/* @provenance RVA 0xe0e sub_E0E */
RKOS_IMG4_EARLY int rkos_img4_find_prop_in_slice(const rkos_der_input_t *in_slice,
                                                 uint32_t unused_arg,
                                                 uint32_t prop_fourcc,
                                                 uint32_t prop_cls_hi,
                                                 uint32_t val_tag_lo,
                                                 uint32_t val_tag_hi,
                                                 uint32_t *out_item)
{
    (void)unused_arg;
    uint32_t bounds[2] = { 0u, 0u };
    int rc = rkos_der_slice_to_bounds(in_slice, bounds);
    if (rc != 0) {
        return rc;
    }
    return rkos_img4_find_prop_in_bounds(bounds, 0u, prop_fourcc, prop_cls_hi,
                                         val_tag_lo, val_tag_hi, out_item);
}

/* @provenance RVA 0x111c sub_111C */
RKOS_IMG4_EARLY void *rkos_img4_zero_item32(void *item_buf)
{
    return rkos_memset(item_buf, 0, 0x20u);
}

/* @provenance RVA 0xe38 sub_E38 */
RKOS_IMG4_EARLY int rkos_img4_get_manb_and_manp(const uint8_t *manifest_ctx,
                                                rkos_der_input_t *out_manb,
                                                rkos_der_input_t *out_manp)
{
    if (manifest_ctx == NULL || (out_manb == NULL && out_manp == NULL)) {
        return 6;
    }

    uint32_t item_manb[8];
    uint32_t item_manp[8];
    (void)rkos_memset(item_manb, 0, sizeof(item_manb));
    (void)rkos_memset(item_manp, 0, sizeof(item_manp));

    rkos_der_tlv_t body_set = { 0u, 0u, 0u, 0u };
    const rkos_der_input_t *body_slice =
        (const rkos_der_input_t *)(const void *)(manifest_ctx + 0x9Cu);
    int rc = rkos_der_decode(body_slice, &body_set);
    if (rc != 0) {
        return rc;
    }
    if (body_set.tag_number_lo != 0x11u || body_set.tag_hi_and_cls != 0x20000000u) {
        return -1;
    }

    uint32_t ver = 0u;
    const rkos_der_input_t *ver_slice =
        (const rkos_der_input_t *)(const void *)(manifest_ctx + 0x94u);
    rc = rkos_der_parse_u32(ver_slice, &ver);
    if (rc != 0) {
        return rc;
    }

    rkos_der_input_t set_inner = { body_set.content_va, body_set.content_len };
    uint32_t manb_val_tag = (ver == 2u) ? 0x10u : 0x11u;
    rc = rkos_img4_find_prop_in_slice(&set_inner, 0u, 0x4D414E42u, 0xE0000000u,
                                      manb_val_tag, 0x20000000u, item_manb);
    if (rc != 0) {
        return rc;
    }
    if (out_manb != NULL) {
        out_manb->ptr_va = item_manb[4];
        out_manb->length = item_manb[5];
    }

    const rkos_der_input_t *manb_inner =
        (const rkos_der_input_t *)(const void *)(item_manb + 4);
    rc = rkos_img4_find_prop_in_slice(manb_inner, 0u, 0x4D414E50u, 0xE0000000u,
                                      0x11u, 0x20000000u, item_manp);
    if (out_manp == NULL) {
        return rc;
    }
    if (rc != 0) {
        return rc;
    }
    out_manp->ptr_va = item_manp[4];
    out_manp->length = item_manp[5];
    return 0;
}

/* @provenance RVA 0xf48 sub_F48 */
RKOS_IMG4_EARLY int rkos_img4_lookup_obj_prop(const uint8_t *manifest_ctx,
                                              uint32_t unused_arg,
                                              uint32_t obj_fourcc,
                                              uint32_t obj_cls_hi,
                                              uint32_t prop_fourcc,
                                              uint32_t prop_cls_hi,
                                              uint32_t val_tag_lo,
                                              uint32_t val_tag_hi,
                                              uint32_t *out_item)
{
    (void)unused_arg;
    uint32_t obj_item[8];
    (void)rkos_memset(obj_item, 0, sizeof(obj_item));

    if (manifest_ctx == NULL || out_item == NULL) {
        return 6;
    }

    rkos_der_input_t manb_slice = { 0u, 0u };
    int rc = rkos_img4_get_manb_and_manp(manifest_ctx, &manb_slice, NULL);
    if (rc == 0) {
        rc = rkos_img4_find_prop_in_slice(&manb_slice, 0u, obj_fourcc,
                                          obj_cls_hi | 0xE0000000u,
                                          0x11u, 0x20000000u, obj_item);
        if (rc == 0) {
            const rkos_der_input_t *obj_inner =
                (const rkos_der_input_t *)(const void *)(obj_item + 4);
            rc = rkos_img4_find_prop_in_slice(obj_inner, 0u, prop_fourcc,
                                              prop_cls_hi, val_tag_lo, val_tag_hi,
                                              out_item);
            if (rc == 0) {
                return 0;
            }
        }
    }
    (void)rkos_img4_zero_item32(out_item);
    return rc;
}

/* @provenance RVA 0xfd0 sub_FD0 */
RKOS_IMG4_EARLY int rkos_img4_get_obj_octet_prop(const uint8_t *manifest_ctx,
                                                 uint32_t unused_arg,
                                                 uint32_t obj_fourcc,
                                                 uint32_t obj_cls_hi,
                                                 uint32_t prop_fourcc,
                                                 uint32_t prop_cls_hi,
                                                 uint32_t *out_data_va,
                                                 uint32_t *out_data_len)
{
    (void)unused_arg;
    uint32_t prop_item[8];
    (void)rkos_memset(prop_item, 0, sizeof(prop_item));

    if (manifest_ctx == NULL || out_data_va == NULL || out_data_len == NULL) {
        return 6;
    }

    int rc = rkos_img4_lookup_obj_prop(manifest_ctx, 0u, obj_fourcc, obj_cls_hi,
                                       prop_fourcc, prop_cls_hi | 0xE0000000u,
                                       4u, 0u, prop_item);
    if (rc == 0) {
        *out_data_va = prop_item[4];
        *out_data_len = prop_item[5];
        return 0;
    }
    *out_data_va = 0u;
    *out_data_len = 0u;
    return rc;
}

/* @provenance RVA 0x1034 sub_1034 */
RKOS_IMG4_EARLY int rkos_img4_init_manifest_ctx(uint32_t buf_va,
                                                uint32_t buf_len,
                                                uint32_t is_signed,
                                                uint8_t *out_ctx,
                                                uint32_t container_fourcc)
{
    if (buf_va == 0u || out_ctx == NULL) {
        return 6;
    }

    rkos_der_input_t input = { buf_va, buf_len };
    (void)rkos_memset(out_ctx, 0, 0x12Cu);

    int rc;
    if (container_fourcc == 0x494D3443u) {
        rc = (is_signed != 0u) ? rkos_img4_decode_im4c_signed(&input, out_ctx + 0x8Cu)
                               : rkos_img4_decode_im4c_unsigned(&input, out_ctx + 0x8Cu);
    } else if (container_fourcc == 0x494D344Du) {
        rc = (is_signed != 0u) ? rkos_img4_decode_im4m_signed(&input, out_ctx + 0x8Cu)
                               : rkos_img4_decode_im4m_unsigned(&input, out_ctx + 0x8Cu);
    } else {
        return 2;
    }

    if (rc != 0) {
        return rc;
    }

    uint32_t body_va = *(const uint32_t *)(const void *)(out_ctx + 0x9Cu);
    if (body_va == 0u) {
        return 3;
    }

    rkos_der_tlv_t outer_tlv = { 0u, 0u, 0u, 0u };
    rc = rkos_der_decode_ex(&input, &outer_tlv, NULL);
    if (rc != 0) {
        return rc;
    }

    uint32_t hdr_bytes = outer_tlv.content_va - buf_va;
    uint32_t total_tlv_bytes = hdr_bytes + outer_tlv.content_len;
    if (total_tlv_bytes < hdr_bytes) {
        rkos_img4_trap();
    }

    uint32_t *ctx_words = (uint32_t *)(void *)out_ctx;
    ctx_words[3] = buf_va;
    ctx_words[4] = total_tlv_bytes;
    if (total_tlv_bytes != buf_len) {
        return 7;
    }
    return 0;
}

/* @provenance RVA 0x101c sub_101C */
RKOS_IMG4_EARLY int rkos_img4_init_im4m_signed(uint32_t buf_va, uint32_t buf_len, uint8_t *out_ctx)
{
    return rkos_img4_init_manifest_ctx(buf_va, buf_len, 1u, out_ctx, 0x494D344Du);
}
