#!/usr/bin/env python3
"""Generate targets/acib/verification/inventory.json and ledger.json from acib + IDB exports."""

from __future__ import annotations

import hashlib
import json
import struct
import uuid
from pathlib import Path

ACIB_DIR = Path(__file__).resolve().parents[1]
RAW_IMAGE = ACIB_DIR.parents[2] / "acib"
IDA_EXPORT_DIR = Path("/tmp/acib-ida")
VERIF_DIR = ACIB_DIR / "verification"

RETAINED_NATIVE: dict[int, tuple[str, str]] = {
    0x0E4A94: (
        "src/rtkit/exception.c",
        "NMI naked veneer (.text.nmi, 128 bytes @ 0xe4a94..0xe4b14 = 114B body + 14B literal pool): 100% byte-identical to raw acib[0xe4a94:0xe4b14] and verified by verify_load_image.py",
    ),
}

RECONSTRUCTED_C: dict[int, tuple[str, str]] = {
    # Boot, MPU, DWT, Clock, Board Props & Stack Watermark (20)
    0x0706D0: ("src/rtkit/boot.c", "acib_mpu_default_attr"),
    0x0706D8: ("src/rtkit/boot.c", "acib_mpu_enable"),
    0x0706E8: ("src/rtkit/boot.c", "acib_mpu_disable"),
    0x0706F8: ("src/rtkit/boot.c", "acib_mpu_config_region"),
    0x07DEB8: ("src/rtkit/boot.c", "rtk_early_hw_init"),
    0x070734: ("src/rtkit/boot.c", "acib_dwt_cyccnt_read"),
    0x070788: ("src/rtkit/boot.c", "acib_get_calibrated_cpu_hz"),
    0x0707A4: ("src/rtkit/boot.c", "acib_measure_cpu_hz"),
    0x07DE44: ("src/rtkit/boot.c", "acib_clear_sleep_deadline"),
    0x07DE50: ("src/rtkit/boot.c", "acib_clock_timing_init"),
    0x07DDF0: ("src/rtkit/boot.c", "acib_load_rtkit_board_props"),
    0x07E18C: ("src/rtkit/boot.c", "acib_board_set_flags"),
    0x07E1A4: ("src/rtkit/boot.c", "acib_board_has_flag_bit0"),
    0x07E1B8: ("src/rtkit/boot.c", "acib_board_get_rev_nibble"),
    0x07E1C8: ("src/rtkit/boot.c", "acib_soc_is_fpga_emulation"),
    0x070BB4: ("src/rtkit/boot.c", "rtk_init_stacks_watermark"),
    0x0E60A8: ("src/rtkit/boot.c", "rtk_get_wake_count"),
    0x0E60B4: ("src/rtkit/boot.c", "rtk_get_sleep_count"),
    0x0E60C0: ("src/rtkit/boot.c", "rtk_stack_watermark_fill"),
    0x0E60DC: ("src/rtkit/boot.c", "rtk_stack_watermark_check"),
    # Exception, Panic & Stack Guard (8)
    0x0E4C48: ("src/rtkit/exception.c", "__stack_chk_fail"),
    0x06BF30: ("src/rtkit/exception.c", "rtk_abort_panic"),
    0x071230: ("src/rtkit/exception.c", "acib_panic_null"),
    0x0725FC: ("src/rtkit/exception.c", "acib_panic_code_line"),
    0x072614: ("src/rtkit/exception.c", "acib_panic_code_group"),
    0x07264C: ("src/rtkit/exception.c", "acib_ctx_store_fault_arg"),
    0x0E6040: ("src/rtkit/exception.c", "rtk_fault_extract_frame"),
    0x0E6474: ("src/rtkit/exception.c", "rtk_unwind_adjust_lr"),
    # IRQ, PRIMASK/BASEPRI, NVIC, SysTick & Scheduler (21)
    0x0E5844: ("src/rtkit/irq.c", "rtk_irq_mask_save"),
    0x0E585C: ("src/rtkit/irq.c", "rtk_irq_mask_restore"),
    0x0E5862: ("src/rtkit/irq.c", "rtk_primask_disable_save"),
    0x0E5874: ("src/rtkit/irq.c", "rtk_primask_restore"),
    0x0E587E: ("src/rtkit/irq.c", "rtk_irq_enable_all"),
    0x0E588C: ("src/rtkit/irq.c", "rtk_irq_are_enabled"),
    0x0E58AC: ("src/rtkit/irq.c", "rtk_irq_set_prio_group"),
    0x0E58DC: ("src/rtkit/irq.c", "rtk_cpu_in_thread_mode"),
    0x0EF0DC: ("src/rtkit/irq.c", "outlined_sub_EF0DC"),
    0x0E6104: ("src/rtkit/irq.c", "rtk_spinlock_try_enter"),
    0x0E58F0: ("src/rtkit/irq.c", "SysTick_Handler"),
    0x0E5908: ("src/rtkit/irq.c", "rtk_sched_switch"),
    0x0E59A8: ("src/rtkit/irq.c", "rtk_nvic_method_4"),
    0x0E59B0: ("src/rtkit/irq.c", "rtk_nvic_method_5"),
    0x0E5A34: ("src/rtkit/irq.c", "rtk_nvic_method_7"),
    0x0E5A78: ("src/rtkit/irq.c", "rtk_nvic_method_8"),
    0x0E5B0C: ("src/rtkit/irq.c", "rtk_nvic_dispatch_irq"),
    0x0E5B50: ("src/rtkit/irq.c", "IRQ16_Handler"),
    0x0E5BA0: ("src/rtkit/irq.c", "rtk_nvic_method_15"),
    0x0E5BE8: ("src/rtkit/irq.c", "rtk_nvic_method_6"),
    0x0E5C4C: ("src/rtkit/irq.c", "rtk_nvic_method_9"),
    # Libc & AEABI (9)
    0x0E4C58: ("src/libc/string.c", "acib_memchr"),
    0x0E4C7C: ("src/libc/string.c", "acib_memcmp"),
    0x0E4DA8: ("src/libc/string.c", "memcpy_wrapper"),
    0x0E4DB0: ("src/libc/string.c", "memmove"),
    0x0E4EC0: ("src/libc/string.c", "memset"),
    0x0E4F40: ("src/libc/string.c", "acib_strlen"),
    0x0E4FC8: ("src/libc/string.c", "acib_strncmp"),
    0x0E5094: ("src/libc/string.c", "acib_aeabi_llsl"),
    0x0E53DC: ("src/libc/string.c", "acib_aeabi_llsr"),
    # MAC <-> PHY IPC, Mailbox, Dekker Spinlock, FRS & RF Control (35)
    0x07A0DC: ("src/phy/phy_ipc.c", "phy_hw_trig_frs_start_arm"),
    0x07A104: ("src/phy/phy_ipc.c", "phy_hw_trig_frs_abort_arm"),
    0x07C820: ("src/phy/phy_ipc.c", "phy_ipc_init"),
    0x07CA30: ("src/phy/phy_ipc.c", "phy_ipc_is_ready"),
    0x07CA64: ("src/phy/phy_ipc.c", "phy_ipc_cmd_set_op2"),
    0x07CA78: ("src/phy/phy_ipc.c", "phy_ipc_cmd_set_op3_u16"),
    0x07CA8C: ("src/phy/phy_ipc.c", "phy_ipc_noop"),
    0x07CA90: ("src/phy/phy_ipc.c", "phy_ipc_cmd_set_op6_mode"),
    0x07CABC: ("src/phy/phy_ipc.c", "phy_ipc_wait_and_ack_cmd"),
    0x07CB20: ("src/phy/phy_ipc.c", "phy_ipc_exec_cmd"),
    0x07CB4C: ("src/phy/phy_ipc.c", "phy_ipc_ring_cmd_doorbell"),
    0x07CB70: ("src/phy/phy_ipc.c", "phy_ipc_clear_cmd_status_flag"),
    0x07CB80: ("src/phy/phy_ipc.c", "phy_ipc_query_op3_ready"),
    0x07CBC0: ("src/phy/phy_ipc.c", "phy_ipc_complete_async_cmd"),
    0x07CD5C: ("src/phy/phy_ipc.c", "phy_ipc_irq_handler"),
    0x07D234: ("src/phy/phy_ipc.c", "phy_aci_if_enable_gates"),
    0x07D258: ("src/phy/phy_ipc.c", "phy_ipc_get_cached_version"),
    0x07D264: ("src/phy/phy_ipc.c", "phy_ipc_wait_warm_boot_ack"),
    0x07D2EC: ("src/phy/phy_ipc.c", "phy_ipc_cmd_op9"),
    0x07D300: ("src/phy/phy_ipc.c", "phy_ipc_cmd_op10"),
    0x07D314: ("src/phy/phy_ipc.c", "phy_ipc_cmd_op11_u8"),
    0x07D328: ("src/phy/phy_ipc.c", "phy_ipc_cmd_op12_u8"),
    0x07D33C: ("src/phy/phy_ipc.c", "phy_ipc_cmd_op13_pair"),
    0x07D35C: ("src/phy/phy_ipc.c", "phy_ipc_frs_prepare_scan"),
    0x07D424: ("src/phy/phy_ipc.c", "phy_ipc_frs_validate_param"),
    0x07D470: ("src/phy/phy_ipc.c", "phy_ipc_frs_is_scan_complete"),
    0x07D9D4: ("src/phy/phy_ipc.c", "phy_ipc_frs_run_scan"),
    0x07DB44: ("src/phy/phy_ipc.c", "phy_ipc_coex_ring_doorbell"),
    0x07DB60: ("src/phy/phy_ipc.c", "phy_ipc_frs_copy_results"),
    0x07DBBC: ("src/phy/phy_ipc.c", "phy_ipc_frs_read_window"),
    0x07DBE4: ("src/phy/phy_ipc.c", "phy_ipc_coex_noop"),
    0x07DCA8: ("src/phy/phy_ipc.c", "phy_ipc_coex_get_active"),
    0x084E74: ("src/phy/phy_ipc.c", "phy_ipc_on_cmd_ack_clear_flags"),
    0x084EB8: ("src/phy/phy_ipc.c", "phy_ipc_sync_shared_table_dekker"),
    0x097920: ("src/phy/phy_ipc.c", "phy_seq_rf_ctrl_apply_tx_mode"),
    # ACI, Trace, OSA & BT Controller (27)
    0x0E44A0: ("src/aci/trace.c", "tm_buffer_util_cb_init"),
    0x0E44C0: ("src/aci/trace.c", "tm_buffer_util_get_free"),
    0x0E44CC: ("src/aci/trace.c", "tm_buffer_util_decrement_remaining"),
    0x0E44EC: ("src/aci/trace.c", "tm_buffer_util_get_used"),
    0x0E44F0: ("src/aci/trace.c", "tm_buffer_util_reset"),
    0x0E44F8: ("src/aci/trace.c", "tm_buffer_util_bind"),
    0x0E4504: ("src/aci/trace.c", "tm_buffer_util_has_storage"),
    0x0E450E: ("src/aci/trace.c", "tm_buffer_util_write_ptr"),
    0x0E451C: ("src/aci/trace.c", "tm_buffer_util_base_ptr"),
    0x0E47D0: ("src/aci/trace.c", "tm_pkt_write_hdr4"),
    0x0E47F8: ("src/aci/trace.c", "tm_pkt_write_hdr8"),
    0x0E4828: ("src/aci/trace.c", "tm_pkt_write_ts_event"),
    0x072570: ("src/aci/osa.c", "osa_buf_tag_owner"),
    0x072580: ("src/aci/osa.c", "osa_crash_copy_recent_events"),
    0x0727B4: ("src/aci/osa.c", "osa_task_clear_pending_flag"),
    0x072940: ("src/aci/osa.c", "osa_mbx_get_relay_dest"),
    0x0729AC: ("src/aci/osa.c", "osa_task_get_msg_count"),
    0x020A58: ("src/aci/bt_ctrl.c", "hci_test_mode_get_opcode_from_type"),
    0x0AEF9E: ("src/aci/bt_ctrl.c", "bt_bdaddr_swap"),
    0x078D44: ("src/aci/bt_ctrl.c", "bt_ts_ring_record"),
    0x078D84: ("src/aci/bt_ctrl.c", "bt_ts_ring_record_alt"),
    0x071A74: ("src/aci/bt_ctrl.c", "acib_set_bt_ctrl_mode"),
    0x071A84: ("src/aci/bt_ctrl.c", "acib_get_bt_ctrl_mode"),
    0x071A94: ("src/aci/bt_ctrl.c", "acib_get_conn_slot_state"),
    0x070BF0: ("src/aci/bt_ctrl.c", "acib_log_build_banner_and_start"),
    0x071A50: ("src/aci/bt_ctrl.c", "main_entry"),
    0x002EA8: ("src/aci/bt_ctrl.c", "bt_system_thread_init"),
}


def cstr(buf: bytes, off: int) -> str:
    end = buf.find(b"\x00", off)
    if end < 0:
        end = len(buf)
    return buf[off:end].decode("latin1")


def parse_fwsg(raw: bytes) -> dict:
    footer_off = len(raw) - 0x20
    magic, ver, table_off, count = struct.unpack_from("<4sIII", raw, footer_off)
    segments = []
    cur = table_off
    for i in range(count):
        vma, foff, filesz, memsz, flags, seg_name_raw = struct.unpack_from("<QIIII8s", raw, cur)
        seg_name = seg_name_raw.split(b"\x00", 1)[0].decode("ascii", errors="replace")
        segments.append(
            {
                "index": i,
                "name": seg_name,
                "file_offset": f"0x{foff:x}",
                "vma": f"0x{vma:x}",
                "file_size": f"0x{filesz:x}",
                "mem_size": f"0x{memsz:x}",
                "flags": flags,
            }
        )
        cur += 0x20
    return {
        "descriptors_offset": f"0x{table_off:x}",
        "footer_offset": f"0x{footer_off:x}",
        "magic": magic.decode("ascii"),
        "version": ver,
        "entry_size": 32,
        "segment_count": count,
        "segments": segments,
    }


def parse_rtkit_header(raw: bytes) -> dict:
    magic, version, rt_str_ptr, prod_str_ptr = struct.unpack_from("<4sIII", raw, 0xC0)
    u = uuid.UUID(bytes=raw[0xD0:0xE0])
    rsv0, rsv1, meta_ptr, meta_len, rom_end, rsv2, rsv3, rsv4, rt_dup, prod_dup = struct.unpack_from(
        "<10I", raw, 0xE0
    )
    tlvs = []
    pos = meta_ptr
    end_meta = meta_ptr + meta_len
    while pos + 8 <= end_meta:
        tag_le = raw[pos : pos + 4]
        fourcc = tag_le[::-1].decode("latin1")
        vlen = struct.unpack_from("<I", raw, pos + 4)[0]
        val_bytes = raw[pos + 8 : pos + 8 + vlen]
        rec: dict[str, object] = {
            "rva": f"0x{pos:x}",
            "fourcc": fourcc,
            "length": vlen,
            "raw_hex": val_bytes.hex(),
        }
        if vlen == 4:
            rec["u32"] = f"0x{struct.unpack('<I', val_bytes)[0]:08x}"
        elif vlen == 1:
            rec["u8"] = val_bytes[0]
        tlvs.append(rec)
        # SLID/ASRS/ASRD carry 8-byte payload + 1-byte flag in RTKit TLV packing
        pos += 8 + vlen
        if fourcc in ("SLID", "ASRS", "ASRD") and pos < end_meta and raw[pos] == 0:
            rec["trailing_flag_u8"] = raw[pos]
            pos += 1

    ctors = [f"0x{w:08x}" for w in struct.unpack_from("<61I", raw, 0xF9758)]
    return {
        "offset": "0xc0",
        "size": 72,
        "magic": magic.decode("ascii"),
        "version": version,
        "uuid": str(u),
        "runtime_version_rva": f"0x{rt_str_ptr:x}",
        "runtime_version": cstr(raw, rt_str_ptr),
        "product_version_rva": f"0x{prod_str_ptr:x}",
        "product_version": cstr(raw, prod_str_ptr),
        "metadata_rva": f"0x{meta_ptr:x}",
        "metadata_size": f"0x{meta_len:x}",
        "rom_image_end": f"0x{rom_end:x}",
        "reserved_words": [f"0x{x:x}" for x in (rsv0, rsv1, rsv2, rsv3, rsv4)],
        "duplicate_version_rvas": [f"0x{rt_dup:x}", f"0x{prod_dup:x}"],
        "metadata_tlvs": tlvs,
        "constructor_table_rva": "0xf9758",
        "constructor_table_end": "0xf984c",
        "constructor_count": len(ctors),
        "constructors": ctors,
    }


def parse_vectors(raw: bytes) -> dict:
    prim = struct.unpack_from("<48I", raw, 0x0)
    core_names = [
        "Initial_MSP",
        "Reset",
        "NMI",
        "HardFault",
        "MemManage",
        "BusFault",
        "UsageFault",
        "Reserved_7",
        "Reserved_8",
        "Reserved_9",
        "Reserved_10",
        "SVCall",
        "DebugMonitor",
        "Reserved_13",
        "PendSV",
        "SysTick",
    ]
    primary_entries = []
    for idx, val in enumerate(prim):
        label = core_names[idx] if idx < 16 else f"IRQ{idx - 16}"
        entry = {"index": idx, "offset": f"0x{idx * 4:02x}", "label": label, "raw": f"0x{val:08x}"}
        if idx >= 1 and val != 0 and idx <= 15:
            entry["target_rva"] = f"0x{val & ~1:x}"
        primary_entries.append(entry)

    sec = struct.unpack_from("<144I", raw, 0x400)
    sec_unique = sorted({f"0x{w:08x}" for w in sec})
    return {
        "primary_vtor": "0x0",
        "primary_size": "0xc0",
        "primary_entry_count": 48,
        "primary_entries": primary_entries,
        "boot_banner_metadata_words": {
            "vector_17_offset_0x44": f"0x{prim[17]:08x}",
            "vector_18_offset_0x48": f"0x{prim[18]:08x}",
            "note": "Vector [18] = 0x0003005b encodes protocol version 3.91 (major=3, minor=0x5b=91), matching product string '3.91.1.0' and MAC->PHY handshake op 0.",
        },
        "secondary_vtor": "0x400",
        "secondary_size": "0x240",
        "secondary_entry_count": 144,
        "secondary_external_irq_count": 128,
        "secondary_unique_words": sec_unique,
        "secondary_summary": "Activated via SCB->VTOR = 0x400 in sub_707F0 (0x709dc); entries [15..143] route SysTick and all 128 external IRQs through unified RTKit entry 0xe58f1 -> rtk_nvic_dispatch_irq (0xe5b0c).",
    }


def main() -> int:
    raw = RAW_IMAGE.read_bytes()
    sha256 = hashlib.sha256(raw).hexdigest()

    segments = json.loads((IDA_EXPORT_DIR / "segments.json").read_text())
    functions = json.loads((IDA_EXPORT_DIR / "functions.json").read_text())
    strings = json.loads((IDA_EXPORT_DIR / "strings.json").read_text())
    mmio_xrefs = json.loads((IDA_EXPORT_DIR / "mmio_xrefs.json").read_text())

    fn_by_rva = {int(f["start"], 16): f for f in functions}

    overlap = set(RETAINED_NATIVE) & set(RECONSTRUCTED_C)
    if overlap:
        raise RuntimeError(f"Overlap between RETAINED_NATIVE and RECONSTRUCTED_C: {overlap}")
    for rva in list(RETAINED_NATIVE) + list(RECONSTRUCTED_C):
        if rva not in fn_by_rva:
            raise RuntimeError(f"RVA 0x{rva:x} missing from IDB functions.json")

    # Classify strings: build/version, source/function assertions, thread names
    build_strings = []
    panic_assert_strings = []
    thread_and_module_strings = []
    for s in strings:
        ea = int(s["ea"], 16) if isinstance(s.get("ea"), str) else s.get("ea", s.get("address", 0))
        if isinstance(ea, str):
            ea = int(ea, 16)
        val = s.get("content", s.get("value", s.get("string", s.get("text", ""))))
        rec = {"ea": f"0x{ea:x}", "value": val}
        low = val.lower()
        if any(k in val for k in ("RTKit", "Built on", "SDK version", "3.91.1.0", "b515d", "Apple")):
            build_strings.append(rec)
        if any(
            k in low
            for k in (
                "invalid",
                "fail",
                "unexpected",
                "unknown",
                "assert",
                "panic",
                "stk_fail",
                "must be",
                "error",
                "overflow",
                "attempt to",
            )
        ):
            panic_assert_strings.append(rec)
        if any(k in low for k in ("_thread", "thread", "osa_", "hci_", "tm_buffer", "tcm_debug", ".c")):
            thread_and_module_strings.append(rec)

    # Summarize MMIO apertures using IDB segment ranges
    seg_ranges = [
        (int(sg["start"], 16), int(sg["end"], 16), sg["name"])
        for sg in segments
    ]
    aperture_counts: dict[str, int] = {}
    phy_shared_targets: dict[str, list[str]] = {}
    for x in mmio_xrefs:
        target = x.get("target", x.get("to", ""))
        t_int = int(target, 16) if isinstance(target, str) else int(target)
        seg_name = "UNMAPPED_MMIO"
        for s_lo, s_hi, s_nm in seg_ranges:
            if s_lo <= t_int < s_hi:
                seg_name = s_nm
                break
        aperture_counts[seg_name] = aperture_counts.get(seg_name, 0) + 1
        if 0x4D154000 <= t_int < 0x4D281000:
            key = f"0x{t_int:08x}"
            raw_func = x.get("from_func", x.get("func", x.get("from", "?")))
            if isinstance(raw_func, str) and raw_func.startswith("0x"):
                fn_info = fn_by_rva.get(int(raw_func, 16))
                caller = f"{fn_info['name']} ({raw_func})" if fn_info else raw_func
            else:
                caller = str(raw_func)
            phy_shared_targets.setdefault(key, [])
            if caller not in phy_shared_targets[key]:
                phy_shared_targets[key].append(caller)

    inv_functions = []
    ledger_functions = []
    for f in sorted(functions, key=lambda item: int(item["start"], 16)):
        rva = int(f["start"], 16)
        end = int(f["end"], 16)
        size = int(f["size"])
        name = f["name"]
        inv_functions.append(
            {
                "rva": f"0x{rva:x}",
                "end": f"0x{end:x}",
                "size": size,
                "instructions": f.get("instructions"),
                "name": name,
                "user_named": bool(f.get("user_named")),
            }
        )
        if rva in RECONSTRUCTED_C:
            src_file, c_symbol = RECONSTRUCTED_C[rva]
            ledger_functions.append(
                {
                    "rva": f"0x{rva:x}",
                    "size": size,
                    "ida_name": name,
                    "c_symbol": c_symbol,
                    "status": "reconstructed_c",
                    "source_file": src_file,
                }
            )
        elif rva in RETAINED_NATIVE:
            src_file, reason = RETAINED_NATIVE[rva]
            ledger_functions.append(
                {
                    "rva": f"0x{rva:x}",
                    "size": size,
                    "ida_name": name,
                    "status": "retained_native",
                    "source_file": src_file,
                    "reason": reason,
                }
            )
        else:
            ledger_functions.append(
                {
                    "rva": f"0x{rva:x}",
                    "size": size,
                    "ida_name": name,
                    "status": "pending_c",
                }
            )

    inventory = {
        "target": "acib",
        "role": "Apple ACI / Bluetooth Controller MAC Firmware (b515d / AirPods3,6)",
        "cpu_abi": {
            "architecture": "ARMv7E-M (Thumb-2)",
            "cpu": "cortex-m4",
            "float_abi": "soft (no hardware FPU instructions; AEABI double helpers in ROM 0xe50b8..0xe564b, integer-only MSP/PSP exception frames)",
            "clang_flags": ["-target", "armv7em-none-eabi", "-mcpu=cortex-m4", "-mfloat-abi=soft"],
        },
        "raw_binary": {
            "path": "../../../acib",
            "size_bytes": len(raw),
            "size_hex": f"0x{len(raw):x}",
            "sha256": sha256,
            "file_layout": [
                {"file_range": "0x000000..0x0000c0", "vma_range": "0x00000000..0x000000c0", "section": "ROM_VECTORS", "size": "0xc0"},
                {"file_range": "0x0000c0..0x000108", "vma_range": "0x000000c0..0x00000108", "section": "RTKIT_UUID_HEADER", "size": "0x48"},
                {"file_range": "0x000108..0x0efea8", "vma_range": "0x00000108..0x000efea8", "section": "ROM_TEXT", "size": "0xefda0"},
                {"file_range": "0x0efea8..0x0f984c", "vma_range": "0x000efea8..0x000f984c", "section": "ROM_RODATA", "size": "0x99a4"},
                {"file_range": "0x0f984c..0x0f9860", "vma_range": None, "section": "FILE_ALIGN_PAD_1", "size": "0x14"},
                {"file_range": "0x0f9860..0x10447e", "vma_range": "0x10000000..0x1000ac1e", "section": "RAM_DATA", "size": "0xac1e"},
                {"file_range": "0x10447e..0x104480", "vma_range": None, "section": "FILE_ALIGN_PAD_2", "size": "0x2"},
                {"file_range": "0x104480..0x1044e0", "vma_range": None, "section": "FWSG_SEGMENT_TRAILER", "size": "0x60"},
                {"file_range": None, "vma_range": "0x1000ac1e..0x1002d2c4", "section": "RAM_BSS", "size": "0x226a6"},
            ],
        },
        "fwsg_trailer": parse_fwsg(raw),
        "rtkit_header": parse_rtkit_header(raw),
        "vector_tables": parse_vectors(raw),
        "idb_segments": segments,
        "mmio_summary": {
            "total_xrefs": len(mmio_xrefs),
            "by_segment": dict(sorted(aperture_counts.items())),
            "mac_phy_shared_interface": {
                "mailbox_ram_base": "0x4d280000",
                "mailbox_req_ptr_global": "0x1002268c (-> 0x4d280000)",
                "mailbox_resp_ptr_global": "0x10022690 (-> 0x4d280400 on B0 phyb; fallback 0x4d2801e6 on legacy PHY)",
                "mpu_region_5": "base=0x4d280000, size=4KB (order=0x0b), TEX/SCB=0x200000 (Normal Non-cacheable), XN=1, AP=1 (Priv RW)",
                "dekker_spinlock": "mac_want_lock @ 0x4d2800c0 (u16), phy_want_lock @ 0x4d2800c2 (u16), protects 0x120-byte shared link/channel table @ 0x4d2800c4..0x4d2801e3 (sub_84EB8)",
                "doorbell_and_rf_registers": {
                    "0x4d154038": "SEQ_RF_CTRL — read/masked by sub_976BC, sub_97920, sub_97D94 to coordinate RF TX/RX mode bits [29:27]",
                    "0x4d154c64": "PHY_IPC_TO_MAC_STATUS (W1C) — bit0=PHY crash (reads 5 words @ 0x4d2805c1..0x4d2805d4), bit1=warm-boot ack, bit2=FRS scan done, bit15=sync cmd response",
                    "0x4d154c68": "PHY_IPC_FROM_MAC_DOORBELL — bit0=FRS scan start (SW + HW timer DMA @ 0x7a0dc), bit1=FRS abort/timeout (SW + HW timer DMA @ 0x7a104), bit2=coex update (@ 0x7db44), bit15=sync cmd request (@ 0x7cb4c)",
                },
                "direct_mmio_xref_targets": phy_shared_targets,
            },
        },
        "build_and_version_strings": build_strings,
        "surviving_source_and_module_strings": thread_and_module_strings,
        "panic_and_assert_strings": panic_assert_strings,
        "total_strings": len(strings),
        "total_functions": len(inv_functions),
        "functions": inv_functions,
    }

    def count_odd_rom_code_ptrs(blob: bytes) -> int:
        n_words = len(blob) // 4
        words = struct.unpack_from(f"<{n_words}I", blob, 0)
        return sum(1 for w in words if (w & 1) == 1 and 0x109 <= w < 0xEFEA8)

    rodata_code_ptrs = count_odd_rom_code_ptrs(raw[0xEFEA8:0xF984C])
    sec_vtor_code_ptrs = count_odd_rom_code_ptrs(raw[0x400:0x640])
    ram_data_code_ptrs = count_odd_rom_code_ptrs(raw[0xF9860:0x10447E])

    ledger = {
        "target": "acib",
        "binary_sha256": sha256,
        "assumptions": [
            "CPU is ARMv7E-M soft-float (-target armv7em-none-eabi -mcpu=cortex-m4 -mfloat-abi=soft): zero VFP/FP instructions exist in acib; ROM 0xe50b8..0xe564b provides software IEEE-754 double routines and exception veneers save integer r4-r11+lr only.",
            "External loader (uarp/fwsg host loader) maps __TEXT (file 0x0..0xf984c -> VMA 0x0..0xf984c) and __DATA (file 0xf9860..0x10447e -> VMA 0x10000000..0x1000ac1e) directly before releasing Reset_Handler; Reset_Handler zeroes .bss (0x1000ac40..0x1002d2c4).",
            "MAC<->PHY IPC shares 4 KB mailbox SRAM at 0x4d280000 (MPU Region 5) and W3 PHY sequencer registers at 0x4d154000 (0x4d154038 SEQ_RF_CTRL, 0x4d154c64 PHY->MAC W1C status, 0x4d154c68 MAC->PHY doorbell) directly with phyb.",
            "Only functions reimplemented in idiomatic, warning-free C11 with @provenance tags and passing Unicorn differential + stub-mutation checks are marked reconstructed_c (120); only byte-exact unedited instruction slices verified against raw acib by verify_load_image.py are marked retained_native (1: NMI_Handler @ 0xe4a94, 114 B); all unported functions and relocated ASM entry veneers remain pending_c (9,796).",
        ],
        "known_deviations": {
            "relocated_entry_veneers": {
                "functions": [
                    {"rva": "0x108", "symbol": "Reset_Handler", "source_file": "src/rtkit/boot.c"},
                    {"rva": "0xe4a50", "symbol": "Fault_Handler", "source_file": "src/rtkit/exception.c"},
                    {"rva": "0xe4b14", "symbol": "SVCall_Handler", "source_file": "src/rtkit/exception.c"},
                    {"rva": "0xe4bb0", "symbol": "rtk_sleep_restore_context", "source_file": "src/rtkit/exception.c"},
                ],
                "status_in_ledger": "pending_c",
                "explanation": "Naked ARMv7-M stack-frame/bootstrap veneers whose bl targets or literal pools relocate to rebuilt .text addresses rather than matching raw ROM bytes; classified honestly as pending_c rather than retained_native or reconstructed_c.",
            },
            "binary_preserved_raw_code_pointers": {
                "rom_rodata_odd_code_words": rodata_code_ptrs,
                "secondary_vtor_odd_code_words": sec_vtor_code_ptrs,
                "ram_data_odd_code_words": ram_data_code_ptrs,
                "constructor_table": {
                    "vma_range": "0xf9758..0xf984c",
                    "count": 61,
                    "note": "61 raw Thumb constructor pointers in binary-preserved .rom.rodata point to unported pending_c RVAs in raw ROM.",
                },
                "secondary_vtor_note": "Entries [15..143] in .vectors.secondary (0x400..0x640) hold 0x000e58f1, which matches SysTick_Handler pinned at VMA 0x000e58f0 in link/acib.ld; entries [1..6, 11, 14] hold raw exception veneer addresses.",
                "explanation": "Binary-preserved .rom.rodata (0xefea8..0xf984c) and .ram.data (0x10000000..0x1000ac1e) retain raw ROM byte-identity for load-image verification; embedded vtables, callback tables, and C++ static constructors inside those blobs still hold raw Thumb RVAs until full-image symbolization.",
            },
        },
        "summary": {
            "total_functions": len(ledger_functions),
            "reconstructed_c": len(RECONSTRUCTED_C),
            "retained_native": len(RETAINED_NATIVE),
            "pending_c": len(ledger_functions) - len(RECONSTRUCTED_C) - len(RETAINED_NATIVE),
        },
        "functions": ledger_functions,
    }

    VERIF_DIR.mkdir(parents=True, exist_ok=True)
    (VERIF_DIR / "inventory.json").write_text(json.dumps(inventory, indent=2) + "\n")
    (VERIF_DIR / "ledger.json").write_text(json.dumps(ledger, indent=2) + "\n")
    print(
        f"Wrote inventory.json ({len(inv_functions)} functions, {len(strings)} strings) "
        f"and ledger.json (reconstructed_c={len(RECONSTRUCTED_C)}, "
        f"retained_native={len(RETAINED_NATIVE)}, "
        f"pending_c={ledger['summary']['pending_c']})"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
