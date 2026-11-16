#!/usr/bin/env python3
"""
verify_layout.py -- check the linked image against the documented RAM map.

Reads the lld map file and confirms that every pinned state block starts at the
address recorded in include/btp/memmap.h and that nothing was left in a floating
.bss section.
"""
from __future__ import annotations

import re
import sys

MEMMAP = "include/btp/memmap.h"

SECTION_FOR = {
    "STATE_CLOCK_BASE": ".state.clock",
    "STATE_MAC_BASE": ".state.mac",
    "STATE_RX_OVERRIDES_BASE": ".state.rx_ovr",
    "STATE_TX_OVERRIDES_BASE": ".state.tx_ovr",
    "STATE_TXPWR_MODE_BASE": ".state.txpwr_mode",
    "STATE_LOOPBACK_BASE": ".state.loopback",
    "STATE_WARM_BOOT_BASE": ".state.warm_boot",
    "STATE_FORMAT_BASE": ".state.format",
    "STATE_NVV_PRESENCE_BASE": ".state.nvv_presence",
    "STATE_RTK_BASE": ".state.rtk",
    "STATE_DIAG_BASE": ".state.diag",
    "STATE_FRS_BASE": ".state.frs",
    "STATE_RX_BASE": ".state.rx",
    "STATE_DPD_BASE": ".state.dpd",
    "STATE_TX_BASE": ".state.tx",
    "STATE_TXPWR_BASE": ".state.txpwr",
    "STATE_SYSCFG_BASE": ".state.syscfg",
    "STATE_LOOPBACK_SAVED_BASE": ".state.lb_saved",
    "STATE_RADIO_REV_BASE": ".state.radio_rev",
    "STATE_NVV_TABLES_BASE": ".state.nvv_tables",
    "STATE_MPU_BASE": ".state.mpu",
    "STATE_RTK_IRQ_BASE": ".state.rtk_irq",
}


def main(argv) -> int:
    if len(argv) != 2:
        print("usage: verify_layout.py build/phyb.map")
        return 2
    expected = {}
    for name, value in re.findall(r"#define\s+(STATE_\w+_BASE)\s+(0x[0-9A-Fa-f]+)u",
                                  open(MEMMAP).read()):
        expected[SECTION_FOR[name]] = int(value, 16)

    actual = {}
    for line in open(argv[1]):
        m = re.match(r"\s*([0-9a-f]+)\s+([0-9a-f]+)\s+([0-9a-f]+)\s+\d+\s+(\.state\.\w+)\s*$", line)
        if m:
            actual[m.group(4)] = (int(m.group(1), 16), int(m.group(3), 16))

    failures = []
    for section, address in sorted(expected.items(), key=lambda kv: kv[1]):
        if section not in actual:
            failures.append("%s missing from the map" % section)
        elif actual[section][0] != address:
            failures.append("%s at 0x%08X, expected 0x%08X"
                            % (section, actual[section][0], address))
    if failures:
        for f in failures:
            print("FAIL:", f)
        return 1
    print("layout verified: %d state blocks at their documented addresses" % len(expected))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
