"""Phase 2 Batch 1 Unicorn differential test suites (~117 functions)."""
from __future__ import annotations

import struct
from typing import Any, Callable
from unicorn import Uc
from unicorn.arm_const import (
    UC_ARM_REG_BASEPRI,
    UC_ARM_REG_IPSR,
    UC_ARM_REG_PRIMASK,
    UC_ARM_REG_PSP,
)


def _der_len(n: int) -> bytes:
    if n < 0x80:
        return bytes([n])
    if n <= 0xFF:
        return bytes([0x81, n])
    return bytes([0x82, (n >> 8) & 0xFF, n & 0xFF])


def _der_tlv(id_byte: int, content: bytes) -> bytes:
    return bytes([id_byte]) + _der_len(len(content)) + content


def _der_high_tag(cls_cons: int, tag_val: int, content: bytes) -> bytes:
    parts: list[int] = [tag_val & 0x7F]
    rem = tag_val >> 7
    while rem > 0:
        parts.append(0x80 | (rem & 0x7F))
        rem >>= 7
    parts.reverse()
    return bytes([cls_cons | 0x1F] + parts) + _der_len(len(content)) + content


def _img4_prop(fourcc_str: bytes, val_tlv: bytes) -> bytes:
    fourcc_u32 = int.from_bytes(fourcc_str, "big")
    seq_body = _der_tlv(0x16, fourcc_str) + val_tlv
    seq_tlv = _der_tlv(0x30, seq_body)
    return _der_high_tag(0xE0, fourcc_u32, seq_tlv)


def build_sample_im4m(signed: bool = True, fourcc: bytes = b"IM4M") -> bytes:
    dgst_prop = _img4_prop(b"DGST", _der_tlv(0x04, bytes(range(1, 33))))
    rkos_obj = _img4_prop(b"rkos", _der_tlv(0x31, dgst_prop))
    manp_prop = _img4_prop(b"MANP", _der_tlv(0x31, _img4_prop(b"BORD", _der_tlv(0x02, b"\x06"))))
    manb_prop = _img4_prop(b"MANB", _der_tlv(0x31, manp_prop + rkos_obj))
    body_set = _der_tlv(0x31, manb_prop)

    elems = _der_tlv(0x16, fourcc) + _der_tlv(0x02, b"\x00") + body_set
    if signed:
        elems += _der_tlv(0x04, b"\xAA\xBB\xCC\xDD" * 4)
        if fourcc == b"IM4M":
            elems += _der_tlv(0x30, _der_tlv(0x04, b"\x11\x22\x33\x44"))
    return _der_tlv(0x30, elems)


# Tiny 16-byte Thumb-2 mock UART mux callback placed at 0x02007000 (fn_va = 0x02007001):
#   ldrb r2, [r2]        ; 12 78
#   ldr  r1, [r0, #4]    ; 41 68
#   add.w r3, r0, #8     ; 00 f1 08 03
#   strb r2, [r3, r1]    ; 5a 54
#   adds r1, #1          ; 01 31
#   str  r1, [r0, #4]    ; 41 60
#   movs r0, #0          ; 00 20
#   bx   lr              ; 70 47
MOCK_MUX_CB_THUMB = bytes.fromhex("1278416800f108035a540131416000207047")


def wire_console_mux(uc: Uc) -> None:
    uc.mem_write(0x02007000, MOCK_MUX_CB_THUMB)
    uc.mem_write(0x02007080, struct.pack("<I", 0x02007100))       # devs[0] = dev_va
    uc.mem_write(0x02007090, struct.pack("<II", 0, 0x02007001))   # vtbl[1] = fn_va
    uc.mem_write(0x02007100, struct.pack("<II", 0x02007090, 0) + b"\x00" * 0x78)
    uc.mem_write(0x2000071C, struct.pack("<II", 0x02007080, 1))   # mux.devs, mux.count


def add_batch1_cases(
    pair: Any,
    safe: Callable[..., None],
    wr: Callable[[Uc, int, bytes], None],
    fn_target: Callable[[Uc, int, str], int],
) -> None:
    def guard(uc: Uc) -> None:
        wr(uc, 0x20000198, struct.pack("<I", 0x5EED1234))

    # =========================================================================
    # 1. DER & IMG4 Manifest Suite (24 functions: 0x582..0x111C)
    # =========================================================================
    im4m_signed = build_sample_im4m(signed=True, fourcc=b"IM4M")
    im4m_unsigned = build_sample_im4m(signed=False, fourcc=b"IM4M")
    im4c_signed = build_sample_im4m(signed=True, fourcc=b"IM4C")
    im4c_unsigned = build_sample_im4m(signed=False, fourcc=b"IM4C")

    for cname, blob, raw_out in [
        ("seq_with_len", b"\x30\x04ABCD", 0x02000220),
        ("bad_short", b"\x30", 0),
        ("nonmin_len", b"\x04\x81\x03ABC", 0x02000220),
    ]:
        def mk_dex(b: bytes = blob, ro: int = raw_out) -> Callable[[Uc], list[int]]:
            def _s(uc: Uc) -> list[int]:
                wr(uc, 0x02000100, b)
                wr(uc, 0x02000180, struct.pack("<II", 0x02000100, len(b)))
                wr(uc, 0x02000200, b"\xEE" * 48)
                return [0x02000180, 0x02000200, ro]
            return _s
        safe("img4", f"der_decode_ex_{cname}", 0x582, "rkos_der_decode_ex", mk_dex(), [(0x02000200, 48)])

    for cname, blob, allow_tr in [
        ("trunc_disallowed", b"\x04\x10ABCD", 0),
        ("trunc_allowed", b"\x04\x10ABCD", 1),
        ("high_tag_ok", _der_high_tag(0xE0, 0x4D414E42, b"XYZ"), 0),
    ]:
        def mk_draw(b: bytes = blob, at: int = allow_tr) -> Callable[[Uc], list[int]]:
            def _s(uc: Uc) -> list[int]:
                wr(uc, 0x02000100, b)
                wr(uc, 0x02000180, struct.pack("<II", 0x02000100, len(b)))
                wr(uc, 0x02000200, b"\xEE" * 32)
                return [0x02000180, 0x02000200, at]
            return _s
        safe("img4", f"der_decode_raw_{cname}", 0x712, "rkos_der_decode_raw", mk_draw(), [(0x02000200, 32)])

    for cname, int_bytes in [
        ("zero", b"\x00"),
        ("u32_val", b"\x12\x34\x56\x78"),
        ("u64_lead0", b"\x00\x89\xAB\xCD\xEF\x01\x23\x45\x67"),
        ("neg_rejected", b"\x80\x01"),
        ("empty_err", b""),
    ]:
        def mk_u64(b: bytes = int_bytes) -> Callable[[Uc], list[int]]:
            def _s(uc: Uc) -> list[int]:
                wr(uc, 0x02000100, b)
                wr(uc, 0x02000180, struct.pack("<II", 0x02000100, len(b)))
                wr(uc, 0x02000200, b"\xDD" * 16)
                return [0x02000180, 0x02000200]
            return _s
        safe("img4", f"der_parse_u64_{cname}", 0x8C8, "rkos_der_parse_u64", mk_u64(), [(0x02000200, 16)])
        safe("img4", f"der_parse_u32_{cname}", 0x876, "rkos_der_parse_u32", mk_u64(), [(0x02000200, 16)])

    safe("img4", "der_slice_to_bounds_ok", 0x938, "rkos_der_slice_to_bounds",
         lambda uc: (wr(uc, 0x02000100, struct.pack("<II", 0x02001000, 0x48)),
                     wr(uc, 0x02000200, b"\xFF" * 16), [0x02000100, 0x02000200])[2],
         [(0x02000200, 16)])

    for cname, cur_off in [("first_elem", 0), ("end_reached", 6)]:
        def mk_ntlv(off: int = cur_off) -> Callable[[Uc], list[int]]:
            def _s(uc: Uc) -> list[int]:
                blob = b"\x02\x01\x05\x04\x01\xAA"
                wr(uc, 0x02000100, blob)
                wr(uc, 0x02000180, struct.pack("<II", 0x02000100 + off, 0x02000100 + len(blob)))
                wr(uc, 0x02000200, b"\xEE" * 16)
                return [0x02000180, 0x02000200]
            return _s
        safe("img4", f"der_next_tlv_{cname}", 0x950, "rkos_der_next_tlv", mk_ntlv(),
             [(0x02000180, 8), (0x02000200, 16)])

    for cname, target_tag in [("hit_4", 0x04), ("miss_99", 0x16)]:
        def mk_ftag(tg: int = target_tag) -> Callable[[Uc], list[int]]:
            def _s(uc: Uc) -> list[int]:
                blob = b"\x02\x01\x05\x04\x02ABCD"
                wr(uc, 0x02000100, blob)
                wr(uc, 0x02000180, struct.pack("<II", 0x02000100, 0x02000100 + len(blob)))
                wr(uc, 0x02000200, b"\xEE" * 16)
                return [0x02000180, 0, tg, 0, 0x02000200]
            return _s
        safe("img4", f"der_find_tag_{cname}", 0xC08, "rkos_der_find_tag_in_bounds", mk_ftag(),
             [(0x02000180, 8), (0x02000200, 16)])

    # Sequence & Sequence Body via ROM Unsigned spec table (3 specs @ 0x00013438)
    for cname, outer_blob in [("valid_unsigned", im4m_unsigned), ("wrong_outer_tag", b"\x31\x02\x05\x00")]:
        def mk_pseq(b: bytes = outer_blob) -> Callable[[Uc], list[int]]:
            def _s(uc: Uc) -> list[int]:
                wr(uc, 0x02001000, b)
                wr(uc, 0x02000100, struct.pack("<II", 0x02001000, len(b)))
                wr(uc, 0x02000200, b"\xCC" * 0x90)
                return [0x02000100, 3, 0x00013438, 0x02000200, 0x90, 0x90]
            return _s
        safe("img4", f"der_parse_sequence_{cname}", 0x9C0, "rkos_der_parse_sequence", mk_pseq(), [(0x02000200, 0x90)])

    for cname, body_blob in [
        ("valid_2elems", _der_tlv(0x16, b"IM4M") + _der_tlv(0x02, b"\x01")),
        ("missing_req", _der_tlv(0x16, b"IM4M")),
    ]:
        def mk_psbody(b: bytes = body_blob) -> Callable[[Uc], list[int]]:
            def _s(uc: Uc) -> list[int]:
                wr(uc, 0x02001000, b)
                wr(uc, 0x02000100, struct.pack("<II", 0x02001000, len(b)))
                wr(uc, 0x02000200, b"\xCC" * 0x40)
                return [0x02000100, 2, 0x00013438, 0x02000200, 0x40, 0x40]
            return _s
        safe("img4", f"der_parse_seq_body_{cname}", 0xA46, "rkos_der_parse_sequence_body", mk_psbody(), [(0x02000200, 0x40)])

    for cname, fbytes, exp_u32 in [
        ("eq", b"IM4M", 0x494D344D),
        ("lt", b"IM4C", 0x494D344D),
        ("gt", b"IM4P", 0x494D344D),
        ("short", b"IM", 0x494D344D),
    ]:
        safe("img4", f"img4_cmp_fourcc_{cname}", 0xC50, "rkos_img4_cmp_fourcc",
             lambda uc, fb=fbytes, ex=exp_u32: (
                 wr(uc, 0x02000100, fb),
                 wr(uc, 0x02000120, struct.pack("<II", 0x02000100, len(fb))),
                 [0x02000120, ex],
             )[2])

    for rva, sym, good_blob, bad_blob in [
        (0xC8C, "rkos_img4_decode_im4m_signed", im4m_signed, im4c_signed),
        (0xD00, "rkos_img4_decode_im4m_unsigned", im4m_unsigned, im4c_unsigned),
        (0xD20, "rkos_img4_decode_im4c_signed", im4c_signed, im4m_signed),
        (0xD40, "rkos_img4_decode_im4c_unsigned", im4c_unsigned, im4m_unsigned),
    ]:
        for tag_lbl, b in [("ok", good_blob), ("wrong4cc", bad_blob)]:
            def mk_dec4(blob_in: bytes = b) -> Callable[[Uc], list[int]]:
                def _s(uc: Uc) -> list[int]:
                    wr(uc, 0x02001000, blob_in)
                    wr(uc, 0x02000100, struct.pack("<II", 0x02001000, len(blob_in)))
                    wr(uc, 0x02000200, b"\x00" * 0x90)
                    return [0x02000100, 0x02000200]
                return _s
            safe("img4", f"{sym}_{tag_lbl}", rva, sym, mk_dec4(), [(0x02000200, 0x90)])

    safe("img4", "img4_decode_typed_seq_null", 0xCAC, "rkos_img4_decode_typed_seq",
         lambda uc: [0, 3, 0x00013438, 0x02000200, 0x494D344D])
    safe("img4", "img4_decode_typed_seq_ok", 0xCAC, "rkos_img4_decode_typed_seq",
         lambda uc: (wr(uc, 0x02001000, im4m_unsigned),
                     wr(uc, 0x02000100, struct.pack("<II", 0x02001000, len(im4m_unsigned))),
                     wr(uc, 0x02000200, b"\x00" * 0x90),
                     [0x02000100, 3, 0x00013438, 0x02000200, 0x494D344D])[3],
         [(0x02000200, 0x90)])

    # Property entry & property search helpers (0xD60, 0xDE4, 0xE0E, 0x111C)
    prop_seq_inner = _der_tlv(0x30, _der_tlv(0x16, b"DGST") + _der_tlv(0x04, b"01234567"))
    prop_wrapped = _img4_prop(b"DGST", _der_tlv(0x04, b"01234567"))
    for p_lbl, exp_4cc in [("ok", 0x44475354), ("mismatch", 0x424F5244)]:
        safe("img4", f"img4_parse_prop_{p_lbl}", 0xD60, "rkos_img4_parse_prop_entry",
             lambda uc, fc=exp_4cc: (
                 wr(uc, 0x02001000, prop_seq_inner),
                 wr(uc, 0x02000100, struct.pack("<II", 0x02001000, len(prop_seq_inner))),
                 wr(uc, 0x02000200, b"\xAA" * 32),
                 [0x02000100, 0, fc, 0xE0000000, 4, 0, 0x02000200],
             )[3], [(0x02000200, 32)])
        safe("img4", f"img4_find_prop_bounds_{p_lbl}", 0xDE4, "rkos_img4_find_prop_in_bounds",
             lambda uc, fc=exp_4cc: (
                 wr(uc, 0x02001000, prop_wrapped),
                 wr(uc, 0x02000100, struct.pack("<II", 0x02001000, 0x02001000 + len(prop_wrapped))),
                 wr(uc, 0x02000200, b"\xAA" * 32),
                 [0x02000100, 0, fc, 0xE0000000, 4, 0, 0x02000200],
             )[3], [(0x02000200, 32)])
        safe("img4", f"img4_find_prop_slice_{p_lbl}", 0xE0E, "rkos_img4_find_prop_in_slice",
             lambda uc, fc=exp_4cc: (
                 wr(uc, 0x02001000, prop_wrapped),
                 wr(uc, 0x02000100, struct.pack("<II", 0x02001000, len(prop_wrapped))),
                 wr(uc, 0x02000200, b"\xAA" * 32),
                 [0x02000100, 0, fc, 0xE0000000, 4, 0, 0x02000200],
             )[3], [(0x02000200, 32)])

    safe("img4", "img4_zero_item32", 0x111C, "rkos_img4_zero_item32",
         lambda uc: (wr(uc, 0x02000200, b"\xFF" * 40), [0x02000200])[1], [(0x02000200, 40)])

    # Full manifest ctx init + MANB/MANP + object property lookup (0x101C, 0x1034, 0xE38, 0xF48, 0xFD0)
    safe("img4", "img4_init_im4m_signed_ok", 0x101C, "rkos_img4_init_im4m_signed",
         lambda uc: (wr(uc, 0x02001000, im4m_signed), wr(uc, 0x02002000, b"\xEE" * 0x130),
                     [0x02001000, len(im4m_signed), 0x02002000])[2], [(0x02002000, 0x130)])
    safe("img4", "img4_init_im4m_signed_null", 0x101C, "rkos_img4_init_im4m_signed",
         lambda uc: [0, 16, 0x02002000])
    safe("img4", "img4_init_manifest_ctx_im4c", 0x1034, "rkos_img4_init_manifest_ctx",
         lambda uc: (wr(uc, 0x02001000, im4c_unsigned), wr(uc, 0x02002000, b"\xEE" * 0x130),
                     [0x02001000, len(im4c_unsigned), 0, 0x02002000, 0x494D3443])[2], [(0x02002000, 0x130)])
    safe("img4", "img4_init_manifest_ctx_bad4cc", 0x1034, "rkos_img4_init_manifest_ctx",
         lambda uc: (wr(uc, 0x02001000, im4c_unsigned), wr(uc, 0x02002000, b"\xEE" * 0x130),
                     [0x02001000, len(im4c_unsigned), 0, 0x02002000, 0x11223344])[2], [(0x02002000, 0x130)])

    def setup_inited_im4m(uc: Uc) -> None:
        wr(uc, 0x02001000, im4m_signed)
        wr(uc, 0x02002000, b"\x00" * 0x130)
        pair.call_fn(uc, fn_target(uc, 0x101C, "rkos_img4_init_im4m_signed"),
                     [0x02001000, len(im4m_signed), 0x02002000])

    safe("img4", "img4_get_manb_manp_ok", 0xE38, "rkos_img4_get_manb_and_manp",
         lambda uc: (setup_inited_im4m(uc), wr(uc, 0x02000300, b"\xFF" * 16),
                     [0x02002000, 0x02000300, 0x02000308])[2], [(0x02000300, 16)])
    safe("img4", "img4_get_manb_manp_null", 0xE38, "rkos_img4_get_manb_and_manp",
         lambda uc: [0, 0x02000300, 0x02000308])

    for lbl, obj_4cc in [("rkos_hit", 0x726B6F73), ("miss_xxxx", 0x78787878)]:
        safe("img4", f"img4_lookup_obj_prop_{lbl}", 0xF48, "rkos_img4_lookup_obj_prop",
             lambda uc, oc=obj_4cc: (
                 setup_inited_im4m(uc), wr(uc, 0x02000300, b"\xAA" * 32),
                 [0x02002000, 0, oc, 0, 0x44475354, 0xE0000000, 4, 0, 0x02000300],
             )[2], [(0x02000300, 32)])
        safe("img4", f"img4_get_obj_octet_{lbl}", 0xFD0, "rkos_img4_get_obj_octet_prop",
             lambda uc, oc=obj_4cc: (
                 setup_inited_im4m(uc), wr(uc, 0x02000300, b"\xAA" * 8),
                 [0x02002000, 0, oc, 0, 0x44475354, 0, 0x02000300, 0x02000304],
             )[2], [(0x02000300, 8)])

    # =========================================================================
    # 2. CoreCrypto AES, BigNum (`ccn`), HMAC-DRBG & Outlined AAPCS Helpers
    # =========================================================================
    aes_key32 = bytes(range(0x10, 0x30))
    aes_pt32 = bytes((i * 11 + 5) & 0xFF for i in range(32))
    aes_iv16 = bytes(range(0xA0, 0xB0))

    for rva, sym, nk, nr in [
        (0x1988, "rkos_aes_key_expand_enc", 4, 10),
        (0x1BA4, "rkos_aes_key_expand_dec", 4, 10),
    ]:
        safe("crypto", f"{sym}_128", rva, sym,
             lambda uc, k=nk, r=nr: (wr(uc, 0x02000100, aes_key32), wr(uc, 0x02000200, b"\x00" * 0x100),
                                     [0x02000100, 0x02000200, k, r])[2],
             [(0x02000200, 0x100)], check_r0=False)

    for rva, sym, chk_r0 in [
        (0x1B80, "rkos_aes_key_expand_enc128", False),
        (0x1B8C, "rkos_aes_key_expand_enc192", False),
        (0x1B98, "rkos_aes_key_expand_enc256", False),
        (0x1C08, "rkos_aes_key_expand_dec128", True),
        (0x1C20, "rkos_aes_key_expand_dec192", True),
        (0x1C38, "rkos_aes_key_expand_dec256", True),
    ]:
        safe("crypto", sym, rva, sym,
             lambda uc: (wr(uc, 0x02000100, aes_key32), wr(uc, 0x02000200, b"\x00" * 0x100),
                         [0x02000100, 0x02000200])[2],
             [(0x02000200, 0x100)], check_r0=chk_r0)

    for rva, sym in [(0x1C50, "rkos_aes_setup_enc"), (0x1C8C, "rkos_aes_setup_dec")]:
        for kbits in (16, 24, 32, 128, 17):
            safe("crypto", f"{sym}_{kbits}", rva, sym,
                 lambda uc, kb=kbits: (wr(uc, 0x02000100, aes_key32), wr(uc, 0x02000200, b"\x00" * 0x100),
                                       [0x02000100, kb, 0x02000200])[2],
                 [(0x02000200, 0x100)])

    def setup_aes_ctx(uc: Uc, dec: bool = False) -> None:
        wr(uc, 0x02000100, aes_key32)
        wr(uc, 0x02000200, b"\x00" * 0x100)
        if dec:
            pair.call_fn(uc, fn_target(uc, 0x1C8C, "rkos_aes_setup_dec"), [0x02000100, 16, 0x02000200])
        else:
            pair.call_fn(uc, fn_target(uc, 0x1C50, "rkos_aes_setup_enc"), [0x02000100, 16, 0x02000200])

    for rva, sym, is_dec in [(0x256C, "rkos_aes_encrypt_block", False), (0x1E50, "rkos_aes_decrypt_block", True)]:
        safe("crypto", f"{sym}_ok", rva, sym,
             lambda uc, d=is_dec: (setup_aes_ctx(uc, d), wr(uc, 0x02000400, aes_pt32[:16]),
                                   wr(uc, 0x02000440, b"\x00" * 16), [0x02000400, 0x02000440, 0x02000200])[3],
             [(0x02000440, 16)])
        safe("crypto", f"{sym}_badctx", rva, sym,
             lambda uc: (wr(uc, 0x02000200, b"\x00" * 0x100), [0x02000400, 0x02000440, 0x02000200])[1])

    for rva, sym, is_dec in [(0x190C, "rkos_aes_cbc_encrypt", False), (0x1880, "rkos_aes_cbc_decrypt", True)]:
        safe("crypto", f"{sym}_2blk", rva, sym,
             lambda uc, d=is_dec: (setup_aes_ctx(uc, d), wr(uc, 0x02000400, aes_pt32),
                                   wr(uc, 0x02000440, aes_iv16), wr(uc, 0x02000500, b"\x00" * 32),
                                   [0x02000400, 0x02000440, 2, 0x02000500, 0x02000200])[4],
             [(0x02000500, 32)], check_r0=False)

    safe("crypto", "rkos_aes_ctr_crypt_2blk", 0x1DD4, "rkos_aes_ctr_crypt",
         lambda uc: (setup_aes_ctx(uc, False), wr(uc, 0x02000400, aes_pt32),
                     wr(uc, 0x02000440, aes_iv16), wr(uc, 0x02000500, b"\x00" * 32),
                     [0x02000400, 0x02000500, 32, 0x02000440, 0x02000200])[4],
         [(0x02000500, 32), (0x02000440, 16)], check_r0=False)

    for rva, sym in [(0x2468, "rkos_aes_ccm_encrypt"), (0x1CC8, "rkos_aes_ccm_decrypt")]:
        safe("crypto", f"{sym}_2blk", rva, sym,
             lambda uc: (setup_aes_ctx(uc, False), wr(uc, 0x02000400, aes_pt32),
                         wr(uc, 0x02000440, aes_iv16), wr(uc, 0x02000480, b"\x11" * 16),
                         wr(uc, 0x02000500, b"\x00" * 32),
                         [0x02000400, 0x02000500, 0x02000480, 2, 0x02000200, 0x02000440, 3])[5],
             [(0x02000500, 32), (0x02000480, 16), (0x02000440, 16)], check_r0=False)

    # BigNum `ccn` suite (10 functions: 0x2080..0x23D0)
    ccn_a = struct.pack("<IIII", 0xFFFFFFFF, 0x12345678, 0x9ABCDEF0, 0x00000001)
    ccn_b = struct.pack("<IIII", 0x00000002, 0xFEDCBA98, 0x01020304, 0xFFFFFFFF)
    for rva, sym in [(0x2080, "rkos_ccn_add"), (0x235C, "rkos_ccn_sub")]:
        for cnt, b_vec in [(4, ccn_b), (4, b"\x00" * 16)]:
            safe("crypto", f"{sym}_c{cnt}_{b_vec[0]}", rva, sym,
                 lambda uc, n=cnt, bv=b_vec: (wr(uc, 0x02000100, ccn_a), wr(uc, 0x02000140, bv),
                                              wr(uc, 0x02000200, b"\xCC" * 16),
                                              [n, 0x02000200, 0x02000100, 0x02000140])[3],
                 [(0x02000200, 16)])

    for rva, sym in [(0x20EC, "rkos_ccn_add1"), (0x2234, "rkos_ccn_sub1"),
                     (0x2118, "rkos_ccn_addmul1"), (0x2264, "rkos_ccn_mul1")]:
        for scalar in (0, 7, 0xFFFFFFFF):
            safe("crypto", f"{sym}_s{scalar & 0xFF}", rva, sym,
                 lambda uc, sc=scalar: (wr(uc, 0x02000100, ccn_a), wr(uc, 0x02000200, ccn_b),
                                        [4, 0x02000200, 0x02000100, sc])[2],
                 [(0x02000200, 16)])

    for lbl, x, y in [("eq", ccn_a, ccn_a), ("lt", ccn_a, ccn_b), ("gt", ccn_b, ccn_a)]:
        safe("crypto", f"rkos_ccn_cmp_{lbl}", 0x2204, "rkos_ccn_cmp",
             lambda uc, vx=x, vy=y: (wr(uc, 0x02000100, vx), wr(uc, 0x02000140, vy),
                                     [4, 0x02000100, 0x02000140])[2])

    for lbl, vec in [("zero", b"\x00" * 16), ("three", struct.pack("<IIII", 1, 0, 5, 0))]:
        safe("crypto", f"rkos_ccn_n_{lbl}", 0x2304, "rkos_ccn_n",
             lambda uc, v=vec: (wr(uc, 0x02000100, v), [4, 0x02000100])[1])

    safe("crypto", "rkos_ccn_set_4", 0x2328, "rkos_ccn_set",
         lambda uc: (wr(uc, 0x02000100, ccn_a), wr(uc, 0x02000200, b"\x00" * 16),
                     [4, 0x02000200, 0x02000100])[2],
         [(0x02000200, 16)], check_r0=False)

    for sh in (0, 5, 19):
        safe("crypto", f"rkos_ccn_shift_right_{sh}", 0x23D0, "rkos_ccn_shift_right",
             lambda uc, s=sh: (wr(uc, 0x02000100, ccn_a), wr(uc, 0x02000200, b"\xEE" * 16),
                               [4, 0x02000200, 0x02000100, s])[2],
             [(0x02000200, 16)], check_r0=False)

    # NIST SP 800-90A HMAC-DRBG & CoreCrypto helpers
    def setup_drbg_info(uc: Uc, strict: int = 1) -> int:
        # custom @ 0x02003000: { di_va=0x0001612C (SHA-256), strict_reseed }
        wr(uc, 0x02003000, struct.pack("<II", 0x0001612C, strict))
        # drbg_info @ 0x02003020: +0x14 = custom_va
        wr(uc, 0x02003020, b"\x00" * 0x20)
        wr(uc, 0x02003034, struct.pack("<I", 0x02003000))
        wr(uc, 0x02003100, b"entropy_seed_32bytes_for_sha256!")
        wr(uc, 0x02003140, b"nonce_16_bytes!!")
        wr(uc, 0x02003180, b"pers_str")
        wr(uc, 0x02003200, b"\x00" * 0x90)
        return 0x02003200

    safe("crypto", "drbg_hmac_init_ok", 0x12374, "rkos_drbg_hmac_init",
         lambda uc: (setup_drbg_info(uc),
                     [0x02003020, 0x02003200, 32, 0x02003100, 16, 0x02003140, 8, 0x02003180])[1],
         [(0x02003200, 0x90)])
    safe("crypto", "drbg_hmac_init_short_ent", 0x12374, "rkos_drbg_hmac_init",
         lambda uc: (setup_drbg_info(uc),
                     [0x02003020, 0x02003200, 4, 0x02003100, 16, 0x02003140, 8, 0x02003180])[1],
         [(0x02003200, 0x90)])

    def setup_inited_drbg(uc: Uc) -> int:
        drbg_va = setup_drbg_info(uc)
        pair.call_fn(uc, fn_target(uc, 0x12374, "rkos_drbg_hmac_init"),
                     [0x02003020, drbg_va, 32, 0x02003100, 16, 0x02003140, 8, 0x02003180])
        return drbg_va

    safe("crypto", "drbg_hmac_update_1in", 0x125CC, "rkos_drbg_hmac_update",
         lambda uc: [setup_inited_drbg(uc), 1, 8, 0x02003180], [(0x02003200, 0x90)], check_r0=False)

    for rva, sym in [(0x12520, "rkos_drbg_hmac_reseed"), (0x12526, "rkos_drbg_hmac_reseed_body")]:
        safe("crypto", f"{sym}_ok", rva, sym,
             lambda uc: [setup_inited_drbg(uc), 32, 0x02003100, 8, 0x02003180], [(0x02003200, 0x90)])
        safe("crypto", f"{sym}_short", rva, sym,
             lambda uc: [setup_inited_drbg(uc), 2, 0x02003100, 8, 0x02003180])

    safe("crypto", "drbg_hmac_generate_48b", 0x123F4, "rkos_drbg_hmac_generate",
         lambda uc: (setup_inited_drbg(uc), wr(uc, 0x02003400, b"\x00" * 64),
                     [0x02003200, 48, 0x02003400, 8, 0x02003180])[2],
         [(0x02003200, 0x90), (0x02003400, 64)])
    safe("crypto", "drbg_hmac_generate_reseed_req", 0x123F4, "rkos_drbg_hmac_generate",
         lambda uc: (setup_inited_drbg(uc),
                     wr(uc, 0x02003288, struct.pack("<II", 1, 0x00010000)),
                     [0x02003200, 32, 0x02003400, 0, 0])[2])

    safe("crypto", "drbg_clear", 0x1256C, "rkos_drbg_clear",
         lambda uc: [setup_inited_drbg(uc)], [(0x02003200, 0x90)])

    for lbl, lo, hi in [("fresh", 1, 0), ("expired", 1, 0x00010000)]:
        safe("crypto", f"drbg_need_reseed_{lbl}", 0x125A8, "rkos_drbg_need_reseed",
             lambda uc, l=lo, h=hi: (setup_inited_drbg(uc),
                                     wr(uc, 0x02003288, struct.pack("<II", l, h)),
                                     [0x02003200])[2])

    for cnt_init in (2, 1):
        def mk_rmtx(ci: int = cnt_init) -> Callable[[Uc], list[int]]:
            def _s(uc: Uc) -> list[int]:
                wr(uc, 0x2000084C, struct.pack("<I", 0x02001100))
                wr(uc, 0x02001000, struct.pack("<III", 0, 0x02001100, ci))
                return [0x02001000]
            return _s
        safe("crypto", f"ref_mutex_dec_{cnt_init}", 0x118B4, "rkos_ref_mutex_dec_unlock",
             mk_rmtx(), [(0x02001000, 12)])

    safe("crypto", "crypto_panic_str_null", 0x11DB8, "rkos_crypto_panic_str",
         lambda uc: [0], check_r0=False, trap_pending=[(0x3EE0, "rtkit_panic", None)])
    safe("crypto", "crypto_panic_str_msg", 0x11DB8, "rkos_crypto_panic_str",
         lambda uc: (wr(uc, 0x02000100, b"fips_err\x00"), [0x02000100])[1],
         check_r0=False, trap_pending=[(0x3EE0, "rtkit_panic", None)])
    safe("crypto", "crypto_fips_abort", 0x118C4, "rkos_crypto_fips_abort",
         lambda uc: [], check_r0=False, trap_pending=[(0x3EE0, "rtkit_panic", None)])

    # Outlined AAPCS helpers (0x127B0..0x12ACE)
    safe("crypto", "take_and_dec_field58", 0x127B0, "rkos_take_and_dec_field58",
         lambda uc: (wr(uc, 0x02000158, struct.pack("<I", 0x81)), [0x02000100])[1], [(0x02000158, 4)])
    safe("crypto", "bzero32", 0x127C8, "rkos_bzero32",
         lambda uc: (wr(uc, 0x02000100, b"\xAA" * 40), [0x02000100])[1], [(0x02000100, 40)])
    safe("crypto", "bzero_len_20", 0x128FE, "rkos_bzero_len",
         lambda uc: (wr(uc, 0x02000100, b"\xBB" * 32), [0x02000100, 99, 20])[1], [(0x02000100, 32)])
    for bidx in (3, 37):
        safe("crypto", f"bitmap_shift_word_{bidx}", 0x129B6, "rkos_bitmap_shift_word",
             lambda uc, b=bidx: (wr(uc, 0x02000100, struct.pack("<II", 0xA5A5F008, 0x12345678)),
                                 [b, 0, 0x02000100])[1])
    safe("crypto", "umul_u32_u64", 0x12A1A, "rkos_umul_u32_u64",
         lambda uc: [0, 0x10003, 0x89ABCDEF, 0x00000123], check_r1=True)
    for zv in (0, 42):
        safe("crypto", f"is_zero_u32_{zv}", 0x12A64, "rkos_is_zero_u32", lambda uc, v=zv: [v])
    safe("crypto", "memcpy16", 0x12A96, "rkos_memcpy16",
         lambda uc: (wr(uc, 0x02000100, bytes(range(16))), wr(uc, 0x02000200, b"\xFF" * 24),
                     [0x02000200, 0x02000100])[2], [(0x02000200, 24)])
    safe("crypto", "pack_low2_and_idx", 0x12A9C, "rkos_pack_low2_and_idx",
         lambda uc: [0xABCD0002, 0, 9])
    safe("crypto", "const_256", 0x12AB8, "rkos_const_256", lambda uc: [0])
    safe("crypto", "deref_table_slot", 0x12ACE, "rkos_deref_table_slot",
         lambda uc: (wr(uc, 0x02000100, struct.pack("<I", 0x02000140)),
                     wr(uc, 0x02000140, struct.pack("<III", 0x11, 0x22, 0x778899AA)),
                     wr(uc, 0x02000200, b"\xFF" * 4),
                     [0x02000100, 0, 2, 0x02000200])[3], [(0x02000200, 4)])

    # =========================================================================
    # 3. Libc String, Strtoull & Printf / Console Mux Suite (10 functions)
    # =========================================================================
    for nbytes in (0, 20, 68):
        safe("libc", f"word_copy32_{nbytes}", 0x5CF2, "rkos_word_copy32",
             lambda uc, nb=nbytes: (wr(uc, 0x02000100, bytes((i * 5 + 1) & 0xFF for i in range(96))),
                                    wr(uc, 0x02000200, b"\xEE" * 96),
                                    [0x02000200, 0x02000100, nb])[2], [(0x02000200, 96)])

    safe("libc", "memset_chk_fail", 0x5D3C, "rkos_memset_chk_fail",
         lambda uc: [], check_r0=False, trap_pending=[(0x3EE0, "rtkit_panic", None)])

    for lbl, txt, b_in in [("auto_hex", b"  -0x1aF!", 0), ("octal", b"0755", 0), ("dec_plus", b"+42", 10)]:
        safe("libc", f"strtol_parse_base_{lbl}", 0x54BC, "rkos_strtol_parse_base",
             lambda uc, s=txt, bi=b_in: (wr(uc, 0x02000100, s + b"\x00"),
                                         wr(uc, 0x02000180, bytes([0xFF])),
                                         wr(uc, 0x02000190, struct.pack("<I", bi)),
                                         [0x02000100, 0x02000180, 0x02000190])[3],
             [(0x02000180, 1), (0x02000190, 4)])

    for lbl, digits, base, sign_in in [
        ("hex64", b"123456789abcdef0!", 16, 0),
        ("neg_dec", b"1000!", 10, 1),
        ("ovfl", b"fffffffffffffffff!", 16, 0),
    ]:
        def mk_scan(dg: bytes = digits, bs: int = base, sg: int = sign_in) -> Callable[[Uc], list[int]]:
            def _s(uc: Uc) -> list[int]:
                wr(uc, 0x02000100, dg + b"\x00")
                wr(uc, 0x02000180, struct.pack("<I", 0x02000100))
                wr(uc, 0x02000190, bytes([sg]))
                cutoff = (0xFFFFFFFFFFFFFFFF // bs)
                cutlim = (0xFFFFFFFFFFFFFFFF % bs)
                return [0x02000180, bs, 0xFFFFFFFF, 0xFFFFFFFF,
                        cutoff & 0xFFFFFFFF, (cutoff >> 32) & 0xFFFFFFFF, cutlim, 0x02000190]
            return _s
        safe("libc", f"strtoull_scanner_{lbl}", 0x53F0, "rkos_strtoull_scanner", mk_scan(),
             [(0x02000180, 4), (0x02000190, 1)], check_r1=True)

    for ch in (ord("A"), 0):
        safe("libc", f"snprintf_putc_{ch}", 0x5BD0, "rkos_snprintf_putc",
             lambda uc, c=ch: (wr(uc, 0x02000200, b"X" * 16),
                               wr(uc, 0x02000100, struct.pack("<IIIII", 0, 8, 0x02000200, 2, 0)),
                               [0x02000100, c])[2],
             [(0x02000100, 20), (0x02000200, 16)])

    safe("libc", "console_mux_putc_ok", 0x5DD8, "rkos_console_mux_putc",
         lambda uc: (wire_console_mux(uc), [0x20000718, ord("Z")])[1], [(0x02007100, 32)])
    safe("libc", "console_putc_char", 0x55D4, "rkos_console_putc",
         lambda uc: (wire_console_mux(uc), [0x02000100, ord("Q")])[1], [(0x02007100, 32)])
    safe("libc", "console_putc_nul", 0x55D4, "rkos_console_putc",
         lambda uc: (wire_console_mux(uc), [0x02000100, 0])[1], [(0x02007100, 32)])

    def setup_vprintf_args(uc: Uc) -> tuple[int, int]:
        wire_console_mux(uc)
        wr(uc, 0x02000100, b"s=%s null=%s x=%#X p=%p d=%+06d ll=%llx c=%c %% %q\x00")
        wr(uc, 0x02000180, b"hi\x00")
        # ARM AAPCS va_list: 8-byte aligned uint64_t at offset +24 (6 words before it!)
        ap_words = struct.pack("<IIIIIIII",
                               0x02000180, 0, 0x1A2B, 0x20000100, -42 & 0xFFFFFFFF, 0xDEAD,
                               0x55667788, 0x11223344) + struct.pack("<I", ord("!"))
        # Wait: in fmt above: %s(w0), %s(w1), %#X(w2), %p(w3), %+06d(w4), [pad w5 for 8B align!], %llx(w6:w7), %c(w8)
        wr(uc, 0x02000200, ap_words)
        return 0x02000100, 0x02000200

    safe("libc", "vprintf_core_rich", 0x55E0, "rkos_vprintf_core",
         lambda uc: (setup_vprintf_args(uc),
                     wr(uc, 0x02000300, struct.pack("<IIIII",
                        (pair.sym_addr("rkos_console_putc") | 1) if getattr(uc, "_is_elf", False) else 0x55D5,
                        0, 0, 0, 0)),
                     [0x02000300, 0x02000100, 0x02000200])[2],
         [(0x02007100, 0x80)])

    safe("libc", "vprintf_rich", 0x55B0, "rkos_vprintf",
         lambda uc: list(setup_vprintf_args(uc)), [(0x02007100, 0x80)])

    safe("libc", "printf_variadic", 0x5592, "rkos_printf",
         lambda uc: (wire_console_mux(uc),
                     wr(uc, 0x02000100, b"rkos=%s n=%d hex=0x%04x\n\x00"),
                     wr(uc, 0x02000140, b"ok\x00"),
                     [0x02000100, 0x02000140, -7 & 0xFFFFFFFF, 0xBEE])[3],
         [(0x02007100, 0x80)])

    # =========================================================================
    # 4. Hardened Kernel Heap Suite (13 newly promoted functions)
    # =========================================================================
    heap_va = 0x2001DC38
    heap_cmp = [(heap_va, 0x68), (0x20001078, 0x10), (0x20001140, 0x200), (0x2001CF80, 0x1C0)]

    def setup_fresh_heap(uc: Uc) -> None:
        guard(uc)
        pair.call_fn(uc, fn_target(uc, 0x50B4, "crit_enter"), [])
        pair.call_fn(uc, fn_target(uc, 0x5124, "crit_exit"), [0])

    safe("heap", "heap_range_in_regions_hit", 0x31C0, "rkos_heap_range_in_regions",
         lambda uc: (setup_fresh_heap(uc), [heap_va, 0x20001200, 0x20001300])[1])
    safe("heap", "heap_range_in_regions_miss", 0x31C0, "rkos_heap_range_in_regions",
         lambda uc: (setup_fresh_heap(uc), [heap_va, 0x20001000, 0x20001200])[1])

    safe("heap", "heap_mark_and_unmark_alloc", 0x2C58, "rkos_heap_mark_allocated",
         lambda uc: (setup_fresh_heap(uc), [heap_va, 0x20002000, 0x40, 1])[1],
         [(heap_va, 0x68), (0x20002000, 0x40)])

    def setup_marked_alloc(uc: Uc) -> list[int]:
        setup_fresh_heap(uc)
        pair.call_fn(uc, fn_target(uc, 0x2C58, "rkos_heap_mark_allocated"), [heap_va, 0x20002000, 0x40, 1])
        return [heap_va, 0x20002000, 1]

    safe("heap", "heap_unmark_allocated", 0x2F70, "rkos_heap_unmark_allocated",
         setup_marked_alloc, [(heap_va, 0x68), (0x20002000, 0x40)])

    safe("heap", "heap_mark_free_blk", 0x2F94, "rkos_heap_mark_free",
         lambda uc: (guard(uc), pair.call_fn(uc, fn_target(uc, 0x4B58, "rkos_heap_init_struct"), [heap_va]),
                     wr(uc, heap_va, struct.pack("<II", 0x20001140, 0x1000)),
                     wr(uc, heap_va + 0x40, struct.pack("<I", 1)),
                     [heap_va, 0x20001200, 0x80])[4],
         [(heap_va, 0x68), (0x20001200, 0x80)])

    def setup_two_free_blks(uc: Uc) -> None:
        guard(uc)
        pair.call_fn(uc, fn_target(uc, 0x4B58, "rkos_heap_init_struct"), [heap_va])
        wr(uc, heap_va, struct.pack("<II", 0x20001140, 0x1000))
        wr(uc, heap_va + 0x40, struct.pack("<I", 1))
        pair.call_fn(uc, fn_target(uc, 0x2F94, "rkos_heap_mark_free"), [heap_va, 0x20001200, 0x40])
        pair.call_fn(uc, fn_target(uc, 0x2F94, "rkos_heap_mark_free"), [heap_va, 0x20001240, 0x60])

    safe("heap", "heap_freelist_insert_second", 0x30DC, "rkos_heap_freelist_insert",
         lambda uc: (guard(uc), pair.call_fn(uc, fn_target(uc, 0x4B58, "rkos_heap_init_struct"), [heap_va]),
                     wr(uc, heap_va, struct.pack("<II", 0x20001140, 0x1000)),
                     wr(uc, heap_va + 0x40, struct.pack("<I", 1)),
                     pair.call_fn(uc, fn_target(uc, 0x2F94, "rkos_heap_mark_free"), [heap_va, 0x20001200, 0x60]),
                     wr(uc, 0x20001300, struct.pack("<IIII", 0, 0x41, 0, 0)),
                     wr(uc, 0x2000133C, struct.pack("<I", 0x40)),
                     wr(uc, heap_va + 0x50, struct.pack("<II", 2, 0x20001200 ^ 0x20001300)),
                     [heap_va, 0x20001300])[8],
         [(heap_va, 0x68), (0x20001200, 0x140)])

    safe("heap", "heap_freelist_unlink", 0x3030, "rkos_heap_freelist_unlink",
         lambda uc: (setup_fresh_heap(uc), [heap_va, struct.unpack("<I", bytes(uc.mem_read(heap_va + 0x48, 4)))[0]])[1],
         [(heap_va, 0x68), (0x20001140, 0x40)])

    safe("heap", "heap_unmark_free", 0x3154, "rkos_heap_unmark_free",
         lambda uc: (setup_two_free_blks(uc), [heap_va, 0x20001200])[1],
         [(heap_va, 0x68), (0x20001200, 0xA0)])

    safe("heap", "heap_coalesce_pair", 0x30A0, "rkos_heap_coalesce_pair",
         lambda uc: (setup_two_free_blks(uc), [heap_va, 0x20001200, 0x20001240])[1],
         [(heap_va, 0x68), (0x20001200, 0xA0)])

    safe("heap", "heap_alloc_or_coalesce_pass", 0x2D84, "heap_alloc_or_coalesce",
         lambda uc: (setup_fresh_heap(uc), [heap_va])[1], heap_cmp)

    safe("heap", "heap_alloc_impl_64", 0x31E8, "heap_alloc_impl",
         lambda uc: (setup_fresh_heap(uc), [heap_va, 64, 0, 0])[1], heap_cmp)

    def setup_alloc_and_free_impl(uc: Uc) -> list[int]:
        setup_fresh_heap(uc)
        p1, _ = pair.call_fn(uc, fn_target(uc, 0x31E8, "heap_alloc_impl"), [heap_va, 48, 0, 0])
        pair.call_fn(uc, fn_target(uc, 0x31E8, "heap_alloc_impl"), [heap_va, 32, 0, 0])
        return [heap_va, p1, 1]

    safe("heap", "heap_free_impl_tracked", 0x2CAC, "heap_free_impl",
         setup_alloc_and_free_impl, heap_cmp)

    safe("heap", "kernel_heap_add_region_extra", 0x4F1C, "rkos_kernel_heap_add_region",
         lambda uc: (setup_fresh_heap(uc), [0x02004000, 0x400])[1],
         [(heap_va, 0x68), (0x02004000, 0x400)])

    safe("heap", "heap_disable_checks", 0x5134, "rkos_heap_disable_checks",
         lambda uc: (setup_fresh_heap(uc), [])[1], [(heap_va, 0x68)])

    def setup_realloc_grow(uc: Uc) -> list[int]:
        setup_fresh_heap(uc)
        p1, _ = pair.call_fn(uc, fn_target(uc, 0x515C, "rtkit_malloc"), [32])
        p2, _ = pair.call_fn(uc, fn_target(uc, 0x515C, "rtkit_malloc"), [32])
        wr(uc, p2, bytes(range(1, 33)))
        pair.call_fn(uc, fn_target(uc, 0x53B4, "rtkit_free"), [p1])
        return [p2, 48]

    safe("heap", "rkos_realloc_inplace_grow", 0x5184, "rkos_realloc", setup_realloc_grow, heap_cmp)
    safe("heap", "rkos_realloc_null_alloc", 0x5184, "rkos_realloc",
         lambda uc: (setup_fresh_heap(uc), [0, 40])[1], heap_cmp)

    # =========================================================================
    # 5. NVIC, Critical Sections, Scheduler & Unwinder Suite (18 functions)
    # =========================================================================
    for pm in (0, 1):
        safe("nvic", f"primask_disable_pm{pm}", 0x3488, "rkos_primask_disable_if_enabled",
             lambda uc, p=pm: (uc.reg_write(UC_ARM_REG_PRIMASK, p), [0])[1], check_sysregs=True)
        safe("nvic", f"irq_enable_all_pm{pm}", 0x349A, "rkos_irq_enable_all",
             lambda uc, p=pm: (uc.reg_write(UC_ARM_REG_PRIMASK, p),
                               uc.reg_write(UC_ARM_REG_BASEPRI, 0x80), [0])[2], check_sysregs=True)

    for en in (0, 1):
        safe("nvic", f"nvic_set_enable_state_e{en}", 0x35BC, "rkos_nvic_set_enable_state",
             lambda uc, e=en: (wr(uc, 0x200002B8, struct.pack("<I", 72)), [1, 21, e])[1],
             [(0xE000E100, 0x10), (0xE000E180, 0x10)])

    # IRQ_CommonHandler with registered callback in SCRATCH_BASE + unregistered fallback
    # Tiny Thumb callback at 0x02006000: ldr r0, [r0]; adds r0, #0x22; bx lr -> 00 68 22 30 70 47
    irq_cb_thumb = bytes.fromhex("006822307047")
    def setup_irq_dispatched(uc: Uc) -> list[int]:
        wr(uc, 0x200002B8, struct.pack("<I", 72))
        wr(uc, 0x02006000, irq_cb_thumb)
        wr(uc, 0x02006100, struct.pack("<I", 0x100))
        wr(uc, 0x20000D50 + 4 * 6, struct.pack("<I", 0x02006001))
        wr(uc, 0x20000E78 + 4 * 6, struct.pack("<I", 0x02006100))
        uc.reg_write(UC_ARM_REG_IPSR, 20)
        return []

    def setup_irq_unhandled(uc: Uc) -> list[int]:
        wr(uc, 0x200002B8, struct.pack("<I", 72))
        uc.reg_write(UC_ARM_REG_IPSR, 18)
        return []

    safe("nvic", "irq_common_dispatched", 0x36F8, "IRQ_CommonHandler", setup_irq_dispatched)
    safe("nvic", "irq_common_unhandled", 0x36F8, "IRQ_CommonHandler", setup_irq_unhandled)

    def setup_nvic_drv(uc: Uc, op: int, a2: int = 0, a3: int = 0) -> list[int]:
        setup_fresh_heap(uc)
        wr(uc, 0x200002B8, struct.pack("<I", 32))
        wr(uc, 0xE000E004, struct.pack("<I", 1))  # ICTR -> 64 max IRQs
        wr(uc, 0x02005000, struct.pack("<I", 0x02005080) + b"\x00" * 0x3C)
        wr(uc, 0x02005080, struct.pack("<IIII", 0x1111, 0x2222, 0x3333, 0x4444))
        if op in (4, 5):
            pair.call_fn(uc, fn_target(uc, 0x3838, "rkos_nvic_drv_op"), [0x02005000, 1, 0, 0])
            wr(uc, 0xE000E100, struct.pack("<II", 0x12345678, 0x9ABCDEF0))
        return [0x02005000, op, a2, a3]

    for lbl, op, a2, a3 in [
        ("init_op1", 1, 0, 0),
        ("noop_op2", 2, 0, 0),
        ("save_op4", 4, 3, 0),
        ("restore_op5", 5, 0, 0),
        ("unsupported_op6", 6, 0, 0),
        ("vtbl_op8", 8, 2, 0x02005100),
        ("oob_op99", 99, 0, 0),
    ]:
        safe("nvic", f"nvic_drv_{lbl}", 0x3838, "rkos_nvic_drv_op",
             lambda uc, o=op, x=a2, y=a3: setup_nvic_drv(uc, o, x, y),
             [(0x02005000, 0x40), (0x02005100, 4), (0xE000E100, 8), (0xE000ED08, 4)])

    safe("nvic", "crit_enter_init", 0x50B4, "crit_enter",
         lambda uc: (guard(uc), [])[1], [(0x2001DE24, 4), (0x20001078, 4), (heap_va, 0x68)])
    for st in (0, 0x20002):
        safe("nvic", f"crit_exit_{st}", 0x5124, "crit_exit",
             lambda uc, s=st: (wr(uc, 0x2000084C, struct.pack("<I", 4)),
                               wr(uc, 0x2001DE24, struct.pack("<I", 4)), [s])[2],
             [(0x2001DE24, 4)])

    # Boot & Unwinder (`0x1140, 0x1158, 0x3B3C, 0x3EAC, 0x3B58, 0x1090C, 0x1091E`)
    safe("boot", "stack_guard_init", 0x1140, "rkos_stack_guard_init",
         lambda uc: (wr(uc, 0x20000198, b"\x00" * 8), [])[1], [(0x20000198, 8)])
    safe("boot", "stack_chk_fail", 0x1158, "__stack_chk_fail",
         lambda uc: [], check_r0=False, trap_pending=[(0x3EE0, "rtkit_panic", None)])

    for init_v in (0, 5):
        safe("boot", f"crit_lock_once_{init_v}", 0x3B3C, "rkos_crit_lock_once",
             lambda uc, iv=init_v: (wr(uc, 0x2001F060, struct.pack("<I", 0x80)),
                                    uc.reg_write(UC_ARM_REG_BASEPRI, 0x20),
                                    wr(uc, 0x02000100, struct.pack("<I", iv)),
                                    [0x02000100])[3], [(0x02000100, 4)])

    def setup_unwind_env(uc: Uc) -> None:
        guard(uc)
        pair.call_fn(uc, fn_target(uc, 0x11528, "rkos_mem_region_ensure_init"), [])
        pair.call_fn(uc, fn_target(uc, 0x113E0, "rkos_mem_region_register"), [6, 0x02008000, 0x1000])
        # Synthetic Thumb-2 code in shared code slot 6 (0x02008000..0x02009000):
        #   0x02008100: bl #0x02008200  (00 f0 7e f8), lr = 0x02008105
        #   0x02008200: nop; nop        (00 bf 00 bf), leaf prologue at pc = 0x02008203
        wr(uc, 0x02008100, bytes.fromhex("00f07ef800bf"))
        wr(uc, 0x02008200, bytes.fromhex("00bf00bf7047"))
        wr(uc, 0x2001D200, struct.pack("<II", 0x2001D220, 0x02008105))
        wr(uc, 0x2001D220, struct.pack("<II", 0x77777700, 0x02008105))
        wr(uc, 0x2001D300, b"\x00" * 0x90)
        wr(uc, 0x2001D300, struct.pack("<I", 0x10))
        wr(uc, 0x2001D314, struct.pack("<I", 0x2001D200))
        wr(uc, 0x2001D33C, struct.pack("<I", 0x02008105))
        wr(uc, 0x2001D340, struct.pack("<I", 0x02008203))
        wr(uc, 0x02000400, b"\xEE" * 0x60)
        wr(uc, 0x02000480, b"\xFF" * 4)

    for lbl, lr_in in [("bl32_code", 0x02008105), ("unmapped_lr", 0x40001001)]:
        safe("boot", f"unwind_callsite_{lbl}", 0x3EAC, "rkos_unwind_callsite_pc",
             lambda uc, lv=lr_in: (setup_unwind_env(uc), [lv])[1])

    for lbl, eframe, maxf in [("exc_walk", 0x2001D300, 8), ("max1", 0x2001D300, 1), ("max0", 0, 0), ("cur_sp", 0, 4)]:
        safe("boot", f"unwind_frame_{lbl}", 0x3B58, "rkos_unwind_frame",
             lambda uc, ef=eframe, mf=maxf: (setup_unwind_env(uc), [ef, 0x02000400, mf, 0x02000480])[1],
             [(0x02000400, 0x40), (0x02000480, 4)])

    safe("sched", "unwind_frames_cnt_exc", 0x1091E, "rkos_unwind_frames_cnt",
         lambda uc: (setup_unwind_env(uc), [0x2001D300, 0x02000400, 8])[1], [(0x02000400, 0x40)])

    for lbl, tva in [("other_task", 0x02000500), ("null_task", 0)]:
        def mk_utask(tv: int = tva) -> Callable[[Uc], list[int]]:
            def _s(uc: Uc) -> list[int]:
                setup_unwind_env(uc)
                wr(uc, 0x2000084C, struct.pack("<I", 0x02000600))
                wr(uc, 0x02000528, struct.pack("<I", 0x02000540))
                wr(uc, 0x02000540, struct.pack("<I", 0x2001D300))
                return [tv, 0x02000400, 8]
            return _s
        safe("sched", f"unwind_task_{lbl}", 0x1090C, "rkos_unwind_task", mk_utask(), [(0x02000400, 0x40)])

    # Scheduler switch, ready-queue remove, priority inheritance, spinlock & region table
    for resched in (0, 1):
        def mk_ssw(rs: int = resched) -> Callable[[Uc], list[int]]:
            def _s(uc: Uc) -> list[int]:
                wr(uc, 0x2001F060, struct.pack("<I", 0x80))
                wr(uc, 0x2001DDE8, bytes([rs]))
                # cur_task @ 0x02001000 (save_sp_slot=0x02001100), next_task @ 0x02001200 (load_sp_slot=0x02001300 -> 0x2001D800)
                wr(uc, 0x02001016, bytes([0]))
                wr(uc, 0x02001028, struct.pack("<I", 0x02001100))
                wr(uc, 0x02001216, bytes([0]))
                wr(uc, 0x02001228, struct.pack("<I", 0x02001300))
                wr(uc, 0x02001300, struct.pack("<I", 0x2001D800))
                wr(uc, 0x2000084C, struct.pack("<I", 0x02001000))
                wr(uc, 0x2001DCD0, struct.pack("<I", 1 << 5))
                wr(uc, 0x2001DCD8 + 8 * 5, struct.pack("<I", 0x02001200))
                return [0x2001D700]
            return _s
        safe("sched", f"rtkit_sched_switch_r{resched}", 0x34F4, "rtkit_sched_switch", mk_ssw(),
             [(0x2001DDE8, 1), (0x2001DDFC, 1), (0x2000084C, 4), (0x02001100, 4)])

    safe("sched", "sched_tailq_remove_last", 0x109D0, "rkos_sched_tailq_remove",
         lambda uc: (wr(uc, 0x2001DCD8 + 8 * 7 + 4, struct.pack("<I", 0x2001DCD8 + 8 * 7)),
                     pair.call_fn(uc, fn_target(uc, 0x10668, "rkos_sched_tailq_enqueue"), [0x02000200, 7]),
                     [0, 0x2001DCD8 + 8 * 7, 7])[2],
         [(0x2001DCD0, 8), (0x2001DCD8 + 8 * 7, 8)])

    for prev_st in (0, 0x20002):
        safe("sched", f"spinlock_release_{prev_st}", 0x10930, "rkos_spinlock_release",
             lambda uc, ps=prev_st: (wr(uc, 0x2000084C, struct.pack("<I", 0x02000500)),
                                     wr(uc, 0x02000400, struct.pack("<I", 0x02000500)),
                                     [0x02000400, ps])[2], [(0x02000400, 4)])

    for lbl, t_prio, c_prio in [("preempt", 20, 10), ("no_preempt", 5, 10)]:
        safe("sched", f"task_clear_yield_{lbl}", 0x10968, "rkos_task_clear_yield_check_preempt",
             lambda uc, tp=t_prio, cp=c_prio: (
                 wr(uc, 0x02000114, bytes([tp, tp, 1])),
                 wr(uc, 0x02000214, bytes([cp, cp, 0])),
                 wr(uc, 0x2000084C, struct.pack("<I", 0x02000200)),
                 [0x02000100],
             )[3], [(0x02000114, 4)])

    for waiter_prio in (4, 25):
        def mk_recalc(wp: int = waiter_prio) -> Callable[[Uc], list[int]]:
            def _s(uc: Uc) -> list[int]:
                # task @ 0x02000100: cur_prio=10, base_prio=10
                wr(uc, 0x02000114, bytes([10, 10]))
                # lock @ 0x02000300 owned by 0x02000100
                wr(uc, 0x02000300, struct.pack("<I", 0x02000101))
                # waiter @ 0x02000200: next=0, +0x0C=0x02000300, +0x14=wp
                wr(uc, 0x02000200, b"\x00" * 0x20)
                wr(uc, 0x0200020C, struct.pack("<I", 0x02000300))
                wr(uc, 0x02000214, bytes([wp]))
                wr(uc, 0x2000083C, struct.pack("<I", 0x02000200))
                return [0x02000100]
            return _s
        safe("sched", f"task_recalc_prio_{waiter_prio}", 0x1099C, "rkos_task_recalc_priority",
             mk_recalc(), [(0x02000114, 2)])

    safe("sched", "mem_region_register_ok", 0x113E0, "rkos_mem_region_register",
         lambda uc: (guard(uc), [2, 0x40100000, 0x2000])[1], [(0x20001078, 0xC8), (heap_va, 0x68)])
    safe("sched", "mem_region_register_badslot", 0x113E0, "rkos_mem_region_register",
         lambda uc: (guard(uc), [9, 0x40100000, 0x2000])[1])

    safe("sched", "mem_region_apply_table_2ents", 0x11604, "rkos_mem_region_apply_table",
         lambda uc: (guard(uc),
                     wr(uc, 0x2001DE44, struct.pack("<II", 2, 0x02000800)),
                     wr(uc, 0x02000800, struct.pack("<IIIIIIIII",
                        1, 0x40100000, 0x40102000,
                        2, 0x40200000, 0x40204000,
                        0, 0, 0)),
                     [])[3], [(0x20001078, 0xC8), (heap_va, 0x68)])
