#!/usr/bin/env python3
"""Linker section geometry and pinned SRAM/ROM symbol audit for acib.elf."""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys

from elftools.elf.elffile import ELFFile

EXPECTED_SECTIONS = {
    ".vectors":           (0x00000000, 0x000000C0),
    ".rtkit.header":      (0x000000C0, 0x00000048),
    ".text.reset":        (0x00000108, None),
    ".vectors.secondary": (0x00000400, 0x00000240),
    ".text":              (0x00000680, None),
    ".text.fault":        (0x000E4A50, None),
    ".text.nmi":          (0x000E4A94, None),
    ".text.svcall":       (0x000E4B14, None),
    ".text.sleep_restore":(0x000E4BB0, None),
    ".text.systick":      (0x000E58F0, None),
    ".rom.rodata":        (0x000EFEA8, 0x000099A4),
    ".ram.data":          (0x10000000, 0x0000AC1E),
}

EXPECTED_PINNED_SYMBOLS = {
    "g_acib_vector_table":     0x00000000,
    "Reset_Handler":           0x00000108,
    "Fault_Handler":           0x000E4A50,
    "NMI_Handler":             0x000E4A94,
    "SVCall_Handler":          0x000E4B14,
    "rtk_sleep_restore_context": 0x000E4BB0,
    "SysTick_Handler":         0x000E58F0,
    "__bss_start":             0x1000AC1E,
    "__bss_zero_start":        0x1000AC40,
    "__bss_zero_end":          0x1002D2C4,
    "g_rtk_irq_handlers":      0x1000EEB4,
    "g_rtk_irq_contexts":      0x1000F0BC,
    "g_psp_init_stack":        0x1001AF80,
    "g_msp_stack":             0x1001B580,
    "g_boot_mode_ctx":         0x1002006C,
    "g_bt_sys_threads_state":  0x10020310,
    "g_acib_clock_ctx":        0x10020968,
    "g_acib_panic_reason":     0x10020B60,
    "g_bt_ctrl_ctx":           0x10020B78,
    "g_osa_task_table":        0x10021228,
    "g_phy_crash_log_ctx":     0x100217CC,
    "g_rtk_thread_ptr_table":  0x10021C04,
    "g_phy_table_shadow":      0x10021EA9,
    "g_phy_ipc_state":         0x1002268C,
    "g_acib_board_props":      0x10022984,
    "g_rf_tx_mode_ctx":        0x10027958,
    "g_phy_ack_ctx":           0x1002A168,
    "g_tm_default_cb":         0x1002CF68,
    "g_rtk_nmi_saved_sp":      0x1002CFB4,
    "g_rtk_ready_mask":        0x1002D0C8,
    "g_rtk_ready_queues":      0x1002D0D0,
    "g_rtk_sched_need_resched":0x1002D240,
    "g_rtk_sched_preempted":   0x1002D244,
    "g_rtk_irq_mask_basepri":  0x1002D2BC,
    "g_rtk_irq_prio_group":    0x1002D2C0,
}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--elf", type=Path, required=True)
    parser.add_argument("--map", type=Path, required=True)
    parser.add_argument("--inventory", type=Path, required=True)
    args = parser.parse_args()

    if not args.map.is_file():
        print(f"ERROR: missing linker map {args.map}", file=sys.stderr)
        return 1

    _ = json.loads(args.inventory.read_text(encoding="utf-8"))
    errors: list[str] = []

    with args.elf.open("rb") as fp:
        elf = ELFFile(fp)
        if elf["e_machine"] != "EM_ARM":
            errors.append(f"unexpected ELF machine {elf['e_machine']}")

        for sec_name, (exp_addr, exp_size) in EXPECTED_SECTIONS.items():
            sec = elf.get_section_by_name(sec_name)
            if sec is None:
                errors.append(f"missing section {sec_name}")
                continue
            actual_addr = sec["sh_addr"]
            actual_size = sec["sh_size"]
            if actual_addr != exp_addr:
                errors.append(
                    f"section {sec_name} VMA 0x{actual_addr:08x} != expected 0x{exp_addr:08x}"
                )
            if exp_size is not None and actual_size != exp_size:
                errors.append(
                    f"section {sec_name} size 0x{actual_size:x} != expected 0x{exp_size:x}"
                )
            if sec_name == ".text" and (actual_addr + actual_size) > 0x000E4A50:
                errors.append(
                    f".text overflows into .text.fault (end 0x{actual_addr + actual_size:08x})"
                )

        symtab = elf.get_section_by_name(".symtab")
        if symtab is None:
            errors.append("missing .symtab in ELF")
        else:
            syms = {s.name: s["st_value"] for s in symtab.iter_symbols() if s.name}
            for sym_name, exp_vma in EXPECTED_PINNED_SYMBOLS.items():
                if sym_name not in syms:
                    errors.append(f"missing pinned symbol {sym_name}")
                    continue
                actual_vma = syms[sym_name]
                # Function symbols in Thumb have bit 0 set
                if exp_vma < 0x00100000 and sym_name != "g_acib_vector_table":
                    actual_vma &= ~1
                if actual_vma != exp_vma:
                    errors.append(
                        f"symbol {sym_name} @ 0x{actual_vma:08x} != expected 0x{exp_vma:08x}"
                    )

            unp_s = syms.get("__unpinned_bss_start", -1)
            unp_e = syms.get("__unpinned_bss_end", -2)
            if unp_s != unp_e:
                errors.append(
                    f"unpinned BSS non-empty: 0x{unp_s:08x}..0x{unp_e:08x}"
                )

    if errors:
        for err in errors:
            print(f"ERROR: {err}", file=sys.stderr)
        return 1

    print(
        f"verify_layout: PASS ({len(EXPECTED_SECTIONS)} sections & "
        f"{len(EXPECTED_PINNED_SYMBOLS)} pinned VMAs verified, unpinned_bss=0)"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
